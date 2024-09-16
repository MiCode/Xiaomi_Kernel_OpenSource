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
** Id: @(#) gl_p2p_cfg80211.c@@
*/

/*! \file   gl_p2p_cfg80211.c
*    \brief  Main routines of Linux driver interface for Wi-Fi Direct
*	    using cfg80211 interface
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

#include "config.h"

#if CFG_ENABLE_WIFI_DIRECT && CFG_ENABLE_WIFI_DIRECT_CFG_80211
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/wireless.h>
#include <linux/ieee80211.h>
#include <net/cfg80211.h>

#include "precomp.h"
#include "gl_cfg80211.h"
#include "gl_p2p_os.h"

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

BOOLEAN
mtk_p2p_cfg80211func_channel_sco_switch(IN enum nl80211_channel_type channel_type,
					   IN P_ENUM_CHNL_EXT_T prChnlSco)
{
	BOOLEAN fgIsValid = FALSE;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211func_channel_sco_switch\n");
#endif

	do {
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
}

BOOLEAN
mtk_p2p_cfg80211func_channel_format_switch(IN struct cfg80211_chan_def *channel_def,
					   IN struct ieee80211_channel *channel,
					   IN P_RF_CHANNEL_INFO_T prRfChnlInfo)
{
	BOOLEAN fgIsValid = FALSE;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR,
			"mtk_p2p_cfg80211func_channel_format_switch\n");
#endif

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

		if (channel_def && prRfChnlInfo) {

			switch (channel_def->width) {
			case NL80211_CHAN_WIDTH_20_NOHT:
			case NL80211_CHAN_WIDTH_20:
				prRfChnlInfo->ucChnlBw = MAX_BW_20MHZ;
				break;
			case NL80211_CHAN_WIDTH_40:
				prRfChnlInfo->ucChnlBw = MAX_BW_40MHZ;
				break;
			case NL80211_CHAN_WIDTH_80:
				prRfChnlInfo->ucChnlBw = MAX_BW_80MHZ;
				break;
			case NL80211_CHAN_WIDTH_80P80:
				prRfChnlInfo->ucChnlBw = MAX_BW_80_80_MHZ;
				break;
			case NL80211_CHAN_WIDTH_160:
				prRfChnlInfo->ucChnlBw = MAX_BW_160MHZ;
				break;
			default:
				prRfChnlInfo->ucChnlBw = MAX_BW_20MHZ;
				break;
			}
			prRfChnlInfo->u2PriChnlFreq = channel->center_freq;
			prRfChnlInfo->u4CenterFreq1 = channel_def->center_freq1;
			prRfChnlInfo->u4CenterFreq2 = channel_def->center_freq2;
		}

		fgIsValid = TRUE;

	} while (FALSE);

	return fgIsValid;
}

/* mtk_p2p_cfg80211func_channel_format_switch */

INT_32 mtk_Netdev_To_RoleIdx(P_GLUE_INFO_T prGlueInfo, struct net_device *ndev, PUINT_8 pucRoleIdx)
{
	INT_32 i4Ret = -1;
	UINT_32 u4Idx = 0;

	if (pucRoleIdx == NULL)
		return i4Ret;
#if 0
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
#else
	for (u4Idx = 0; u4Idx < prGlueInfo->prAdapter->prP2pInfo->u4DeviceNum ; u4Idx++) {

		if (prGlueInfo->prP2PInfo[u4Idx]->aprRoleHandler == NULL)
			continue;

		if (prGlueInfo->prP2PInfo[u4Idx]->aprRoleHandler == ndev) {
			*pucRoleIdx = (UINT_8) u4Idx;
			i4Ret = 0;

		}
	}
#endif

	return i4Ret;

}				/* mtk_Netdev_To_RoleIdx */

static void mtk_vif_destructor(struct net_device *dev)
{
	struct wireless_dev *prWdev = ERR_PTR(-ENOMEM);
	UINT_32 u4Idx = 0;

	DBGLOG(P2P, INFO, "mtk_newInf_destructor\n");

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, WARN, "mtk_newInf_destructor\n");
#endif

	if (dev) {
		prWdev = dev->ieee80211_ptr;
		free_netdev(dev);
		if (prWdev) {
			/* Role[i] and Dev share the same wireless dev by default */
			for (u4Idx = 0; u4Idx < KAL_P2P_NUM; u4Idx++) {
				if ((prWdev == gprP2pRoleWdev[u4Idx]) &&
					(gprP2pRoleWdev[u4Idx] != gprP2pWdev)) {
					DBGLOG(P2P, INFO, "mtk_newInf_destructor remove added Wd\n");
					gprP2pRoleWdev[u4Idx] = gprP2pWdev;
				}
			}
			kfree(prWdev);
		}
	}
}

#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
struct wireless_dev *mtk_p2p_cfg80211_add_iface(struct wiphy *wiphy,
			const char *name, unsigned char name_assign_type,
			enum nl80211_iftype type, struct vif_params *params)
#elif KERNEL_VERSION(4, 1, 0) <= CFG80211_VERSION_CODE
struct wireless_dev *mtk_p2p_cfg80211_add_iface(struct wiphy *wiphy,
						const char *name, unsigned char name_assign_type,
						enum nl80211_iftype type, u32 *flags, struct vif_params *params)
#else
struct wireless_dev *mtk_p2p_cfg80211_add_iface(struct wiphy *wiphy,
						const char *name,
						enum nl80211_iftype type, u32 *flags, struct vif_params *params)
#endif
{
	/* 2 TODO: Fit kernel 3.10 modification */
	P_ADAPTER_T prAdapter;
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	struct net_device *prNewNetDevice = NULL;
	UINT_32 u4Idx = 0;
	P_GL_P2P_INFO_T prP2pInfo = (P_GL_P2P_INFO_T) NULL;
	P_GL_HIF_INFO_T prHif = NULL;
	P_MSG_P2P_SWITCH_OP_MODE_T prSwitchModeMsg = (P_MSG_P2P_SWITCH_OP_MODE_T) NULL;
	struct wireless_dev *prWdev = ERR_PTR(-ENOMEM);
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T) NULL;
	P_NETDEV_PRIVATE_GLUE_INFO prNetDevPriv = (P_NETDEV_PRIVATE_GLUE_INFO) NULL;
	PARAM_MAC_ADDRESS rMacAddr;
	P_MSG_P2P_ACTIVE_DEV_BSS_T prMsgActiveBss = (P_MSG_P2P_ACTIVE_DEV_BSS_T) NULL;


#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_add_iface\n");
#endif

	DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_add_iface\n");

	do {
		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		if (prGlueInfo == NULL)
			break;

		prAdapter = prGlueInfo->prAdapter;

		for (u4Idx = 0; u4Idx < KAL_P2P_NUM; u4Idx++) {
			if (prGlueInfo->prP2PInfo[u4Idx]->aprRoleHandler ==
				prGlueInfo->prP2PInfo[u4Idx]->prDevHandler)
				break;
			if (prGlueInfo->prP2PInfo[u4Idx]->aprRoleHandler == NULL) {
				p2pRoleFsmInit(prGlueInfo->prAdapter, u4Idx);
				break;
			}
		}

		if (u4Idx == KAL_P2P_NUM) {
			/* Role port full. */
			break;
		}

		/*u4Idx = 0;*/
		DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_add_iface u4Idx=%d\n", u4Idx);

		prP2pInfo = prGlueInfo->prP2PInfo[u4Idx];

		DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_add_iface name = %s\n", name);
#if KERNEL_VERSION(3, 17, 0) <= CFG80211_VERSION_CODE
		prNewNetDevice = alloc_netdev_mq(sizeof(NETDEV_PRIVATE_GLUE_INFO), name,
						NET_NAME_PREDICTABLE, ether_setup, CFG_MAX_TXQ_NUM);
#else
		prNewNetDevice = alloc_netdev_mq(sizeof(NETDEV_PRIVATE_GLUE_INFO), name, ether_setup, CFG_MAX_TXQ_NUM);
#endif
		if (prNewNetDevice == NULL) {
			DBGLOG(P2P, TRACE, "alloc_netdev_mq fail\n");
			break;
		}
		prP2pInfo->aprRoleHandler = prNewNetDevice;

		*((P_GLUE_INFO_T *) netdev_priv(prNewNetDevice)) = prGlueInfo;

		prNewNetDevice->needed_headroom += NIC_TX_HEAD_ROOM;
		prNewNetDevice->netdev_ops = &p2p_netdev_ops;

		prHif = &prGlueInfo->rHifInfo;
		ASSERT(prHif);

#if defined(_HIF_SDIO)
#if (MTK_WCN_HIF_SDIO == 0)
		SET_NETDEV_DEV(prNewNetDevice, &(prHif->func->dev));
#endif
#endif

#if CFG_ENABLE_WIFI_DIRECT_CFG_80211
		prWdev = kzalloc(sizeof(struct wireless_dev), GFP_KERNEL);
		if (!prWdev) {
			DBGLOG(P2P, ERROR, "allocate p2p wireless device fail, no memory\n");
			prWdev = ERR_PTR(-ENOMEM);
			free_netdev(prP2pInfo->aprRoleHandler);
			prP2pInfo->aprRoleHandler = NULL;
			break;
		}
		kalMemCopy(prWdev, gprP2pWdev, sizeof(struct wireless_dev));
		prWdev->netdev = prNewNetDevice;
		prNewNetDevice->ieee80211_ptr = prWdev;
		if (prNewNetDevice->ieee80211_ptr)
			prNewNetDevice->ieee80211_ptr->iftype = type;
		/* register destructor function for virtual interface */
#if KERNEL_VERSION(4, 11, 0) <= CFG80211_VERSION_CODE
		prNewNetDevice->priv_destructor = mtk_vif_destructor;
#else
		prNewNetDevice->destructor = mtk_vif_destructor;
#endif

		gprP2pRoleWdev[u4Idx] = prWdev;
		/*prP2pInfo->prRoleWdev[0] = prWdev;*//* TH3 multiple P2P */
#endif

#if CFG_TCP_IP_CHKSUM_OFFLOAD
		/* set HW checksum offload */
		if (prAdapter->fgIsSupportCsumOffload)
			prNewNetDevice->features = NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM |
						   NETIF_F_RXCSUM;
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */

		kalResetStats(prNewNetDevice);
		/* net device initialize */
		netif_carrier_off(prP2pInfo->aprRoleHandler);
		netif_tx_stop_all_queues(prP2pInfo->aprRoleHandler);

		/* register for net device */
		if (register_netdevice(prP2pInfo->aprRoleHandler) < 0) {
			DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_add_iface 456\n");
			DBGLOG(INIT, WARN, "unable to register netdevice for p2p\n");
			kfree(prWdev);
			prWdev = ERR_PTR(-ENOMEM);
			free_netdev(prP2pInfo->aprRoleHandler);
			prP2pInfo->aprRoleHandler = NULL;
			break;
		} else {
			DBGLOG(P2P, TRACE, "register_netdev OK\n");
			prGlueInfo->prAdapter->rP2PNetRegState = ENUM_NET_REG_STATE_REGISTERED;
		}
		prP2pRoleFsmInfo = prAdapter->rWifiVar.aprP2pRoleFsmInfo[u4Idx];

		/* 13. bind netdev pointer to netdev index */
		wlanBindBssIdxToNetInterface(prGlueInfo, prP2pRoleFsmInfo->ucBssIndex,
									(PVOID) prP2pInfo->aprRoleHandler);
		prNetDevPriv = (P_NETDEV_PRIVATE_GLUE_INFO) netdev_priv(prP2pInfo->aprRoleHandler);
		prNetDevPriv->prGlueInfo = prGlueInfo;
		prNetDevPriv->ucBssIdx = prP2pRoleFsmInfo->ucBssIndex;

		/* 4.2 fill hardware address */
		COPY_MAC_ADDR(rMacAddr, prAdapter->rMyMacAddr);
		if (prGlueInfo->prAdapter->rWifiVar.ucP2pShareMacAddr &&
		    (type == NL80211_IFTYPE_P2P_CLIENT || type == NL80211_IFTYPE_P2P_GO)) {
			rMacAddr[0] = gPrP2pDev[0]->dev_addr[0];
		} else {
			rMacAddr[0] |= 0x2; /* change to local administrated address */
			if (u4Idx > 0)
				rMacAddr[0] ^= u4Idx << 2;
			else
				rMacAddr[0] ^= prGlueInfo->prAdapter->prP2pInfo->u4DeviceNum << 2;
		}
		kalMemCopy(prNewNetDevice->dev_addr, rMacAddr, ETH_ALEN);
		kalMemCopy(prNewNetDevice->perm_addr, rMacAddr, ETH_ALEN);

		DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_add_iface ucBssIdx=%d\n", prNetDevPriv->ucBssIdx);

		/* Switch OP MOde. */
		prSwitchModeMsg =
			(P_MSG_P2P_SWITCH_OP_MODE_T) cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG,
								 sizeof(MSG_P2P_SWITCH_OP_MODE_T));

		if (prSwitchModeMsg == NULL) {
			ASSERT(FALSE);
			DBGLOG(INIT, WARN, "unable to alloc msg\n");
			kfree(prWdev);
			prWdev = ERR_PTR(-ENOMEM);
			free_netdev(prGlueInfo->prP2PInfo[u4Idx]->aprRoleHandler);
			prGlueInfo->prP2PInfo[u4Idx]->aprRoleHandler = NULL;
			break;
		} else {
			prSwitchModeMsg->rMsgHdr.eMsgId = MID_MNY_P2P_FUN_SWITCH;
			prSwitchModeMsg->ucRoleIdx = 0;
			switch (type) {
			case NL80211_IFTYPE_P2P_CLIENT:
				DBGLOG(P2P, TRACE, "NL80211_IFTYPE_P2P_CLIENT.\n");
				prSwitchModeMsg->eOpMode = OP_MODE_INFRASTRUCTURE;
				kalP2PSetRole(prGlueInfo, 1, u4Idx);
				break;
			case NL80211_IFTYPE_STATION:
					DBGLOG(P2P, TRACE, "NL80211_IFTYPE_STATION.\n");
				prSwitchModeMsg->eOpMode = OP_MODE_INFRASTRUCTURE;
				kalP2PSetRole(prGlueInfo, 1, u4Idx);
				break;
			case NL80211_IFTYPE_AP:
				DBGLOG(P2P, TRACE, "NL80211_IFTYPE_AP.\n");
				prSwitchModeMsg->eOpMode = OP_MODE_ACCESS_POINT;
				kalP2PSetRole(prGlueInfo, 2, u4Idx);
				break;
			case NL80211_IFTYPE_P2P_GO:
					DBGLOG(P2P, TRACE, "NL80211_IFTYPE_P2P_GO not AP.\n");
				prSwitchModeMsg->eOpMode = OP_MODE_ACCESS_POINT;
				kalP2PSetRole(prGlueInfo, 2, u4Idx);
				break;
			default:
				DBGLOG(P2P, TRACE, "Other type :%d .\n", type);
				prSwitchModeMsg->eOpMode = OP_MODE_P2P_DEVICE;
				kalP2PSetRole(prGlueInfo, 0, u4Idx);
				break;
			}
			mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prSwitchModeMsg,
				MSG_SEND_METHOD_BUF);
		}

		/* Send Msg to DevFsm and active P2P dev BSS */
		prMsgActiveBss =
			(P_MSG_P2P_ACTIVE_DEV_BSS_T) cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG,
								sizeof(MSG_P2P_ACTIVE_DEV_BSS_T));

		if (prMsgActiveBss == NULL) {
			ASSERT(FALSE);
			DBGLOG(INIT, WARN, "unable to alloc msg\n");
			kfree(prWdev);
			prWdev = ERR_PTR(-ENOMEM);
			free_netdev(prGlueInfo->prP2PInfo[u4Idx]->aprRoleHandler);
			prGlueInfo->prP2PInfo[u4Idx]->aprRoleHandler = NULL;
			break;
		}

		prMsgActiveBss->rMsgHdr.eMsgId = MID_MNY_P2P_ACTIVE_BSS;
		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgActiveBss, MSG_SEND_METHOD_BUF);

	} while (FALSE);

	return prWdev;
}				/* mtk_p2p_cfg80211_add_iface */

int mtk_p2p_cfg80211_del_iface(struct wiphy *wiphy, struct wireless_dev *wdev)
{
#if 0
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	P_MSG_P2P_DEL_IFACE_T prP2pDelIfaceMsg = (P_MSG_P2P_DEL_IFACE_T) NULL;
	P_MSG_P2P_ACTIVE_DEV_BSS_T prMsgActiveBss = (P_MSG_P2P_ACTIVE_DEV_BSS_T) NULL;

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));
	if (prGlueInfo == NULL)
		return -EINVAL;

	prP2pDelIfaceMsg =
		(P_MSG_P2P_DEL_IFACE_T) cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG,
							 sizeof(MSG_P2P_DEL_IFACE_T));

	if (prP2pDelIfaceMsg == NULL) {
		ASSERT(FALSE);
		DBGLOG(INIT, WARN, "unable to alloc msg\n");
	} else {
		prP2pDelIfaceMsg->rMsgHdr.eMsgId = MID_MNY_P2P_DEL_IFACE;
		prP2pDelIfaceMsg->ucRoleIdx = 0;

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prP2pDelIfaceMsg, MSG_SEND_METHOD_BUF);

		/* Send Msg to DevFsm and Deactive P2P dev BSS */
		prMsgActiveBss = cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG, sizeof(MSG_P2P_ACTIVE_DEV_BSS_T));
		prMsgActiveBss->rMsgHdr.eMsgId = MID_MNY_P2P_ACTIVE_BSS;
		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgActiveBss, MSG_SEND_METHOD_BUF);
	}

#else
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	P_MSG_P2P_DEL_IFACE_T prP2pDelIfaceMsg = (P_MSG_P2P_DEL_IFACE_T) NULL;
	P_ADAPTER_T prAdapter;
	P_GL_P2P_INFO_T prP2pInfo = (P_GL_P2P_INFO_T) NULL;
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T) NULL;
	P_GL_P2P_DEV_INFO_T prP2pGlueDevInfo = (P_GL_P2P_DEV_INFO_T) NULL;
	struct net_device *UnregRoleHander = (struct net_device *)NULL;

	GLUE_SPIN_LOCK_DECLARATION();

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_del_iface\n");
#endif

	DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_del_iface\n");

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));
	if (prGlueInfo == NULL)
		return -EINVAL;

	prAdapter = prGlueInfo->prAdapter;
	prP2pInfo = prGlueInfo->prP2PInfo[0];
	prP2pGlueDevInfo = prGlueInfo->prP2PDevInfo;

	KAL_ACQUIRE_MUTEX(prAdapter, MUTEX_DEL_INF);

	prP2pRoleFsmInfo = prAdapter->rWifiVar.aprP2pRoleFsmInfo[0];
	wlanBindBssIdxToNetInterface(prGlueInfo, prP2pRoleFsmInfo->ucBssIndex,
							(PVOID) prGlueInfo->prP2PInfo[0]->prDevHandler);

	UnregRoleHander = prP2pInfo->aprRoleHandler;

	/* Wait for kalSendCompleteAndAwakeQueue() complete */
	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
	prP2pInfo->aprRoleHandler = prP2pInfo->prDevHandler;
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_NET_DEV);
	/* Check the Scan request is pending and we abort it before delete interface */
	if (prP2pGlueDevInfo->prScanRequest != NULL) {
		/* Check the wdev with backup scan req due to */
		/* the kernel will free this request by error handling */
		if (prP2pGlueDevInfo->rBackupScanRequest.wdev == UnregRoleHander->ieee80211_ptr) {
			kalCfg80211ScanDone(&(prP2pGlueDevInfo->rBackupScanRequest), TRUE);
			/* clear the request to avoid the Role FSM calls the scan_done again */
			prP2pGlueDevInfo->prScanRequest = NULL;
		}
	}

	/* prepare for removal */
	if (netif_carrier_ok(UnregRoleHander))
		netif_carrier_off(UnregRoleHander);

	netif_tx_stop_all_queues(UnregRoleHander);

	/* Here are functions which need rtnl_lock */
	unregister_netdevice(UnregRoleHander);
	/* free is called at destructor */
	/* free_netdev(UnregRoleHander); */

	KAL_RELEASE_MUTEX(prAdapter, MUTEX_DEL_INF);

	prP2pDelIfaceMsg =
		(P_MSG_P2P_DEL_IFACE_T) cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG,
							 sizeof(MSG_P2P_DEL_IFACE_T));

	if (prP2pDelIfaceMsg == NULL) {
		ASSERT(FALSE);
		DBGLOG(INIT, WARN, "unable to alloc msg\n");
	} else {
		prP2pDelIfaceMsg->rMsgHdr.eMsgId = MID_MNY_P2P_DEL_IFACE;
		prP2pDelIfaceMsg->ucRoleIdx = 0;

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prP2pDelIfaceMsg, MSG_SEND_METHOD_BUF);
	}
#endif
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
	/* const UINT_8 aucZeroMacAddr[] = NULL_MAC_ADDR; */
#if CFG_SUPPORT_REPLAY_DETECTION
	P_BSS_INFO_T prBssInfo = NULL;
	struct SEC_DETECT_REPLAY_INFO *prDetRplyInfo = NULL;
	UINT_8 ucCheckZeroKey = 0;
	UINT_8 i = 0;
#endif

	ASSERT(wiphy);

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_add_key\n");
#endif

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	if (mtk_Netdev_To_RoleIdx(prGlueInfo, ndev, &ucRoleIdx) != 0)
		return -EINVAL;
#if DBG
	DBGLOG(RSN, TRACE, "mtk_p2p_cfg80211_add_key\n");
	if (mac_addr) {
		DBGLOG(RSN, INFO,
		       "keyIdx = %d pairwise = %d mac = " MACSTR "\n", key_index, pairwise, MAC2STR(mac_addr));
	} else {
		DBGLOG(RSN, INFO, "keyIdx = %d pairwise = %d null mac\n", key_index, pairwise);
	}
	DBGLOG(RSN, TRACE, "Cipher = %x\n", params->cipher);
	DBGLOG_MEM8(RSN, TRACE, params->key, params->key_len);
#endif

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

	/* For BC addr case: ex: AP mode, driver_nl80211 will not have mac_addr */
	if (pairwise) {
		rKey.u4KeyIndex |= BIT(31);	/* Tx */
		rKey.u4KeyIndex |= BIT(30);	/* Pairwise */
		COPY_MAC_ADDR(rKey.arBSSID, mac_addr);
	} else {
		COPY_MAC_ADDR(rKey.arBSSID, aucBCAddr);
	}

	/* Check if add key under AP mode */
	if (kalP2PGetRole(prGlueInfo, ucRoleIdx) == 2)
		rKey.u4KeyIndex |= BIT(28); /* authenticator */

#if CFG_SUPPORT_REPLAY_DETECTION
	if (params->key) {
		for (i = 0; i < params->key_len; i++) {
			if (params->key[i] == 0x00)
				ucCheckZeroKey++;
		}

		if (ucCheckZeroKey == params->key_len)
			return 0;
	}
#endif

	if (params->key)
		kalMemCopy(rKey.aucKeyMaterial, params->key, params->key_len);
	rKey.u4KeyLength = params->key_len;
	rKey.u4Length = ((ULONG)&(((P_P2P_PARAM_KEY_T) 0)->aucKeyMaterial)) + rKey.u4KeyLength;

#if CFG_SUPPORT_REPLAY_DETECTION
	prBssInfo = GET_BSS_INFO_BY_INDEX(prGlueInfo->prAdapter, rKey.ucBssIdx);

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

int mtk_p2p_cfg80211_get_key(struct wiphy *wiphy,
			     struct net_device *ndev,
			     u8 key_index,
			     bool pairwise,
			     const u8 *mac_addr, void *cookie, void (*callback) (void *cookie, struct key_params *)
)
{
	P_GLUE_INFO_T prGlueInfo = NULL;

	ASSERT(wiphy);

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_get_key\n");
#endif
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

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_del_key\n");
#endif
	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	if (mtk_Netdev_To_RoleIdx(prGlueInfo, ndev, &ucRoleIdx) < 0)
		return -EINVAL;
#if DBG
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
	if (mac_addr) {
		COPY_MAC_ADDR(rRemoveKey.arBSSID, mac_addr);
		rRemoveKey.u4KeyIndex |= BIT(30);
	}

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidSetRemoveKey, &rRemoveKey, rRemoveKey.u4Length, FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus == WLAN_STATUS_SUCCESS)
		i4Rslt = 0;

	return i4Rslt;
}

int
mtk_p2p_cfg80211_set_default_key(struct wiphy *wiphy,
				 struct net_device *netdev, u8 key_index, bool unicast, bool multicast)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_DEFAULT_KEY_T rDefaultKey;
	UINT_8 ucRoleIdx = 0;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	INT_32 i4Rst = -EINVAL;
	UINT_32 u4BufLen = 0;
	BOOLEAN fgDef = FALSE, fgMgtDef = FALSE;

	ASSERT(wiphy);

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_set_default_key\n");
#endif
	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	if (mtk_Netdev_To_RoleIdx(prGlueInfo, netdev, &ucRoleIdx) != 0)
		return -EINVAL;
#if DBG
	DBGLOG(RSN, TRACE, "mtk_p2p_cfg80211_set_default_key\n");
	DBGLOG(RSN, TRACE, "keyIdx = %d unicast = %d multicast = %d\n", key_index, unicast, multicast);
#endif


	/* For wep case, this set the key for tx */
	if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, ucRoleIdx, &rDefaultKey.ucBssIdx) != WLAN_STATUS_SUCCESS)
		return -EINVAL;


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

#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
int mtk_p2p_cfg80211_get_station(struct wiphy *wiphy, struct net_device *ndev,
		const u8 *mac, struct station_info *sinfo)
{
	INT_32 i4RetRslt = -EINVAL;
	UINT_8 ucRoleIdx = 0;
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	P_GL_P2P_INFO_T prP2pGlueInfo = (P_GL_P2P_INFO_T) NULL;
	P2P_STATION_INFO_T rP2pStaInfo;

	ASSERT(wiphy);

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_get_station\n");
#endif
	do {
		if ((wiphy == NULL) || (ndev == NULL) || (sinfo == NULL) || (mac == NULL))
			break;

		DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_get_station\n");

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		if (mtk_Netdev_To_RoleIdx(prGlueInfo, ndev, &ucRoleIdx) != 0)
			return -EINVAL;

		prP2pGlueInfo = prGlueInfo->prP2PInfo[ucRoleIdx];

		sinfo->filled = 0;

		/* Get station information. */
		/* 1. Inactive time? */
		p2pFuncGetStationInfo(prGlueInfo->prAdapter, (PUINT_8)mac, &rP2pStaInfo);

		/* Inactive time. */
#if KERNEL_VERSION(4, 0, 0) <= CFG80211_VERSION_CODE
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
#else
int mtk_p2p_cfg80211_get_station(struct wiphy *wiphy, struct net_device *ndev, u8 *mac, struct station_info *sinfo)
{
	INT_32 i4RetRslt = -EINVAL;
	UINT_8 ucRoleIdx = 0;
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	P_GL_P2P_INFO_T prP2pGlueInfo = (P_GL_P2P_INFO_T) NULL;
	P2P_STATION_INFO_T rP2pStaInfo;

	ASSERT(wiphy);

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_get_station\n");
#endif

	do {
		if ((wiphy == NULL) || (ndev == NULL) || (sinfo == NULL) || (mac == NULL))
			break;

		DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_get_station\n");

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		if (mtk_Netdev_To_RoleIdx(prGlueInfo, ndev, &ucRoleIdx) != 0)
			return -EINVAL;

		prP2pGlueInfo = prGlueInfo->prP2PInfo[ucRoleIdx];

		sinfo->filled = 0;

		/* Get station information. */
		/* 1. Inactive time? */
		p2pFuncGetStationInfo(prGlueInfo->prAdapter, mac, &rP2pStaInfo);

		/* Inactive time. */
		sinfo->filled |= STATION_INFO_INACTIVE_TIME;
		sinfo->inactive_time = rP2pStaInfo.u4InactiveTime;
		sinfo->generation = prP2pGlueInfo->i4Generation;

		i4RetRslt = 0;
	} while (FALSE);

	return i4RetRslt;
}
#endif

int mtk_p2p_cfg80211_scan(struct wiphy *wiphy, struct cfg80211_scan_request *request)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	P_GL_P2P_INFO_T prP2pGlueInfo = (P_GL_P2P_INFO_T) NULL;
	P_GL_P2P_DEV_INFO_T prP2pGlueDevInfo = (P_GL_P2P_DEV_INFO_T) NULL;
	P_MSG_P2P_SCAN_REQUEST_T prMsgScanRequest = (P_MSG_P2P_SCAN_REQUEST_T) NULL;
	UINT_32 u4MsgSize = 0, u4Idx = 0;
	INT_32 i4RetRslt = -EINVAL;
	P_RF_CHANNEL_INFO_T prRfChannelInfo = (P_RF_CHANNEL_INFO_T) NULL;
	P_P2P_SSID_STRUCT_T prSsidStruct = (P_P2P_SSID_STRUCT_T) NULL;
	struct ieee80211_channel *prChannel = NULL;
	struct cfg80211_ssid *prSsid = NULL;
	UINT_8 ucBssIdx = 0;
	BOOLEAN fgIsFullChanScan = FALSE;

	/* [---------Channel---------] [---------SSID---------][---------IE---------] */

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_scan\n");
#endif

	do {
		if ((wiphy == NULL) || (request == NULL))
			break;

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		if (wlanIsChipAssert(prGlueInfo->prAdapter))
			break;

		if (!wlanGetHifState(prGlueInfo))
			break;

		prP2pGlueInfo = prGlueInfo->prP2PInfo[0];
		prP2pGlueDevInfo = prGlueInfo->prP2PDevInfo;

		if ((prP2pGlueInfo == NULL) || (prP2pGlueDevInfo == NULL)) {
			ASSERT(FALSE);
			break;
		}

		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_scan.\n");

		if (prP2pGlueDevInfo->prScanRequest != NULL) {
			/* There have been a scan request on-going processing. */
			DBGLOG(P2P, TRACE, "There have been a scan request on-going processing.\n");
			break;
		}

		prP2pGlueDevInfo->prScanRequest = request;

		/* Should find out why the n_channels so many? */
		if (request->n_channels > MAXIMUM_OPERATION_CHANNEL_LIST) {
			request->n_channels = MAXIMUM_OPERATION_CHANNEL_LIST;
			fgIsFullChanScan = TRUE;
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
			ASSERT(FALSE);
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
			DBGLOG(P2P, TRACE,
			       "Scanning Channel:%d,  freq: %d\n", prRfChannelInfo->ucChannelNum,
			       prChannel->center_freq);
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
		if (fgIsFullChanScan) {
			prMsgScanRequest->u4NumChannel = SCN_P2P_FULL_SCAN_PARAM;
			DBGLOG(P2P, INFO, "request->n_channels = SCN_P2P_FULL_SCAN_PARAM\n");
		}
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

		/* Backup scan request structure */
		/* The purpose of this backup is due to the kernel free the scan req */
		/* when the wpa supplicant down the iface before down, and it will */
		/* free the original scan request structure */
		/* In this case, the scan resoure could be locked by kernel, and */
		/* driver needs this work around to clear the state */
		kalMemCopy(&(prP2pGlueDevInfo->rBackupScanRequest),
			request, sizeof(struct cfg80211_scan_request));

		i4RetRslt = 0;
	} while (FALSE);

	return i4RetRslt;
}				/* mtk_p2p_cfg80211_scan */

int mtk_p2p_cfg80211_set_wiphy_params(struct wiphy *wiphy, u32 changed)
{
	INT_32 i4Rslt = -EINVAL;
	P_GLUE_INFO_T prGlueInfo = NULL;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_set_wiphy_params\n");
#endif

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

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_join_ibss\n");
#endif

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	/* not implemented yet */

	return -EINVAL;
}

int mtk_p2p_cfg80211_leave_ibss(struct wiphy *wiphy, struct net_device *dev)
{
	P_GLUE_INFO_T prGlueInfo = NULL;

	ASSERT(wiphy);

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_leave_ibss\n");
#endif

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	/* not implemented yet */

	return -EINVAL;
}

int mtk_p2p_cfg80211_set_txpower(struct wiphy *wiphy,
				 struct wireless_dev *wdev, enum nl80211_tx_power_setting type, int mbm)
{
	P_GLUE_INFO_T prGlueInfo = NULL;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_set_txpower\n");
#endif

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	/* not implemented yet */

	return -EINVAL;
}

int mtk_p2p_cfg80211_get_txpower(struct wiphy *wiphy, struct wireless_dev *wdev, int *dbm)
{
	P_GLUE_INFO_T prGlueInfo = NULL;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_get_txpower\n");
#endif

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

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_set_power_mgmt\n");
#endif

	ASSERT(wiphy);

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	if (enabled)
		ePowerMode = Param_PowerModeFast_PSP;
	else
		ePowerMode = Param_PowerModeCAM;

	DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_set_power_mgmt ps=%d.\n", enabled);

	if (mtk_Netdev_To_RoleIdx(prGlueInfo, ndev, &ucRoleIdx) != 0)
		return -EINVAL;

	if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, ucRoleIdx, &rPowerMode.ucBssIdx) != WLAN_STATUS_SUCCESS)
		return -EINVAL;

	rPowerMode.ePowerMode = ePowerMode;

	/* p2p_set_power_save */
	kalIoctl(prGlueInfo,
		 wlanoidSet802dot11PowerSaveProfile, &rPowerMode, sizeof(rPowerMode), FALSE, FALSE, TRUE, &u4Leng);

	return 0;
}

/* &&&&&&&&&&&&&&&&&&&&&&&&&& Add for ICS Wi-Fi Direct Support. &&&&&&&&&&&&&&&&&&&&&&& */
int mtk_p2p_cfg80211_start_ap(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_ap_settings *settings)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	INT_32 i4Rslt = -EINVAL;
	P_MSG_P2P_BEACON_UPDATE_T prP2pBcnUpdateMsg = (P_MSG_P2P_BEACON_UPDATE_T) NULL;
	P_MSG_P2P_START_AP_T prP2pStartAPMsg = (P_MSG_P2P_START_AP_T) NULL;
	PUINT_8 pucBuffer = (PUINT_8) NULL;
	UINT_8 ucRoleIdx = 0;
	struct cfg80211_chan_def *chandef;
	RF_CHANNEL_INFO_T rRfChnlInfo;

	/* RF_CHANNEL_INFO_T rRfChnlInfo; */
/* P_IE_SSID_T prSsidIE = (P_IE_SSID_T)NULL; */

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_start_ap\n");
#endif

	do {
		if ((wiphy == NULL) || (settings == NULL))
			break;

		DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_start_ap.\n");
		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

#if (CFG_SUPPORT_DFS_MASTER == 1)
		/*DFS todo 20161220_DFS*/
		netif_carrier_on(dev);
		netif_tx_start_all_queues(dev);
#endif

		chandef = &settings->chandef;

		if (mtk_Netdev_To_RoleIdx(prGlueInfo, dev, &ucRoleIdx) < 0)
			break;

		if (prGlueInfo->prP2PInfo[ucRoleIdx]->fgIsNetDevDetach) {
			netif_device_attach(dev);
			prGlueInfo->prP2PInfo[ucRoleIdx]
					->fgIsNetDevDetach = FALSE;
		}

		if (chandef) {
			mtk_p2p_cfg80211func_channel_format_switch(chandef, chandef->chan, &rRfChnlInfo);

			/* Follow the channel info from wifi.cfg prior to hostapd.conf */
			{
				P_ADAPTER_T prAdapter = (P_ADAPTER_T) NULL;
				P_WIFI_VAR_T prWifiVar = (P_WIFI_VAR_T)NULL;

				prAdapter = prGlueInfo->prAdapter;
				prWifiVar = &prAdapter->rWifiVar;

				if ((prWifiVar->ucApChannel != 0) &&
					(prWifiVar->ucApChnlDefFromCfg != 0) &&
					(prWifiVar->ucApChannel != rRfChnlInfo.ucChannelNum)) {
					rRfChnlInfo.ucChannelNum = prWifiVar->ucApChannel;
					rRfChnlInfo.eBand =
						(rRfChnlInfo.ucChannelNum <= 14) ? BAND_2G4 : BAND_5G;
					/* [TODO][20160829]If we will set SCO by nl80211_channel_type afterward,
					* to check if we need to modify SCO by wifi.cfg here
					*/
				}
			}

			p2pFuncSetChannel(prGlueInfo->prAdapter, ucRoleIdx, &rRfChnlInfo);
		} else
			DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_start_ap. !!! no CH def!!!\n");

		prP2pBcnUpdateMsg = (P_MSG_P2P_BEACON_UPDATE_T) cnmMemAlloc(prGlueInfo->prAdapter,
									    RAM_TYPE_MSG,
									    (sizeof(MSG_P2P_BEACON_UPDATE_T)
									     +
									     settings->beacon.head_len +
									     settings->beacon.tail_len +
									     settings->beacon.assocresp_ies_len));

		if (prP2pBcnUpdateMsg == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		prP2pBcnUpdateMsg->ucRoleIndex = ucRoleIdx;
		prP2pBcnUpdateMsg->rMsgHdr.eMsgId = MID_MNY_P2P_BEACON_UPDATE;
		pucBuffer = prP2pBcnUpdateMsg->aucBuffer;
		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_start_ap.(role %d)\n", ucRoleIdx);

#if (CFG_SUPPORT_DFS_MASTER == 1)
		if (p2pFuncGetDfsState() == DFS_STATE_DETECTED) {
			p2pFuncSetDfsState(DFS_STATE_INACTIVE);
		}
#endif
		if (settings->beacon.head_len != 0) {
			kalMemCopy(pucBuffer, settings->beacon.head, settings->beacon.head_len);

			prP2pBcnUpdateMsg->u4BcnHdrLen = settings->beacon.head_len;

			prP2pBcnUpdateMsg->pucBcnHdr = pucBuffer;

			pucBuffer += settings->beacon.head_len;
		} else {
			prP2pBcnUpdateMsg->u4BcnHdrLen = 0;

			prP2pBcnUpdateMsg->pucBcnHdr = NULL;
		}

		if (settings->beacon.tail_len != 0) {
			UINT_8 ucLen = settings->beacon.tail_len;

			prP2pBcnUpdateMsg->pucBcnBody = pucBuffer;
			kalMemCopy(pucBuffer, settings->beacon.tail, settings->beacon.tail_len);

			prP2pBcnUpdateMsg->u4BcnBodyLen = ucLen;

			pucBuffer += settings->beacon.tail_len;
		} else {
			prP2pBcnUpdateMsg->u4BcnBodyLen = 0;

			prP2pBcnUpdateMsg->pucBcnBody = NULL;
		}

		if ((settings->crypto.cipher_group == WLAN_CIPHER_SUITE_WEP40) ||
			(settings->crypto.cipher_group == WLAN_CIPHER_SUITE_WEP104))
			prP2pBcnUpdateMsg->fgIsWepCipher = TRUE;
		else
			prP2pBcnUpdateMsg->fgIsWepCipher = FALSE;

		if (settings->beacon.assocresp_ies_len != 0 && settings->beacon.assocresp_ies != NULL) {
			prP2pBcnUpdateMsg->pucAssocRespIE = pucBuffer;
			kalMemCopy(pucBuffer, settings->beacon.assocresp_ies, settings->beacon.assocresp_ies_len);
			prP2pBcnUpdateMsg->u4AssocRespLen = settings->beacon.assocresp_ies_len;
		} else {
			prP2pBcnUpdateMsg->u4AssocRespLen = 0;
			prP2pBcnUpdateMsg->pucAssocRespIE = NULL;
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

		prP2pStartAPMsg->fgIsPrivacy = settings->privacy;

		prP2pStartAPMsg->u4BcnInterval = settings->beacon_interval;

		prP2pStartAPMsg->u4DtimPeriod = settings->dtim_period;

		/* Copy NO SSID. */
		prP2pStartAPMsg->ucHiddenSsidType = settings->hidden_ssid;

		prP2pStartAPMsg->ucRoleIdx = ucRoleIdx;

		kalP2PSetRole(prGlueInfo, 2, ucRoleIdx);

		COPY_SSID(prP2pStartAPMsg->aucSsid, prP2pStartAPMsg->u2SsidLen, settings->ssid, settings->ssid_len);

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prP2pStartAPMsg, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;

	} while (FALSE);

	return i4Rslt;

/* /////////////////////// */
    /**
	 * struct cfg80211_ap_settings - AP configuration
	 *
	 * Used to configure an AP interface.
	 *
	 * @beacon: beacon data
	 * @beacon_interval: beacon interval
	 * @dtim_period: DTIM period
	 * @ssid: SSID to be used in the BSS (note: may be %NULL if not provided from
	 *      user space)
	 * @ssid_len: length of @ssid
	 * @hidden_ssid: whether to hide the SSID in Beacon/Probe Response frames
	 * @crypto: crypto settings
	 * @privacy: the BSS uses privacy
	 * @auth_type: Authentication type (algorithm)
	 * @inactivity_timeout: time in seconds to determine station's inactivity.
	 */
/* struct cfg80211_ap_settings { */
/* struct cfg80211_beacon_data beacon; */
/*  */
/* int beacon_interval, dtim_period; */
/* const u8 *ssid; */
/* size_t ssid_len; */
/* enum nl80211_hidden_ssid hidden_ssid; */
/* struct cfg80211_crypto_settings crypto; */
/* bool privacy; */
/* enum nl80211_auth_type auth_type; */
/* int inactivity_timeout; */
/* }; */
/* ////////////////// */
}				/* mtk_p2p_cfg80211_start_ap */

#if (CFG_SUPPORT_DFS_MASTER == 1)

static int mtk_p2p_cfg80211_start_radar_detection_impl(struct wiphy *wiphy, struct net_device *dev,
					struct cfg80211_chan_def *chandef, unsigned int cac_time_ms)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	INT_32 i4Rslt = -EINVAL;
	P_MSG_P2P_DFS_CAC_T prP2pDfsCacMsg = (P_MSG_P2P_DFS_CAC_T) NULL;
	UINT_8 ucRoleIdx = 0;
	RF_CHANNEL_INFO_T rRfChnlInfo;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR,
			"mtk_p2p_cfg80211_start_radar_detection_impl\n");
#endif

	do {
		if ((wiphy == NULL) || (chandef == NULL))
			break;

		DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_start_radar_detection.\n");
		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		if (mtk_Netdev_To_RoleIdx(prGlueInfo, dev, &ucRoleIdx) < 0)
			break;

		if (prGlueInfo->prP2PInfo[ucRoleIdx]->chandef == NULL) {
			prGlueInfo->prP2PInfo[ucRoleIdx]->chandef = (struct cfg80211_chan_def *)
				cnmMemAlloc(prGlueInfo->prAdapter,
				RAM_TYPE_BUF, sizeof(struct cfg80211_chan_def));

			prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->chan = (struct ieee80211_channel *)
				cnmMemAlloc(prGlueInfo->prAdapter,
				RAM_TYPE_BUF, sizeof(struct ieee80211_channel));
		}

		/* Copy chan def to local buffer*/
		prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->center_freq1 = chandef->center_freq1;
		prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->center_freq2 = chandef->center_freq2;
		prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->width = chandef->width;
		memcpy(prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->chan, chandef->chan,
							sizeof(struct ieee80211_channel));
		prGlueInfo->prP2PInfo[ucRoleIdx]->cac_time_ms = cac_time_ms;


			mtk_p2p_cfg80211func_channel_format_switch(chandef, chandef->chan, &rRfChnlInfo);

			p2pFuncSetChannel(prGlueInfo->prAdapter, ucRoleIdx, &rRfChnlInfo);

		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_start_radar_detection.(role %d)\n", ucRoleIdx);

		p2pFuncSetDfsState(DFS_STATE_INACTIVE);

		prP2pDfsCacMsg = (P_MSG_P2P_DFS_CAC_T) cnmMemAlloc(prGlueInfo->prAdapter,
								RAM_TYPE_MSG, sizeof(*prP2pDfsCacMsg));

		if (prP2pDfsCacMsg == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		prP2pDfsCacMsg->rMsgHdr.eMsgId = MID_MNY_P2P_DFS_CAC;

		switch (chandef->width) {
		case NL80211_CHAN_WIDTH_20_NOHT:
		case NL80211_CHAN_WIDTH_20:
		case NL80211_CHAN_WIDTH_40:
			prP2pDfsCacMsg->eChannelWidth = CW_20_40MHZ;
			break;

		case NL80211_CHAN_WIDTH_80:
			prP2pDfsCacMsg->eChannelWidth = CW_80MHZ;
			break;

		case NL80211_CHAN_WIDTH_80P80:
			prP2pDfsCacMsg->eChannelWidth = CW_80P80MHZ;
			break;

		default:
			DBGLOG(P2P, ERROR,
				"mtk_p2p_cfg80211_start_radar_detection. !!!Bandwidth do not support!!!\n");
			ASSERT(FALSE);
			break;
		}

		prP2pDfsCacMsg->ucRoleIdx = ucRoleIdx;

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prP2pDfsCacMsg, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;

	} while (FALSE);

	return i4Rslt;
}

#if KERNEL_VERSION(3, 15, 0) <= CFG80211_VERSION_CODE
int mtk_p2p_cfg80211_start_radar_detection(struct wiphy *wiphy, struct net_device *dev,
					struct cfg80211_chan_def *chandef, unsigned int cac_time_ms)
{
	return mtk_p2p_cfg80211_start_radar_detection_impl(
			wiphy, dev, chandef, cac_time_ms);
}
#else
int mtk_p2p_cfg80211_start_radar_detection(struct wiphy *wiphy, struct net_device *dev,
					struct cfg80211_chan_def *chandef)
{
	return mtk_p2p_cfg80211_start_radar_detection_impl(
			wiphy, dev, chandef, IEEE80211_DFS_MIN_CAC_TIME_MS);
}
#endif

#if KERNEL_VERSION(3, 13, 0) <= CFG80211_VERSION_CODE
int mtk_p2p_cfg80211_channel_switch(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_csa_settings *params)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	INT_32 i4Rslt = -EINVAL;
	P_MSG_P2P_BEACON_UPDATE_T prP2pBcnUpdateMsg = (P_MSG_P2P_BEACON_UPDATE_T) NULL;
	P_MSG_P2P_SET_NEW_CHANNEL_T prP2pSetNewChannelMsg = (P_MSG_P2P_SET_NEW_CHANNEL_T) NULL;
	PUINT_8 pucBuffer = (PUINT_8) NULL;
	UINT_8 ucRoleIdx = 0;
	RF_CHANNEL_INFO_T rRfChnlInfo;
	P_BSS_INFO_T prBssInfo;
	UINT_8 ucBssIndex;
	UINT_32 u4Len;


#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_channel_switch\n");
#endif

	do {
		if ((wiphy == NULL) || (params == NULL))
			break;

		DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_channel_switch.\n");
		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		if (mtk_Netdev_To_RoleIdx(prGlueInfo, dev, &ucRoleIdx) < 0)
			break;

		if (prGlueInfo->prP2PInfo[ucRoleIdx]->chandef == NULL) {
			prGlueInfo->prP2PInfo[ucRoleIdx]->chandef = (struct cfg80211_chan_def *)
				cnmMemAlloc(prGlueInfo->prAdapter,
				RAM_TYPE_BUF, sizeof(struct cfg80211_chan_def));

			prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->chan = (struct ieee80211_channel *)
				cnmMemAlloc(prGlueInfo->prAdapter,
				RAM_TYPE_BUF, sizeof(struct ieee80211_channel));
		}
		/* Copy chan def to local buffer*/
		prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->center_freq1 = params->chandef.center_freq1;
		prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->center_freq2 = params->chandef.center_freq2;
		prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->width = params->chandef.width;
		memcpy(prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->chan, params->chandef.chan,
							sizeof(struct ieee80211_channel));

			mtk_p2p_cfg80211func_channel_format_switch(&params->chandef,
								params->chandef.chan, &rRfChnlInfo);

			p2pFuncSetChannel(prGlueInfo->prAdapter, ucRoleIdx, &rRfChnlInfo);

		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_channel_switch.(role %d)\n", ucRoleIdx);

		if (prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->chan->
			dfs_state == NL80211_DFS_AVAILABLE
#if KERNEL_VERSION(3, 15, 0) <= CFG80211_VERSION_CODE
			&& prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->chan->
			dfs_cac_ms != 0
#endif
			)
			p2pFuncSetDfsState(DFS_STATE_ACTIVE);
		else
			p2pFuncSetDfsState(DFS_STATE_INACTIVE);

		/* Set CSA IE parameters */
		prGlueInfo->prAdapter->rWifiVar.fgCsaInProgress = TRUE;
		prGlueInfo->prAdapter->rWifiVar.ucChannelSwitchMode = 1;
		prGlueInfo->prAdapter->rWifiVar.ucNewChannelNumber =
							nicFreq2ChannelNum(params->chandef.chan->center_freq * 1000);
		prGlueInfo->prAdapter->rWifiVar.ucChannelSwitchCount = params->count;

		/* Set new channel parameters */
		prP2pSetNewChannelMsg = (P_MSG_P2P_SET_NEW_CHANNEL_T) cnmMemAlloc(prGlueInfo->prAdapter,
								RAM_TYPE_MSG, sizeof(*prP2pSetNewChannelMsg));

		if (prP2pSetNewChannelMsg == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		prP2pSetNewChannelMsg->rMsgHdr.eMsgId = MID_MNY_P2P_SET_NEW_CHANNEL;

		switch (params->chandef.width) {
		case NL80211_CHAN_WIDTH_20_NOHT:
		case NL80211_CHAN_WIDTH_20:
		case NL80211_CHAN_WIDTH_40:
			prP2pSetNewChannelMsg->eChannelWidth = CW_20_40MHZ;
			break;

		case NL80211_CHAN_WIDTH_80:
			prP2pSetNewChannelMsg->eChannelWidth = CW_80MHZ;
			break;

		case NL80211_CHAN_WIDTH_80P80:
			prP2pSetNewChannelMsg->eChannelWidth = CW_80P80MHZ;
			break;

		default:
			DBGLOG(P2P, ERROR,
				"mtk_p2p_cfg80211_channel_switch. !!!Bandwidth do not support!!!\n");
			ASSERT(FALSE);
			break;
		}

		prP2pSetNewChannelMsg->ucRoleIdx = ucRoleIdx;

		for (ucBssIndex = 0; ucBssIndex < BSS_INFO_NUM; ucBssIndex++) {
			prBssInfo = GET_BSS_INFO_BY_INDEX(prGlueInfo->prAdapter, ucBssIndex);

			if (prBssInfo && prBssInfo->fgIsDfsActive) {
				prP2pSetNewChannelMsg->ucBssIndex = ucBssIndex;
				break;
			}
		}

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prP2pSetNewChannelMsg, MSG_SEND_METHOD_BUF);

		/* Update beacon */
		if ((params->beacon_csa.head_len != 0) || (params->beacon_csa.tail_len != 0)) {
			u4Len = sizeof(MSG_P2P_BEACON_UPDATE_T) +
				params->beacon_csa.head_len + params->beacon_csa.tail_len;
			prP2pBcnUpdateMsg = (P_MSG_P2P_BEACON_UPDATE_T) cnmMemAlloc(prGlueInfo->prAdapter,
				RAM_TYPE_MSG, u4Len);

			if (prP2pBcnUpdateMsg == NULL) {
				ASSERT(FALSE);
				i4Rslt = -ENOMEM;
				break;
			}
			kalMemZero(prP2pBcnUpdateMsg, u4Len);

			prP2pBcnUpdateMsg->ucRoleIndex = ucRoleIdx;
			prP2pBcnUpdateMsg->rMsgHdr.eMsgId = MID_MNY_P2P_BEACON_UPDATE;
			pucBuffer = prP2pBcnUpdateMsg->aucBuffer;

			if (params->beacon_csa.head_len != 0) {
				kalMemCopy(pucBuffer, params->beacon_csa.head, params->beacon_csa.head_len);

				prP2pBcnUpdateMsg->u4BcnHdrLen = params->beacon_csa.head_len;

				prP2pBcnUpdateMsg->pucBcnHdr = pucBuffer;

				pucBuffer = (PUINT_8) ((ULONG) pucBuffer + (ULONG) params->beacon_csa.head_len);
			} else {
				prP2pBcnUpdateMsg->u4BcnHdrLen = 0;

				prP2pBcnUpdateMsg->pucBcnHdr = NULL;
			}

			if (params->beacon_csa.tail_len != 0) {
				UINT_8 ucLen = params->beacon_csa.tail_len;

				prP2pBcnUpdateMsg->pucBcnBody = pucBuffer;
				kalMemCopy(pucBuffer, params->beacon_csa.tail, params->beacon_csa.tail_len);

				prP2pBcnUpdateMsg->u4BcnBodyLen = ucLen;
			} else {
				prP2pBcnUpdateMsg->u4BcnBodyLen = 0;
				prP2pBcnUpdateMsg->pucBcnBody = NULL;
			}

			kalP2PSetRole(prGlueInfo, 2, ucRoleIdx);

			mboxSendMsg(prGlueInfo->prAdapter,
					MBOX_ID_0, (P_MSG_HDR_T) prP2pBcnUpdateMsg, MSG_SEND_METHOD_BUF);

			i4Rslt = 0; /* Return Success */
		}

	} while (FALSE);

	return i4Rslt;
}
#endif
#endif

#if 0
struct cfg80211_beacon_data {
	const u8 *head, *tail;
	const u8 *beacon_ies;
	const u8 *proberesp_ies;
	const u8 *assocresp_ies;
	const u8 *probe_resp;

	size_t head_len, tail_len;
	size_t beacon_ies_len;
	size_t proberesp_ies_len;
	size_t assocresp_ies_len;
	size_t probe_resp_len;
};
#endif

int mtk_p2p_cfg80211_change_beacon(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_beacon_data *info)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	INT_32 i4Rslt = -EINVAL;
	P_MSG_P2P_BEACON_UPDATE_T prP2pBcnUpdateMsg = (P_MSG_P2P_BEACON_UPDATE_T) NULL;
	PUINT_8 pucBuffer = (PUINT_8) NULL;
	UINT_8 ucRoleIdx = 0;
	UINT_32 u4Len = 0;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_change_beacon\n");
#endif

	do {
		if ((wiphy == NULL) || (info == NULL))
			break;

		DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_change_beacon.\n");
		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		if (mtk_Netdev_To_RoleIdx(prGlueInfo, dev, &ucRoleIdx) < 0)
			break;

		if ((info->head_len != 0) || (info->tail_len != 0)) {
			u4Len = sizeof(MSG_P2P_BEACON_UPDATE_T) +
				info->head_len + info->tail_len +
				info->assocresp_ies_len;

			prP2pBcnUpdateMsg =
			    (P_MSG_P2P_BEACON_UPDATE_T) cnmMemAlloc(prGlueInfo->prAdapter,
					RAM_TYPE_MSG, u4Len);

			if (prP2pBcnUpdateMsg == NULL) {
				ASSERT(FALSE);
				i4Rslt = -ENOMEM;
				break;
			}
			kalMemZero(prP2pBcnUpdateMsg, u4Len);

			prP2pBcnUpdateMsg->ucRoleIndex = ucRoleIdx;
			prP2pBcnUpdateMsg->rMsgHdr.eMsgId = MID_MNY_P2P_BEACON_UPDATE;
			pucBuffer = prP2pBcnUpdateMsg->aucBuffer;

			if (info->head_len != 0) {
				kalMemCopy(pucBuffer, info->head, info->head_len);

				prP2pBcnUpdateMsg->u4BcnHdrLen = info->head_len;

				prP2pBcnUpdateMsg->pucBcnHdr = pucBuffer;

				pucBuffer += info->head_len;
			} else {
				prP2pBcnUpdateMsg->u4BcnHdrLen = 0;

				prP2pBcnUpdateMsg->pucBcnHdr = NULL;
			}

			if (info->tail_len != 0) {
				UINT_8 ucLen = info->tail_len;

				prP2pBcnUpdateMsg->pucBcnBody = pucBuffer;
				kalMemCopy(pucBuffer, info->tail, info->tail_len);

				prP2pBcnUpdateMsg->u4BcnBodyLen = ucLen;

				pucBuffer += info->tail_len;
			} else {
				prP2pBcnUpdateMsg->u4BcnBodyLen = 0;
				prP2pBcnUpdateMsg->pucBcnBody = NULL;
			}

			if (info->assocresp_ies_len != 0 && info->assocresp_ies != NULL) {

				prP2pBcnUpdateMsg->pucAssocRespIE = pucBuffer;
				kalMemCopy(pucBuffer, info->assocresp_ies, info->assocresp_ies_len);
				prP2pBcnUpdateMsg->u4AssocRespLen = info->assocresp_ies_len;
			} else {
				prP2pBcnUpdateMsg->u4AssocRespLen = 0;
				prP2pBcnUpdateMsg->pucAssocRespIE = NULL;
			}

			kalP2PSetRole(prGlueInfo, 2, ucRoleIdx);

			mboxSendMsg(prGlueInfo->prAdapter,
				    MBOX_ID_0, (P_MSG_HDR_T) prP2pBcnUpdateMsg, MSG_SEND_METHOD_BUF);

			i4Rslt = 0; /* Return Success */
		}

		/* TODO: Probe Rsp, Assoc Rsp, Beacon IE update. */

/* ////////////////////////// */
/**
 * struct cfg80211_beacon_data - beacon data
 * @head: head portion of beacon (before TIM IE)
 *     or %NULL if not changed
 * @tail: tail portion of beacon (after TIM IE)
 *     or %NULL if not changed
 * @head_len: length of @head
 * @tail_len: length of @tail
 * @beacon_ies: extra information element(s) to add into Beacon frames or %NULL
 * @beacon_ies_len: length of beacon_ies in octets
 * @proberesp_ies: extra information element(s) to add into Probe Response
 *      frames or %NULL
 * @proberesp_ies_len: length of proberesp_ies in octets
 * @assocresp_ies: extra information element(s) to add into (Re)Association
 *      Response frames or %NULL
 * @assocresp_ies_len: length of assocresp_ies in octets
 * @probe_resp_len: length of probe response template (@probe_resp)
 * @probe_resp: probe response template (AP mode only)
 */
/* struct cfg80211_beacon_data { */
/* const u8 *head, *tail; */
/* const u8 *beacon_ies; */
/* const u8 *proberesp_ies; */
/* const u8 *assocresp_ies; */
/* const u8 *probe_resp; */

/* size_t head_len, tail_len; */
/* size_t beacon_ies_len; */
/* size_t proberesp_ies_len; */
/* size_t assocresp_ies_len; */
/* size_t probe_resp_len; */
/* }; */

/* ////////////////////////// */

	} while (FALSE);

	return i4Rslt;
}				/* mtk_p2p_cfg80211_change_beacon */

int mtk_p2p_cfg80211_stop_ap(struct wiphy *wiphy, struct net_device *dev)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	INT_32 i4Rslt = -EINVAL;
	P_MSG_P2P_SWITCH_OP_MODE_T prP2pSwitchMode = (P_MSG_P2P_SWITCH_OP_MODE_T) NULL;
	UINT_8 ucRoleIdx = 0;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_stop_ap\n");
#endif

	do {
		if (wiphy == NULL)
			break;

		DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_stop_ap.\n");
		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

#if (CFG_SUPPORT_DFS_MASTER == 1)
		if (dev->ieee80211_ptr->iftype == NL80211_IFTYPE_AP) {
			netif_carrier_off(dev);
			netif_tx_stop_all_queues(dev);
		}
#endif

		if (mtk_Netdev_To_RoleIdx(prGlueInfo, dev, &ucRoleIdx) < 0)
			break;
		/* Switch OP MOde. */
		prP2pSwitchMode = cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG, sizeof(MSG_P2P_SWITCH_OP_MODE_T));

		if (prP2pSwitchMode == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		prP2pSwitchMode->rMsgHdr.eMsgId = MID_MNY_P2P_STOP_AP;
		prP2pSwitchMode->ucRoleIdx = ucRoleIdx;

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prP2pSwitchMode, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;
}				/* mtk_p2p_cfg80211_stop_ap */

/* TODO: */
int mtk_p2p_cfg80211_deauth(struct wiphy *wiphy, struct net_device *dev, struct cfg80211_deauth_request *req)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;

	ASSERT(wiphy);

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_deauth\n");
#endif

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

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_disassoc\n");
#endif

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_disassoc.\n");

	/* not implemented yet */

	return -EINVAL;
}				/* mtk_p2p_cfg80211_disassoc */

int mtk_p2p_cfg80211_remain_on_channel(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       struct ieee80211_channel *chan, unsigned int duration, u64 *cookie)
{
	INT_32 i4Rslt = -EINVAL;
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	P_GL_P2P_DEV_INFO_T prGlueP2pDevInfo = (P_GL_P2P_DEV_INFO_T) NULL;
	P_MSG_P2P_CHNL_REQUEST_T prMsgChnlReq = (P_MSG_P2P_CHNL_REQUEST_T) NULL;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_remain_on_channel\n");
#endif

	DBGLOG(P2P, STATE, "mtk_p2p_cfg80211_remain_on_channel\n");

	do {
		if ((wiphy == NULL) ||
		    /* (dev == NULL) || */
		    (chan == NULL) || (cookie == NULL)) {
			break;
		}

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		if (!wlanGetHifState(prGlueInfo))
			break;

		prGlueP2pDevInfo = prGlueInfo->prP2PDevInfo;

		*cookie = prGlueP2pDevInfo->u8Cookie++;

		prMsgChnlReq = cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG, sizeof(MSG_P2P_CHNL_REQUEST_T));

		if (prMsgChnlReq == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_remain_on_channel:%d\n", (INT_32) *cookie);

		prMsgChnlReq->rMsgHdr.eMsgId = MID_MNY_P2P_CHNL_REQ;
		prMsgChnlReq->u8Cookie = *cookie;
		prMsgChnlReq->u4Duration = duration;
		prMsgChnlReq->eChnlReqType = CH_REQ_TYPE_P2P_LISTEN;

		mtk_p2p_cfg80211func_channel_format_switch(NULL, chan,
							   &prMsgChnlReq->rChannelInfo);
		mtk_p2p_cfg80211func_channel_sco_switch(NL80211_CHAN_NO_HT, &prMsgChnlReq->eChnlSco);

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgChnlReq, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;
}

/* mtk_p2p_cfg80211_remain_on_channel */

int mtk_p2p_cfg80211_cancel_remain_on_channel(struct wiphy *wiphy, struct wireless_dev *wdev, u64 cookie)
{
	INT_32 i4Rslt = -EINVAL;
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	P_GL_P2P_INFO_T prGlueP2pInfo = (P_GL_P2P_INFO_T) NULL;
	P_MSG_P2P_CHNL_ABORT_T prMsgChnlAbort = (P_MSG_P2P_CHNL_ABORT_T) NULL;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR,
			"mtk_p2p_cfg80211_cancel_remain_on_channel\n");
#endif

	do {
		if (wiphy == NULL)
			break;

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));
		prGlueP2pInfo = prGlueInfo->prP2PInfo[0];

		prMsgChnlAbort = cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG, sizeof(MSG_P2P_CHNL_ABORT_T));

		if (prMsgChnlAbort == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_cancel_remain_on_channel%d\n", (INT_32) cookie);

		prMsgChnlAbort->rMsgHdr.eMsgId = MID_MNY_P2P_CHNL_ABORT;
		prMsgChnlAbort->u8Cookie = cookie;

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prMsgChnlAbort, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;
}				/* mtk_p2p_cfg80211_cancel_remain_on_channel */
#if KERNEL_VERSION(3, 14, 0) <= CFG80211_VERSION_CODE
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

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_mgmt_tx\n");
#endif

	do {
		if ((wiphy == NULL) || (wdev == NULL) || (params == 0) || (cookie == NULL))
			break;

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));
		/* The owner of this function please check following line*/
		prGlueP2pInfo = (P_GL_P2P_INFO_T) prGlueInfo->prP2PInfo;

		dev = wdev->netdev;

		/* The owner of this function please check following line*/
		if (mtk_Netdev_To_RoleIdx(prGlueInfo, dev, &ucRoleIdx) < 0) {
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
		/* The owner of this function please check following line*/
		*cookie = prGlueInfo->u8Cookie++;

		/* Channel & Channel Type & Wait time are ignored. */
		prMsgTxReq = cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG, sizeof(MSG_P2P_MGMT_TX_REQUEST_T));

		if (prMsgTxReq == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		if (params->offchan) {
			DBGLOG(P2P, TRACE, "   Off channel TRUE\n");
			prMsgTxReq->fgIsOffChannel = TRUE;

			mtk_p2p_cfg80211func_channel_format_switch(NULL, params->chan,
								   &prMsgTxReq->rChannelInfo);
			mtk_p2p_cfg80211func_channel_sco_switch(NL80211_CHAN_NO_HT, &prMsgTxReq->eChnlExt);
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
#else
int mtk_p2p_cfg80211_mgmt_tx(struct wiphy *wiphy,
			     struct wireless_dev *wdev,
			     struct ieee80211_channel *chan, bool offchan,
			     unsigned int wait, const u8 *buf, size_t len,
			     bool no_cck, bool dont_wait_for_ack, u64 *cookie)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	P_GL_P2P_DEV_INFO_T prGlueP2pDevInfo = (P_GL_P2P_DEV_INFO_T) NULL;
	INT_32 i4Rslt = -EINVAL;
	P_MSG_P2P_MGMT_TX_REQUEST_T prMsgTxReq = (P_MSG_P2P_MGMT_TX_REQUEST_T) NULL;
	P_MSDU_INFO_T prMgmtFrame = (P_MSDU_INFO_T) NULL;
	PUINT_8 pucFrameBuf = (PUINT_8) NULL;
	PUINT_64 pu8GlCookie = (PUINT_64) NULL;
	UINT_8 ucRoleIdx = 0, ucBssIdx = 0;
	struct net_device *dev = NULL;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_mgmt_tx\n");
#endif

	do {
		if ((wiphy == NULL) || (buf == NULL) || (len == 0) ||
		    /* (dev == NULL) || */
		    (cookie == NULL)) {
			break;
		}

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));
		prGlueP2pDevInfo = prGlueInfo->prP2PDevInfo;

		dev = wdev->netdev;
		if (mtk_Netdev_To_RoleIdx(prGlueInfo, dev, &ucRoleIdx) < 0) {
			/* Device Interface. */
			ucBssIdx = P2P_DEV_BSS_INDEX;
		} else {
			ASSERT(ucRoleIdx < KAL_P2P_NUM);
			/* Role Interface. */
			if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, ucRoleIdx, &ucBssIdx) < 0) {
				/* Can't find BSS index. */
				break;
			}
		}

		*cookie = prGlueP2pDevInfo->u8Cookie++;

		/* Channel & Channel Type & Wait time are ignored. */
		prMsgTxReq = cnmMemAlloc(prGlueInfo->prAdapter, RAM_TYPE_MSG, sizeof(MSG_P2P_MGMT_TX_REQUEST_T));

		if (prMsgTxReq == NULL) {
			ASSERT(FALSE);
			i4Rslt = -ENOMEM;
			break;
		}

		if (offchan) {
			DBGLOG(P2P, TRACE, "   Off channel TRUE\n");
			prMsgTxReq->fgIsOffChannel = TRUE;

			mtk_p2p_cfg80211func_channel_format_switch(NULL, chan,
								   &prMsgTxReq->rChannelInfo);
			mtk_p2p_cfg80211func_channel_sco_switch(NL80211_CHAN_NO_HT, &prMsgTxReq->eChnlExt);
		} else {
			prMsgTxReq->fgIsOffChannel = FALSE;
		}

		if (no_cck)
			prMsgTxReq->fgNoneCckRate = TRUE;
		else
			prMsgTxReq->fgNoneCckRate = FALSE;

		if (dont_wait_for_ack)
			prMsgTxReq->fgIsWaitRsp = FALSE;
		else
			prMsgTxReq->fgIsWaitRsp = TRUE;
		prMgmtFrame =
		    cnmMgtPktAlloc(prGlueInfo->prAdapter, (UINT_32) (len + sizeof(UINT_64) + MAC_TX_RESERVED_FIELD));
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

		pu8GlCookie = (PUINT_64) ((ULONG) prMgmtFrame->prPacket + (ULONG) len + MAC_TX_RESERVED_FIELD);

		kalMemCopy(pucFrameBuf, buf, len);

		*pu8GlCookie = *cookie;

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
}				/* mtk_p2p_cfg80211_mgmt_tx */
#endif

int mtk_p2p_cfg80211_mgmt_tx_cancel_wait(struct wiphy *wiphy, struct wireless_dev *wdev, u64 cookie)
{
	INT_32 i4Rslt = -EINVAL;

	return i4Rslt;
}				/* mtk_p2p_cfg80211_mgmt_tx_cancel_wait */

int mtk_p2p_cfg80211_change_bss(struct wiphy *wiphy, struct net_device *dev, struct bss_parameters *params)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	INT_32 i4Rslt = -EINVAL;

	ASSERT(wiphy);

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_change_bss\n");
#endif

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

#if KERNEL_VERSION(3, 16, 0) <= CFG80211_VERSION_CODE
#if KERNEL_VERSION(3, 19, 0) <= CFG80211_VERSION_CODE
static const u8 bcast_addr[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
int mtk_p2p_cfg80211_del_station(struct wiphy *wiphy, struct net_device *dev, struct station_del_parameters *params)
{
	const u8 *mac = params->mac ? params->mac : bcast_addr;
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	INT_32 i4Rslt = -EINVAL;
	P_MSG_P2P_CONNECTION_ABORT_T prDisconnectMsg = (P_MSG_P2P_CONNECTION_ABORT_T) NULL;
	UINT_8 aucBcMac[] = BC_MAC_ADDR;
	UINT_8 ucRoleIdx = 0;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_del_station\n");
#endif

	do {
		if ((wiphy == NULL) || (dev == NULL))
			break;

		if (mac == NULL)
			mac = aucBcMac;

		DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_del_station.\n");

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));


		if (mtk_Netdev_To_RoleIdx(prGlueInfo, dev, &ucRoleIdx) < 0)
			break;
		/* prDisconnectMsg = (P_MSG_P2P_CONNECTION_ABORT_T)kalMemAlloc(sizeof(MSG_P2P_CONNECTION_ABORT_T),
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
		prDisconnectMsg->u2ReasonCode = REASON_CODE_UNSPECIFIED;
		prDisconnectMsg->fgSendDeauth = TRUE;


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
	UINT_8 ucRoleIdx = 0;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_del_station\n");
#endif

	do {
		if ((wiphy == NULL) || (dev == NULL))
			break;

		if (mac == NULL)
			mac = aucBcMac;

		DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_del_station.\n");

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));


		if (mtk_Netdev_To_RoleIdx(prGlueInfo, dev, &ucRoleIdx) < 0)
			break;
		/* prDisconnectMsg = (P_MSG_P2P_CONNECTION_ABORT_T)kalMemAlloc(sizeof(MSG_P2P_CONNECTION_ABORT_T),
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
		prDisconnectMsg->u2ReasonCode = REASON_CODE_UNSPECIFIED;
		prDisconnectMsg->fgSendDeauth = TRUE;


		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prDisconnectMsg, MSG_SEND_METHOD_BUF);

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;

}				/* mtk_p2p_cfg80211_del_station */
#endif
#else
int mtk_p2p_cfg80211_del_station(struct wiphy *wiphy, struct net_device *dev, u8 *mac)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	INT_32 i4Rslt = -EINVAL;
	P_MSG_P2P_CONNECTION_ABORT_T prDisconnectMsg = (P_MSG_P2P_CONNECTION_ABORT_T) NULL;
	UINT_8 aucBcMac[] = BC_MAC_ADDR;
	UINT_8 ucRoleIdx = 0;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_del_station\n");
#endif

	do {
		if ((wiphy == NULL) || (dev == NULL))
			break;

		if (mac == NULL)
			mac = aucBcMac;

		DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_del_station.\n");

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));


		if (mtk_Netdev_To_RoleIdx(prGlueInfo, dev, &ucRoleIdx) < 0)
			break;
		/* prDisconnectMsg = (P_MSG_P2P_CONNECTION_ABORT_T)kalMemAlloc(sizeof(MSG_P2P_CONNECTION_ABORT_T),
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
		prDisconnectMsg->u2ReasonCode = REASON_CODE_UNSPECIFIED;
		prDisconnectMsg->fgSendDeauth = TRUE;


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
	UINT_8 ucRoleIdx = 0;
#if CFG_SUPPORT_REPLAY_DETECTION
	UINT_8 ucBssIndex = 0;
	P_BSS_INFO_T prBssInfo = NULL;
	struct SEC_DETECT_REPLAY_INFO *prDetRplyInfo = NULL;
#endif

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_connect\n");
#endif

	do {
		if ((wiphy == NULL) || (dev == NULL) || (sme == NULL))
			break;

		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_connect.\n");

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		if (mtk_Netdev_To_RoleIdx(prGlueInfo, dev, &ucRoleIdx) < 0)
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
		COPY_MAC_ADDR(prConnReqMsg->aucSrcMacAddr, dev->dev_addr);

		DBGLOG(P2P, TRACE, "Assoc Req IE Buffer Length:%zu\n",
		       sme->ie_len);
		kalMemCopy(prConnReqMsg->aucIEBuf, sme->ie, sme->ie_len);
		prConnReqMsg->u4IELen = sme->ie_len;

		kalP2PSetCipher(prGlueInfo, IW_AUTH_CIPHER_NONE, ucRoleIdx);

		if (sme->crypto.n_ciphers_pairwise) {
			switch (sme->crypto.ciphers_pairwise[0]) {
			case WLAN_CIPHER_SUITE_WEP40:
			case WLAN_CIPHER_SUITE_WEP104:
				kalP2PSetCipher(prGlueInfo, IW_AUTH_CIPHER_WEP40, ucRoleIdx);
				break;
			case WLAN_CIPHER_SUITE_TKIP:
				kalP2PSetCipher(prGlueInfo, IW_AUTH_CIPHER_TKIP, ucRoleIdx);
				break;
			case WLAN_CIPHER_SUITE_CCMP:
			case WLAN_CIPHER_SUITE_AES_CMAC:
				kalP2PSetCipher(prGlueInfo, IW_AUTH_CIPHER_CCMP, ucRoleIdx);
				break;
			default:
				cnmMemFree(prGlueInfo->prAdapter, prConnReqMsg);
				DBGLOG(REQ, WARN, "invalid cipher pairwise (%d)\n", sme->crypto.ciphers_pairwise[0]);
				return -EINVAL;
			}
		}

		mtk_p2p_cfg80211func_channel_format_switch(NULL, sme->channel,
							   &prConnReqMsg->rChannelInfo);
		mtk_p2p_cfg80211func_channel_sco_switch(NL80211_CHAN_NO_HT, &prConnReqMsg->eChnlSco);

		mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0, (P_MSG_HDR_T) prConnReqMsg, MSG_SEND_METHOD_BUF);

#if CFG_SUPPORT_REPLAY_DETECTION
		if (p2pFuncRoleToBssIdx(prGlueInfo->prAdapter, ucRoleIdx, &ucBssIndex) != WLAN_STATUS_SUCCESS)
			return -EINVAL;

		prBssInfo = GET_BSS_INFO_BY_INDEX(prGlueInfo->prAdapter, ucBssIndex);
		ASSERT(prBssInfo);
		prDetRplyInfo = &prBssInfo->rDetRplyInfo;
		kalMemZero(prDetRplyInfo, sizeof(struct SEC_DETECT_REPLAY_INFO));
#endif

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

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_disconnect\n");
#endif

	do {
		if ((wiphy == NULL) || (dev == NULL))
			break;

		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_disconnect.\n");

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		if (mtk_Netdev_To_RoleIdx(prGlueInfo, dev, &ucRoleIdx) < 0)
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

#if KERNEL_VERSION(4, 12, 0) <= CFG80211_VERSION_CODE
int
mtk_p2p_cfg80211_change_iface(IN struct wiphy *wiphy,
			      IN struct net_device *ndev,
			      IN enum nl80211_iftype type,
			      IN struct vif_params *params)
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

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_change_iface\n");
#endif

	do {
		if ((wiphy == NULL) || (ndev == NULL))
			break;

		DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_change_iface.\n");

		if (ndev->ieee80211_ptr)
			ndev->ieee80211_ptr->iftype = type;

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		if (mtk_Netdev_To_RoleIdx(prGlueInfo, ndev, &ucRoleIdx) != 0) {
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
			DBGLOG(P2P, TRACE, "NL80211_IFTYPE_P2P_CLIENT.\n");
			prSwitchModeMsg->eOpMode = OP_MODE_INFRASTRUCTURE;
			kalP2PSetRole(prGlueInfo, 1, ucRoleIdx);
			break;
		case NL80211_IFTYPE_STATION:
				DBGLOG(P2P, TRACE, "NL80211_IFTYPE_STATION.\n");
			prSwitchModeMsg->eOpMode = OP_MODE_INFRASTRUCTURE;
			kalP2PSetRole(prGlueInfo, 1, ucRoleIdx);
			break;
		case NL80211_IFTYPE_AP:
			DBGLOG(P2P, TRACE, "NL80211_IFTYPE_AP.\n");
			prSwitchModeMsg->eOpMode = OP_MODE_ACCESS_POINT;
			kalP2PSetRole(prGlueInfo, 2, ucRoleIdx);
			break;
		case NL80211_IFTYPE_P2P_GO:
				DBGLOG(P2P, TRACE, "NL80211_IFTYPE_P2P_GO not AP.\n");
			prSwitchModeMsg->eOpMode = OP_MODE_ACCESS_POINT;
			kalP2PSetRole(prGlueInfo, 2, ucRoleIdx);
			break;
		default:
			DBGLOG(P2P, TRACE, "Other type :%d .\n", type);
			prSwitchModeMsg->eOpMode = OP_MODE_P2P_DEVICE;
			kalP2PSetRole(prGlueInfo, 0, ucRoleIdx);
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
	struct net_device *dev = (struct net_device *) NULL;
	RF_CHANNEL_INFO_T rRfChnlInfo;
	UINT_8 ucRoleIdx = 0;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_set_channel\n");
#endif

	if ((wiphy == NULL) || (chandef == NULL))
		return i4Rslt;

	dev = (struct net_device *) wiphy_dev(wiphy);

	do {
		DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_set_channel.\n");

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		mtk_p2p_cfg80211func_channel_format_switch(chandef, chandef->chan, &rRfChnlInfo);

		if (mtk_Netdev_To_RoleIdx(prGlueInfo, dev, &ucRoleIdx) < 0)
			break;

		p2pFuncSetChannel(prGlueInfo->prAdapter, ucRoleIdx, &rRfChnlInfo);

		i4Rslt = 0;
	} while (FALSE);

	return i4Rslt;

}

/* mtk_p2p_cfg80211_set_channel */

int
mtk_p2p_cfg80211_set_bitrate_mask(IN struct wiphy *wiphy,
				  IN struct net_device *dev,
				  IN const u8 *peer, IN const struct cfg80211_bitrate_mask *mask)
{
	INT_32 i4Rslt = -EINVAL;
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_set_bitrate_mask\n");
#endif

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
					  struct wireless_dev *wdev, IN u16 frame_type, IN bool reg)
{
#if 0
	P_MSG_P2P_MGMT_FRAME_REGISTER_T prMgmtFrameRegister = (P_MSG_P2P_MGMT_FRAME_REGISTER_T) NULL;
#endif
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) NULL;
	UINT_8 ucRoleIdx = 0;
	PUINT_32 pu4P2pPacketFilter = NULL;
	P_P2P_ROLE_FSM_INFO_T prP2pRoleFsmInfo = (P_P2P_ROLE_FSM_INFO_T) NULL;

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState() || g_u4HaltFlag) {
		DBGLOG(INIT, WARN, "wlan is halt, skip reg callback");
		return;
	}
	rst_data.entry_conut++;
	DBGLOG(INIT, TRACE, "entry_conut = %d\n", rst_data.entry_conut);
#else
	if (g_u4HaltFlag) {
		DBGLOG(RLM, WARN, "wlan is halt, skip reg callback\n");
		return;
	}
#endif

	do {
		if ((wiphy == NULL) || (wdev == NULL))
			break;

		DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_mgmt_frame_register\n");

		prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

		/* since p2p device share the aprRoleHandler so needs to check DevHandler 1st */
		if (prGlueInfo->prP2PInfo[0]->prDevHandler == wdev->netdev) {
			/* P2P device*/
			pu4P2pPacketFilter = &prGlueInfo->prP2PDevInfo->u4OsMgmtFrameFilter;
		} else {
			if (mtk_Netdev_To_RoleIdx(prGlueInfo, wdev->netdev, &ucRoleIdx) < 0) {
				/* P2P device*/
				DBGLOG(P2P, WARN, "mtk_p2p_cfg80211_mgmt_frame_register wireless dev match fail!\n");
				pu4P2pPacketFilter = &prGlueInfo->prP2PDevInfo->u4OsMgmtFrameFilter;
			} else {
				/* Non P2P device*/
				ASSERT(ucRoleIdx < KAL_P2P_NUM);
				DBGLOG(P2P, TRACE, "Open packet filer RoleIdx %u\n", ucRoleIdx);
				prP2pRoleFsmInfo = prGlueInfo->prAdapter->rWifiVar.aprP2pRoleFsmInfo[ucRoleIdx];
				pu4P2pPacketFilter = &prP2pRoleFsmInfo->u4P2pPacketFilter;
			}
		}
		switch (frame_type) {
		case MAC_FRAME_PROBE_REQ:
			if (reg) {
				*pu4P2pPacketFilter |= PARAM_PACKET_FILTER_PROBE_REQ;
				DBGLOG(P2P, TRACE, "Open packet filer probe request\n");
			} else {
				*pu4P2pPacketFilter &= ~PARAM_PACKET_FILTER_PROBE_REQ;
				DBGLOG(P2P, TRACE, "Close packet filer probe request\n");
			}
			break;
		case MAC_FRAME_ACTION:
			if (reg) {
				*pu4P2pPacketFilter |= PARAM_PACKET_FILTER_ACTION_FRAME;
				DBGLOG(P2P, TRACE, "Open packet filer action frame.\n");
			} else {
				*pu4P2pPacketFilter &= ~PARAM_PACKET_FILTER_ACTION_FRAME;
				DBGLOG(P2P, TRACE, "Close packet filer action frame.\n");
			}
			break;
		default:
			DBGLOG(P2P, ERROR, "Ask frog to add code for mgmt:%x\n", frame_type);
			break;
		}

		set_bit(GLUE_FLAG_FRAME_FILTER_BIT, &prGlueInfo->ulFlag);

		/* wake up main thread */
		wake_up_interruptible(&prGlueInfo->waitq);

		if (in_interrupt())
			DBGLOG(P2P, TRACE, "It is in interrupt level\n");
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

}				/* mtk_p2p_cfg80211_mgmt_frame_register */

#ifdef CONFIG_NL80211_TESTMODE

#if KERNEL_VERSION(3, 12, 0) <= CFG80211_VERSION_CODE
int mtk_p2p_cfg80211_testmode_cmd(struct wiphy *wiphy,
				  struct wireless_dev *wdev, void *data,
				  int len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_NL80211_DRIVER_TEST_PARAMS prParams = (P_NL80211_DRIVER_TEST_PARAMS) NULL;
	INT_32 i4Status = -EINVAL;

	ASSERT(wiphy);
	ASSERT(wdev);

#if CFG_CHIP_RESET_SUPPORT
		if (checkResetState())
			DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_testmode_cmd\n");
#endif

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_testmode_cmd\n");

	if (data && len) {
		prParams = (P_NL80211_DRIVER_TEST_PARAMS) data;
	} else {
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_testmode_cmd, data is NULL\n");
		return i4Status;
	}
	if (prParams->index >> 24 == 0x01) {
		/* New version */
		prParams->index = prParams->index & ~BITS(24, 31);
	} else {
		/* Old version */
		mtk_p2p_cfg80211_testmode_p2p_sigma_pre_cmd(wiphy, data, len);
		i4Status = 0;
		return i4Status;
	}

	/* Clear the version byte */
	prParams->index = prParams->index & ~BITS(24, 31);

	if (prParams) {
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
		case 2:	/* WFD */
#if CFG_SUPPORT_WFD
			/* use normal driver command wifi_display */
			/* i4Status = mtk_p2p_cfg80211_testmode_wfd_update_cmd(wiphy, data, len); */
#endif
			break;
		case 3:	/* Hotspot Client Management */
#if CFG_SUPPORT_HOTSPOT_WPS_MANAGER
			i4Status = mtk_p2p_cfg80211_testmode_hotspot_block_list_cmd(wiphy, data, len);
#endif
			break;
		case 0x10:
			i4Status = mtk_cfg80211_testmode_get_sta_statistics(wiphy, data, len, prGlueInfo);
			break;
#if CFG_SUPPORT_NFC_BEAM_PLUS
		case 0x11:	/*NFC Beam + Indication */
			if (data && len) {
				P_NL80211_DRIVER_SET_NFC_PARAMS prParams = (P_NL80211_DRIVER_SET_NFC_PARAMS) data;

				DBGLOG(P2P, INFO, "NFC: BEAM[%d]\n", prParams->NFC_Enable);
			}
			break;
		case 0x12:	/*NFC Beam + Indication */
			DBGLOG(P2P, INFO, "NFC: Polling\n");
			i4Status = mtk_cfg80211_testmode_get_scan_done(wiphy, data, len, prGlueInfo);
			break;
#endif
#if CFG_AUTO_CHANNEL_SEL_SUPPORT
		case 0x30:
			i4Status = mtk_p2p_cfg80211_testmode_get_best_channel(wiphy, data, len);
			break;
#endif
#if GO_STA_SCC
		case 0x50:
			4Status = mtk_p2p_cfg80211_go_channel_switch(
					wiphy, wdev, data, len);
			break
#endif

		default:
			i4Status = -EINVAL;
			break;
		}
	}

	return i4Status;

}
#else
int mtk_p2p_cfg80211_testmode_cmd(struct wiphy *wiphy, void *data, int len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_NL80211_DRIVER_TEST_PARAMS prParams = (P_NL80211_DRIVER_TEST_PARAMS) NULL;
	INT_32 i4Status = -EINVAL;

	ASSERT(wiphy);

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_testmode_cmd\n");
#endif

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	DBGLOG(P2P, INFO, "mtk_p2p_cfg80211_testmode_cmd\n");

	if (data && len) {
		prParams = (P_NL80211_DRIVER_TEST_PARAMS) data;
	} else {
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_testmode_cmd, data is NULL\n");
		return i4Status;
	}
	if (prParams->index >> 24 == 0x01) {
		/* New version */
		prParams->index = prParams->index & ~BITS(24, 31);
	} else {
		/* Old version */
		mtk_p2p_cfg80211_testmode_p2p_sigma_pre_cmd(wiphy, data, len);
		i4Status = 0;
		return i4Status;
	}

	/* Clear the version byte */
	prParams->index = prParams->index & ~BITS(24, 31);

	if (prParams) {
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
		case 2:	/* WFD */
#if CFG_SUPPORT_WFD
			/* use normal driver command wifi_display */
			/* i4Status = mtk_p2p_cfg80211_testmode_wfd_update_cmd(wiphy, data, len); */
#endif
			break;
		case 3:	/* Hotspot Client Management */
#if CFG_SUPPORT_HOTSPOT_WPS_MANAGER
			i4Status = mtk_p2p_cfg80211_testmode_hotspot_block_list_cmd(wiphy, data, len);
#endif
			break;
		case 0x10:
			i4Status = mtk_cfg80211_testmode_get_sta_statistics(wiphy, data, len, prGlueInfo);
			break;
#if CFG_SUPPORT_NFC_BEAM_PLUS
		case 0x11:	/*NFC Beam + Indication */
			if (data && len) {
				P_NL80211_DRIVER_SET_NFC_PARAMS prParams = (P_NL80211_DRIVER_SET_NFC_PARAMS) data;

				DBGLOG(P2P, INFO, "NFC: BEAM[%d]\n", prParams->NFC_Enable);
			}
			break;
		case 0x12:	/*NFC Beam + Indication */
			DBGLOG(P2P, INFO, "NFC: Polling\n");
			i4Status = mtk_cfg80211_testmode_get_scan_done(wiphy, data, len, prGlueInfo);
			break;
#endif
#if CFG_AUTO_CHANNEL_SEL_SUPPORT
		case 0x30:  /* Auto channel selection in LTE safe channels */
			i4Status = mtk_p2p_cfg80211_testmode_get_best_channel(wiphy, data, len);
			break;
#endif

		default:
			i4Status = -EINVAL;
			break;
		}
	}

	return i4Status;

}
#endif

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

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR,
			"mtk_p2p_cfg80211_testmode_p2p_sigma_pre_cmd\n");
#endif

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	kalMemZero(&rParams, sizeof(NL80211_DRIVER_TEST_PRE_PARAMS));

	prP2pSpecificBssInfo = prGlueInfo->prAdapter->rWifiVar.prP2pSpecificBssInfo[0];
	/* prP2pConnSettings = prGlueInfo->prAdapter->rWifiVar.prP2PConnSettings; */

	DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_testmode_cmd\n");

	if (data && len)
		memcpy(&rParams, data, len);

	DBGLOG(P2P, TRACE, "NL80211_ATTR_TESTDATA,idx_mode=%d idx=%d value=%lu\n",
	       (INT_16) rParams.idx_mode, (INT_16) rParams.idx, rParams.value);

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
		/* status = mtk_p2p_wext_set_oppps_param(prDev, info, wrqu,
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
	UINT_32 i;

	ASSERT(wiphy);

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR,
			"mtk_p2p_cfg80211_testmode_p2p_sigma_cmd\n");
#endif

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	prP2pSpecificBssInfo = prGlueInfo->prAdapter->rWifiVar.prP2pSpecificBssInfo[0];
	/* prP2pConnSettings = prGlueInfo->prAdapter->rWifiVar.prP2PConnSettings; */

	DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_testmode_p2p_sigma_cmd\n");

	if (data && len)
		prParams = (P_NL80211_DRIVER_P2P_SIGMA_PARAMS) data;

	index = (INT_32) prParams->idx;
	value = (INT_32) prParams->value;

	DBGLOG(P2P, TRACE, "NL80211_ATTR_TESTDATA, idx=%lu value=%lu\n",
	       (INT_32) prParams->idx, (INT_32) prParams->value);

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
		/* status = mtk_p2p_wext_set_oppps_param(prDev, info, wrqu,
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
		for (i = 0; i < KAL_P2P_NUM; i++)
			kalP2PSetMaxClients(prGlueInfo, value, i);
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

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR,
			"mtk_p2p_cfg80211_testmode_wfd_update_cmd\n");
#endif

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
	UINT_32 i;

	ASSERT(wiphy);

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR,
			"mtk_p2p_cfg80211_testmode_hotspot_block_list_cmd\n");
#endif

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));

	if (data && len)
		prParams = (P_NL80211_DRIVER_hotspot_block_PARAMS) data;

	DBGLOG(P2P, TRACE, "mtk_p2p_cfg80211_testmode_hotspot_block_list_cmd\n");

	for (i = 0; i < KAL_P2P_NUM; i++)
		fgIsValid |= kalP2PSetBlackList(prGlueInfo, prParams->aucBssid, prParams->ucblocked, i);

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

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR, "mtk_p2p_cfg80211_testmode_sw_cmd\n");
#endif

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
					   &prParams->adr, (UINT_32) 8, FALSE, FALSE, TRUE, &u4SetInfoLen);
		}
	}

	if (rstatus != WLAN_STATUS_SUCCESS)
		fgIsValid = -EFAULT;

	return fgIsValid;
}

#if CFG_AUTO_CHANNEL_SEL_SUPPORT
int mtk_p2p_cfg80211_testmode_get_best_channel(IN struct wiphy *wiphy, IN void *data, IN int len)
{

	struct sk_buff *skb;

	BOOLEAN fgIsReady = FALSE;

	P_GLUE_INFO_T prGlueInfo = NULL;
	RF_CHANNEL_INFO_T aucChannelList[MAX_2G_BAND_CHN_NUM];
	UINT_8 ucNumOfChannel, i, ucIdx;
	UINT_16 u2APNumScore = 0, u2UpThreshold = 0, u2LowThreshold = 0, ucInnerIdx = 0;
	UINT_32 u4BufLen, u4LteSafeChnBitMask_2G = 0;
	UINT_32 u4AcsChnReport[5];

	P_PARAM_GET_CHN_INFO prGetChnLoad, prQueryLteChn;
	PARAM_PREFER_CHN_INFO rPreferChannel = { 0, 0xFFFF, 0 };
	PARAM_PREFER_CHN_INFO arChannelDirtyScore_2G[MAX_2G_BAND_CHN_NUM];

	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;

	ASSERT(wiphy);

#if CFG_CHIP_RESET_SUPPORT
	if (checkResetState())
		DBGLOG(P2P, ERROR,
			"mtk_p2p_cfg80211_testmode_get_best_channel\n");
#endif

	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));
	if (!prGlueInfo) {
		DBGLOG(P2P, ERROR, "No glue info\n");
		return -EFAULT;
	}

	/* Prepare reply skb buffer */
	skb = cfg80211_testmode_alloc_reply_skb(wiphy, sizeof(u4AcsChnReport));
	if (!skb) {
		DBGLOG(P2P, ERROR, "Allocate skb failed\n");
		return -ENOMEM;
	}

	kalMemZero(u4AcsChnReport, sizeof(u4AcsChnReport));

	fgIsReady = prGlueInfo->prAdapter->rWifiVar.rChnLoadInfo.fgDataReadyBit;
	if (fgIsReady == FALSE)
		goto acs_report;

	/*
	 * 1. Get 2.4G Band channel list in current regulatory domain
	 */
	rlmDomainGetChnlList(prGlueInfo->prAdapter, BAND_2G4, TRUE,
			     MAX_2G_BAND_CHN_NUM, &ucNumOfChannel, aucChannelList);

	/*
	 * 2. Calculate each channel's dirty score
	 */
	prGetChnLoad = &(prGlueInfo->prAdapter->rWifiVar.rChnLoadInfo);

	for (i = 0; i < ucNumOfChannel; i++) {
		ucIdx = aucChannelList[i].ucChannelNum - 1;

		/* Current channel's dirty score */
		u2APNumScore = prGetChnLoad->rEachChnLoad[ucIdx].u2APNum * CHN_DIRTY_WEIGHT_UPPERBOUND;
		u2LowThreshold = u2UpThreshold = 3;

		if (ucIdx < 3) {
			u2LowThreshold = ucIdx;
			u2UpThreshold = 3;
		} else if (ucIdx >= (ucNumOfChannel - 3)) {
			u2LowThreshold = 3;
			u2UpThreshold = ucNumOfChannel - (ucIdx + 1);
		}

		/* Lower channel's dirty score */
		for (ucInnerIdx = 0; ucInnerIdx < u2LowThreshold; ucInnerIdx++) {
			u2APNumScore +=
				(prGetChnLoad->rEachChnLoad[ucIdx - ucInnerIdx - 1].u2APNum *
				 (CHN_DIRTY_WEIGHT_UPPERBOUND - 1 - ucInnerIdx));
		}

		/* Upper channel's dirty score */
		for (ucInnerIdx = 0; ucInnerIdx < u2UpThreshold; ucInnerIdx++) {
			u2APNumScore +=
				(prGetChnLoad->rEachChnLoad[ucIdx + ucInnerIdx + 1].u2APNum *
				 (CHN_DIRTY_WEIGHT_UPPERBOUND - 1 - ucInnerIdx));
		}

		arChannelDirtyScore_2G[i].ucChannel = aucChannelList[i].ucChannelNum;
		arChannelDirtyScore_2G[i].u2APNumScore = u2APNumScore;

		DBGLOG(P2P, INFO, "[ACS]channel=%d, AP num=%d, score=%d\n", aucChannelList[i].ucChannelNum,
		    prGetChnLoad->rEachChnLoad[ucIdx].u2APNum, u2APNumScore);
	}

	/*
	 * 3. Query LTE safe channels
	 */
	prQueryLteChn = kalMemAlloc(sizeof(PARAM_GET_CHN_INFO), VIR_MEM_TYPE);
	if (prQueryLteChn == NULL) {
		DBGLOG(P2P, ERROR, "Alloc prQueryLteChn failed\n");
		/* Continue anyway */
	} else {
		kalMemZero(prQueryLteChn, sizeof(PARAM_GET_CHN_INFO));

		rStatus = kalIoctl(prGlueInfo,
				   wlanoidQueryLteSafeChannel,
				   prQueryLteChn,
				   sizeof(PARAM_GET_CHN_INFO),
				   TRUE,
				   FALSE,
				   TRUE,
				   &u4BufLen);
		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(P2P, ERROR, "Query LTE safe channels failed\n");
			/* Continue anyway */
		}

		u4LteSafeChnBitMask_2G =
			prQueryLteChn->rLteSafeChnList.au4SafeChannelBitmask
				[NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_2G_BASE_1 - 1];
		if (!u4LteSafeChnBitMask_2G) {
			DBGLOG(P2P, WARN, "  Can't get any 2G4 safe channel from fw!?\n");
			u4LteSafeChnBitMask_2G = BITS(1, 14);
		}

		kalMemFree(prQueryLteChn, VIR_MEM_TYPE, sizeof(PARAM_GET_CHN_INFO));
	}

	/* 4. Find out the best channel, skip LTE unsafe channels */
	for (i = 0; i < ucNumOfChannel; i++) {
		if (!(u4LteSafeChnBitMask_2G & BIT(arChannelDirtyScore_2G[i].ucChannel)))
			continue;

		if (rPreferChannel.u2APNumScore >= arChannelDirtyScore_2G[i].u2APNumScore) {
			rPreferChannel.ucChannel = arChannelDirtyScore_2G[i].ucChannel;
			rPreferChannel.u2APNumScore = arChannelDirtyScore_2G[i].u2APNumScore;
		}
	}

	u4AcsChnReport[NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_2G_BASE_1 - 1] = fgIsReady ? BIT(31) : 0;
	if (rPreferChannel.ucChannel > 0)
		u4AcsChnReport[NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_2G_BASE_1 - 1] |= BIT(rPreferChannel.ucChannel - 1);

	/* ToDo: Support 5G Channel Selection */

acs_report:
	if (unlikely(nla_put_u32(skb, NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_2G_BASE_1,
		     u4AcsChnReport[NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_2G_BASE_1 - 1]) < 0))
		goto nla_put_failure;

	if (unlikely(nla_put_u32(skb, NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_5G_BASE_36,
		     u4AcsChnReport[NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_5G_BASE_36 - 1]) < 0))
		goto nla_put_failure;

	if (unlikely(nla_put_u32(skb, NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_5G_BASE_52,
		     u4AcsChnReport[NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_5G_BASE_52 - 1]) < 0))
		goto nla_put_failure;

	if (unlikely(nla_put_u32(skb, NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_5G_BASE_100,
		     u4AcsChnReport[NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_5G_BASE_100 - 1]) < 0))
		goto nla_put_failure;

	if (unlikely(nla_put_u32(skb, NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_5G_BASE_149,
		     u4AcsChnReport[NL80211_TESTMODE_AVAILABLE_CHAN_ATTR_5G_BASE_149 - 1]) < 0))
		goto nla_put_failure;

	DBGLOG(P2P, INFO, "[ACS]Relpy u4AcsChnReport[2G_BASE_1]=0x%08x\n", u4AcsChnReport[0]);

	return cfg80211_testmode_reply(skb);

nla_put_failure:
	kfree_skb(skb);
	return -EMSGSIZE;
}
#endif


#if GO_STA_SCC
int mtk_p2p_cfg80211_go_channel_switch(IN struct wiphy *wiphy,
		IN struct wireless_dev *wdev, IN void *data, IN int len)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	NT_32 i4Status = -EINVAL
	struct net_device *dev = (struct net_device *) NULL;
	P_MSG_P2P_SET_NEW_CHANNEL_T prP2pSetNewChannelMsg =
				(P_MSG_P2P_SET_NEW_CHANNEL_T) NULL;
	UINT_8 ucRoleIdx = 0;
	RF_CHANNEL_INFO_T rRfChnlInfo;
	struct cfg80211_go_chan_switch *p_go_chan_switch;
	struct cfg80211_chan_def *chan_def;

	p_go_chan_switch = (struct cfg80211_go_chan_switch *) data;
	dev = wdev->netdev;
	prGlueInfo = *((P_GLUE_INFO_T *) wiphy_priv(wiphy));
	if (mtk_Netdev_To_RoleIdx(prGlueInfo, dev, &ucRoleIdx) < 0)
		return -EINVAL;

	if (prGlueInfo->prP2PInfo[ucRoleIdx]->chandef == NULL) {
		prGlueInfo->prP2PInfo[ucRoleIdx]->chandef =
			(struct cfg80211_chan_def *)
			cnmMemAlloc(prGlueInfo->prAdapter,
			RAM_TYPE_BUF, sizeof(struct cfg80211_chan_def));

		prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->chan =
						(struct ieee80211_channel *)
			cnmMemAlloc(prGlueInfo->prAdapter,
			RAM_TYPE_BUF, sizeof(struct ieee80211_channel));
	}

	chan_def = p_go_chan_switch->chan_def;
	/* Copy chan def to local buffer*/
	prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->center_freq1 =
						chan_def->center_freq1;
	prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->center_freq2 =
						chan_def->center_freq2;
	prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->width = chan_def->width;
	memcpy(prGlueInfo->prP2PInfo[ucRoleIdx]->chandef->chan, chan_def->chan,
					sizeof(struct ieee80211_channel));

	mtk_p2p_cfg80211func_channel_format_switch(chan_def,
						chan_def->chan, &rRfChnlInfo);
	p2pFuncSetChannel(prGlueInfo->prAdapter, ucRoleIdx, &rRfChnlInfo);
	/* Set new channel parameters */
	prP2pSetNewChannelMsg =
	(P_MSG_P2P_SET_NEW_CHANNEL_T) cnmMemAlloc(prGlueInfo->prAdapter,
				RAM_TYPE_MSG, sizeof(*prP2pSetNewChannelMsg));
	if (prP2pSetNewChannelMsg == NULL) {
		i4Status = -ENOMEM;
		return i4Status;
	}
	prP2pSetNewChannelMsg->rMsgHdr.eMsgId = MID_MNY_P2P_SET_NEW_CHANNEL;

	switch (chan_def->width) {
	case NL80211_CHAN_WIDTH_20_NOHT:
	case NL80211_CHAN_WIDTH_20:
	case NL80211_CHAN_WIDTH_40:
		prP2pSetNewChannelMsg->eChannelWidth = CW_20_40MHZ;
		break;

	case NL80211_CHAN_WIDTH_80:
		prP2pSetNewChannelMsg->eChannelWidth = CW_80MHZ;
		break;

	case NL80211_CHAN_WIDTH_80P80:
		prP2pSetNewChannelMsg->eChannelWidth = CW_80P80MHZ;
		break;

	default:
		DBGLOG(P2P, ERROR,
			"Bandwidth do not support!!!\n");
		i4Status = -EINVAL;
		return i4Status;
	}

	prP2pSetNewChannelMsg->ucRoleIdx = ucRoleIdx;
	prP2pSetNewChannelMsg->ucBssIndex = 1;

	mboxSendMsg(prGlueInfo->prAdapter, MBOX_ID_0,
		 (P_MSG_HDR_T) prP2pSetNewChannelMsg, MSG_SEND_METHOD_BUF);

	i4Status = 0;
	return i4Status;
}
#endif

#endif

#endif /* CFG_ENABLE_WIFI_DIRECT && CFG_ENABLE_WIFI_DIRECT_CFG_80211 */
