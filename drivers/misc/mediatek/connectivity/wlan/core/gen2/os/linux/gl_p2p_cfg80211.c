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

#include "config.h"

#if CFG_ENABLE_WIFI_DIRECT && CFG_ENABLE_WIFI_DIRECT_CFG_80211
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/wireless.h>
#include <linux/ieee80211.h>
#include <net/cfg80211.h>
#include <linux/can/netlink.h>
#include <net/netlink.h>

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
				       IN P_RF_CHANNEL_INFO_T prRfChnlInfo, IN P_ENUM_CHNL_EXT_T prChnlSco)
{
	if (channel == NULL)
		return FALSE;

	DBGLOG(P2P, INFO, "channel band: %d, freq: %d\n", channel->band, channel->center_freq);

	if (prRfChnlInfo) {
		prRfChnlInfo->ucChannelNum = nicFreq2ChannelNum(channel->center_freq * 1000);

		switch (channel->band) {
		case IEEE80211_BAND_2GHZ:
			prRfChnlInfo->eBand = BAND_2G4;
			break;
		case IEEE80211_BAND_5GHZ:
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
			*prChnlSco = CHNL_EXT_SCN;
			break;
		}
	}

	return TRUE;
}

int mtk_p2p_cfg80211_add_key(struct wiphy *wiphy,
			     struct net_device *ndev,
			     u8 key_index, bool pairwise, const u8 *mac_addr, struct key_params *params)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 i4Rslt = -EINVAL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	PARAM_KEY_T rKey;
	const UINT_8 aucBCAddr[] = BC_MAC_ADDR;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	DBGLOG(RSN, TRACE, "mtk_p2p_cfg80211_add_key\n");

	if (mac_addr) {
		DBGLOG(RSN, TRACE,
		   "keyIdx = %d pairwise = %d mac = " MACSTR "\n", key_index, pairwise,
			MAC2STR(mac_addr));
	} else {
		DBGLOG(RSN, TRACE, "keyIdx = %d pairwise = %d null mac\n", key_index,
			pairwise);
	}
	DBGLOG(RSN, TRACE, "Cipher = %x\n", params->cipher);
	DBGLOG_MEM8(RSN, TRACE, params->key, params->key_len);

	if (params->key_len > 32) {
		DBGLOG(RSN, WARN, "key_len [%d] is invalid!\n",
			params->key_len);
		return -EINVAL;
	}

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


	/* For BC addr case: ex: AP mode,
	 * driver_nl80211 will not have mac_addr
	 */
	if (pairwise) {
		rKey.u4KeyIndex |= BIT(31);	/* Tx */
		rKey.u4KeyIndex |= BIT(30);	/* Pairwise */
		COPY_MAC_ADDR(rKey.arBSSID, mac_addr);
	} else {
		COPY_MAC_ADDR(rKey.arBSSID, aucBCAddr);
	}

	/* Check if add key under AP mode */
	if (kalP2PGetRole(prGlueInfo) == 2)
		rKey.u4KeyIndex |= BIT(28); /* authenticator */

	if (params->key) {
		/* rKey.aucKeyMaterial[0] = kalMemAlloc(params->key_len, VIR_MEM_TYPE); */
		kalMemCopy(rKey.aucKeyMaterial, params->key, params->key_len);
	}
	rKey.u4KeyLength = params->key_len;
	rKey.u4Length = ((ULONG)&(((P_PARAM_KEY_T) 0)->aucKeyMaterial)) + rKey.u4KeyLength;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetAddP2PKey, &rKey, rKey.u4Length, FALSE, FALSE, TRUE, TRUE, &u4BufLen);
	if (rStatus == WLAN_STATUS_SUCCESS)
		i4Rslt = 0;

	return i4Rslt;
}

int mtk_p2p_cfg80211_get_key(struct wiphy *wiphy,
			     struct net_device *ndev,
			     u8 key_index,
			     bool pairwise,
			     const u8 *mac_addr, void *cookie, void (*callback) (void *cookie, struct key_params *)
)
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
	PARAM_REMOVE_KEY_T prRemoveKey;
	INT_32 i4Rslt = -EINVAL;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	DBGLOG(P2P, INFO, "--> %s()\n", __func__);

	if (prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
		i4Rslt = 0;
		return i4Rslt;
	}

	kalMemZero(&prRemoveKey, sizeof(PARAM_REMOVE_KEY_T));
	if (mac_addr)
		memcpy(prRemoveKey.arBSSID, mac_addr, PARAM_MAC_ADDR_LEN);
	prRemoveKey.u4KeyIndex = key_index;
	prRemoveKey.u4Length = sizeof(PARAM_REMOVE_KEY_T);

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetRemoveP2PKey,
			   &prRemoveKey, prRemoveKey.u4Length, FALSE, FALSE, TRUE, TRUE, &u4BufLen);

	if (rStatus == WLAN_STATUS_SUCCESS)
		i4Rslt = 0;

	return i4Rslt;
}

int
mtk_p2p_cfg80211_set_default_key(struct wiphy *wiphy,
				 struct net_device *netdev, u8 key_index, bool unicast, bool multicast)
{
	P_GLUE_INFO_T prGlueInfo = NULL;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	/* not implemented yet */

	return WLAN_STATUS_SUCCESS;
}

int mtk_p2p_cfg80211_set_mgmt_key(struct wiphy *wiphy, struct net_device *dev
, u8 key_index)
{
	DBGLOG(RSN, INFO, "mtk_p2p_cfg80211_set_mgmt_key, kid:%d\n", key_index);

	return 0;
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

		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_get_station\n");

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

		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_scan\n");

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

		u4MsgSize = sizeof(MSG_P2P_SCAN_REQUEST_T) +
		    (request->n_channels * sizeof(RF_CHANNEL_INFO_T)) +
		    (request->n_ssids * sizeof(PARAM_SSID_T)) + request->ie_len;

		prMsgScanRequest = cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG, u4MsgSize);

		if (prMsgScanRequest == NULL) {
			DBGLOG(P2P, TRACE, "Allocate MsgScanRequest failed\n");
			prP2pGlueInfo->prScanRequest = NULL;
			i4RetRslt = -ENOMEM;
			break;
		}

		DBGLOG(P2P, TRACE, "Generating scan request message.\n");

		prMsgScanRequest->rMsgHdr.eMsgId = MID_MNY_P2P_DEVICE_DISCOVERY;

		DBGLOG(P2P, INFO, "Requesting channel number:%d.\n", request->n_channels);

		for (u4Idx = 0; u4Idx < request->n_channels; u4Idx++) {
			/* Translate Freq from MHz to channel number. */
			prRfChannelInfo = &(prMsgScanRequest->arChannelListInfo[u4Idx]);
			prChannel = request->channels[u4Idx];

			prRfChannelInfo->ucChannelNum = nicFreq2ChannelNum(prChannel->center_freq * 1000);
			DBGLOG(P2P, TRACE, "Scanning Channel: %d, freq: %d\n",
			       prRfChannelInfo->ucChannelNum, prChannel->center_freq);
			switch (prChannel->band) {
			case IEEE80211_BAND_2GHZ:
				prRfChannelInfo->eBand = BAND_2G4;
				break;
			case IEEE80211_BAND_5GHZ:
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
		if (prSsidStruct) {
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
			}
		}

		DBGLOG(P2P, TRACE, "Finish IE Buffer.\n");

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgScanRequest, MSG_SEND_METHOD_BUF);

		i4RetRslt = 0;
	} while (FALSE);

	return i4RetRslt;
}				/* mtk_p2p_cfg80211_scan */

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
void mtk_p2p_cfg80211_abort_scan(struct wiphy *wiphy, struct wireless_dev *wdev)
{
	UINT_32 u4SetInfoLen = 0;
	WLAN_STATUS rStatus;
	P_GLUE_INFO_T prGlueInfo = NULL;

	DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_abort_scan\n");

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	rStatus = kalIoctl(prGlueInfo,
					   wlanoidAbortP2pScan,
					   NULL, 1, FALSE, FALSE, TRUE, TRUE, &u4SetInfoLen);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(REQ, ERROR, "mtk_p2p_cfg80211_abort_scan fail 0x%x\n", rStatus);
}

int mtk_p2p_cfg80211_set_wiphy_params(struct wiphy *wiphy, u32 changed)
{
	INT_32 i4Rslt = -EINVAL;
	P_GLUE_INFO_T prGlueInfo = NULL;

	do {
		if (wiphy == NULL)
			break;

		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_set_wiphy_params\n");
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

int mtk_p2p_cfg80211_set_power_mgmt(struct wiphy *wiphy, struct net_device *dev, bool enabled, int timeout)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 value;
	UINT_32 u4Leng;

	ASSERT(wiphy);

	if (enabled)
		value = 2;
	else
		value = 0;
	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));
	DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_set_power_mgmt ps=%d.\n", enabled);

	/* p2p_set_power_save */
	kalIoctl(prGlueInfo, wlanoidSetP2pPowerSaveProfile, &value, sizeof(value), FALSE, FALSE, TRUE, TRUE, &u4Leng);
	return 0;
}

int mtk_p2p_cfg80211_start_ap(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_ap_settings *settings)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	INT_32 i4Rslt = -EINVAL;
	P_MSG_P2P_BEACON_UPDATE_T prP2pBcnUpdateMsg = (P_MSG_P2P_BEACON_UPDATE_T) NULL;
	P_MSG_P2P_START_AP_T prP2pStartAPMsg = (P_MSG_P2P_START_AP_T) NULL;
	PUINT_8 pucBuffer = (PUINT_8) NULL;

	do {
		if ((wiphy == NULL) || (settings == NULL))
			break;

		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_start_ap.\n");

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		/* switch netif on */
		netif_carrier_on(dev);

		mtk_p2p_cfg80211_set_channel(wiphy, &settings->chandef);

		prP2pBcnUpdateMsg = (P_MSG_P2P_BEACON_UPDATE_T) cnmMemAlloc(prGlueInfo->prAdapter,
									    RAM_TYPE_MSG,
									    (sizeof(MSG_P2P_BEACON_UPDATE_T) +
									     settings->beacon.head_len +
									     settings->beacon.tail_len));

		if (prP2pBcnUpdateMsg == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

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

		COPY_SSID(prP2pStartAPMsg->aucSsid, prP2pStartAPMsg->u2SsidLen, settings->ssid, settings->ssid_len);

		prP2pStartAPMsg->ucHiddenSsidType = settings->hidden_ssid;

		prP2pStartAPMsg->fgIsPrivacy = settings->privacy;

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prP2pStartAPMsg, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;

#if CFG_SPM_WORKAROUND_FOR_HOTSPOT
		if (glIsChipNeedWakelock(prGlueInfo))
			KAL_WAKE_LOCK(prGlueInfo->prAdapter, prGlueInfo->prAdapter->rApWakeLock);
#endif

	} while (FALSE);

	return i4Rslt;
}				/* mtk_p2p_cfg80211_start_ap */

int mtk_p2p_cfg80211_change_beacon(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_beacon_data *beacon)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	INT_32 i4Rslt = -EINVAL;
	P_MSG_P2P_BEACON_UPDATE_T prP2pBcnUpdateMsg = (P_MSG_P2P_BEACON_UPDATE_T) NULL;
	PUINT_8 pucBuffer = (PUINT_8) NULL;

	do {
		if ((wiphy == NULL) || (beacon == NULL))
			break;

		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_change_beacon.\n");

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		prP2pBcnUpdateMsg = (P_MSG_P2P_BEACON_UPDATE_T) cnmMemAlloc(prGlueInfo->prAdapter,
									    RAM_TYPE_MSG,
									    (sizeof(MSG_P2P_BEACON_UPDATE_T) +
									     beacon->head_len + beacon->tail_len));

		if (prP2pBcnUpdateMsg == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

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
#if CFG_SUPPORT_P2P_GO_OFFLOAD_PROBE_RSP
		if (beacon->proberesp_ies) {
			prP2pBcnUpdateMsg->pucProbeRsp = kalMemAlloc(beacon->proberesp_ies_len, VIR_MEM_TYPE);
			if (!prP2pBcnUpdateMsg->pucProbeRsp)
				return -ENOMEM;
			prP2pBcnUpdateMsg->u4ProbeRsp_len = beacon->proberesp_ies_len;
			kalMemCopy(prP2pBcnUpdateMsg->pucProbeRsp, beacon->proberesp_ies, beacon->proberesp_ies_len);
		}
#endif
		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prP2pBcnUpdateMsg, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;

	} while (FALSE);

	return i4Rslt;
}				/* mtk_p2p_cfg80211_change_beacon */

UINT_32
mtk_oid_stop_ap_role(P_ADAPTER_T prAdapter, void *pvSetBuffer,
	UINT_32 u4SetBufferLen, UINT_32 *pu4SetInfoLen)
{
	unsigned char u4Idx = 0;

	if ((prAdapter == NULL) || (pvSetBuffer == NULL)
		|| (pu4SetInfoLen == NULL))
		return WLAN_STATUS_FAILURE;

	*pu4SetInfoLen = sizeof(unsigned char);
	if (u4SetBufferLen < sizeof(unsigned char))
		return WLAN_STATUS_INVALID_LENGTH;

	ASSERT(pvSetBuffer);
	u4Idx = *(unsigned char *) pvSetBuffer;

	DBGLOG(INIT, TRACE, "ucRoleIdx = %d\n", u4Idx);

	p2pFsmRunEventStopAP(prAdapter, NULL);

	DBGLOG(INIT, INFO, "done, ucRoleIdx = %d\n", u4Idx);

	return 0;

}

int mtk_p2p_cfg80211_stop_ap(struct wiphy *wiphy, struct net_device *dev)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	INT_32 i4Rslt = -EINVAL;

	unsigned char u4Idx = 0;
	UINT_32 rStatus;
	UINT_32 u4BufLen;

	do {
		if (wiphy == NULL)
			break;

		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_stop_ap.\n");
		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));
		if (prGlueInfo == NULL)
			break;

		rStatus = kalIoctl(prGlueInfo,
			mtk_oid_stop_ap_role, &u4Idx,
			sizeof(unsigned char),
			FALSE, FALSE, TRUE, TRUE,
			&u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS)
			DBGLOG(REQ, ERROR, "stop ap fail 0x%x\n", rStatus);

		i4Rslt = 0;
#if CFG_SPM_WORKAROUND_FOR_HOTSPOT
		if (glIsChipNeedWakelock(prGlueInfo))
			KAL_WAKE_UNLOCK(prGlueInfo->prAdapter, prGlueInfo->prAdapter->rApWakeLock);
#endif
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
	DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_deauth.\n");

	return -EINVAL;
}				/* mtk_p2p_cfg80211_deauth */

/* TODO: */
int mtk_p2p_cfg80211_disassoc(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_disassoc_request *req)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_disassoc.\n");

	/* not implemented yet */

	return -EINVAL;
}				/* mtk_p2p_cfg80211_disassoc */

int mtk_p2p_cfg80211_remain_on_channel(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       struct ieee80211_channel *chan,
				       unsigned int duration, u64 *cookie)
{
	INT_32 i4Rslt = -EINVAL;
	ULONG timeout = 0;
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	P_GL_P2P_INFO_T prGlueP2pInfo = (P_GL_P2P_INFO_T) NULL;
	P_MSG_P2P_CHNL_REQUEST_T prChnlReqMsg = (P_MSG_P2P_CHNL_REQUEST_T) NULL;
	P_P2P_CHNL_REQ_INFO_T prChnlReqInfo = (P_P2P_CHNL_REQ_INFO_T)NULL;
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T)NULL;

	do {
		if ((wiphy == NULL) || (wdev == NULL) || (chan == NULL) || (cookie == NULL))
			break;

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));
		prGlueP2pInfo = prGlueInfo->prP2PInfo;
		prP2pFsmInfo = prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo;
		prChnlReqInfo = &(prP2pFsmInfo->rChnlReqInfo);

		*cookie = prGlueP2pInfo->u8Cookie++;

		prChnlReqMsg = cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG, sizeof(MSG_P2P_CHNL_REQUEST_T));

		if (prChnlReqMsg == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_remain_on_channel, cookie: 0x%llx\n", *cookie);

		prChnlReqMsg->rMsgHdr.eMsgId = MID_MNY_P2P_CHNL_REQ;
		prChnlReqMsg->u8Cookie = *cookie;
		prChnlReqMsg->u4Duration = duration;

		__channel_format_switch(chan, NL80211_CHAN_HT20,	/* 4 KH Need Check */
							   &prChnlReqMsg->rChannelInfo, &prChnlReqMsg->eChnlSco);
		reinit_completion(&prGlueInfo->rP2pReq);

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prChnlReqMsg, MSG_SEND_METHOD_BUF);

		/*
		 * Need wait until firmare grant channel to sync with supplicant,
		 * wait 2HZ to avoid grant channel takes too long or failed
		 */
		timeout = wait_for_completion_timeout(&prGlueInfo->rP2pReq, msecs_to_jiffies(2000));
		if (timeout == 0) {
			DBGLOG(P2P, INFO, "Request channel timeout(2HZ)\n");
			/*
			 * Throw out remain/cancel on request channel to
			 * simulate this channel request has been success
			 */
			kalP2PIndicateChannelReady(prGlueInfo,
				prChnlReqInfo->u8Cookie,
				prChnlReqInfo->ucReqChnlNum,
				prChnlReqInfo->eBand,
				prChnlReqInfo->eChnlSco,
				prChnlReqInfo->u4MaxInterval);
			/* Indicate channel return. */
			kalP2PIndicateChannelExpired(prGlueInfo, &prP2pFsmInfo->rChnlReqInfo);

			/* Return Channel */
			p2pFuncReleaseCh(prGlueInfo->prAdapter, &(prP2pFsmInfo->rChnlReqInfo));
		}

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
	ULONG timeout = 0;
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	P_MSG_P2P_CHNL_ABORT_T prMsgChnlAbort = (P_MSG_P2P_CHNL_ABORT_T) NULL;
	P_P2P_FSM_INFO_T prP2pFsmInfo = (P_P2P_FSM_INFO_T)NULL;

	do {
		if ((wiphy == NULL) || (wdev == NULL))
			break;

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));
		prP2pFsmInfo = prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo;

		prMsgChnlAbort = cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG, sizeof(MSG_P2P_CHNL_ABORT_T));

		if (prMsgChnlAbort == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_cancel_remain_on_channel, cookie: 0x%llx\n", cookie);

		prMsgChnlAbort->rMsgHdr.eMsgId = MID_MNY_P2P_CHNL_ABORT;
		prMsgChnlAbort->u8Cookie = cookie;
		reinit_completion(&prGlueInfo->rP2pReq);
		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgChnlAbort, MSG_SEND_METHOD_BUF);
		timeout = wait_for_completion_timeout(&prGlueInfo->rP2pReq, msecs_to_jiffies(2000));
		if (timeout == 0) {
			DBGLOG(P2P, INFO, "Cancel remain on channel timeout(2HZ)\n");
			/* Indicate channel return. */
			kalP2PIndicateChannelExpired(prGlueInfo, &prP2pFsmInfo->rChnlReqInfo);

			/* Return Channel */
			p2pFuncReleaseCh(prGlueInfo->prAdapter, &(prP2pFsmInfo->rChnlReqInfo));
		}

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
	struct _MSG_P2P_EXTEND_LISTEN_INTERVAL_T *prMsgExtListenReq = NULL;
	ULONG timeout = 0;
	P_MSG_P2P_MGMT_TX_REQUEST_T prMsgTxReq = (P_MSG_P2P_MGMT_TX_REQUEST_T) NULL;
	P_MSDU_INFO_T prMgmtFrame = (P_MSDU_INFO_T) NULL;
	PUINT_8 pucFrameBuf = (PUINT_8) NULL;
	UINT_32 u4OriRegValue = 0;
	UINT_32 u4NextRegValue = 0;

	DBGLOG(P2P, TRACE, "--> %s() ext list,wait :%d\n"
		, __func__, params->wait);

	do {
		if ((wiphy == NULL) || (wdev == NULL) || (params == 0) || (cookie == NULL))
			break;
		/* DBGLOG(P2P, TRACE, ("mtk_p2p_cfg80211_mgmt_tx\n")); */

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));
		prGlueP2pInfo = prGlueInfo->prP2PInfo;

		*cookie = prGlueP2pInfo->u8Cookie++;

		/* Channel & Channel Type & Wait time are ignored. */
		prMsgTxReq = cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG, sizeof(MSG_P2P_MGMT_TX_REQUEST_T));

		if (prMsgTxReq == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		/* Here need to extend the listen interval */
		prMsgExtListenReq = cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG,
			sizeof(struct _MSG_P2P_EXTEND_LISTEN_INTERVAL_T));
		if (prMsgExtListenReq) {
			prMsgExtListenReq->rMsgHdr.eMsgId = MID_MNY_P2P_EXTEND_LISTEN_INTERVAL;
			prMsgExtListenReq->wait = params->wait;
			DBGLOG(P2P, TRACE, "ext listen, wait: %d\n", prMsgExtListenReq->wait);
			mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T)prMsgExtListenReq,
				MSG_SEND_METHOD_BUF);
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

		prMgmtFrame->u8Cookie = *cookie;
		prMsgTxReq->rMsgHdr.eMsgId = MID_MNY_P2P_MGMT_TX;

		pucFrameBuf = (PUINT_8) ((ULONG) prMgmtFrame->prPacket + MAC_TX_RESERVED_FIELD);

		kalMemCopy(pucFrameBuf, params->buf, params->len);

		prMgmtFrame->u2FrameLength = params->len;
		reinit_completion(&prGlueInfo->rP2pReq);

		/*record MailBox2*/
		HAL_MCR_RD(prGlueInfo->prAdapter, MCR_D2HRM2R, &u4OriRegValue);

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgTxReq, MSG_SEND_METHOD_BUF);

		timeout = wait_for_completion_timeout(&prGlueInfo->rP2pReq, msecs_to_jiffies(2000));
		if (timeout == 0) {
			DBGLOG(P2P, INFO, "wait mgmt tx done timeout cookie 0x%llx and mgmt_tx status: false\n",
				*cookie);

			/* Indicate mgmt_tx status return. */
			if (prMgmtFrame != NULL) {
				if (prMgmtFrame->prPacket != NULL) {
					kalP2PIndicateMgmtTxStatus(prGlueInfo,
								   *cookie,
								   false,
								   prMgmtFrame->prPacket,
								   (UINT_32) prMgmtFrame->u2FrameLength);
				}
			}
			/*dump mailbox info from FW*/
			HAL_MCR_RD(prGlueInfo->prAdapter, MCR_D2HRM2R, &u4NextRegValue);

			DBGLOG(P2P, WARN, "<WiFi> MCR_D2HRM2R = 0x%x, ORI_MCR_D2HRM2R = 0x%x\n",
			u4NextRegValue, u4OriRegValue);
		}

		i4Rslt = 0;
	} while (FALSE);

	if ((i4Rslt != 0) && (prMsgTxReq != NULL)) {
		if (prMsgTxReq->prMgmtMsduInfo != NULL)
			cnmMgtPktFree(prGlueInfo->prAdapter, prMsgTxReq->prMgmtMsduInfo);

		cnmMemFree(prGlueInfo->prAdapter, prMsgTxReq);
	}

	return i4Rslt;
}				/* mtk_p2p_cfg80211_mgmt_tx */

int mtk_p2p_cfg80211_change_bss(struct wiphy *wiphy, struct net_device *dev, struct bss_parameters *params)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	INT_32 i4Rslt = -EINVAL;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	DBGLOG(P2P, INFO, "--> %s() CTS:%d,ShortPramble:%d\n"
		, __func__, params->use_cts_prot, params->use_short_preamble);

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
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	INT_32 i4Rslt = -EINVAL;
	P_MSG_P2P_CONNECTION_ABORT_T prDisconnectMsg = (P_MSG_P2P_CONNECTION_ABORT_T) NULL;
	UINT_8 aucBcMac[] = BC_MAC_ADDR;
	const UINT_8 *mac = NULL;

	do {
		if ((wiphy == NULL) || (dev == NULL))
			break;

		if (params->mac == NULL)
			mac = aucBcMac;
		else
			mac = params->mac;

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

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
		COPY_MAC_ADDR(prDisconnectMsg->aucTargetID, mac);
		if (params->reason_code == 0)
			prDisconnectMsg->u2ReasonCode = REASON_CODE_DEAUTH_LEAVING_BSS;
		else
			prDisconnectMsg->u2ReasonCode = params->reason_code;
		prDisconnectMsg->fgSendDeauth = TRUE;

		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_del_station ReasonCode = %d\n", prDisconnectMsg->u2ReasonCode);

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prDisconnectMsg, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;

}				/* mtk_p2p_cfg80211_del_station */
#else
int mtk_p2p_cfg80211_del_station(struct wiphy *wiphy, struct net_device *dev, const u8 *mac)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	INT_32 i4Rslt = -EINVAL;
	P_MSG_P2P_CONNECTION_ABORT_T prDisconnectMsg = (P_MSG_P2P_CONNECTION_ABORT_T) NULL;
	UINT_8 aucBcMac[] = BC_MAC_ADDR;

	do {
		if ((wiphy == NULL) || (dev == NULL))
			break;

		if (mac == NULL)
			mac = aucBcMac;

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		/* prDisconnectMsg = (P_MSG_P2P_CONNECTION_ABORT_T)kalMemAlloc(sizeof(MSG_P2P_CONNECTION_ABORT_T),
		VIR_MEM_TYPE); */
		prDisconnectMsg =
		    (P_MSG_P2P_CONNECTION_ABORT_T) cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG,
							       sizeof(MSG_P2P_CONNECTION_ABORT_T));

		if (prDisconnectMsg == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		prDisconnectMsg->rMsgHdr.eMsgId = MID_MNY_P2P_CONNECTION_ABORT;
		COPY_MAC_ADDR(prDisconnectMsg->aucTargetID, mac);
#if CFG_TC10_FEATURE
		prDisconnectMsg->u2ReasonCode = REASON_CODE_DEAUTH_LEAVING_BSS;
#else
		prDisconnectMsg->u2ReasonCode = REASON_CODE_UNSPECIFIED;
#endif
		prDisconnectMsg->fgSendDeauth = TRUE;

		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_del_station ReasonCode = %d\n", prDisconnectMsg->u2ReasonCode);

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prDisconnectMsg, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;

}				/* mtk_p2p_cfg80211_del_station */
#endif

int mtk_p2p_cfg80211_connect(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_connect_params *sme)
{
	INT_32 i4Rslt = -EINVAL;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_MSG_P2P_CONNECTION_REQUEST_T prConnReqMsg = (P_MSG_P2P_CONNECTION_REQUEST_T) NULL;

	do {
		if ((wiphy == NULL) || (dev == NULL) || (sme == NULL))
			break;

		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_connect.\n");

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		prConnReqMsg =
		    (P_MSG_P2P_CONNECTION_REQUEST_T) cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG,
								 (sizeof(MSG_P2P_CONNECTION_REQUEST_T) + sme->ie_len));

		if (prConnReqMsg == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		prConnReqMsg->rMsgHdr.eMsgId = MID_MNY_P2P_CONNECTION_REQ;

		COPY_SSID(prConnReqMsg->rSsid.aucSsid, prConnReqMsg->rSsid.ucSsidLen, sme->ssid, sme->ssid_len);

		COPY_MAC_ADDR(prConnReqMsg->aucBssid, sme->bssid);
		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_connect to %pM, IE len: %zu\n",
				prConnReqMsg->aucBssid, sme->ie_len);

		DBGLOG(P2P, TRACE, "Assoc Req IE Buffer Length:%zu\n", sme->ie_len);
		kalMemCopy(prConnReqMsg->aucIEBuf, sme->ie, sme->ie_len);
		prConnReqMsg->u4IELen = sme->ie_len;

		__channel_format_switch(sme->channel, NL80211_CHAN_NO_HT,
				&prConnReqMsg->rChannelInfo, &prConnReqMsg->eChnlSco);

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

	do {
		if ((wiphy == NULL) || (dev == NULL))
			break;

		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_disconnect.\n");

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

/* prDisconnMsg = (P_MSG_P2P_CONNECTION_ABORT_T)kalMemAlloc(sizeof(P_MSG_P2P_CONNECTION_ABORT_T), VIR_MEM_TYPE); */
		prDisconnMsg =
		    (P_MSG_P2P_CONNECTION_ABORT_T) cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG,
							       sizeof(MSG_P2P_CONNECTION_ABORT_T));

		if (prDisconnMsg == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_disconnect.Allocate Memory Failed.\n");
			break;
		}

		prDisconnMsg->rMsgHdr.eMsgId = MID_MNY_P2P_CONNECTION_ABORT;
		prDisconnMsg->u2ReasonCode = reason_code;
		prDisconnMsg->fgSendDeauth = TRUE;
		COPY_MAC_ADDR(prDisconnMsg->aucTargetID, aucBCAddr);

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prDisconnMsg, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;
}				/* mtk_p2p_cfg80211_disconnect */
#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
int mtk_p2p_cfg80211_change_iface(IN struct wiphy *wiphy,
				  IN struct net_device *ndev,
				  IN enum nl80211_iftype type, IN struct vif_params *params)
#else
int mtk_p2p_cfg80211_change_iface(IN struct wiphy *wiphy,
				  IN struct net_device *ndev,
				  IN enum nl80211_iftype type, IN u32 *flags, IN struct vif_params *params)
#endif
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	INT_32 i4Rslt = -EINVAL;
	P_MSG_P2P_SWITCH_OP_MODE_T prSwitchModeMsg = (P_MSG_P2P_SWITCH_OP_MODE_T) NULL;

	do {
		if ((wiphy == NULL) || (ndev == NULL))
			break;

		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_change_iface. type: %d\n", type);

		if (ndev->ieee80211_ptr)
			ndev->ieee80211_ptr->iftype = type;

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

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

		switch (type) {
		case NL80211_IFTYPE_P2P_CLIENT:
			DBGLOG(P2P, TRACE, "NL80211_IFTYPE_P2P_CLIENT.\n");
			prSwitchModeMsg->eIftype = IFTYPE_P2P_CLIENT;
			/* This case need to fall through */
		case NL80211_IFTYPE_STATION:
			if (type == NL80211_IFTYPE_STATION) {
				DBGLOG(P2P, TRACE, "NL80211_IFTYPE_STATION.\n");
				prSwitchModeMsg->eIftype = IFTYPE_STATION;
			}
			prSwitchModeMsg->eOpMode = OP_MODE_INFRASTRUCTURE;
			break;
		case NL80211_IFTYPE_AP:
			DBGLOG(P2P, TRACE, "NL80211_IFTYPE_AP.\n");
			prSwitchModeMsg->eIftype = IFTYPE_AP;
			/* This case need to fall through */
		case NL80211_IFTYPE_P2P_GO:
			if (type == NL80211_IFTYPE_P2P_GO) {
				DBGLOG(P2P, TRACE, "NL80211_IFTYPE_P2P_GO not AP.\n");
				prSwitchModeMsg->eIftype = IFTYPE_P2P_GO;
			}
			prSwitchModeMsg->eOpMode = OP_MODE_ACCESS_POINT;
			break;
		default:
			DBGLOG(P2P, TRACE, "Other type :%d .\n", type);
			prSwitchModeMsg->eOpMode = OP_MODE_P2P_DEVICE;
			prSwitchModeMsg->eIftype = IFTYPE_P2P_DEVICE;
			break;
		}

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prSwitchModeMsg, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;

	} while (FALSE);

	return i4Rslt;
}				/* mtk_p2p_cfg80211_change_iface */

int mtk_p2p_cfg80211_set_channel(IN struct wiphy *wiphy, struct cfg80211_chan_def *chandef)
{
	INT_32 i4Rslt = -EINVAL;
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	RF_CHANNEL_INFO_T rRfChnlInfo = {0, 0};
	P_P2P_CONNECTION_SETTINGS_T prP2pConnSettings = NULL;

	do {
		if (wiphy == NULL)
			break;

		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_set_channel.\n");

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		__channel_format_switch(chandef->chan, 0, &rRfChnlInfo, NULL);

		prP2pConnSettings = prGlueInfo->prAdapter->rWifiVar.prP2PConnSettings;
		prP2pConnSettings->ucOperatingChnl = rRfChnlInfo.ucChannelNum;
		prP2pConnSettings->eBand = rRfChnlInfo.eBand;

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;
}				/* mtk_p2p_cfg80211_set_channel */

int mtk_p2p_cfg80211_set_bitrate_mask(IN struct wiphy *wiphy,
				      IN struct net_device *dev,
				      IN const u8 *peer, IN const struct cfg80211_bitrate_mask *mask)
{
	INT_32 i4Rslt = -EINVAL;
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;

	do {
		if ((wiphy == NULL) || (dev == NULL) || (mask == NULL))
			break;

		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_set_bitrate_mask\n");

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
#if 0
	P_MSG_P2P_MGMT_FRAME_REGISTER_T prMgmtFrameRegister = (P_MSG_P2P_MGMT_FRAME_REGISTER_T) NULL;
#endif
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;

	do {
		if ((wiphy == NULL) || (wdev == NULL))
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

		if ((prGlueInfo->prAdapter != NULL) && (prGlueInfo->prAdapter->fgIsP2PRegistered == TRUE)) {

			/* prGlueInfo->u4Flag |= GLUE_FLAG_FRAME_FILTER; */
			set_bit(GLUE_FLAG_FRAME_FILTER_BIT, &prGlueInfo->ulFlag);

			/* wake up main thread */
			wake_up_interruptible(&prGlueInfo->waitq);

			if (in_interrupt())
				DBGLOG(P2P, TRACE, "It is in interrupt level\n");
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

}				/* mtk_p2p_cfg80211_mgmt_frame_register */

#if CONFIG_NL80211_TESTMODE
int mtk_p2p_cfg80211_testmode_cmd(IN struct wiphy *wiphy, IN struct wireless_dev *wdev, IN void *data, IN int len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_NL80211_DRIVER_TEST_PARAMS prParams = NULL;
	INT_32 i4Status = -EINVAL;
	P_P2P_CHNL_REQ_INFO_T prChnlReqInfo = NULL;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	DBGLOG(P2P, INFO, "--> %s()\n", __func__);

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
		i4Status = mtk_p2p_cfg80211_testmode_wfd_update_cmd(wiphy, data, len);
		break;
#endif
	case 3:	/* Hotspot Client Management */
		i4Status = mtk_p2p_cfg80211_testmode_hotspot_block_cmd(wiphy, data, len);
		break;
	case 0x10:
		i4Status = mtk_cfg80211_testmode_get_sta_statistics(wiphy, data, len, prGlueInfo);
		break;
	case 0x11: /*NFC Beam + Indication */
		prChnlReqInfo = &prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo->rChnlReqInfo;
		if (data && len) {
			P_NL80211_DRIVER_SET_NFC_PARAMS prParams = (P_NL80211_DRIVER_SET_NFC_PARAMS) data;

			prChnlReqInfo->NFC_BEAM = prParams->NFC_Enable;
			DBGLOG(P2P, INFO, "NFC: BEAM[%d]\n", prChnlReqInfo->NFC_BEAM);
		}
		break;
	case 0x12: /*NFC Beam + Indication */
		DBGLOG(P2P, INFO, "NFC: Polling\n");
		i4Status = mtk_cfg80211_testmode_get_scan_done(wiphy, data, len, prGlueInfo);
		break;
	case TESTMODE_CMD_ID_STR_CMD:
		i4Status = mtk_cfg80211_process_str_cmd(prGlueInfo, (PUINT_8)(prParams + 1),
				len - sizeof(*prParams));
		break;
	case TESTMODE_CMD_ID_HS_CONFIG:
		i4Status = mtk_p2p_cfg80211_testmode_hotspot_config_cmd(wiphy, data, len);
		break;
	default:
		i4Status = -EINVAL;
		break;
	}

	DBGLOG(P2P, TRACE, "prParams->index=%d, status=%d\n", prParams->index, i4Status);

	return i4Status;
}

int mtk_p2p_cfg80211_testmode_hotspot_config_cmd(IN struct wiphy *wiphy, IN void *data, IN int len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	struct NL80211_DRIVER_HOTSPOT_CONFIG_PARAMS *prParams = (struct NL80211_DRIVER_HOTSPOT_CONFIG_PARAMS *) NULL;
	UINT_32 index;
	UINT_32 value;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	if (data && len) {
		prParams = (struct NL80211_DRIVER_HOTSPOT_CONFIG_PARAMS *) data;
	} else {
		DBGLOG(P2P, ERROR, "data is NULL or len is 0\n");
		return -EINVAL;
	}

	index = prParams->idx;
	value = prParams->value;

	DBGLOG(P2P, INFO, "NL80211_ATTR_TESTDATA, idx=%d value=%d\n",
			    (UINT_32) prParams->idx, (UINT_32) prParams->value);

	switch (index) {
	case 1:		/* Max Clients */
		kalP2PSetMaxClients(prGlueInfo, value);
		break;
	default:
		break;
	}

	return 0;
}

int mtk_p2p_cfg80211_testmode_p2p_sigma_pre_cmd(IN struct wiphy *wiphy, IN void *data, IN int len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	NL80211_DRIVER_TEST_PRE_PARAMS rParams;
	P_P2P_SPECIFIC_BSS_INFO_T prP2pSpecificBssInfo = (P_P2P_SPECIFIC_BSS_INFO_T) NULL;
	P_P2P_CONNECTION_SETTINGS_T prP2pConnSettings = (P_P2P_CONNECTION_SETTINGS_T) NULL;
	UINT_32 index_mode;
	UINT_32 index;
	INT_32 value;
	int status = 0;
	UINT_32 u4Leng;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	kalMemZero(&rParams, sizeof(NL80211_DRIVER_TEST_PRE_PARAMS));

	prP2pSpecificBssInfo = prGlueInfo->prAdapter->rWifiVar.prP2pSpecificBssInfo;
	prP2pConnSettings = prGlueInfo->prAdapter->rWifiVar.prP2PConnSettings;

	DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_testmode_p2p_sigma_pre_cmd\n");

	if (data && len)
		memcpy(&rParams, data, len);

	DBGLOG(P2P, TRACE, "NL80211_ATTR_TESTDATA,idx_mode=%d idx=%d value=%u\n",
			    (INT_16) rParams.idx_mode, (INT_16) rParams.idx, rParams.value);

	index_mode = rParams.idx_mode;
	index = rParams.idx;
	value = rParams.value;

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
		prP2pConnSettings->u2LocalConfigMethod = value;
		break;
	case 102:		/* Sigma P2p reset */
		/* kalMemZero(prP2pConnSettings->aucTargetDevAddr, MAC_ADDR_LEN); */
		/* prP2pConnSettings->eConnectionPolicy = ENUM_P2P_CONNECTION_POLICY_AUTO; */
		p2pFsmUninit(prGlueInfo->prAdapter);
		p2pFsmInit(prGlueInfo->prAdapter);
		break;
	case 103:		/* WPS MODE */
		kalP2PSetWscMode(prGlueInfo, value);
		break;
	case 104:		/* P2p send persence, duration */
		break;
	case 105:		/* P2p send persence, interval */
		break;
	case 106:		/* P2P set sleep  */
		value = 1;
		kalIoctl(prGlueInfo,
			 wlanoidSetP2pPowerSaveProfile, &value, sizeof(value), FALSE, FALSE, TRUE, TRUE, &u4Leng);
		break;
	case 107:		/* P2P set opps, CTWindowl */
		prP2pSpecificBssInfo->rOppPsParam.u4CTwindowMs = value;
		/* status = mtk_p2p_wext_set_oppps_param(prDev,info,wrqu,(char *)&prP2pSpecificBssInfo->rOppPsParam); */
		break;
	case 108:		/* p2p_set_power_save */
		kalIoctl(prGlueInfo,
			 wlanoidSetP2pPowerSaveProfile, &value, sizeof(value), FALSE, FALSE, TRUE, TRUE, &u4Leng);

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
	P_P2P_CONNECTION_SETTINGS_T prP2pConnSettings = (P_P2P_CONNECTION_SETTINGS_T) NULL;
	UINT_32 index;
	INT_32 value;
	int status = 0;
	UINT_32 u4Leng;
	struct NL80211_DRIVER_P2P_NOA_PARAMS {
		NL80211_DRIVER_TEST_PARAMS hdr;
		UINT_32 idx;
		UINT_32 value; /* should not be used in this case */
		UINT_32 count;
		UINT_32 interval;
		UINT_32 duration;
	};
	struct NL80211_DRIVER_P2P_NOA_PARAMS *prNoaParams = NULL;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	prP2pSpecificBssInfo = prGlueInfo->prAdapter->rWifiVar.prP2pSpecificBssInfo;
	prP2pConnSettings = prGlueInfo->prAdapter->rWifiVar.prP2PConnSettings;

	DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_testmode_p2p_sigma_cmd\n");

	if (data && len) {
		prParams = (P_NL80211_DRIVER_P2P_SIGMA_PARAMS) data;
	} else {
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_testmode_p2p_sigma_cmd, data is NULL or len is 0\n");
		return -EINVAL;
	}

	index = (INT_32) prParams->idx;
	value = (INT_32) prParams->value;

	DBGLOG(P2P, TRACE, "NL80211_ATTR_TESTDATA, idx=%d value=%d\n",
			    (INT_32) prParams->idx, (INT_32) prParams->value);

	switch (index) {
	case 0:		/* Listen CH */
		break;
	case 1:		/* P2p mode */
		break;
	case 4:		/* Noa duration */
		prNoaParams = data;
		prP2pSpecificBssInfo->rNoaParam.u4NoaCount = prNoaParams->count;
		prP2pSpecificBssInfo->rNoaParam.u4NoaIntervalMs = prNoaParams->interval;
		prP2pSpecificBssInfo->rNoaParam.u4NoaDurationMs = prNoaParams->duration;
		DBGLOG(P2P, INFO, "SET NOA: %d %d %d\n",
		       prNoaParams->count, prNoaParams->interval, prNoaParams->duration);

		/* only to apply setting when setting NOA count */
		kalIoctl(prGlueInfo, wlanoidSetNoaParam, &prP2pSpecificBssInfo->rNoaParam,
			 sizeof(PARAM_CUSTOM_NOA_PARAM_STRUCT_T),
			 FALSE, FALSE, TRUE, TRUE, &u4Leng);
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
		prP2pConnSettings->u2LocalConfigMethod = value;
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
		value = 1;
		kalIoctl(prGlueInfo,
			 wlanoidSetP2pPowerSaveProfile, &value, sizeof(value), FALSE, FALSE, TRUE, TRUE, &u4Leng);
		break;
	case 107:		/* P2P set opps, CTWindowl */
		prP2pSpecificBssInfo->rOppPsParam.u4CTwindowMs = value;

		DBGLOG(P2P, INFO, "SET OPPS: %d\n", value);
		kalIoctl(prGlueInfo, wlanoidSetOppPsParam, &prP2pSpecificBssInfo->rOppPsParam,
			 sizeof(PARAM_CUSTOM_OPPPS_PARAM_STRUCT_T),
			 FALSE, FALSE, TRUE, TRUE, &u4Leng);
		break;
	case 108:		/* p2p_set_power_save */
		kalIoctl(prGlueInfo,
			 wlanoidSetP2pPowerSaveProfile, &value, sizeof(value), FALSE, FALSE, TRUE, TRUE, &u4Leng);

		break;
	case 109:		/* Max Clients */
		kalP2PSetMaxClients(prGlueInfo, value);
		break;
	case 110:		/* Hotspot WPS mode */
		kalIoctl(prGlueInfo, wlanoidSetP2pWPSmode, &value, sizeof(value), FALSE, FALSE, TRUE, TRUE, &u4Leng);
		break;
	default:
		break;
	}

	return status;

}

#if CFG_SUPPORT_WFD
int mtk_p2p_cfg80211_testmode_wfd_update_cmd(IN struct wiphy *wiphy, IN void *data, IN int len)
{
	static UINT_8 prevWfdEnable;

	P_GLUE_INFO_T prGlueInfo = NULL;
	P_NL80211_DRIVER_WFD_PARAMS prParams = (P_NL80211_DRIVER_WFD_PARAMS) NULL;
	int status = 0;

	P_WFD_CFG_SETTINGS_T prWfdCfgSettings = (P_WFD_CFG_SETTINGS_T) NULL;
	P_MSG_WFD_CONFIG_SETTINGS_CHANGED_T prMsgWfdCfgUpdate = (P_MSG_WFD_CONFIG_SETTINGS_CHANGED_T) NULL;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	prParams = (P_NL80211_DRIVER_WFD_PARAMS) data;

	DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_testmode_wfd_update_cmd\n");

	/* to reduce log, print when state changed */
	if (prevWfdEnable != prParams->WfdEnable) {
		prevWfdEnable = prParams->WfdEnable;
		DBGLOG(P2P, INFO, "WFD Enable:%x\n", prParams->WfdEnable);
		DBGLOG(P2P, INFO, "WFD Session Available:%x\n", prParams->WfdSessionAvailable);
		DBGLOG(P2P, INFO, "WFD Couple Sink Status:%x\n", prParams->WfdCoupleSinkStatus);
		/* aucReserved0[2] */
		DBGLOG(P2P, INFO, "WFD Device Info:%x\n", prParams->WfdDevInfo);
		DBGLOG(P2P, INFO, "WFD Control Port:%x\n", prParams->WfdControlPort);
		DBGLOG(P2P, INFO, "WFD Maximum Throughput:%x\n", prParams->WfdMaximumTp);
		DBGLOG(P2P, INFO, "WFD Extend Capability:%x\n", prParams->WfdExtendCap);
		DBGLOG(P2P, INFO, "WFD Couple Sink Addr %pM\n", prParams->WfdCoupleSinkAddress);
		DBGLOG(P2P, INFO, "WFD Associated BSSID %pM\n", prParams->WfdAssociatedBssid);
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
		DBGLOG(P2P, INFO, "WFD Primary Sink Addr %pM\n", prParams->aucWfdPrimarySinkMac);
		DBGLOG(P2P, INFO, "WFD Secondary Sink Addr %pM\n", prParams->aucWfdSecondarySinkMac);
		DBGLOG(P2P, INFO, "WFD Advanced Flag:%x\n", prParams->WfdAdvanceFlag);
		DBGLOG(P2P, INFO, "WFD Sigma mode:%x\n", prParams->WfdSigmaMode);
		/* UINT_8 aucReserved2[64]; */
		/* UINT_8 aucReserved3[64]; */
		/* UINT_8 aucReserved4[64]; */
	}

	prWfdCfgSettings = &(prGlueInfo->prAdapter->rWifiVar.prP2pFsmInfo->rWfdConfigureSettings);

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

int mtk_p2p_cfg80211_testmode_hotspot_block_cmd(IN struct wiphy *wiphy, IN void *data, IN int len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_NL80211_DRIVER_HOTSPOT_BLOCK_PARAMS prParams = (P_NL80211_DRIVER_HOTSPOT_BLOCK_PARAMS) NULL;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	DBGLOG(P2P, INFO, "--> %s()\n", __func__);

	if (data && len) {
		prParams = (P_NL80211_DRIVER_HOTSPOT_BLOCK_PARAMS) data;
	} else {
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_testmode_hotspot_block_cmd, data is NULL or len is 0\n");
		return -EINVAL;
	}

	DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_testmode_hotspot_block_cmd\n");

	return kalP2PSetBlackList(prGlueInfo, prParams->aucBssid, prParams->ucblocked);
}

int mtk_p2p_cfg80211_testmode_sw_cmd(IN struct wiphy *wiphy, IN void *data, IN int len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_NL80211_DRIVER_SW_CMD_PARAMS prParams = (P_NL80211_DRIVER_SW_CMD_PARAMS) NULL;
	WLAN_STATUS rstatus = WLAN_STATUS_SUCCESS;
	int fgIsValid = 0;
	UINT_32 u4SetInfoLen = 0;

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

#if 1
	DBGLOG(P2P, TRACE, "--> %s()\n", __func__);
#endif

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

#endif /* CONFIG_NL80211_TESTMODE */

#endif /* CFG_ENABLE_WIFI_DIRECT && CFG_ENABLE_WIFI_DIRECT_CFG_80211 */
