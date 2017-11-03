/* Copyright (c) 2017 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/ipa_wdi3.h>
#include <linux/msm_ipa.h>
#include <linux/string.h>
#include "../ipa_common_i.h"

#define OFFLOAD_DRV_NAME "ipa_wdi3"
#define IPA_WDI3_DBG(fmt, args...) \
	do { \
		pr_debug(OFFLOAD_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_WDI3_DBG_LOW(fmt, args...) \
	do { \
		pr_debug(OFFLOAD_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_WDI3_ERR(fmt, args...) \
	do { \
		pr_err(OFFLOAD_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

struct ipa_wdi3_intf_info {
	char netdev_name[IPA_RESOURCE_NAME_MAX];
	u8 hdr_len;
	u32 partial_hdr_hdl[IPA_IP_MAX];
	struct list_head link;
};

struct ipa_wdi3_context {
	struct list_head head_intf_list;
	ipa_notify_cb notify;
	void *priv;
	struct completion wdi3_completion;
	struct mutex lock;
};

static struct ipa_wdi3_context *ipa_wdi3_ctx;

static int ipa_wdi3_commit_partial_hdr(
	struct ipa_ioc_add_hdr *hdr,
	const char *netdev_name,
	struct ipa_wdi3_hdr_info *hdr_info)
{
	int i;

	if (!hdr || !hdr_info || !netdev_name) {
		IPA_WDI3_ERR("Invalid input\n");
		return -EINVAL;
	}

	hdr->commit = 1;
	hdr->num_hdrs = 2;

	snprintf(hdr->hdr[0].name, sizeof(hdr->hdr[0].name),
			 "%s_ipv4", netdev_name);
	snprintf(hdr->hdr[1].name, sizeof(hdr->hdr[1].name),
			 "%s_ipv6", netdev_name);
	for (i = IPA_IP_v4; i < IPA_IP_MAX; i++) {
		hdr->hdr[i].hdr_len = hdr_info[i].hdr_len;
		memcpy(hdr->hdr[i].hdr, hdr_info[i].hdr, hdr->hdr[i].hdr_len);
		hdr->hdr[i].type = hdr_info[i].hdr_type;
		hdr->hdr[i].is_partial = 1;
		hdr->hdr[i].is_eth2_ofst_valid = 1;
		hdr->hdr[i].eth2_ofst = hdr_info[i].dst_mac_addr_offset;
	}

	if (ipa_add_hdr(hdr)) {
		IPA_WDI3_ERR("fail to add partial headers\n");
		return -EFAULT;
	}

	return 0;
}

int ipa_wdi3_reg_intf(struct ipa_wdi3_reg_intf_in_params *in)
{
	struct ipa_ioc_add_hdr *hdr;
	struct ipa_wdi3_intf_info *new_intf;
	struct ipa_wdi3_intf_info *entry;
	struct ipa_tx_intf tx;
	struct ipa_rx_intf rx;
	struct ipa_ioc_tx_intf_prop tx_prop[2];
	struct ipa_ioc_rx_intf_prop rx_prop[2];
	u32 len;
	int ret = 0;

	if (in == NULL) {
		IPA_WDI3_ERR("invalid params in=%pK\n", in);
		return -EINVAL;
	}

	if (!ipa_wdi3_ctx) {
		ipa_wdi3_ctx = kzalloc(sizeof(*ipa_wdi3_ctx), GFP_KERNEL);
		if (ipa_wdi3_ctx == NULL) {
			IPA_WDI3_ERR("fail to alloc wdi3 ctx\n");
			return -ENOMEM;
		}
		mutex_init(&ipa_wdi3_ctx->lock);
		INIT_LIST_HEAD(&ipa_wdi3_ctx->head_intf_list);
	}

	IPA_WDI3_DBG("register interface for netdev %s\n",
		in->netdev_name);

	mutex_lock(&ipa_wdi3_ctx->lock);
	list_for_each_entry(entry, &ipa_wdi3_ctx->head_intf_list, link)
		if (strcmp(entry->netdev_name, in->netdev_name) == 0) {
			IPA_WDI3_DBG("intf was added before.\n");
			mutex_unlock(&ipa_wdi3_ctx->lock);
			return 0;
		}

	IPA_WDI3_DBG("intf was not added before, proceed.\n");
	new_intf = kzalloc(sizeof(*new_intf), GFP_KERNEL);
	if (new_intf == NULL) {
		IPA_WDI3_ERR("fail to alloc new intf\n");
		mutex_unlock(&ipa_wdi3_ctx->lock);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&new_intf->link);
	strlcpy(new_intf->netdev_name, in->netdev_name,
		sizeof(new_intf->netdev_name));
	new_intf->hdr_len = in->hdr_info[0].hdr_len;

	/* add partial header */
	len = sizeof(struct ipa_ioc_add_hdr) + 2 * sizeof(struct ipa_hdr_add);
	hdr = kzalloc(len, GFP_KERNEL);
	if (hdr == NULL) {
		IPA_WDI3_ERR("fail to alloc %d bytes\n", len);
		ret = -EFAULT;
		goto fail_alloc_hdr;
	}

	if (ipa_wdi3_commit_partial_hdr(hdr, in->netdev_name, in->hdr_info)) {
		IPA_WDI3_ERR("fail to commit partial headers\n");
		ret = -EFAULT;
		goto fail_commit_hdr;
	}

	new_intf->partial_hdr_hdl[IPA_IP_v4] = hdr->hdr[IPA_IP_v4].hdr_hdl;
	new_intf->partial_hdr_hdl[IPA_IP_v6] = hdr->hdr[IPA_IP_v6].hdr_hdl;
	IPA_WDI3_DBG("IPv4 hdr hdl: %d IPv6 hdr hdl: %d\n",
		hdr->hdr[IPA_IP_v4].hdr_hdl, hdr->hdr[IPA_IP_v6].hdr_hdl);

	/* populate tx prop */
	tx.num_props = 2;
	tx.prop = tx_prop;

	memset(tx_prop, 0, sizeof(tx_prop));
	tx_prop[0].ip = IPA_IP_v4;
	tx_prop[0].dst_pipe = IPA_CLIENT_WLAN1_CONS;
	tx_prop[0].hdr_l2_type = in->hdr_info[0].hdr_type;
	strlcpy(tx_prop[0].hdr_name, hdr->hdr[IPA_IP_v4].name,
		sizeof(tx_prop[0].hdr_name));

	tx_prop[1].ip = IPA_IP_v6;
	tx_prop[1].dst_pipe = IPA_CLIENT_WLAN1_CONS;
	tx_prop[1].hdr_l2_type = in->hdr_info[1].hdr_type;
	strlcpy(tx_prop[1].hdr_name, hdr->hdr[IPA_IP_v6].name,
		sizeof(tx_prop[1].hdr_name));

	/* populate rx prop */
	rx.num_props = 2;
	rx.prop = rx_prop;

	memset(rx_prop, 0, sizeof(rx_prop));
	rx_prop[0].ip = IPA_IP_v4;
	rx_prop[0].src_pipe = IPA_CLIENT_WLAN1_PROD;
	rx_prop[0].hdr_l2_type = in->hdr_info[0].hdr_type;
	if (in->is_meta_data_valid) {
		rx_prop[0].attrib.attrib_mask |= IPA_FLT_META_DATA;
		rx_prop[0].attrib.meta_data = in->meta_data;
		rx_prop[0].attrib.meta_data_mask = in->meta_data_mask;
	}

	rx_prop[1].ip = IPA_IP_v6;
	rx_prop[1].src_pipe = IPA_CLIENT_WLAN1_PROD;
	rx_prop[1].hdr_l2_type = in->hdr_info[1].hdr_type;
	if (in->is_meta_data_valid) {
		rx_prop[1].attrib.attrib_mask |= IPA_FLT_META_DATA;
		rx_prop[1].attrib.meta_data = in->meta_data;
		rx_prop[1].attrib.meta_data_mask = in->meta_data_mask;
	}

	if (ipa_register_intf(in->netdev_name, &tx, &rx)) {
		IPA_WDI3_ERR("fail to add interface prop\n");
		ret = -EFAULT;
		goto fail_commit_hdr;
	}

	list_add(&new_intf->link, &ipa_wdi3_ctx->head_intf_list);
	init_completion(&ipa_wdi3_ctx->wdi3_completion);

	kfree(hdr);
	mutex_unlock(&ipa_wdi3_ctx->lock);
	return 0;

fail_commit_hdr:
	kfree(hdr);
fail_alloc_hdr:
	kfree(new_intf);
	mutex_unlock(&ipa_wdi3_ctx->lock);
	return ret;
}
EXPORT_SYMBOL(ipa_wdi3_reg_intf);

int ipa_wdi3_dereg_intf(const char *netdev_name)
{
	int len, ret = 0;
	struct ipa_ioc_del_hdr *hdr = NULL;
	struct ipa_wdi3_intf_info *entry;
	struct ipa_wdi3_intf_info *next;

	if (!netdev_name) {
		IPA_WDI3_ERR("no netdev name.\n");
		return -EINVAL;
	}

	if (!ipa_wdi3_ctx) {
		IPA_WDI3_ERR("wdi3 ctx is not initialized.\n");
		return -EPERM;
	}

	mutex_lock(&ipa_wdi3_ctx->lock);
	list_for_each_entry_safe(entry, next, &ipa_wdi3_ctx->head_intf_list,
		link)
		if (strcmp(entry->netdev_name, netdev_name) == 0) {
			len = sizeof(struct ipa_ioc_del_hdr) +
				2 * sizeof(struct ipa_hdr_del);
			hdr = kzalloc(len, GFP_KERNEL);
			if (hdr == NULL) {
				IPA_WDI3_ERR("fail to alloc %d bytes\n", len);
				mutex_unlock(&ipa_wdi3_ctx->lock);
				return -ENOMEM;
			}

			hdr->commit = 1;
			hdr->num_hdls = 2;
			hdr->hdl[0].hdl = entry->partial_hdr_hdl[0];
			hdr->hdl[1].hdl = entry->partial_hdr_hdl[1];
			IPA_WDI3_DBG("IPv4 hdr hdl: %d IPv6 hdr hdl: %d\n",
				hdr->hdl[0].hdl, hdr->hdl[1].hdl);

			if (ipa_del_hdr(hdr)) {
				IPA_WDI3_ERR("fail to delete partial header\n");
				ret = -EFAULT;
				goto fail;
			}

			if (ipa_deregister_intf(entry->netdev_name)) {
				IPA_WDI3_ERR("fail to del interface props\n");
				ret = -EFAULT;
				goto fail;
			}
			list_del(&entry->link);
			kfree(entry);

			break;
		}

fail:
	kfree(hdr);
	mutex_unlock(&ipa_wdi3_ctx->lock);
	return ret;
}
EXPORT_SYMBOL(ipa_wdi3_dereg_intf);

static void ipa_wdi3_rm_notify(void *user_data, enum ipa_rm_event event,
		unsigned long data)
{
	if (!ipa_wdi3_ctx) {
		IPA_WDI3_ERR("Invalid context\n");
		return;
	}

	switch (event) {
	case IPA_RM_RESOURCE_GRANTED:
		complete_all(&ipa_wdi3_ctx->wdi3_completion);
		break;

	case IPA_RM_RESOURCE_RELEASED:
		break;

	default:
		IPA_WDI3_ERR("Invalid RM Evt: %d", event);
		break;
	}
}

static int ipa_wdi3_cons_release(void)
{
	return 0;
}

static int ipa_wdi3_cons_request(void)
{
	int ret = 0;

	if (!ipa_wdi3_ctx) {
		IPA_WDI3_ERR("wdi3 ctx is not initialized\n");
		ret = -EFAULT;
	}

	return ret;
}

int ipa_wdi3_conn_pipes(struct ipa_wdi3_conn_in_params *in,
			struct ipa_wdi3_conn_out_params *out)
{
	int ret = 0;
	struct ipa_rm_create_params param;

	if (!(in && out)) {
		IPA_WDI3_ERR("empty parameters. in=%pK out=%pK\n", in, out);
		return -EINVAL;
	}

	if (!ipa_wdi3_ctx) {
		ipa_wdi3_ctx = kzalloc(sizeof(*ipa_wdi3_ctx), GFP_KERNEL);
		if (ipa_wdi3_ctx == NULL) {
			IPA_WDI3_ERR("fail to alloc wdi3 ctx\n");
			return -EFAULT;
		}
		mutex_init(&ipa_wdi3_ctx->lock);
		INIT_LIST_HEAD(&ipa_wdi3_ctx->head_intf_list);
	}
	ipa_wdi3_ctx->notify = in->notify;
	ipa_wdi3_ctx->priv = in->priv;

	memset(&param, 0, sizeof(param));
	param.name = IPA_RM_RESOURCE_WLAN_PROD;
	param.reg_params.user_data = ipa_wdi3_ctx;
	param.reg_params.notify_cb = ipa_wdi3_rm_notify;
	param.floor_voltage = IPA_VOLTAGE_SVS;
	ret = ipa_rm_create_resource(&param);
	if (ret) {
		IPA_WDI3_ERR("fail to create WLAN_PROD resource\n");
		return -EFAULT;
	}

	memset(&param, 0, sizeof(param));
	param.name = IPA_RM_RESOURCE_WLAN_CONS;
	param.request_resource = ipa_wdi3_cons_request;
	param.release_resource = ipa_wdi3_cons_release;
	ret = ipa_rm_create_resource(&param);
	if (ret) {
		IPA_WDI3_ERR("fail to create WLAN_CONS resource\n");
		goto fail_create_rm_cons;
	}

	if (ipa_rm_add_dependency(IPA_RM_RESOURCE_WLAN_PROD,
		IPA_RM_RESOURCE_APPS_CONS)) {
		IPA_WDI3_ERR("fail to add rm dependency\n");
		ret = -EFAULT;
		goto fail;
	}

	if (ipa_conn_wdi3_pipes(in, out)) {
		IPA_WDI3_ERR("fail to setup wdi3 pipes\n");
		ret = -EFAULT;
		goto fail;
	}

	return 0;

fail:
	ipa_rm_delete_resource(IPA_RM_RESOURCE_WLAN_CONS);
fail_create_rm_cons:
	ipa_rm_delete_resource(IPA_RM_RESOURCE_WLAN_PROD);

	return ret;
}
EXPORT_SYMBOL(ipa_wdi3_conn_pipes);

int ipa_wdi3_disconn_pipes(void)
{
	int ipa_ep_idx_rx, ipa_ep_idx_tx;

	if (!ipa_wdi3_ctx) {
		IPA_WDI3_ERR("wdi3 ctx is not initialized\n");
		return -EPERM;
	}

	ipa_ep_idx_rx = ipa_get_ep_mapping(IPA_CLIENT_WLAN1_PROD);
	ipa_ep_idx_tx = ipa_get_ep_mapping(IPA_CLIENT_WLAN1_CONS);
	if (ipa_disconn_wdi3_pipes(ipa_ep_idx_rx, ipa_ep_idx_tx)) {
		IPA_WDI3_ERR("fail to tear down wdi3 pipes\n");
		return -EFAULT;
	}

	if (ipa_rm_delete_dependency(IPA_RM_RESOURCE_WLAN_PROD,
				IPA_RM_RESOURCE_APPS_CONS)) {
		IPA_WDI3_ERR("fail to delete rm dependency\n");
		return -EFAULT;
	}

	if (ipa_rm_delete_resource(IPA_RM_RESOURCE_WLAN_PROD)) {
		IPA_WDI3_ERR("fail to delete WLAN_PROD resource\n");
		return -EFAULT;
	}

	if (ipa_rm_delete_resource(IPA_RM_RESOURCE_WLAN_CONS)) {
		IPA_WDI3_ERR("fail to delete WLAN_CONS resource\n");
		return -EFAULT;
	}

	return 0;
}
EXPORT_SYMBOL(ipa_wdi3_disconn_pipes);

int ipa_wdi3_enable_pipes(void)
{
	int ret;
	int ipa_ep_idx_tx, ipa_ep_idx_rx;

	if (!ipa_wdi3_ctx) {
		IPA_WDI3_ERR("wdi3 ctx is not initialized.\n");
		return -EPERM;
	}

	ipa_ep_idx_rx = ipa_get_ep_mapping(IPA_CLIENT_WLAN1_PROD);
	ipa_ep_idx_tx = ipa_get_ep_mapping(IPA_CLIENT_WLAN1_CONS);
	if (ipa_enable_wdi3_pipes(ipa_ep_idx_tx, ipa_ep_idx_rx)) {
		IPA_WDI3_ERR("fail to enable wdi3 pipes\n");
		return -EFAULT;
	}

	ret = ipa_rm_request_resource(IPA_RM_RESOURCE_WLAN_PROD);
	if (ret == -EINPROGRESS) {
		if (wait_for_completion_timeout(&ipa_wdi3_ctx->wdi3_completion,
			10*HZ) == 0) {
			IPA_WDI3_ERR("WLAN_PROD resource req time out\n");
			return -EFAULT;
		}
	} else if (ret != 0) {
		IPA_WDI3_ERR("fail to request resource\n");
		return -EFAULT;
	}

	return 0;
}
EXPORT_SYMBOL(ipa_wdi3_enable_pipes);

int ipa_wdi3_disable_pipes(void)
{
	int ret;
	int ipa_ep_idx_tx, ipa_ep_idx_rx;

	if (!ipa_wdi3_ctx) {
		IPA_WDI3_ERR("wdi3 ctx is not initialized.\n");
		return -EPERM;
	}

	ret = ipa_rm_release_resource(IPA_RM_RESOURCE_WLAN_PROD);
	if (ret != 0) {
		IPA_WDI3_ERR("fail to release resource\n");
		return -EFAULT;
	}

	ipa_ep_idx_rx = ipa_get_ep_mapping(IPA_CLIENT_WLAN1_PROD);
	ipa_ep_idx_tx = ipa_get_ep_mapping(IPA_CLIENT_WLAN1_CONS);
	if (ipa_disable_wdi3_pipes(ipa_ep_idx_tx, ipa_ep_idx_rx)) {
		IPA_WDI3_ERR("fail to disable wdi3 pipes\n");
		return -EFAULT;
	}

	return 0;
}
EXPORT_SYMBOL(ipa_wdi3_disable_pipes);

int ipa_wdi3_set_perf_profile(struct ipa_wdi3_perf_profile *profile)
{
	struct ipa_rm_perf_profile rm_profile;
	enum ipa_rm_resource_name resource_name;

	if (profile == NULL) {
		IPA_WDI3_ERR("Invalid input\n");
		return -EINVAL;
	}

	rm_profile.max_supported_bandwidth_mbps =
		profile->max_supported_bw_mbps;

	if (profile->client == IPA_CLIENT_WLAN1_PROD) {
		resource_name = IPA_RM_RESOURCE_WLAN_PROD;
	} else if (profile->client == IPA_CLIENT_WLAN1_CONS) {
		resource_name = IPA_RM_RESOURCE_WLAN_CONS;
	} else {
		IPA_WDI3_ERR("not supported\n");
		return -EINVAL;
	}

	if (ipa_rm_set_perf_profile(resource_name, &rm_profile)) {
		IPA_WDI3_ERR("fail to setup rm perf profile\n");
		return -EFAULT;
	}

	return 0;
}
EXPORT_SYMBOL(ipa_wdi3_set_perf_profile);
