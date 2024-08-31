/*
 * Exported  API by wl_cfg80211 Modules
 * Common function shared by MASTER driver
 *
 * Portions of this code are copyright (c) 2023 Cypress Semiconductor Corporation,
 * an Infineon company
 *
 * This program is the proprietary software of infineon and/or
 * its licensors, and may only be used, duplicated, modified or distributed
 * pursuant to the terms and conditions of a separate, written license
 * agreement executed between you and infineon (an "Authorized License").
 * Except as set forth in an Authorized License, infineon grants no license
 * (express or implied), right to use, or waiver of any kind with respect to
 * the Software, and infineon expressly reserves all rights in and to the
 * Software and all intellectual property rights therein.  IF YOU HAVE NO
 * AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
 * WAY, AND SHOULD IMMEDIATELY NOTIFY INFINEON AND DISCONTINUE ALL USE OF
 * THE SOFTWARE.
 *
 * Except as expressly set forth in the Authorized License,
 *
 * 1. This program, including its structure, sequence and organization,
 * constitutes the valuable trade secrets of infineon, and you shall use
 * all reasonable efforts to protect the confidentiality thereof, and to
 * use this information only in connection with your use of infineon
 * integrated circuit products.
 *
 * 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 * "AS IS" AND WITH ALL FAULTS AND INFINEON MAKES NO PROMISES,
 * REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
 * OTHERWISE, WITH RESPECT TO THE SOFTWARE.  INFINEON SPECIFICALLY
 * DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
 * NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
 * ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
 * OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 * 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
 * INFINEON OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL,
 * SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR
 * IN ANY WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
 * IF INFINEON HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii)
 * ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF
 * OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
 * NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 *
 *
 * <<Infineon-WL-IPTag/Open:>>
 *
 * $Id$
 */

#ifdef  WL_DHD_XR
#include <osl.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <wldev_common.h>
#include <bcmutils.h>
#include <dhd.h>
#include <dhd_dbg.h>
#ifdef WL_CFG80211
#include <wl_cfg80211.h>
#include <wl_cfg80211_xr.h>
#endif /* WL_CFG80211 */
#include <wl_cfgscan.h>
#ifdef DHD_BANDSTEER
#include <dhd_bandsteer.h>
#endif /* DHD_BANDSTEER */
#include <brcm_nl80211.h>
#include <wl_cfgvendor.h>
#include <ifx_nl80211.h>
#include <dhd_linux.h>
#include <linux/rtnetlink.h>
#ifdef  WL_DHD_XR_MASTER
int wl_cfg80211_dhd_xr_prim_netdev_attach(struct net_device *ndev,
	wl_iftype_t  wl_iftype, void *ifp,
		u8 bssidx, u8 ifidx) {
	struct wireless_dev *wdev = NULL;
	int ret =  0;
	struct bcm_cfg80211 *cfg = wl_cfg80211_get_bcmcfg();

	wdev = (struct wireless_dev *)MALLOCZ(cfg->osh, sizeof(*wdev));
	if (!wdev) {
		DHD_ERROR(("BCMDHDX wireless_dev alloc failed!\n"));
		return BCME_ERROR;
	}

	wdev->wiphy = bcmcfg_to_wiphy(cfg);
	wdev->iftype = wl_mode_to_nl80211_iftype(WL_MODE_BSS);

	wdev->netdev = ndev;
	ndev->ieee80211_ptr = wdev;
	SET_NETDEV_DEV(ndev, wiphy_dev(wdev->wiphy));

	/* Initialize with the station mode params */
	ret = wl_alloc_netinfo(cfg, ndev, wdev, wl_iftype,
			PM_ENABLE, bssidx, ifidx);
	if (unlikely(ret)) {
		printk("BCMDHDX wl_alloc_netinfo Error (%d)\n", ret);
		return BCME_ERROR;
	}

	cfg->xr_slave_prim_wdev = wdev;
	return BCME_OK;
}
EXPORT_SYMBOL(wl_cfg80211_dhd_xr_prim_netdev_attach);
#endif /* WL_DHD_XR_MASTER */

int dhd_xr_init(dhd_pub_t *dhdp)
{
	int ret = BCME_OK;
	xr_ctx_t *xr_ctx = NULL;
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	if (!dhdp) {
		WL_ERR(("dhdp is null\n"));
		return BCME_ERROR;
	}

	dhdp->xr_ctx = (void *) kzalloc(sizeof(xr_ctx_t), flags);
	if (!dhdp->xr_ctx) {
		DHD_ERROR(("XR ctx allocation failed\n"));
		return BCME_ERROR;
	}

	xr_ctx = (xr_ctx_t *)dhdp->xr_ctx;
	xr_ctx->xr_role = XR_ROLE;
	return ret;
}

int dhd_xr_deinit(dhd_pub_t *dhdp)
{
	int ret = BCME_OK;

	if (!dhdp) {
		WL_ERR(("dhdp is null\n"));
		return BCME_ERROR;
	}

	if (dhdp->xr_ctx) {
		DHD_ERROR(("XR ctx freed \n"));
		kfree(dhdp->xr_ctx);
		dhdp->xr_ctx = NULL;
	}

	return ret;
}

/* add_if */
struct wireless_dev *wl_cfg80211_add_if_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub,
		u8 wl_iftype, const char *name, u8 *mac)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_add_if_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_add_if_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return NULL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_ADD_IF;
	cmd->len = sizeof(xr_cmd_add_if_t);
	data = (xr_cmd_add_if_t *)&cmd->data[0];

	data->wl_iftype = wl_iftype;
	data->name = name;
	data->mac = mac;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->add_if_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return NULL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.add_if_wdev;

}

int wl_cfg80211_add_if_xr_reply(dhd_pub_t *dest_pub, struct wireless_dev *wdev)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_add_if_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_add_if_t *data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_ADD_IF;
	cmd->len = sizeof(xr_cmd_reply_add_if_t);
	data = (xr_cmd_reply_add_if_t *)&cmd->data[0];

	data->wdev = wdev;
	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);
	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return BCME_ERROR;
	}

	if (cmd)
		kfree(cmd);

	return ret;
}
int xr_cmd_add_if_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	struct net_device *primary_ndev = dhd_linux_get_primary_netdev(pub);
	struct bcm_cfg80211 *cfg = wl_get_cfg(primary_ndev);
	struct wireless_dev *wdev = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_add_if_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_add_if_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_add_if_t *)&xr_cmd->data[0];

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	wdev = wl_cfg80211_add_if(cfg, primary_ndev, cmd->wl_iftype, cmd->name, cmd->mac);

	if (dest_pub)
		ret = wl_cfg80211_add_if_xr_reply(dest_pub, wdev);

	return ret;
}

int xr_cmd_reply_add_if_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_add_if_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_add_if_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.add_if_wdev = reply->wdev;
	complete(&xr_ctx->xr_cmd_wait.add_if_wait);
	return ret;
}

/* del_if */
s32 wl_cfg80211_del_if_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub,
		struct wireless_dev *wdev, char *ifname)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_del_if_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_del_if_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_DEL_IF;
	cmd->len = sizeof(xr_cmd_del_if_t);
	data = (xr_cmd_del_if_t *)&cmd->data[0];

	data->wdev = wdev;
	data->ifname = ifname;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->del_if_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}
	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.del_if_status;

}

int wl_cfg80211_del_if_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_del_if_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_del_if_t *data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_DEL_IF;
	cmd->len = sizeof(xr_cmd_reply_del_if_t);
	data = (xr_cmd_reply_del_if_t *)&cmd->data[0];

	data->status = status;
	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return BCME_ERROR;
	}

	if (cmd)
		kfree(cmd);

	return ret;
}
int xr_cmd_del_if_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	s32 status = 0;
	int ret = BCME_OK;
	struct net_device *primary_ndev = dhd_linux_get_primary_netdev(pub);
	struct bcm_cfg80211 *cfg = wl_get_cfg(primary_ndev);
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_del_if_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_del_if_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_del_if_t *)&xr_cmd->data[0];

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = wl_cfg80211_del_if(cfg, primary_ndev, cmd->wdev, cmd->ifname);

	if (dest_pub) {
		ret = wl_cfg80211_del_if_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_del_if_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_del_if_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_del_if_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.del_if_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.del_if_wait);
	return ret;
}

#if defined(WL_CFG80211_P2P_DEV_IF)
s32
wl_cfg80211_scan_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy,
	struct cfg80211_scan_request *request)
#else
s32
wl_cfg80211_scan_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy,
	struct net_device *ndev, struct cfg80211_scan_request *request)
#endif /* WL_CFG80211_P2P_DEV_IF */
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_scan_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_scan_t *data = NULL;
	int ret = BCME_OK;
	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		return -EINVAL;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_SCAN;
	cmd->len = sizeof(xr_cmd_scan_t);
	data = (xr_cmd_scan_t *)&cmd->data[0];

	data->wiphy = wiphy;
#if !defined(WL_CFG80211_P2P_DEV_IF)
	data->ndev = ndev;
#endif // endif
	data->request = request;
	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return ret;
}

int xr_cmd_scan_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_scan_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);
	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_scan_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_scan_t *)&xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {

		DHD_ERROR(("XR_CMD_SCAN cfg is NULL\n"));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

#if defined(WL_CFG80211_P2P_DEV_IF)
	status = wl_cfg80211_scan(cmd->wiphy, cmd->request);
#else
	status = wl_cfg80211_scan(cmd->wiphy, cmd->ndev, cmd->request);
#endif /* WL_CFG80211_P2P_DEV_IF */

	return ret;
}

#if defined(WL_CFG80211_P2P_DEV_IF)
s32 wl_cfg80211_get_tx_power_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy,
        struct wireless_dev *wdev, s32 *dbm)
#else
s32 wl_cfg80211_get_tx_power_xr(dhd_pub_t *src_pub,dhd_pub_t *dest_pub, struct wiphy *wiphy, s32 *dbm)
#endif /* WL_CFG80211_P2P_DEV_IF */
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_get_tx_power_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_get_tx_power_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = 0;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL){
		return -EINVAL;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_GET_TX_POWER;
	cmd->len = sizeof(xr_cmd_get_tx_power_t);
	data = (xr_cmd_get_tx_power_t *)&cmd->data[0];

	data->wiphy = wiphy;
#if defined(WL_CFG80211_P2P_DEV_IF)
	data->wdev = wdev;
#endif // endif
	data->dbm = dbm;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->get_tx_power_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.get_tx_power_status;

}

int wl_cfg80211_get_tx_power_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_get_tx_power_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_get_tx_power_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_GET_TX_POWER;
	cmd->len = sizeof(xr_cmd_reply_get_tx_power_t);
	data = (xr_cmd_reply_get_tx_power_t *) &cmd->data[0];

	data->status = status;
	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);
	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);
	return ret;
}

int xr_cmd_get_tx_power_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_get_tx_power_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_get_tx_power_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_get_tx_power_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("XR_CMD_GET_TX_POWER cfg is NULL\n"));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

#if defined(WL_CFG80211_P2P_DEV_IF)
	status = wl_cfg80211_get_tx_power(cmd->wiphy, cmd->wdev, cmd->dbm);
#else
	status = wl_cfg80211_get_tx_power(cmd->wiphy, cmd->dbm);
#endif /* WL_CFG80211_P2P_DEV_IF */
	if (dest_pub) {
		wl_cfg80211_get_tx_power_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_get_tx_power_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_get_tx_power_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_get_tx_power_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.get_tx_power_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.get_tx_power_wait);

	return ret;
}
/* set_power_mgmt */
s32
wl_cfg80211_set_power_mgmt_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy, struct net_device *dev,
        bool enabled, s32 timeout)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_set_power_mgmt_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_set_power_mgmt_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = 0;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		return -EINVAL;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_SET_POWER_MGMT;
	cmd->len = sizeof(xr_cmd_set_power_mgmt_t);
	data = (xr_cmd_set_power_mgmt_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;
	data->enabled = enabled;
	data->timeout = timeout;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->set_power_mgmt_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.set_power_mgmt_status;

}

int wl_cfg80211_set_power_mgmt_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_set_power_mgmt_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_set_power_mgmt_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_SET_POWER_MGMT;
	cmd->len = sizeof(xr_cmd_reply_set_power_mgmt_t);
	data = (xr_cmd_reply_set_power_mgmt_t *) &cmd->data[0];

	data->status = status;
	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);
	return ret;
}

int xr_cmd_set_power_mgmt_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_set_power_mgmt_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_set_power_mgmt_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_set_power_mgmt_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);
	status = wl_cfg80211_set_power_mgmt(cmd->wiphy, cmd->dev, cmd->enabled, cmd->timeout);

	if (dest_pub) {
		wl_cfg80211_set_power_mgmt_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_set_power_mgmt_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_set_power_mgmt_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_set_power_mgmt_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.set_power_mgmt_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.set_power_mgmt_wait);

	return ret;
}

/* wl_cfg80211_flush_pmksa */
s32 wl_cfg80211_flush_pmksa_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy, struct net_device *dev)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_flush_pmksa_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_flush_pmksa_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL){
		return -EINVAL;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_FLUSH_PMKSA;
	cmd->len = sizeof(xr_cmd_flush_pmksa_t);
	data = (xr_cmd_flush_pmksa_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->flush_pmksa_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.flush_pmksa_status;
}

int wl_cfg80211_flush_pmksa_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_flush_pmksa_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_flush_pmksa_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_FLUSH_PMKSA;
	cmd->len = sizeof(xr_cmd_reply_flush_pmksa_t);
	data = (xr_cmd_reply_flush_pmksa_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);

	return ret;
}

int xr_cmd_flush_pmksa_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_flush_pmksa_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_flush_pmksa_t))	{
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_flush_pmksa_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);
	status = wl_cfg80211_flush_pmksa(cmd->wiphy, cmd->dev);

	if (dest_pub) {
		wl_cfg80211_flush_pmksa_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_flush_pmksa_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_flush_pmksa_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_flush_pmksa_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.flush_pmksa_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.flush_pmksa_wait);

	return ret;
}
/* change_virtual_iface */
s32 wl_cfg80211_change_virtual_iface_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy, struct net_device *ndev,
        enum nl80211_iftype type,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
        u32 *flags,
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0) */
        struct vif_params *params)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_change_virtual_iface_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_change_virtual_iface_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL){
		return -EINVAL;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_CHANGE_VIRUTAL_IFACE;
	cmd->len = sizeof(xr_cmd_change_virtual_iface_t);
	data = (xr_cmd_change_virtual_iface_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->ndev = ndev;
	data->type = type;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
	data->flags = flags;
#endif // endif
	data->params = params;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->change_virtual_iface_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.change_virtual_iface_status;

}

int wl_cfg80211_change_virtual_iface_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_change_virtual_iface_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_change_virtual_iface_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_CHANGE_VIRUTAL_IFACE;
	cmd->len = sizeof(xr_cmd_reply_change_virtual_iface_t);
	data = (xr_cmd_reply_change_virtual_iface_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);
	return ret;
}

int xr_cmd_change_virtual_iface_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_change_virtual_iface_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n",__func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_change_virtual_iface_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_change_virtual_iface_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
	status = wl_cfg80211_change_virtual_iface(cmd->wiphy, cmd->ndev, cmd->type, cmd->flags, cmd->params);
#else
	status = wl_cfg80211_change_virtual_iface(cmd->wiphy, cmd->ndev, cmd->type, cmd->params);
#endif // endif
	if (dest_pub) {
		wl_cfg80211_change_virtual_iface_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_change_virtual_iface_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_change_virtual_iface_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_change_virtual_iface_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.change_virtual_iface_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.change_virtual_iface_wait);

	return ret;
}

#ifdef WL_6E
s32
wl_stop_fils_6g_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy, struct net_device *dev, u8 fils_stop)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_stop_fils_6g_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_stop_fils_6g_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_STOP_FILS_6G;
	cmd->len = sizeof(xr_cmd_stop_fils_6g_t);
	data = (xr_cmd_stop_fils_6g_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;
	data->stop_fils_6g_value = fils_stop;
	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->stop_fils_6g_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.stop_fils_6g_status;

}
#endif /* WL_6E */

/* start_ap */
s32
wl_cfg80211_start_ap_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy, struct net_device *dev, struct cfg80211_ap_settings *info)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_start_ap_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_start_ap_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_START_AP;
	cmd->len = sizeof(xr_cmd_start_ap_t);
	data = (xr_cmd_start_ap_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;
	data->info = info;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->start_ap_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.start_ap_status;

}

int wl_cfg80211_start_ap_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_start_ap_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_start_ap_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_START_AP;
	cmd->len = sizeof(xr_cmd_reply_start_ap_t);
	data = (xr_cmd_reply_start_ap_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);

	return ret;
}
int xr_cmd_start_ap_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_start_ap_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_start_ap_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_start_ap_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = wl_cfg80211_start_ap(cmd->wiphy, cmd->dev, cmd->info);

	if(dest_pub) {
		wl_cfg80211_start_ap_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_start_ap_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_start_ap_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_start_ap_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.start_ap_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.start_ap_wait);

	return ret;
}

#ifdef WL_CFG80211_ACL
/*set_mac_acl*/
int wl_cfg80211_set_mac_acl_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy, struct net_device *cfgdev, const struct cfg80211_acl_data *acl)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_set_mac_acl_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_set_mac_acl_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_SET_MAC_ACL;
	cmd->len = sizeof(xr_cmd_set_mac_acl_t);
	data = (xr_cmd_set_mac_acl_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->cfgdev = cfgdev;
	data->acl = acl;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->set_mac_acl_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.set_mac_acl_status;

}

int wl_cfg80211_set_mac_acl_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_set_mac_acl_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_set_mac_acl_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_SET_MAC_ACL;
	cmd->len = sizeof(xr_cmd_reply_set_mac_acl_t);
	data = (xr_cmd_reply_set_mac_acl_t *) &cmd->data[0];

	data->status = status;

	dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);

	return ret;
}
int xr_cmd_set_mac_acl_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_set_mac_acl_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_set_mac_acl_t))	{
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_set_mac_acl_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {

		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = wl_cfg80211_set_mac_acl(cmd->wiphy, cmd->cfgdev, cmd->acl);

	if (dest_pub) {
		wl_cfg80211_set_mac_acl_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_set_mac_acl_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_set_mac_acl_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_set_mac_acl_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.set_mac_acl_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.set_mac_acl_wait);

	return ret;
}
#endif /* WL_CFG80211_ACL */
/* change_bss */
s32
wl_cfg80211_change_bss_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy, struct net_device *dev, struct bss_parameters *params)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_change_bss_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_change_bss_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_CHANGE_BSS;
	cmd->len = sizeof(xr_cmd_change_bss_t);
	data = (xr_cmd_change_bss_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;
	data->params = params;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->change_bss_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.change_bss_status;

}

int wl_cfg80211_change_bss_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_change_bss_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_change_bss_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_CHANGE_BSS;
	cmd->len = sizeof(xr_cmd_reply_change_bss_t);
	data = (xr_cmd_reply_change_bss_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);

	return ret;
}
int xr_cmd_change_bss_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_change_bss_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if(xr_cmd->len != sizeof(xr_cmd_change_bss_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_change_bss_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = wl_cfg80211_change_bss(cmd->wiphy, cmd->dev, cmd->params);

	if (dest_pub) {
		wl_cfg80211_change_bss_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_change_bss_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_change_bss_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_change_bss_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.change_bss_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.change_bss_wait);

	return ret;
}

/* add_key */
s32
wl_cfg80211_add_key_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy, struct net_device *dev, u8 key_idx, bool pairwise, const u8 *mac_addr,
        struct key_params *params)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_add_key_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_add_key_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_ADD_KEY;
	cmd->len = sizeof(xr_cmd_add_key_t);
	data = (xr_cmd_add_key_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;
	data->key_idx = key_idx;
	data->pairwise = pairwise;
	data->mac_addr = mac_addr;
	data->params = params;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->add_key_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.add_key_status;

}

int wl_cfg80211_add_key_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_add_key_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_add_key_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_ADD_KEY;
	cmd->len = sizeof(xr_cmd_reply_add_key_t);
	data = (xr_cmd_reply_add_key_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if(cmd)
		kfree(cmd);

	return ret;
}

int xr_cmd_add_key_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_add_key_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_add_key_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_add_key_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);

	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = WL_CFG80211_ADD_KEY(cmd->wiphy, cmd->dev, cmd->link_id,
			cmd->key_idx, cmd->pairwise, cmd->mac_addr, cmd->params);

	if (dest_pub) {
		wl_cfg80211_add_key_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_add_key_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_add_key_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_add_key_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.add_key_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.add_key_wait);

	return ret;
}

/* set_channel */
s32
wl_cfg80211_set_channel_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy, struct net_device *dev, struct ieee80211_channel *chan, enum nl80211_channel_type channel_type)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_set_channel_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_set_channel_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_SET_CHANNEL;
	cmd->len = sizeof(xr_cmd_set_channel_t);
	data = (xr_cmd_set_channel_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;
	data->chan = chan;
	data->channel_type = channel_type;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->set_channel_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.set_channel_status;

}

int wl_cfg80211_set_channel_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_set_channel_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_set_channel_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_SET_CHANNEL;
	cmd->len = sizeof(xr_cmd_reply_set_channel_t);
	data = (xr_cmd_reply_set_channel_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);

	return ret;
}
int xr_cmd_set_channel_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_set_channel_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_set_channel_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_set_channel_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = wl_cfg80211_set_channel(cmd->wiphy, cmd->dev, cmd->chan,
		cmd->channel_type);

	if (dest_pub) {
		wl_cfg80211_set_channel_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_set_channel_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_set_channel_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_set_channel_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.set_channel_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.set_channel_wait);

	return ret;
}

/* config_default_key */
s32
wl_cfg80211_config_default_key_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy,
	struct net_device *dev, u8 key_idx, bool unicast, bool multicast)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_config_default_key_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_config_default_key_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_CONFIG_DEFAULT_KEY;
	cmd->len = sizeof(xr_cmd_config_default_key_t);
	data = (xr_cmd_config_default_key_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;
	data->key_idx = key_idx;
	data->unicast = unicast;
	data->multicast = multicast;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->config_default_key_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.config_default_key_status;

}

int wl_cfg80211_config_default_key_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_config_default_key_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_config_default_key_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_CONFIG_DEFAULT_KEY;
	cmd->len = sizeof(xr_cmd_reply_config_default_key_t);
	data = (xr_cmd_reply_config_default_key_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);

	return ret;
}
int xr_cmd_config_default_key_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_config_default_key_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_config_default_key_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_config_default_key_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = WL_CFG80211_CONFIG_DEFAULT_KEY(cmd->wiphy, cmd->dev, cmd->link_id,
			cmd->key_idx, cmd->unicast, cmd->multicast);

	if (dest_pub) {
		wl_cfg80211_config_default_key_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_config_default_key_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_config_default_key_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_config_default_key_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.config_default_key_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.config_default_key_wait);

	return ret;
}

/* stop_ap */
s32
wl_cfg80211_stop_ap_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy, struct net_device *dev)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_stop_ap_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_stop_ap_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_STOP_AP;
	cmd->len = sizeof(xr_cmd_stop_ap_t);
	data = (xr_cmd_stop_ap_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->stop_ap_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.stop_ap_status;

}

int wl_cfg80211_stop_ap_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_stop_ap_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_stop_ap_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_STOP_AP;
	cmd->len = sizeof(xr_cmd_reply_stop_ap_t);
	data = (xr_cmd_reply_stop_ap_t *) &cmd->data[0];

	data->status = status;

	dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);

	return ret;
}
int xr_cmd_stop_ap_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_stop_ap_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_stop_ap_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_stop_ap_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 2)) || \
	defined(DHD_ANDROID_KERNEL5_15_SUPPORT)
	status = wl_cfg80211_stop_ap(cmd->wiphy, cmd->dev, 0);
#else
	status = wl_cfg80211_stop_ap(cmd->wiphy, cmd->dev);
#endif // endif
	if (dest_pub) {
		wl_cfg80211_stop_ap_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_stop_ap_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_stop_ap_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_stop_ap_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.stop_ap_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.stop_ap_wait);

	return ret;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
s32
wl_cfg80211_del_station_xr(
		dhd_pub_t *src_pub, dhd_pub_t *dest_pub,
                struct wiphy *wiphy, struct net_device *ndev,
                struct station_del_parameters *params)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
s32
wl_cfg80211_del_station_xr(
	dhd_pub_t *src_pub, dhd_pub_t *dest_pub,
        struct wiphy *wiphy,
        struct net_device *ndev,
        const u8* mac_addr)
#else
s32
wl_cfg80211_del_station_xr(
	dhd_pub_t *src_pub, dhd_pub_t *dest_pub,
        struct wiphy *wiphy,
        struct net_device *ndev,
        u8* mac_addr)
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)) */
{

	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_del_station_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_del_station_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL){
		return -EINVAL;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_DEL_STATION;
	cmd->len = sizeof(xr_cmd_del_station_t);
	data = (xr_cmd_del_station_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->ndev = ndev;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
	data->params = params;
#else
	data->mac_addr = mac_addr;
#endif // endif

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->del_station_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.del_station_status;

}

int wl_cfg80211_del_station_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_del_station_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_del_station_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_DEL_STATION;
	cmd->len = sizeof(xr_cmd_reply_del_station_t);
	data = (xr_cmd_reply_del_station_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);
	return ret;
}

int xr_cmd_del_station_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_del_station_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_del_station_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_del_station_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("XR_CMD_DEL_STATION cfg is NULL\n"));
		ret = BCME_ERROR;
	}
	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0))
	status = wl_cfg80211_del_station(cmd->wiphy, cmd->ndev, cmd->params);
#else
	status = wl_cfg80211_del_station(cmd->wiphy, cmd->ndev, cmd->mac_addr);
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)) */

	if (dest_pub) {
		wl_cfg80211_del_station_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_del_station_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_del_station_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_del_station_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.del_station_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.del_station_wait);

	return ret;
}

/*change station*/
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
s32
wl_cfg80211_change_station_xr(
	dhd_pub_t *src_pub, dhd_pub_t *dest_pub,
        struct wiphy *wiphy,
        struct net_device *dev,
        const u8* mac,
	struct station_parameters *params)
#else
s32
wl_cfg80211_change_station_xr(
	dhd_pub_t *src_pub, dhd_pub_t *dest_pub,
        struct wiphy *wiphy,
        struct net_device *dev,
        u8* mac,
	struct station_parameters *params)
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)) */
{

	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_change_station_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_change_station_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL){
		return -EINVAL;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_CHANGE_STATION;
	cmd->len = sizeof(xr_cmd_change_station_t);
	data = (xr_cmd_change_station_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;
	data->params = params;
	data->mac = mac;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->change_station_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.change_station_status;

}

int wl_cfg80211_change_station_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_change_station_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_change_station_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_CHANGE_STATION;
	cmd->len = sizeof(xr_cmd_reply_change_station_t);
	data = (xr_cmd_reply_change_station_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);
	return ret;
}

int xr_cmd_change_station_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_change_station_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_change_station_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_change_station_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("XR_CMD_DEL_STATION cfg is NULL\n"));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = wl_cfg80211_change_station(cmd->wiphy, cmd->dev, cmd->mac, cmd->params);

	if (dest_pub) {
		wl_cfg80211_change_station_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_change_station_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_change_station_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_change_station_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.change_station_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.change_station_wait);

	return ret;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
s32
wl_cfg80211_mgmt_tx_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy, bcm_struct_cfgdev *cfgdev,
        struct cfg80211_mgmt_tx_params *params, u64 *cookie)
#else
s32
wl_cfg80211_mgmt_tx_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy, bcm_struct_cfgdev *cfgdev,
        struct ieee80211_channel *channel, bool offchan,
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 7, 0))
        enum nl80211_channel_type channel_type,
        bool channel_type_valid,
#endif /* LINUX_VERSION_CODE <= KERNEL_VERSION(3, 7, 0) */
        unsigned int wait, const u8* buf, size_t len,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0)) || defined(WL_COMPAT_WIRELESS)
        bool no_cck,
#endif // endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0)) || defined(WL_COMPAT_WIRELESS)
        bool dont_wait_for_ack,
#endif // endif
        u64 *cookie)
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0) */
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_mgmt_tx_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_mgmt_tx_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_MGMT_TX;
	cmd->len = sizeof(xr_cmd_mgmt_tx_t);
	data = (xr_cmd_mgmt_tx_t *) &cmd->data[0];
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
	data->wiphy = wiphy;
	data->cfgdev = cfgdev;
	data->params = params;
	data->cookie = cookie;
#else
	data->wiphy = wiphy;
	data->cfgdev = cfgdev;
        data->channel = channel;
	data->offchan = offchan;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 7, 0))
	data->channel_type = channel_type;
	data->channel_type_valid = channel_type_valid;
#endif /* (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 7, 0)) */
	data->wait = wait;
	data->buf = buf;
	data->len = len;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0)) || defined(WL_COMPAT_WIRELESS)
        data->no_cck = no_cck;
#endif // endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0)) || defined(WL_COMPAT_WIRELESS)
	data->dont_wait_for_ack = dont_wait_for_ack;
#endif // endif
        data->cookie = cookie;
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)) */

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->mgmt_tx_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.mgmt_tx_status;

}

int wl_cfg80211_mgmt_tx_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_mgmt_tx_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_mgmt_tx_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_MGMT_TX;
	cmd->len = sizeof(xr_cmd_reply_mgmt_tx_t);
	data = (xr_cmd_reply_mgmt_tx_t *) &cmd->data[0];

	data->status = status;
	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);

	return ret;
}
int xr_cmd_mgmt_tx_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_mgmt_tx_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_mgmt_tx_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_mgmt_tx_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}
	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
	status = wl_cfg80211_mgmt_tx(cmd->wiphy, cmd->cfgdev, cmd->params, cmd->cookie);
#else
	status = wl_cfg80211_mgmt_tx(cmd->wiphy, cmd->cfgdev, cmd->channel, cmd->offchan,
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 7, 0))
			cmd->channel_type, cmd->channel_type_valid,
#endif /* LINUX_VERSION_CODE <= KERNEL_VERSION(3, 7, 0) */
			cmd->wait, cmd->buf, cmd->len,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0)) || defined(WL_COMPAT_WIRELESS)
			cmd->no_cck,
#endif // endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0)) || defined(WL_COMPAT_WIRELESS)
			cmd->dont_wait_for_ack,
#endif // endif
			cmd->cookie);
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0) */

	if (dest_pub) {
		wl_cfg80211_mgmt_tx_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_mgmt_tx_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_mgmt_tx_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_mgmt_tx_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.mgmt_tx_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.mgmt_tx_wait);

	return ret;
}

#ifdef WL_SAE
int
wl_cfg80211_external_auth_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy, struct net_device *dev,
        struct cfg80211_external_auth_params *params)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_external_auth_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_external_auth_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL){
		return -EINVAL;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_EXTERNAL_AUTH;
	cmd->len = sizeof(xr_cmd_external_auth_t);
	data = (xr_cmd_external_auth_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;
	data->params = params;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->external_auth_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.external_auth_status;

}

int wl_cfg80211_external_auth_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_external_auth_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_external_auth_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_EXTERNAL_AUTH;
	cmd->len = sizeof(xr_cmd_reply_external_auth_t);
	data = (xr_cmd_reply_external_auth_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);
	return ret;
}

int xr_cmd_external_auth_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_external_auth_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_external_auth_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_external_auth_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = wl_cfg80211_external_auth(cmd->wiphy, cmd->dev, cmd->params);

	if (dest_pub) {
		wl_cfg80211_external_auth_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_external_auth_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_external_auth_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_external_auth_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.external_auth_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.external_auth_wait);

	return ret;
}

#endif /* WL_SAE */

/* del_key */
s32 wl_cfg80211_del_key_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy, struct net_device *dev,
        u8 key_idx, bool pairwise, const u8 *mac_addr)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_del_key_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_del_key_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_DEL_KEY;
	cmd->len = sizeof(xr_cmd_del_key_t);
	data = (xr_cmd_del_key_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;
	data->key_idx = key_idx;
	data->pairwise = pairwise;
	data->mac_addr = mac_addr;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->del_key_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.del_key_status;

}

int wl_cfg80211_del_key_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_del_key_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_del_key_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_DEL_KEY;
	cmd->len = sizeof(xr_cmd_reply_del_key_t);
	data = (xr_cmd_reply_del_key_t *) &cmd->data[0];

	data->status = status;
	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);

	return ret;
}

int xr_cmd_del_key_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_del_key_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_del_key_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_del_key_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = WL_CFG80211_DEL_KEY(cmd->wiphy, cmd->dev, cmd->link_id,
			cmd->key_idx, cmd->pairwise, cmd->mac_addr);

	if (dest_pub) {
		wl_cfg80211_del_key_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_del_key_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_del_key_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_del_key_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.del_key_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.del_key_wait);

	return ret;
}
/* get_key */
s32 wl_cfg80211_get_key_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy, struct net_device *dev, u8 key_idx, bool pairwise, const u8 *mac_addr, void *cookie,
        void (*callback) (void *cookie, struct key_params * params))
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_get_key_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_get_key_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_GET_KEY;
	cmd->len = sizeof(xr_cmd_get_key_t);
	data = (xr_cmd_get_key_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;
	data->key_idx = key_idx;
	data->pairwise = pairwise;
	data->mac_addr = mac_addr;
	data->cookie = cookie;
	data->callback = callback;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->get_key_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.get_key_status;

}

int wl_cfg80211_get_key_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_get_key_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_get_key_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_GET_KEY;
	cmd->len = sizeof(xr_cmd_reply_get_key_t);
	data = (xr_cmd_reply_get_key_t *) &cmd->data[0];

	data->status = status;
	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);

	return ret;
}

int xr_cmd_get_key_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_get_key_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_get_key_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_get_key_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = WL_CFG80211_GET_KEY(cmd->wiphy, cmd->dev, cmd->link_id, cmd->key_idx,
			cmd->pairwise, cmd->mac_addr, cmd->cookie, cmd->callback);

	if(dest_pub) {
		wl_cfg80211_get_key_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_get_key_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_get_key_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_get_key_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.get_key_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.get_key_wait);

	return ret;
}
/* del_virtual_iface */
s32
wl_cfg80211_del_virtual_iface_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy, bcm_struct_cfgdev *cfgdev)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_del_virtual_iface_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_del_virtual_iface_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_DEL_VIRTUAL_IFACE;
	cmd->len = sizeof(xr_cmd_del_virtual_iface_t);
	data = (xr_cmd_del_virtual_iface_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->cfgdev = cfgdev;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->del_virtual_iface_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.del_virtual_iface_status;

}

int wl_cfg80211_del_virtual_iface_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_del_virtual_iface_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_del_virtual_iface_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_DEL_VIRTUAL_IFACE;
	cmd->len = sizeof(xr_cmd_reply_del_virtual_iface_t);
	data = (xr_cmd_reply_del_virtual_iface_t *) &cmd->data[0];

	data->status = status;
	dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);

	return ret;
}
int xr_cmd_del_virtual_iface_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_del_virtual_iface_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_del_virtual_iface_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_del_virtual_iface_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = wl_cfg80211_del_virtual_iface(cmd->wiphy, cmd->cfgdev);

	if (dest_pub) {
		wl_cfg80211_del_virtual_iface_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_del_virtual_iface_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_del_virtual_iface_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_del_virtual_iface_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.del_virtual_iface_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.del_virtual_iface_wait);

	return ret;
}

/* get station */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
s32
wl_cfg80211_get_station_xr(
	dhd_pub_t *src_pub, dhd_pub_t *dest_pub,
        struct wiphy *wiphy,
        struct net_device *dev,
        const u8* mac,
	struct station_info *sinfo)
#else
s32
wl_cfg80211_get_station_xr(
	dhd_pub_t *src_pub, dhd_pub_t *dest_pub,
        struct wiphy *wiphy,
        struct net_device *dev,
        u8* mac,
	struct station_info *sinfo)
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)) */
{

	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_get_station_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_get_station_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL){
		return -EINVAL;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_GET_STATION;
	cmd->len = sizeof(xr_cmd_get_station_t);
	data = (xr_cmd_get_station_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;
	data->sinfo = sinfo;
	data->mac = mac;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->get_station_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.get_station_status;

}

int wl_cfg80211_get_station_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_get_station_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_get_station_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_GET_STATION;
	cmd->len = sizeof(xr_cmd_reply_get_station_t);
	data = (xr_cmd_reply_get_station_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);
	return ret;
}

int xr_cmd_get_station_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_get_station_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_get_station_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_get_station_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("XR_CMD_GET_STATION cfg is NULL\n"));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = wl_cfg80211_get_station(cmd->wiphy, cmd->dev, cmd->mac, cmd->sinfo);

	if (dest_pub) {
		wl_cfg80211_get_station_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_get_station_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_get_station_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_get_station_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.get_station_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.get_station_wait);

	return ret;
}

#ifdef WL_6E
s32 wl_xr_stop_fils_6g(struct wiphy *wiphy,
        struct net_device *dev,
        u8 stop_fils)
{
	s32 err = BCME_OK;
	s32 bssidx = 0;
	struct bcm_cfg80211 *cfg = wiphy_priv(wiphy);
	u8 stop_fils_6g = 0;

	if ((bssidx = wl_get_bssidx_by_wdev(cfg, dev->ieee80211_ptr)) < 0) {
                WL_ERR(("Find p2p index from wdev(%p) failed\n", dev->ieee80211_ptr));
                return BCME_ERROR;
        }

	stop_fils_6g = stop_fils;
	/* send IOVAR to firmware */
	err = wldev_iovar_setbuf_bsscfg(dev, "stop_fils_6g", &stop_fils_6g, sizeof(u8),
                        cfg->ioctl_buf, WLC_IOCTL_MAXLEN, bssidx, &cfg->ioctl_buf_sync);

	return err;
}

int wl_xr_stop_fils_6g_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_stop_fils_6g_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_stop_fils_6g_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_STOP_FILS_6G;
	cmd->len = sizeof(xr_cmd_reply_stop_fils_6g_t);
	data = (xr_cmd_reply_stop_fils_6g_t *) &cmd->data[0];

	data->status = status;
	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);

	return ret;
}

int xr_cmd_stop_fils_6g_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_stop_fils_6g_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_stop_fils_6g_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_stop_fils_6g_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}
	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = wl_xr_stop_fils_6g(cmd->wiphy, cmd->dev, cmd->stop_fils_6g_value);

	if(dest_pub) {
		wl_xr_stop_fils_6g_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_stop_fils_6g_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_stop_fils_6g_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_stop_fils_6g_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.stop_fils_6g_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.stop_fils_6g_wait);

	return ret;
}
#endif /* WL_6E */

/* change beacon */
s32
wl_cfg80211_change_beacon_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy * wiphy, struct net_device * dev, struct cfg80211_beacon_data * info)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_change_beacon_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_change_beacon_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_CHANGE_BEACON;
	cmd->len = sizeof(xr_cmd_change_beacon_t);
	data = (xr_cmd_change_beacon_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;
	data->info = info;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->change_beacon_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.change_beacon_status;
}

int wl_cfg80211_change_beacon_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_change_beacon_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_change_beacon_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_CHANGE_BEACON;
	cmd->len = sizeof(xr_cmd_reply_change_beacon_t);
	data = (xr_cmd_reply_change_beacon_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);

	return ret;
}

int xr_cmd_change_beacon_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_change_beacon_t *cmd = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_change_beacon_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd = (xr_cmd_change_beacon_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);
	status = wl_cfg80211_change_beacon(cmd->wiphy, cmd->dev, cmd->info);

	if(dest_pub) {
		wl_cfg80211_change_beacon_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_change_beacon_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_change_beacon_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_change_beacon_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.change_beacon_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.change_beacon_wait);

	return ret;
}

/* channel switch */
int
wl_cfg80211_channel_switch_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy * wiphy, struct net_device * dev, struct cfg80211_csa_settings * params)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_channel_switch_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_channel_switch_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_CHANNEL_SWITCH;
	cmd->len = sizeof(xr_cmd_channel_switch_t);
	data = (xr_cmd_channel_switch_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;
	data->params = params;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->channel_switch_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.channel_switch_status;
}

int wl_cfg80211_channel_switch_xr_reply(dhd_pub_t *dest_pub, int status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_channel_switch_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_channel_switch_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}

	/* Create cmd */
	cmd->cmd_id = XR_CMD_REPLY_CHANNEL_SWITCH;
	cmd->len = sizeof(xr_cmd_reply_channel_switch_t);
	data = (xr_cmd_reply_channel_switch_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);

	return ret;
}

int xr_cmd_channel_switch_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	int status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_channel_switch_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_channel_switch_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd = (xr_cmd_channel_switch_t *) &xr_cmd->data[0];

	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = wl_cfg80211_channel_switch(cmd->wiphy, cmd->dev, cmd->params);

	if(dest_pub) {
		wl_cfg80211_channel_switch_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_channel_switch_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_channel_switch_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_channel_switch_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.channel_switch_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.channel_switch_wait);

	return ret;
}

/* set tx power */
#if defined(WL_CFG80211_P2P_DEV_IF)
s32
wl_cfg80211_set_tx_power_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy, struct wireless_dev *wdev, enum nl80211_tx_power_setting type, s32 mbm)
#else
s32
wl_cfg80211_set_tx_power_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy, enum nl80211_tx_power_setting type, s32 dbm)
#endif /* WL_CFG80211_P2P_DEV_IF */
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_set_tx_power_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_set_tx_power_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_SET_TX_POWER;
	cmd->len = sizeof(xr_cmd_set_tx_power_t);
	data = (xr_cmd_set_tx_power_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->type = type;
#if defined(WL_CFG80211_P2P_DEV_IF)
	data->wdev = wdev;
	data->mbm = mbm;
#else
	data->dbm = dbm;
#endif /* WL_CFG80211_P2P_DEV_IF */

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->set_tx_power_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.set_tx_power_status;
}

int wl_cfg80211_set_tx_power_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_set_tx_power_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_set_tx_power_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_SET_TX_POWER;
	cmd->len = sizeof(xr_cmd_reply_set_tx_power_t);
	data = (xr_cmd_reply_set_tx_power_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);
	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);

	return ret;
}

int xr_cmd_set_tx_power_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_set_tx_power_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_set_tx_power_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd = (xr_cmd_set_tx_power_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

#if defined(WL_CFG80211_P2P_DEV_IF)
	status = wl_cfg80211_set_tx_power(cmd->wiphy, cmd->wdev, cmd->type, cmd->mbm);
#else
	status = wl_cfg80211_set_tx_power(cmd->wiphy, cmd->type, cmd->dbm);
#endif /* WL_CFG80211_P2P_DEV_IF */

	if(dest_pub) {
		wl_cfg80211_set_tx_power_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_set_tx_power_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_set_tx_power_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_set_tx_power_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.set_tx_power_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.set_tx_power_wait);

	return ret;
}

/* set wiphy params */
s32
wl_cfg80211_set_wiphy_params_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy, u32 changed)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_set_wiphy_params_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_set_wiphy_params_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_SET_WIPHY_PARAMS;
	cmd->len = sizeof(xr_cmd_set_wiphy_params_t);
	data = (xr_cmd_set_wiphy_params_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->changed = changed;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->set_wiphy_params_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.set_wiphy_params_status;
}

int wl_cfg80211_set_wiphy_params_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_set_wiphy_params_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_set_wiphy_params_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_SET_WIPHY_PARAMS;
	cmd->len = sizeof(xr_cmd_reply_set_wiphy_params_t);
	data = (xr_cmd_reply_set_wiphy_params_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);
	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);

	return ret;
}

int xr_cmd_set_wiphy_params_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_set_wiphy_params_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_set_wiphy_params_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd = (xr_cmd_set_wiphy_params_t *) &xr_cmd->data[0];

	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = wl_cfg80211_set_wiphy_params(cmd->wiphy, cmd->changed);

	if(dest_pub) {
		wl_cfg80211_set_wiphy_params_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_set_wiphy_params_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_set_wiphy_params_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_set_wiphy_params_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.set_wiphy_params_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.set_wiphy_params_wait);

	return ret;
}

/* set cqm rssi config */
int
wl_cfg80211_set_cqm_rssi_config_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy, struct net_device *dev, s32 rssi_thold, u32 rssi_hyst)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_set_cqm_rssi_config_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_set_cqm_rssi_config_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_SET_CQM_RSSI_CONFIG;
	cmd->len = sizeof(xr_cmd_set_cqm_rssi_config_t);
	data = (xr_cmd_set_cqm_rssi_config_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;
	data->rssi_thold = rssi_thold;
	data->rssi_hyst = rssi_hyst;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->set_cqm_rssi_config_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.set_cqm_rssi_config_status;
}

int wl_cfg80211_set_cqm_rssi_config_xr_reply(dhd_pub_t *dest_pub, int status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_set_cqm_rssi_config_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_set_cqm_rssi_config_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_SET_CQM_RSSI_CONFIG;
	cmd->len = sizeof(xr_cmd_reply_set_cqm_rssi_config_t);
	data = (xr_cmd_reply_set_cqm_rssi_config_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);
	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);

	return ret;
}

int xr_cmd_set_cqm_rssi_config_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	int status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_set_cqm_rssi_config_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_set_cqm_rssi_config_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd = (xr_cmd_set_cqm_rssi_config_t *) &xr_cmd->data[0];

	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = wl_cfg80211_set_cqm_rssi_config(cmd->wiphy, cmd->dev, cmd->rssi_thold, cmd->rssi_hyst);

	if(dest_pub) {
		wl_cfg80211_set_cqm_rssi_config_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_set_cqm_rssi_config_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_set_cqm_rssi_config_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_set_cqm_rssi_config_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.set_cqm_rssi_config_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.set_cqm_rssi_config_wait);

	return ret;
}

#ifdef WL_SUPPORT_ACS
/* dump survey */
int
wl_cfg80211_dump_survey_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy, struct net_device *dev, int idx, struct survey_info *info)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_dump_survey_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_dump_survey_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_DUMP_SURVEY;
	cmd->len = sizeof(xr_cmd_dump_survey_t);
	data = (xr_cmd_dump_survey_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;
	data->idx = idx;
	data->info = info;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->dump_survey_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.dump_survey_status;
}

int wl_cfg80211_dump_survey_xr_reply(dhd_pub_t *dest_pub, int status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_dump_survey_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_dump_survey_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_DUMP_SURVEY;
	cmd->len = sizeof(xr_cmd_reply_dump_survey_t);
	data = (xr_cmd_reply_dump_survey_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);
	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if (cmd)
		kfree(cmd);

	return ret;
}

int xr_cmd_dump_survey_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	int status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_dump_survey_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_dump_survey_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd = (xr_cmd_dump_survey_t *) &xr_cmd->data[0];

	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = wl_cfg80211_dump_survey(cmd->wiphy, cmd->dev, cmd->idx, cmd->info);

	if(dest_pub) {
		wl_cfg80211_dump_survey_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_dump_survey_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_dump_survey_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_dump_survey_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.dump_survey_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.dump_survey_wait);

	return ret;
}
#endif /* WL_SUPPORT_ACS */

#ifdef DHD_BANDSTEER
/* dhd_bandsteer_update_slave_ifaces */
s32 dhd_bandsteer_update_ifaces_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub,
		struct net_device *ndev)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_bstr_update_ifaces_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_bstr_update_ifaces_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_BSTR_UPDATE_IFACES;
	cmd->len = sizeof(xr_cmd_bstr_update_ifaces_t);
	data = (xr_cmd_bstr_update_ifaces_t *)&cmd->data[0];

	data->ndev = ndev;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->bstr_update_ifaces_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}
	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.bstr_update_ifaces_status;

}

int dhd_bstr_update_ifaces_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_bstr_update_ifaces_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_bstr_update_ifaces_t *data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_BSTR_UPDATE_IFACES;
	cmd->len = sizeof(xr_cmd_reply_bstr_update_ifaces_t);
	data = (xr_cmd_reply_bstr_update_ifaces_t *)&cmd->data[0];

	data->status = status;
	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return BCME_ERROR;
	}

	if (cmd)
		kfree(cmd);

	return ret;
}
int xr_cmd_bstr_update_ifaces_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	s32 status = 0;
	int ret = BCME_OK;
	struct net_device *primary_ndev = dhd_linux_get_primary_netdev(pub);
	struct bcm_cfg80211 *cfg = wl_get_cfg(primary_ndev);
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_bstr_update_ifaces_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_bstr_update_ifaces_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_bstr_update_ifaces_t *)&xr_cmd->data[0];

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = dhd_bandsteer_update_slave_ifaces(pub, cmd->ndev);

	if (dest_pub) {
		ret = dhd_bstr_update_ifaces_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_bstr_update_ifaces_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_bstr_update_ifaces_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_bstr_update_ifaces_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.bstr_update_ifaces_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.bstr_update_ifaces_wait);
	return ret;
}
#endif /* DHD_BANDSTEER */

int wl_cfgvendor_cmd_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub,
        struct wiphy *wiphy,
	struct wireless_dev *wdev,
	const void *cfgvendor_cmd_value,
	int len,
	int cfgvendor_id)
{

        xr_cmd_t *cmd = NULL;
        int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_cfgvendor_cmd_t);
        gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
        xr_cmd_cfgvendor_cmd_t *data = NULL;
        xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
        xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
        int ret = BCME_OK;
        cmd = (xr_cmd_t *) kzalloc(size, flags);
        if (cmd == NULL){
                return -EINVAL;
        }
        /*Create cmd*/
        cmd->cmd_id = XR_CMD_CFGVENDOR_CMD;
        cmd->len = sizeof(xr_cmd_cfgvendor_cmd_t);
        data = (xr_cmd_cfgvendor_cmd_t *) &cmd->data[0];

        data->wiphy = wiphy;
        data->wdev = wdev;
        data->cfgvendor_cmd_value = cfgvendor_cmd_value;
        data->len = len;
	data->cfgvendor_id = cfgvendor_id;

        ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->cfgvendor_cmd_wait, TRUE);

        if (ret != BCME_OK) {
                DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
                return -EINVAL;
        }

        if (cmd)
                kfree(cmd);

        return xr_ctx->xr_cmd_reply_status.cfgvendor_cmd_status;

}

int wl_cfgvendor_cmd_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
        xr_cmd_t *cmd = NULL;
        int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_cfgvendor_cmd_t);
        gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
        xr_cmd_reply_cfgvendor_cmd_t * data = NULL;
        int ret = BCME_OK;
        cmd = (xr_cmd_t *) kzalloc(size, flags);
        if (cmd == NULL) {
                DHD_ERROR(("cmd is NULL\n"));
                return BCME_ERROR;
        }
        /*Create cmd*/
        cmd->cmd_id = XR_CMD_REPLY_CFGVENDOR_CMD;
        cmd->len = sizeof(xr_cmd_reply_cfgvendor_cmd_t);
        data = (xr_cmd_reply_cfgvendor_cmd_t *) &cmd->data[0];

        data->status = status;

        ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

        if (ret != BCME_OK) {
                DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
                return ret;
        }

        if (cmd)
                kfree(cmd);
        return ret;
}

int cfgvendor_cmd_hndlr(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void *data, int len, int vendor_id)
{
	int ret = BCME_OK;

	switch (vendor_id) {
		case BRCM_VENDOR_SCMD_FRAMEBURST:
			{
				ret = wl_cfgvendor_priv_frameburst(wiphy, wdev, data, len);
				break;
			}
		case BRCM_VENDOR_SCMD_MPC:
			{
				ret = wl_cfgvendor_priv_mpc(wiphy, wdev, data, len);
				break;
			}
		case BRCM_VENDOR_SCMD_BAND:
			{
				ret = wl_cfgvendor_priv_band(wiphy, wdev, data, len);
				break;
			}
		case BRCM_VENDOR_SCMD_PRIV_STR:
			{
				ret = wl_cfgvendor_priv_string_handler(wiphy, wdev, data, len);
				break;
			}
#ifdef BCM_PRIV_CMD_SUPPORTh
		case BRCM_VENDOR_SCMD_BCM_STR:
			{
				ret = wl_cfgvendor_priv_bcm_handler(wiphy, wdev, data, len);
				break;
			}
#endif /* BCM_PRIV_CMD_SUPPORT */
#ifdef WL_SAE
		case BRCM_VENDOR_SCMD_BCM_PSK:
			{
				ret = wl_cfgvendor_set_sae_password(wiphy, wdev, data, len);
				break;
			}
#endif /* WL_SAE */
#ifdef P2P_RAND
		case BRCM_VENDOR_SCMD_SET_MAC:
			{
				ret = wl_cfgvendor_set_p2p_rand_mac(wiphy, wdev, data, len);
				break;
			}
#endif /* P2P_RAND */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0))
		case BRCM_VENDOR_SCMD_SET_PMK:
			{
				ret = wl_cfgvendor_set_pmk(wiphy, wdev, data, len);
				break;
			}
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0) */
		case BRCM_VENDOR_SCMD_GET_FEATURES:
			{
				ret = wl_cfgvendor_get_driver_feature(wiphy, wdev, data, len);
				break;
			}
#ifdef WL_SUPPORT_ACS_OFFLOAD
		case IFX_VENDOR_SCMD_ACS:
			{
				ret = wl_cfgvendor_acs_offload(wiphy, wdev, data, len);
				break;
			}
#endif /* WL_SUPPORT_ACS_OFFLOAD */
#ifdef WL11AX
		case IFX_VENDOR_SCMD_MUEDCA_OPT_ENABLE:
			{
				ret = wl_cfgvendor_priv_muedca_opt_enable(wiphy, wdev, data, len);
				break;
			}
#endif /* WL11AX */
		case IFX_VENDOR_SCMD_LDPC_CAP:
			{
				ret = wl_cfgvendor_priv_ldpc_cap(wiphy, wdev, data, len);
				break;
			}
		case IFX_VENDOR_SCMD_AMSDU:
			{
				ret = wl_cfgvendor_priv_amsdu(wiphy, wdev, data, len);
				break;
			}
#ifdef WL11AX
		case IFX_VENDOR_SCMD_TWT:
			{
				ret = wl_cfgvendor_twt(wiphy, wdev, data, len);
				break;
			}
#endif /* WL11AX */
		case IFX_VENDOR_SCMD_OCE_ENABLE:
			{
				ret = wl_cfgvendor_oce_enable(wiphy, wdev, data, len);
				break;
			}
		case IFX_VENDOR_SCMD_RANDMAC:
			{
				ret = wl_cfgvendor_randmac(wiphy, wdev, data, len);
				break;

			}
		case ANDR_WIFI_SET_COUNTRY:
			{
                                ret = wl_cfgvendor_set_country(wiphy, wdev, data, len);
                                break;
			}
		default:
			DHD_ERROR(("%s:vendor id (%d) is not found\n", __func__, vendor_id));
			ret = BCME_ERROR;
	}
	return ret;
}

int xr_cmd_cfgvendor_cmd_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_cfgvendor_cmd_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);
	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_cfgvendor_cmd_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_cfgvendor_cmd_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);
	if (!cfg) {
		DHD_ERROR(("XR_CMD_CFGVENDOR_CMD cfg is NULL\n"));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = cfgvendor_cmd_hndlr(cmd->wiphy, cmd->wdev, cmd->cfgvendor_cmd_value, cmd->len, cmd->cfgvendor_id);

	if (dest_pub) {
		wl_cfgvendor_cmd_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_cfgvendor_cmd_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_cfgvendor_cmd_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);
	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_cfgvendor_cmd_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.cfgvendor_cmd_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.cfgvendor_cmd_wait);

	return ret;
}

/* connect */
int wl_cfg80211_connect_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy, struct net_device *dev,
	struct cfg80211_connect_params *sme)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_connect_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_connect_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_CONNECT;
	cmd->len = sizeof(xr_cmd_connect_t);
	data = (xr_cmd_connect_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;
	data->sme = sme;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->connect_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.connect_status;

}

int wl_cfg80211_connect_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_connect_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_connect_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_CONNECT;
	cmd->len = sizeof(xr_cmd_reply_connect_t);
	data = (xr_cmd_reply_connect_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if(cmd)
		kfree(cmd);

	return ret;
}

int xr_cmd_connect_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_connect_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_connect_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_connect_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);

	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = wl_cfg80211_connect(cmd->wiphy, cmd->dev, cmd->sme);

	DHD_ERROR(("status:%d\n", status));
	if (dest_pub) {
		wl_cfg80211_connect_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_connect_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_connect_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_connect_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.connect_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.connect_wait);

	return ret;
}

/* disconnect */
s32
wl_cfg80211_disconnect_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy, struct net_device *dev,
	u16 reason_code)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_disconnect_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_disconnect_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_DISCONNECT;
	cmd->len = sizeof(xr_cmd_disconnect_t);
	data = (xr_cmd_disconnect_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;
	data->reason_code = reason_code;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->disconnect_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.disconnect_status;

}

int wl_cfg80211_disconnect_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_disconnect_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_disconnect_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_DISCONNECT;
	cmd->len = sizeof(xr_cmd_reply_disconnect_t);
	data = (xr_cmd_reply_disconnect_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if(cmd)
		kfree(cmd);

	return ret;
}

int xr_cmd_disconnect_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_disconnect_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_disconnect_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_disconnect_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);

	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = wl_cfg80211_disconnect(cmd->wiphy, cmd->dev, cmd->reason_code);

	if (dest_pub) {
		wl_cfg80211_disconnect_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_disconnect_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_disconnect_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_disconnect_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.disconnect_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.disconnect_wait);

	return ret;
}

#ifdef GTK_OFFLOAD_SUPPORT
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 1, 0))
/* rekey_data */
s32
wl_cfg80211_set_rekey_data_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub, struct wiphy *wiphy, struct net_device *dev,
		struct cfg80211_gtk_rekey_data *data)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_rekey_data_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_rekey_data_t *xr_data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REKEY_DATA;
	cmd->len = sizeof(xr_cmd_rekey_data_t);
	xr_data = (xr_cmd_rekey_data_t *) &cmd->data[0];

	xr_data->wiphy = wiphy;
	xr_data->dev = dev;
	xr_data->data = data;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->rekey_data_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.rekey_data_status;

}

int wl_cfg80211_rekey_data_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_rekey_data_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_rekey_data_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_REKEY_DATA;
	cmd->len = sizeof(xr_cmd_reply_rekey_data_t);
	data = (xr_cmd_reply_rekey_data_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if(cmd)
		kfree(cmd);

	return ret;
}

int xr_cmd_rekey_data_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_rekey_data_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_rekey_data_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_rekey_data_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);

	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = wl_cfg80211_set_rekey_data(cmd->wiphy, cmd->dev, cmd->data);

	if (dest_pub) {
		wl_cfg80211_rekey_data_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_rekey_data_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_rekey_data_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_rekey_data_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.rekey_data_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.rekey_data_wait);

	return ret;
}

#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3, 1, 0) */
#endif /* GTK_OFFLOAD_SUPPORT */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0))
/* set_pmk */
s32
wl_cfg80211_set_pmk_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub,
	struct wiphy *wiphy, struct net_device *dev,
	const struct cfg80211_pmk_conf *conf)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_set_pmk_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_set_pmk_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_SET_PMK;
	cmd->len = sizeof(xr_cmd_set_pmk_t);
	data = (xr_cmd_set_pmk_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;
	data->conf = conf;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->set_pmk_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.set_pmk_status;

}

int wl_cfg80211_set_pmk_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_set_pmk_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_set_pmk_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_SET_PMK;
	cmd->len = sizeof(xr_cmd_reply_set_pmk_t);
	data = (xr_cmd_reply_set_pmk_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if(cmd)
		kfree(cmd);

	return ret;
}

int xr_cmd_set_pmk_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_set_pmk_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_set_pmk_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_set_pmk_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);

	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = wl_cfg80211_set_pmk(cmd->wiphy, cmd->dev, cmd->conf);

	if (dest_pub) {
		wl_cfg80211_set_pmk_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_set_pmk_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_set_pmk_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_set_pmk_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.set_pmk_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.set_pmk_wait);

	return ret;
}

/* del_pmk */
s32
wl_cfg80211_del_pmk_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub,
	struct wiphy *wiphy, struct net_device *dev, const u8 *aa)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_del_pmk_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_del_pmk_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_DEL_PMK;
	cmd->len = sizeof(xr_cmd_del_pmk_t);
	data = (xr_cmd_del_pmk_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;
	data->aa = aa;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->del_pmk_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.del_pmk_status;

}

int wl_cfg80211_del_pmk_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_del_pmk_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_del_pmk_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_DEL_PMK;
	cmd->len = sizeof(xr_cmd_reply_del_pmk_t);
	data = (xr_cmd_reply_del_pmk_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if(cmd)
		kfree(cmd);

	return ret;
}

int xr_cmd_del_pmk_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_del_pmk_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_del_pmk_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_del_pmk_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);

	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = wl_cfg80211_del_pmk(cmd->wiphy, cmd->dev, cmd->aa);

	if (dest_pub) {
		wl_cfg80211_del_pmk_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_del_pmk_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_del_pmk_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_del_pmk_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.del_pmk_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.del_pmk_wait);

	return ret;
}

#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0) */
/* set_pmksa */
s32
wl_cfg80211_set_pmksa_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub,
	struct wiphy *wiphy, struct net_device *dev, struct cfg80211_pmksa *pmksa)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_set_pmksa_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_set_pmksa_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_SET_PMKSA;
	cmd->len = sizeof(xr_cmd_set_pmksa_t);
	data = (xr_cmd_set_pmksa_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;
	data->pmksa = pmksa;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->set_pmksa_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.set_pmksa_status;

}

int wl_cfg80211_set_pmksa_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_set_pmksa_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_set_pmksa_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_SET_PMKSA;
	cmd->len = sizeof(xr_cmd_reply_set_pmksa_t);
	data = (xr_cmd_reply_set_pmksa_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if(cmd)
		kfree(cmd);

	return ret;
}

int xr_cmd_set_pmksa_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_set_pmksa_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_set_pmksa_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_set_pmksa_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);

	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = wl_cfg80211_set_pmksa(cmd->wiphy, cmd->dev, cmd->pmksa);

	if (dest_pub) {
		wl_cfg80211_set_pmksa_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_set_pmksa_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_set_pmksa_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_set_pmksa_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.set_pmksa_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.set_pmksa_wait);

	return ret;
}

/* del_pmksa */
s32
wl_cfg80211_del_pmksa_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub,
	struct wiphy *wiphy, struct net_device *dev, struct cfg80211_pmksa *pmksa)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_del_pmksa_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_del_pmksa_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_DEL_PMKSA;
	cmd->len = sizeof(xr_cmd_del_pmksa_t);
	data = (xr_cmd_del_pmksa_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;
	data->pmksa = pmksa;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->del_pmksa_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.del_pmksa_status;

}

int wl_cfg80211_del_pmksa_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_del_pmksa_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_del_pmksa_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_DEL_PMKSA;
	cmd->len = sizeof(xr_cmd_reply_del_pmksa_t);
	data = (xr_cmd_reply_del_pmksa_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if(cmd)
		kfree(cmd);

	return ret;
}

int xr_cmd_del_pmksa_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_del_pmksa_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_del_pmksa_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_del_pmksa_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);

	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = wl_cfg80211_del_pmksa(cmd->wiphy, cmd->dev, cmd->pmksa);

	if (dest_pub) {
		wl_cfg80211_del_pmksa_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_del_pmksa_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_del_pmksa_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_del_pmksa_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.del_pmksa_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.del_pmksa_wait);

	return ret;
}

#if defined(WL_FILS) || defined(WL_OWE)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
/* update_connect_params */
s32
wl_cfg80211_update_connect_params_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub,
	struct wiphy *wiphy, struct net_device *dev, struct cfg80211_connect_params *sme, u32 changed)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_update_connect_params_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_update_connect_params_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_UPDATE_CONNECT_PARAMS;
	cmd->len = sizeof(xr_cmd_update_connect_params_t);
	data = (xr_cmd_update_connect_params_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;
	data->sme = sme;
	data->changed = changed;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->update_connect_params_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.update_connect_params_status;

}

int wl_cfg80211_update_connect_params_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_update_connect_params_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_update_connect_params_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_UPDATE_CONNECT_PARAMS;
	cmd->len = sizeof(xr_cmd_reply_update_connect_params_t);
	data = (xr_cmd_reply_update_connect_params_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if(cmd)
		kfree(cmd);

	return ret;
}

int xr_cmd_update_connect_params_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_update_connect_params_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_update_connect_params_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_update_connect_params_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);

	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = wl_cfg80211_update_connect_params(cmd->wiphy, cmd->dev, cmd->sme, cmd->changed);

	if (dest_pub) {
		wl_cfg80211_update_connect_params_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_update_connect_params_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_update_connect_params_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_update_connect_params_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.update_connect_params_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.update_connect_params_wait);

	return ret;
}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0) */
#endif /* defined(WL_FILS) || defined(WL_OWE) */
#if defined(WL_OWE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0))
/* update_owe_info */
s32
wl_cfg80211_update_owe_info_xr(dhd_pub_t *src_pub, dhd_pub_t *dest_pub,
	struct wiphy *wiphy, struct net_device *dev, struct cfg80211_update_owe_info *owe_info)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_update_owe_info_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_update_owe_info_t *data = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(src_pub);
	xr_comp_wait_t *cmd_wait = &xr_ctx->xr_cmd_wait;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);

	if (cmd == NULL) {
		return -EINVAL;
	}

	/*Create cmd*/
	cmd->cmd_id = XR_CMD_UPDATE_CONNECT_PARAMS;
	cmd->len = sizeof(xr_cmd_update_owe_info_t);
	data = (xr_cmd_update_owe_info_t *) &cmd->data[0];

	data->wiphy = wiphy;
	data->dev = dev;
	data->owe_info = owe_info;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, &cmd_wait->update_owe_info_wait, TRUE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return -EINVAL;
	}

	if (cmd)
		kfree(cmd);

	return xr_ctx->xr_cmd_reply_status.update_owe_info_status;

}

int wl_cfg80211_update_owe_info_xr_reply(dhd_pub_t *dest_pub, s32 status)
{
	xr_cmd_t *cmd = NULL;
	int size = sizeof(xr_cmd_t) + sizeof(xr_cmd_reply_update_owe_info_t);
	gfp_t flags = (in_atomic()) ? GFP_ATOMIC : GFP_KERNEL;
	xr_cmd_reply_update_owe_info_t * data = NULL;
	int ret = BCME_OK;

	cmd = (xr_cmd_t *) kzalloc(size, flags);
	if (cmd == NULL) {
		DHD_ERROR(("cmd is NULL\n"));
		return BCME_ERROR;
	}
	/*Create cmd*/
	cmd->cmd_id = XR_CMD_REPLY_UPDATE_CONNECT_PARAMS;
	cmd->len = sizeof(xr_cmd_reply_update_owe_info_t);
	data = (xr_cmd_reply_update_owe_info_t *) &cmd->data[0];

	data->status = status;

	ret = dhd_send_xr_cmd(dest_pub, cmd, size, NULL, FALSE);

	if (ret != BCME_OK) {
		DHD_ERROR(("%s: dhd_send_xr_cmd fail\n", __func__));
		return ret;
	}

	if(cmd)
		kfree(cmd);

	return ret;
}

int xr_cmd_update_owe_info_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	s32 status = 0;
	struct bcm_cfg80211 *cfg = NULL;
	dhd_pub_t *dest_pub = NULL;
	xr_cmd_update_owe_info_t *cmd  = NULL;
	uint8 xr_role = DHD_GET_XR_ROLE(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	if (xr_cmd->len != sizeof(xr_cmd_update_owe_info_t)) {
		DHD_ERROR(("%s: cmd len error\n", __func__));
		ret = BCME_ERROR;
	}

	cmd  = (xr_cmd_update_owe_info_t *) &xr_cmd->data[0];
	cfg = (struct bcm_cfg80211 *)wiphy_priv(cmd->wiphy);

	if (!cfg) {
		DHD_ERROR(("%s cfg is NULL\n", __func__));
		ret = BCME_ERROR;
	}

	dest_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);

	status = wl_cfg80211_update_owe_info(cmd->wiphy, cmd->dev, cmd->owe_info);

	if (dest_pub) {
		wl_cfg80211_update_owe_info_xr_reply(dest_pub, status);
	}

	return ret;
}

int xr_cmd_reply_update_owe_info_hndlr(dhd_pub_t *pub, xr_cmd_t *xr_cmd)
{
	int ret = BCME_OK;
	xr_cmd_reply_update_owe_info_t *reply = NULL;
	xr_ctx_t *xr_ctx = (xr_ctx_t *) DHD_GET_XR_CTX(pub);

	if (!xr_cmd) {
		DHD_ERROR(("%s: xr_cmd null\n", __func__));
		ret = BCME_ERROR;
	}

	reply = (xr_cmd_reply_update_owe_info_t *) &xr_cmd->data[0];
	xr_ctx->xr_cmd_reply_status.update_owe_info_status = reply->status;
	complete(&xr_ctx->xr_cmd_wait.update_owe_info_wait);

	return ret;
}
#endif /* WL_OWE && LINUX_VERSION_CODE > KERNEL_VERSION(5, 2, 0) */
/* xr_cmd_handler */
int xr_cmd_deferred_handler(dhd_pub_t *pub, xr_buf_t *xr_buf)
{
	xr_cmd_t *xr_cmd = NULL;
	int ret = BCME_OK;

	if (!xr_buf) {
		DHD_ERROR(("xr_buf is NULL\n"));
		return BCME_ERROR;
	}

	xr_cmd = (xr_cmd_t *) &xr_buf->buf[0];

	switch (xr_cmd->cmd_id) {
	case XR_CMD_ADD_IF:
		{
			ret = xr_cmd_add_if_hndlr(pub, xr_cmd);
			break;
		}
	case XR_CMD_DEL_IF:
		{
			ret = xr_cmd_del_if_hndlr(pub, xr_cmd);
			break;
		}
#ifdef DHD_BANDSTEER
	case XR_CMD_BSTR_UPDATE_IFACES:
		{
			ret = xr_cmd_bstr_update_ifaces_hndlr(pub, xr_cmd);
			break;
		}
#endif /* DHD_BANDSTEER */
	case XR_CMD_DEL_VIRTUAL_IFACE:
		{
			xr_cmd_del_virtual_iface_hndlr(pub, xr_cmd);
			break;
		}
	default:
		DHD_ERROR(("%s:cmd id is not found\n", __func__));
		ret = BCME_ERROR;
	};

	return ret;
}

/* xr_cmd_handler */
int xr_cmd_handler(dhd_pub_t *pub, xr_buf_t *xr_buf)
{
	xr_cmd_t *xr_cmd = NULL;
	int ret = BCME_OK;

	if (!xr_buf) {
		DHD_ERROR(("xr_buf is NULL\n"));
		ret = BCME_ERROR;
		goto fail;
	}

	xr_cmd = (xr_cmd_t *) &xr_buf->buf[0];

	switch (xr_cmd->cmd_id) {
	case XR_CMD_ADD_IF:
	{
		dhd_wq_xr_cmd_handler(pub, xr_buf);
		return ret;
	}

	case XR_CMD_REPLY_ADD_IF:
	{
		ret = xr_cmd_reply_add_if_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_DEL_IF:
	{
		dhd_wq_xr_cmd_handler(pub, xr_buf);
		return ret;
	}

	case XR_CMD_REPLY_DEL_IF:
	{
		ret = xr_cmd_reply_del_if_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_SCAN:
	{
		ret = xr_cmd_scan_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_GET_TX_POWER:
	{
		ret = xr_cmd_get_tx_power_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_GET_TX_POWER:
	{
		ret = xr_cmd_reply_get_tx_power_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_SET_POWER_MGMT:
	{
		ret = xr_cmd_set_power_mgmt_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_SET_POWER_MGMT:
	{
		ret = xr_cmd_reply_set_power_mgmt_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_FLUSH_PMKSA:
	{
		ret = xr_cmd_flush_pmksa_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_FLUSH_PMKSA:
	{
		ret = xr_cmd_reply_flush_pmksa_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_CHANGE_VIRUTAL_IFACE:
	{
		ret = xr_cmd_change_virtual_iface_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_CHANGE_VIRUTAL_IFACE:
	{
		ret = xr_cmd_reply_change_virtual_iface_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_START_AP:
	{
		ret = xr_cmd_start_ap_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_START_AP:
	{
		ret = xr_cmd_reply_start_ap_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_SET_MAC_ACL:
	{
		ret = xr_cmd_set_mac_acl_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_SET_MAC_ACL:
	{
		ret = xr_cmd_reply_set_mac_acl_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_CHANGE_BSS:
	{
		ret = xr_cmd_change_bss_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_CHANGE_BSS:
	{
		ret = xr_cmd_reply_change_bss_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_ADD_KEY:
	{
		ret = xr_cmd_add_key_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_ADD_KEY:
	{
		ret = xr_cmd_reply_add_key_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_SET_CHANNEL:
	{
		ret = xr_cmd_set_channel_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_SET_CHANNEL:
	{
		ret = xr_cmd_reply_set_channel_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_CONFIG_DEFAULT_KEY:
	{
		ret = xr_cmd_config_default_key_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_CONFIG_DEFAULT_KEY:
	{
		ret = xr_cmd_reply_config_default_key_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_STOP_AP:
	{
		ret = xr_cmd_stop_ap_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_STOP_AP:
	{
		ret = xr_cmd_reply_stop_ap_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_DEL_STATION:
	{
		ret = xr_cmd_del_station_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_DEL_STATION:
	{
		ret = xr_cmd_reply_del_station_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_CHANGE_STATION:
	{
		ret = xr_cmd_change_station_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_CHANGE_STATION:
	{
		ret = xr_cmd_reply_change_station_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_MGMT_TX:
	{
		ret = xr_cmd_mgmt_tx_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_MGMT_TX:
	{
		ret = xr_cmd_reply_mgmt_tx_hndlr(pub, xr_cmd);
		break;
	}
#ifdef WL_SAE
	case XR_CMD_EXTERNAL_AUTH:
	{
		ret = xr_cmd_external_auth_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_EXTERNAL_AUTH:
	{
		ret = xr_cmd_reply_external_auth_hndlr(pub, xr_cmd);
		break;
	}
#endif /* WL_SAE */
	case XR_CMD_DEL_KEY:
	{
		ret = xr_cmd_del_key_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_DEL_KEY:
	{
		ret = xr_cmd_reply_del_key_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_GET_KEY:
	{
		ret = xr_cmd_get_key_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_GET_KEY:
	{
		ret = xr_cmd_reply_get_key_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_DEL_VIRTUAL_IFACE:
	{
		dhd_wq_xr_cmd_handler(pub, xr_buf);
		return ret;
	}
	case XR_CMD_REPLY_DEL_VIRTUAL_IFACE:
	{
		ret = xr_cmd_reply_del_virtual_iface_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_GET_STATION:
	{
		ret = xr_cmd_get_station_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_GET_STATION:
	{
		ret = xr_cmd_reply_get_station_hndlr(pub, xr_cmd);
		break;
	}
#ifdef DHD_BANDSTEER
	case XR_CMD_BSTR_UPDATE_IFACES:
	{
		dhd_wq_xr_cmd_handler(pub, xr_buf);
		return ret;
	}

	case XR_CMD_REPLY_BSTR_UPDATE_IFACES:
	{
		ret = xr_cmd_reply_bstr_update_ifaces_hndlr(pub, xr_cmd);
		break;
	}
#endif /* DHD_BANDSTEER */
#ifdef WL_6E
	case XR_CMD_STOP_FILS_6G:
	{
		ret = xr_cmd_stop_fils_6g_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_STOP_FILS_6G:
	{
		ret = xr_cmd_reply_stop_fils_6g_hndlr(pub, xr_cmd);
		break;
	}
#endif /* WL_6E */
	case XR_CMD_CHANGE_BEACON:
	{
		ret = xr_cmd_change_beacon_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_CHANGE_BEACON:
	{
		ret = xr_cmd_reply_change_beacon_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_CHANNEL_SWITCH:
	{
		ret = xr_cmd_channel_switch_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_CHANNEL_SWITCH:
	{
		ret = xr_cmd_reply_channel_switch_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_SET_TX_POWER:
	{
		ret = xr_cmd_set_tx_power_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_SET_TX_POWER:
	{
		ret = xr_cmd_reply_set_tx_power_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_SET_WIPHY_PARAMS:
	{
		ret = xr_cmd_set_wiphy_params_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_SET_WIPHY_PARAMS:
	{
		ret = xr_cmd_reply_set_wiphy_params_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_SET_CQM_RSSI_CONFIG:
	{
		ret = xr_cmd_set_cqm_rssi_config_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_SET_CQM_RSSI_CONFIG:
	{
		ret = xr_cmd_reply_set_cqm_rssi_config_hndlr(pub, xr_cmd);
		break;
	}
#ifdef WL_SUPPORT_ACS
	case XR_CMD_DUMP_SURVEY:
	{
		ret = xr_cmd_dump_survey_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_DUMP_SURVEY:
	{
		ret = xr_cmd_reply_dump_survey_hndlr(pub, xr_cmd);
		break;
	}
#endif /* WL_SUPPORT_ACS */
	case XR_CMD_CFGVENDOR_CMD:
	{
		ret = xr_cmd_cfgvendor_cmd_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_CFGVENDOR_CMD:
	{
		ret = xr_cmd_reply_cfgvendor_cmd_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_CONNECT:
	{
		ret = xr_cmd_connect_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_CONNECT:
	{
		ret = xr_cmd_reply_connect_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_DISCONNECT:
	{
		ret = xr_cmd_disconnect_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_DISCONNECT:
	{
		ret = xr_cmd_reply_disconnect_hndlr(pub, xr_cmd);
		break;
	}
#ifdef GTK_OFFLOAD_SUPPORT
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 1, 0))
	case XR_CMD_REKEY_DATA:
	{
		ret = xr_cmd_rekey_data_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_REKEY_DATA:
	{
		ret = xr_cmd_reply_rekey_data_hndlr(pub, xr_cmd);
		break;
	}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3, 1, 0) */
#endif /* GTK_OFFLOAD_SUPPORT */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0))
	case XR_CMD_SET_PMK:
	{
		ret = xr_cmd_set_pmk_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_SET_PMK:
	{
		ret = xr_cmd_reply_set_pmk_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_DEL_PMK:
	{
		ret = xr_cmd_del_pmk_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_DEL_PMK:
	{
		ret = xr_cmd_reply_del_pmk_hndlr(pub, xr_cmd);
		break;
	}
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)) */
	case XR_CMD_SET_PMKSA:
	{
		ret = xr_cmd_set_pmksa_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_SET_PMKSA:
	{
		ret = xr_cmd_reply_set_pmksa_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_DEL_PMKSA:
	{
		ret = xr_cmd_del_pmksa_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_DEL_PMKSA:
	{
		ret = xr_cmd_reply_del_pmksa_hndlr(pub, xr_cmd);
		break;
	}
#if defined(WL_FILS) || defined(WL_OWE)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
	case XR_CMD_UPDATE_CONNECT_PARAMS:
	{
		ret = xr_cmd_update_connect_params_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_UPDATE_CONNECT_PARAMS:
	{
		ret = xr_cmd_reply_update_connect_params_hndlr(pub, xr_cmd);
		break;
	}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0) */
#endif /* defined(WL_FILS) || defined(WL_OWE) */
#if defined(WL_OWE) && (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0))
	case XR_CMD_UPDATE_OWE_INFO:
	{
		ret = xr_cmd_update_owe_info_hndlr(pub, xr_cmd);
		break;
	}
	case XR_CMD_REPLY_UPDATE_OWE_INFO:
	{
		ret = xr_cmd_reply_update_owe_info_hndlr(pub, xr_cmd);
		break;
	}
#endif /* WL_OWE && LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0) */

	default:
		DHD_ERROR(("%s:cmd id (%d) is not found\n", __func__, xr_cmd->cmd_id));
		ret = BCME_ERROR;
	};

fail:
	if (xr_buf)
		kfree(xr_buf);

	return ret;
}

int
dhd_change_bond_active_slave(struct net_device *ndev)
{
	int ret = BCME_OK;
	struct bcm_cfg80211 *cfg = wl_get_cfg(ndev);
#if !(defined(CONFIG_ANDROID_VERSION) && (CONFIG_ANDROID_VERSION >= 13))
	char cmd_str[64] = {0};
	char *cmd[] = { "/bin/sh", "-c", cmd_str, NULL };
#endif /* !CONFIG_ANDROID_VERSION  */

	if (!cfg->xr_sta_bond_ndev){
		DHD_ERROR(("%s : no bond device\n", __FUNCTION__));
		return BCME_ERROR;
	}
#if defined(CONFIG_ANDROID_VERSION) && (CONFIG_ANDROID_VERSION >= 13)
//For android send IFX_VENDOR_EVENT_XR_CONNECTED, HAL will update the active slave through RTM_SETLINK command
	wl_cfgvendor_send_async_event(cfg->wdev->wiphy, ndev, IFX_VENDOR_EVENT_XR_CONNECTED, ndev->name, sizeof(ndev->name));
#else /* CONFIG_ANDROID_VERSION && CONFIG_ANDROID_VERSION >= 13 */
	if (cfg->xr_sta_bond_ndev && netif_is_bond_master(cfg->xr_sta_bond_ndev)) {
		DHD_ERROR(("%s : change active slave interface to:%s\n", __FUNCTION__, ndev->name));
		snprintf(cmd_str,
			sizeof(cmd_str), "echo %s > /sys/class/net/%s/bonding/active_slave",
				ndev->name, cfg->xr_sta_bond_ndev->name);

		ret = call_usermodehelper(cmd[0], cmd, NULL, UMH_WAIT_EXEC);
		if (ret > 0) {
			DHD_ERROR(("DHD: %s - %s  ret = %d\n",
				__FUNCTION__, cmd_str, ret));
			ret = BCME_ERROR;
		}
	}

#endif // endif
	return ret;
}

/* XR Sta SM disconnect event handler */
void xr_sta_sm_disconnect_hdlr(struct bcm_cfg80211 *cfg)
{

	switch (cfg->xr_sta_state) {
	case XR_STA_DISCONNECTED:
	{
		/* Do nothing */
		break;
	}
	case XR_STA_CONNECTING:
	case XR_STA_CONNECTED:
	{
	/*
	 * Update state to disconnected.
	 * Change  cfg->xr_sta_active_wdev to cfg->wdev.
	 * change bonding active slave interface to to cfg->xr_sta_active_wdev.
	*/
		cfg->xr_sta_active_wdev = cfg->wdev;
		dhd_change_bond_active_slave(cfg->xr_sta_active_wdev->netdev);
		XR_STA_SET_STATE(cfg, XR_STA_DISCONNECTED);
		WL_ERR(("Update state from state to XR_STA_DISCONNECTED\n"));
		break;
	}
	default:
		break;

	};
}

/* XR Sta SM connect event handler */
void xr_sta_sm_connect_hdlr(struct bcm_cfg80211 *cfg)
{

	switch (cfg->xr_sta_state) {
	case XR_STA_DISCONNECTED:
	{
		/*Update cfg->xr_sta_active_wdev based on
 		* band of the channel present in the connect request.*/
		WL_ERR(("Update xr_sta_active_wdev to %s,set XR_STA_CONNECTING\n",
			cfg->xr_sta_active_wdev->netdev->name));
		cfg->xr_sta_active_wdev = cfg->xr_sta_conn_evt_wdev;
		XR_STA_SET_STATE(cfg, XR_STA_CONNECTING);
		break;
	}
	case XR_STA_CONNECTING:
	{
		/* Do nothing */
		break;
	}
	case XR_STA_CONNECTED:
	{
		/* Do nothing */
		break;
	}
	default:
		break;
	};
}

/* XR Sta SM connect done event handler */
void xr_sta_sm_connect_done_hdlr(struct bcm_cfg80211 *cfg)
{

	switch (cfg->xr_sta_state) {
	case XR_STA_DISCONNECTED:
	{
		/* Do nothing */
		break;
	}
	case XR_STA_CONNECTING:
	{
		/*Change active slave interface to cfg->xr_sta_active_wdev*/
		WL_ERR(("Update state to XR_STA_CONNECTED\n"));
		dhd_change_bond_active_slave(cfg->xr_sta_active_wdev->netdev);
		XR_STA_SET_STATE(cfg, XR_STA_CONNECTED);
		break;
	}
	case XR_STA_CONNECTED:
	{
		/* Do nothing */
		break;
	}
	default:
		break;

	};
}

/* SM event Handler for XR Single STA*/
void xr_sta_sm_evt_handler(struct bcm_cfg80211 *cfg, int event)
{

	WL_DBG(("Trigger Event:%d, State:%d,curr act ndev:%s\n",
		event, cfg->xr_sta_state, cfg->xr_sta_active_wdev->netdev->name));
	switch (event) {
	case XR_STA_EVT_DISCONNECT:
	{
		xr_sta_sm_disconnect_hdlr(cfg);
		break;
	}
	case XR_STA_EVT_CONNECT:
	{
		xr_sta_sm_connect_hdlr(cfg);
		break;
	}
	case XR_STA_EVT_CONNECT_DONE:
	{
		xr_sta_sm_connect_done_hdlr(cfg);
		break;
	}

	default:
		DHD_ERROR(("Unkown event %d\n", event));
		break;
	};

}

/* XR Single STA RX uses slave primary net I/F(i.e its STA I/F) as a proxy
 * So when we receive an event for slave primary I/F , slave primary I/F has to be changed
 * to the master primary I/F before invoking any kernel cfg80211_api or netlink API.
 * This API returns the struct net_device that has to be sent to the upper layers
 * */
struct net_device * xr_sta_get_ndev_for_cfg_event(struct bcm_cfg80211 *cfg, struct net_device *dev) {

	if ((XR_STA_GET_MODE(cfg) != XR_STA_MODE_SINGLE)) {
		DHD_INFO(("XR STA in mode %d\n", XR_STA_GET_MODE(cfg)));
		return dev;
	}
	/* When netdev is not master primary I/F or slave primary I/F
	*  just return original dev*/
	if ((dev != cfg->wdev->netdev)
		&& dev != DHD_XR_GET_SLAVE_NDEV(cfg)) {
		DHD_INFO(("XR STA dev:%s not a sta I/F\n", dev->name));
		return dev;
	}

	/* RX case: When netdev is matches current active XR sta return master primary I/F */
	if ((dev == cfg->xr_sta_active_wdev->netdev)
		 && (dev == DHD_XR_GET_SLAVE_NDEV(cfg))) {
		DHD_INFO(("RX send %s instead of %s \n", cfg->wdev->netdev->name ,dev->name));
		return cfg->wdev->netdev;
	}
	if ((dev != cfg->xr_sta_active_wdev->netdev)
		 && (dev == cfg->wdev->netdev)) {
	/* TX case: When netdev is matches master primary but current active dev is slave primary I/F */
		DHD_INFO(("TX send %s instead of %s \n",
			((struct net_device *)(DHD_XR_GET_SLAVE_NDEV(cfg)))->name, dev->name));
		return DHD_XR_GET_SLAVE_NDEV(cfg);
	}

	return dev;
}

#if defined(CONFIG_ANDROID_VERSION) && (CONFIG_ANDROID_VERSION >= 13)
/* For Android send vendor event to handle MAC update for bond interface*/
void xr_send_sta_mac_change_event(struct bcm_cfg80211 *cfg, struct net_device *ndev) {

	if (!cfg || !cfg->xr_sta_bond_ndev) {
		DHD_ERROR(("%s : no bond device\n", __FUNCTION__));
		return;
	}
	wl_cfgvendor_send_async_event(cfg->wdev->wiphy, ndev, IFX_VENDOR_EVENT_XR_STA_MAC_CHANGE, ndev->name, sizeof(ndev->name));
}
#endif /* CONFIG_ANDROID_VERSION && CONFIG_ANDROID_VERSION >= 13 */

void xr_sta_init(struct bcm_cfg80211 *cfg, struct net_device *ndev, int mode) {

	if (mode == XR_STA_MODE_SINGLE) {
		if (cfg->wdev)
			cfg->xr_sta_active_wdev = cfg->wdev;
		cfg->xr_sta_bond_ndev = NULL;
		cfg->xr_sta_bond_if_cnt = 0;
		cfg->xr_sta_enabled = FALSE;
		cfg->xr_sta_mode = mode;
		DHD_ERROR(("Set XR_STA_MODE_SINGLE %s\n",
			cfg->xr_sta_active_wdev->netdev->name));
	}
	return;
}
void xr_sta_deinit(struct bcm_cfg80211 *cfg) {

	if (cfg->xr_sta_mode == XR_STA_MODE_SINGLE) {
		if (cfg->xr_sta_bond_ndev)
			cfg->xr_sta_bond_ndev = NULL;
		cfg->xr_sta_bond_if_cnt = 0;
		cfg->xr_sta_active_wdev = NULL;
		cfg->xr_sta_enabled = FALSE;
	}
	return;
}
/*
 * Get dhd_pub_t of dhd on which XR sta should connect
*/
dhd_pub_t *xr_sta_get_conn_pub(struct bcm_cfg80211 *cfg,
	enum nl80211_band band) {

	struct net_info *_net_info, *next;
	unsigned long int flags;
	dhd_pub_t *ap_pub_2g = NULL;
	dhd_pub_t *ap_pub_5g = NULL;
	dhd_pub_t *ap_pub_6g = NULL;
	dhd_pub_t *xr_sta_pub = NULL;
	int ap_cnt = 0;
	uint8 xr_role = -1;
	struct cfg80211_chan_def *chandef = NULL;

	spin_lock_irqsave(&cfg->net_list_sync, flags);
	GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST();
	BCM_LIST_FOR_EACH_ENTRY_SAFE(_net_info, next, &cfg->net_list, list) {
		GCC_DIAGNOSTIC_POP();
		if (_net_info->wdev
			&& (_net_info->wdev->iftype == NL80211_IFTYPE_AP)) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 2)) || \
	defined(DHD_ANDROID_KERNEL5_15_SUPPORT)
			chandef = &_net_info->wdev->u.ap.preset_chandef;
#else
			chandef = &_net_info->wdev->chandef;
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 2) || DHD_ANDROID_KERNEL5_15_SUPPORT */
			if (chandef->chan) {
			/* Check if AP exists in the same band as the band requested
			*  for XR sta connection */
				if (chandef->chan->band == band) {
					xr_sta_pub = dhd_get_pub(_net_info->ndev);
					WL_ERR(("AP found on requested band\n"));
					break;
				}

				if (chandef->chan->band == NL80211_BAND_2GHZ) {
					ap_pub_2g = dhd_get_pub(_net_info->ndev);
					ap_cnt++;
				}
				else if (chandef->chan->band == NL80211_BAND_5GHZ) {
					ap_pub_5g = dhd_get_pub(_net_info->ndev);
					ap_cnt++;
				}
#ifdef WL_6E
				else if (chandef->chan->band == NL80211_BAND_6GHZ) {
					ap_pub_6g = dhd_get_pub(_net_info->ndev);
					ap_cnt++;
				}
#endif /* WL_6E */
			}
		}
	}
	spin_unlock_irqrestore(&cfg->net_list_sync, flags);

	if ((xr_sta_pub == NULL) && ap_cnt == 1) {

		WL_ERR(("AP/STA case AP not on requested band\n"));
		if (ap_pub_2g) {
			xr_role = DHD_GET_XR_ROLE(ap_pub_2g);
			xr_sta_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);
		}
		else if (ap_pub_5g) {
			xr_role = DHD_GET_XR_ROLE(ap_pub_5g);
			xr_sta_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);
		}
		else if (ap_pub_6g) {
			xr_role = DHD_GET_XR_ROLE(ap_pub_6g);
			xr_sta_pub = XR_CMD_GET_DEST_PUB(cfg, xr_role);
		}
		else
			WL_ERR(("AP found  case but requested band not active\n"));

	} else if ((xr_sta_pub == NULL) && ap_cnt > 1) {
			WL_ERR(("AP + AP case but requested band not active\n"));
			xr_sta_pub = NULL;
	}

	return xr_sta_pub;

}
#endif /* WL_DHD_XR */
