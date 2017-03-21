/* Copyright (c) 2015, 2016 The Linux Foundation. All rights reserved.
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

#include <linux/ipa_uc_offload.h>
#include <linux/msm_ipa.h>
#include "../ipa_common_i.h"

#define IPA_NTN_DMA_POOL_ALIGNMENT 8
#define OFFLOAD_DRV_NAME "ipa_uc_offload"
#define IPA_UC_OFFLOAD_DBG(fmt, args...) \
	do { \
		pr_debug(OFFLOAD_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_UC_OFFLOAD_LOW(fmt, args...) \
	do { \
		pr_debug(OFFLOAD_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_UC_OFFLOAD_ERR(fmt, args...) \
	do { \
		pr_err(OFFLOAD_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_UC_OFFLOAD_INFO(fmt, args...) \
	do { \
		pr_info(OFFLOAD_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

enum ipa_uc_offload_state {
	IPA_UC_OFFLOAD_STATE_INVALID,
	IPA_UC_OFFLOAD_STATE_INITIALIZED,
	IPA_UC_OFFLOAD_STATE_UP,
	IPA_UC_OFFLOAD_STATE_DOWN,
};

struct ipa_uc_offload_ctx {
	enum ipa_uc_offload_proto proto;
	enum ipa_uc_offload_state state;
	void *priv;
	u8 hdr_len;
	u32 partial_hdr_hdl[IPA_IP_MAX];
	char netdev_name[IPA_RESOURCE_NAME_MAX];
	ipa_notify_cb notify;
	struct completion ntn_completion;
};

static struct ipa_uc_offload_ctx *ipa_uc_offload_ctx[IPA_UC_MAX_PROT_SIZE];

static int ipa_uc_ntn_cons_release(void);
static int ipa_uc_ntn_cons_request(void);
static void ipa_uc_offload_rm_notify(void *, enum ipa_rm_event, unsigned long);

static int ipa_commit_partial_hdr(
	struct ipa_ioc_add_hdr *hdr,
	const char *netdev_name,
	struct ipa_hdr_info *hdr_info)
{
	int i;

	if (hdr == NULL || hdr_info == NULL) {
		IPA_UC_OFFLOAD_ERR("Invalid input\n");
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
		IPA_UC_OFFLOAD_ERR("fail to add partial headers\n");
		return -EFAULT;
	}

	return 0;
}

static int ipa_uc_offload_ntn_reg_intf(
	struct ipa_uc_offload_intf_params *inp,
	struct ipa_uc_offload_out_params *outp,
	struct ipa_uc_offload_ctx *ntn_ctx)
{
	struct ipa_ioc_add_hdr *hdr = NULL;
	struct ipa_tx_intf tx;
	struct ipa_rx_intf rx;
	struct ipa_ioc_tx_intf_prop tx_prop[2];
	struct ipa_ioc_rx_intf_prop rx_prop[2];
	struct ipa_rm_create_params param;
	u32 len;
	int ret = 0;

	IPA_UC_OFFLOAD_DBG("register interface for netdev %s\n",
					 inp->netdev_name);
	memset(&param, 0, sizeof(param));
	param.name = IPA_RM_RESOURCE_ODU_ADAPT_PROD;
	param.reg_params.user_data = ntn_ctx;
	param.reg_params.notify_cb = ipa_uc_offload_rm_notify;
	param.floor_voltage = IPA_VOLTAGE_SVS;
	ret = ipa_rm_create_resource(&param);
	if (ret) {
		IPA_UC_OFFLOAD_ERR("fail to create ODU_ADAPT_PROD resource\n");
		return -EFAULT;
	}

	memset(&param, 0, sizeof(param));
	param.name = IPA_RM_RESOURCE_ODU_ADAPT_CONS;
	param.request_resource = ipa_uc_ntn_cons_request;
	param.release_resource = ipa_uc_ntn_cons_release;
	ret = ipa_rm_create_resource(&param);
	if (ret) {
		IPA_UC_OFFLOAD_ERR("fail to create ODU_ADAPT_CONS resource\n");
		goto fail_create_rm_cons;
	}

	memcpy(ntn_ctx->netdev_name, inp->netdev_name, IPA_RESOURCE_NAME_MAX);
	ntn_ctx->hdr_len = inp->hdr_info[0].hdr_len;
	ntn_ctx->notify = inp->notify;
	ntn_ctx->priv = inp->priv;

	/* add partial header */
	len = sizeof(struct ipa_ioc_add_hdr) + 2 * sizeof(struct ipa_hdr_add);
	hdr = kzalloc(len, GFP_KERNEL);
	if (hdr == NULL) {
		IPA_UC_OFFLOAD_ERR("fail to alloc %d bytes\n", len);
		ret = -ENOMEM;
		goto fail_alloc;
	}

	if (ipa_commit_partial_hdr(hdr, ntn_ctx->netdev_name, inp->hdr_info)) {
		IPA_UC_OFFLOAD_ERR("fail to commit partial headers\n");
		ret = -EFAULT;
		goto fail;
	}

	/* populate tx prop */
	tx.num_props = 2;
	tx.prop = tx_prop;

	memset(tx_prop, 0, sizeof(tx_prop));
	tx_prop[0].ip = IPA_IP_v4;
	tx_prop[0].dst_pipe = IPA_CLIENT_ODU_TETH_CONS;
	tx_prop[0].hdr_l2_type = inp->hdr_info[0].hdr_type;
	memcpy(tx_prop[0].hdr_name, hdr->hdr[IPA_IP_v4].name,
		sizeof(tx_prop[0].hdr_name));

	tx_prop[1].ip = IPA_IP_v6;
	tx_prop[1].dst_pipe = IPA_CLIENT_ODU_TETH_CONS;
	tx_prop[1].hdr_l2_type = inp->hdr_info[1].hdr_type;
	memcpy(tx_prop[1].hdr_name, hdr->hdr[IPA_IP_v6].name,
		sizeof(tx_prop[1].hdr_name));

	/* populate rx prop */
	rx.num_props = 2;
	rx.prop = rx_prop;

	memset(rx_prop, 0, sizeof(rx_prop));
	rx_prop[0].ip = IPA_IP_v4;
	rx_prop[0].src_pipe = IPA_CLIENT_ODU_PROD;
	rx_prop[0].hdr_l2_type = inp->hdr_info[0].hdr_type;
	if (inp->is_meta_data_valid) {
		rx_prop[0].attrib.attrib_mask |= IPA_FLT_META_DATA;
		rx_prop[0].attrib.meta_data = inp->meta_data;
		rx_prop[0].attrib.meta_data_mask = inp->meta_data_mask;
	}

	rx_prop[1].ip = IPA_IP_v6;
	rx_prop[1].src_pipe = IPA_CLIENT_ODU_PROD;
	rx_prop[1].hdr_l2_type = inp->hdr_info[1].hdr_type;
	if (inp->is_meta_data_valid) {
		rx_prop[1].attrib.attrib_mask |= IPA_FLT_META_DATA;
		rx_prop[1].attrib.meta_data = inp->meta_data;
		rx_prop[1].attrib.meta_data_mask = inp->meta_data_mask;
	}

	if (ipa_register_intf(inp->netdev_name, &tx, &rx)) {
		IPA_UC_OFFLOAD_ERR("fail to add interface prop\n");
		memset(ntn_ctx, 0, sizeof(*ntn_ctx));
		ret = -EFAULT;
		goto fail;
	}

	ntn_ctx->partial_hdr_hdl[IPA_IP_v4] = hdr->hdr[IPA_IP_v4].hdr_hdl;
	ntn_ctx->partial_hdr_hdl[IPA_IP_v6] = hdr->hdr[IPA_IP_v6].hdr_hdl;
	init_completion(&ntn_ctx->ntn_completion);
	ntn_ctx->state = IPA_UC_OFFLOAD_STATE_INITIALIZED;

	kfree(hdr);
	return ret;

fail:
	kfree(hdr);
fail_alloc:
	ipa_rm_delete_resource(IPA_RM_RESOURCE_ODU_ADAPT_CONS);
fail_create_rm_cons:
	ipa_rm_delete_resource(IPA_RM_RESOURCE_ODU_ADAPT_PROD);
	return ret;
}

int ipa_uc_offload_reg_intf(
	struct ipa_uc_offload_intf_params *inp,
	struct ipa_uc_offload_out_params *outp)
{
	struct ipa_uc_offload_ctx *ctx;
	int ret = 0;

	if (inp == NULL || outp == NULL) {
		IPA_UC_OFFLOAD_ERR("invalid params in=%p out=%p\n", inp, outp);
		return -EINVAL;
	}

	if (inp->proto <= IPA_UC_INVALID ||
		inp->proto >= IPA_UC_MAX_PROT_SIZE) {
		IPA_UC_OFFLOAD_ERR("invalid proto %d\n", inp->proto);
		return -EINVAL;
	}

	if (!ipa_uc_offload_ctx[inp->proto]) {
		ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
		if (ctx == NULL) {
			IPA_UC_OFFLOAD_ERR("fail to alloc uc offload ctx\n");
			return -EFAULT;
		}
		ipa_uc_offload_ctx[inp->proto] = ctx;
		ctx->proto = inp->proto;
	} else
		ctx = ipa_uc_offload_ctx[inp->proto];

	if (ctx->state != IPA_UC_OFFLOAD_STATE_INVALID) {
		IPA_UC_OFFLOAD_ERR("Already Initialized\n");
		return -EINVAL;
	}

	if (ctx->proto == IPA_UC_NTN) {
		ret = ipa_uc_offload_ntn_reg_intf(inp, outp, ctx);
		if (!ret)
			outp->clnt_hndl = IPA_UC_NTN;
	}

	return ret;
}
EXPORT_SYMBOL(ipa_uc_offload_reg_intf);

static int ipa_uc_ntn_cons_release(void)
{
	return 0;
}

static int ipa_uc_ntn_cons_request(void)
{
	int ret = 0;
	struct ipa_uc_offload_ctx *ntn_ctx;

	ntn_ctx = ipa_uc_offload_ctx[IPA_UC_NTN];
	if (!ntn_ctx) {
		IPA_UC_OFFLOAD_ERR("NTN is not initialized\n");
		ret = -EFAULT;
	} else if (ntn_ctx->state != IPA_UC_OFFLOAD_STATE_UP) {
		IPA_UC_OFFLOAD_ERR("Invalid State: %d\n", ntn_ctx->state);
		ret = -EFAULT;
	}

	return ret;
}

static void ipa_uc_offload_rm_notify(void *user_data, enum ipa_rm_event event,
		unsigned long data)
{
	struct ipa_uc_offload_ctx *offload_ctx;

	offload_ctx = (struct ipa_uc_offload_ctx *)user_data;
	if (!(offload_ctx && offload_ctx->proto > IPA_UC_INVALID &&
		  offload_ctx->proto < IPA_UC_MAX_PROT_SIZE)) {
		IPA_UC_OFFLOAD_ERR("Invalid user data\n");
		return;
	}

	if (offload_ctx->state != IPA_UC_OFFLOAD_STATE_INITIALIZED)
		IPA_UC_OFFLOAD_ERR("Invalid State: %d\n", offload_ctx->state);

	switch (event) {
	case IPA_RM_RESOURCE_GRANTED:
		complete_all(&offload_ctx->ntn_completion);
		break;

	case IPA_RM_RESOURCE_RELEASED:
		break;

	default:
		IPA_UC_OFFLOAD_ERR("Invalid RM Evt: %d", event);
		break;
	}
}

int ipa_uc_ntn_conn_pipes(struct ipa_ntn_conn_in_params *inp,
			struct ipa_ntn_conn_out_params *outp,
			struct ipa_uc_offload_ctx *ntn_ctx)
{
	int result = 0;
	enum ipa_uc_offload_state prev_state;

	prev_state = ntn_ctx->state;
	if (inp->dl.ring_base_pa % IPA_NTN_DMA_POOL_ALIGNMENT ||
		inp->dl.buff_pool_base_pa % IPA_NTN_DMA_POOL_ALIGNMENT) {
		IPA_UC_OFFLOAD_ERR("alignment failure on TX\n");
		return -EINVAL;
	}
	if (inp->ul.ring_base_pa % IPA_NTN_DMA_POOL_ALIGNMENT ||
		inp->ul.buff_pool_base_pa % IPA_NTN_DMA_POOL_ALIGNMENT) {
		IPA_UC_OFFLOAD_ERR("alignment failure on RX\n");
		return -EINVAL;
	}

	result = ipa_rm_add_dependency(IPA_RM_RESOURCE_ODU_ADAPT_PROD,
		IPA_RM_RESOURCE_APPS_CONS);
	if (result) {
		IPA_UC_OFFLOAD_ERR("fail to add rm dependency: %d\n", result);
		return result;
	}

	result = ipa_rm_request_resource(IPA_RM_RESOURCE_ODU_ADAPT_PROD);
	if (result == -EINPROGRESS) {
		if (wait_for_completion_timeout(&ntn_ctx->ntn_completion,
			10*HZ) == 0) {
			IPA_UC_OFFLOAD_ERR("ODU PROD resource req time out\n");
			result = -EFAULT;
			goto fail;
		}
	} else if (result != 0) {
		IPA_UC_OFFLOAD_ERR("fail to request resource\n");
		result = -EFAULT;
		goto fail;
	}

	ntn_ctx->state = IPA_UC_OFFLOAD_STATE_UP;
	result = ipa_setup_uc_ntn_pipes(inp, ntn_ctx->notify,
		ntn_ctx->priv, ntn_ctx->hdr_len, outp);
	if (result) {
		IPA_UC_OFFLOAD_ERR("fail to setup uc offload pipes: %d\n",
				result);
		ntn_ctx->state = prev_state;
		result = -EFAULT;
		goto fail;
	}

	return 0;

fail:
	ipa_rm_delete_dependency(IPA_RM_RESOURCE_ODU_ADAPT_PROD,
		IPA_RM_RESOURCE_APPS_CONS);
	return result;
}

int ipa_uc_offload_conn_pipes(struct ipa_uc_offload_conn_in_params *inp,
			struct ipa_uc_offload_conn_out_params *outp)
{
	int ret = 0;
	struct ipa_uc_offload_ctx *offload_ctx;

	if (!(inp && outp)) {
		IPA_UC_OFFLOAD_ERR("bad parm. in=%p out=%p\n", inp, outp);
		return -EINVAL;
	}

	if (inp->clnt_hndl <= IPA_UC_INVALID ||
		inp->clnt_hndl >= IPA_UC_MAX_PROT_SIZE) {
		IPA_UC_OFFLOAD_ERR("invalid client handle %d\n",
						   inp->clnt_hndl);
		return -EINVAL;
	}

	offload_ctx = ipa_uc_offload_ctx[inp->clnt_hndl];
	if (!offload_ctx) {
		IPA_UC_OFFLOAD_ERR("Invalid Handle\n");
		return -EINVAL;
	}

	if (offload_ctx->state != IPA_UC_OFFLOAD_STATE_INITIALIZED &&
		offload_ctx->state != IPA_UC_OFFLOAD_STATE_DOWN) {
		IPA_UC_OFFLOAD_ERR("Invalid state %d\n", offload_ctx->state);
		return -EPERM;
	}

	switch (offload_ctx->proto) {
	case IPA_UC_NTN:
		ret = ipa_uc_ntn_conn_pipes(&inp->u.ntn, &outp->u.ntn,
						offload_ctx);
		break;

	default:
		IPA_UC_OFFLOAD_ERR("Invalid Proto :%d\n", offload_ctx->proto);
		ret = -EINVAL;
		break;
	}

	return ret;
}
EXPORT_SYMBOL(ipa_uc_offload_conn_pipes);

int ipa_set_perf_profile(struct ipa_perf_profile *profile)
{
	struct ipa_rm_perf_profile rm_profile;
	enum ipa_rm_resource_name resource_name;

	if (profile == NULL) {
		IPA_UC_OFFLOAD_ERR("Invalid input\n");
		return -EINVAL;
	}

	rm_profile.max_supported_bandwidth_mbps =
		profile->max_supported_bw_mbps;

	if (profile->client == IPA_CLIENT_ODU_PROD) {
		resource_name = IPA_RM_RESOURCE_ODU_ADAPT_PROD;
	} else if (profile->client == IPA_CLIENT_ODU_TETH_CONS) {
		resource_name = IPA_RM_RESOURCE_ODU_ADAPT_CONS;
	} else {
		IPA_UC_OFFLOAD_ERR("not supported\n");
		return -EINVAL;
	}

	if (ipa_rm_set_perf_profile(resource_name, &rm_profile)) {
		IPA_UC_OFFLOAD_ERR("fail to setup rm perf profile\n");
		return -EFAULT;
	}

	return 0;
}
EXPORT_SYMBOL(ipa_set_perf_profile);

static int ipa_uc_ntn_disconn_pipes(struct ipa_uc_offload_ctx *ntn_ctx)
{
	int ipa_ep_idx_ul, ipa_ep_idx_dl;
	int ret = 0;

	ntn_ctx->state = IPA_UC_OFFLOAD_STATE_DOWN;

	ret = ipa_rm_release_resource(IPA_RM_RESOURCE_ODU_ADAPT_PROD);
	if (ret) {
		IPA_UC_OFFLOAD_ERR("fail to release ODU_ADAPT_PROD res: %d\n",
						  ret);
		return -EFAULT;
	}

	ret = ipa_rm_delete_dependency(IPA_RM_RESOURCE_ODU_ADAPT_PROD,
		IPA_RM_RESOURCE_APPS_CONS);
	if (ret) {
		IPA_UC_OFFLOAD_ERR("fail to del dep ODU->APPS, %d\n", ret);
		return -EFAULT;
	}

	ipa_ep_idx_ul = ipa_get_ep_mapping(IPA_CLIENT_ODU_PROD);
	ipa_ep_idx_dl = ipa_get_ep_mapping(IPA_CLIENT_ODU_TETH_CONS);
	ret = ipa_tear_down_uc_offload_pipes(ipa_ep_idx_ul, ipa_ep_idx_dl);
	if (ret) {
		IPA_UC_OFFLOAD_ERR("fail to tear down ntn offload pipes, %d\n",
						 ret);
		return -EFAULT;
	}

	return ret;
}

int ipa_uc_offload_disconn_pipes(u32 clnt_hdl)
{
	struct ipa_uc_offload_ctx *offload_ctx;
	int ret = 0;

	if (clnt_hdl <= IPA_UC_INVALID ||
		clnt_hdl >= IPA_UC_MAX_PROT_SIZE) {
		IPA_UC_OFFLOAD_ERR("Invalid client handle %d\n", clnt_hdl);
		return -EINVAL;
	}

	offload_ctx = ipa_uc_offload_ctx[clnt_hdl];
	if (!offload_ctx) {
		IPA_UC_OFFLOAD_ERR("Invalid client Handle\n");
		return -EINVAL;
	}

	if (offload_ctx->state != IPA_UC_OFFLOAD_STATE_UP) {
		IPA_UC_OFFLOAD_ERR("Invalid state\n");
		return -EINVAL;
	}

	switch (offload_ctx->proto) {
	case IPA_UC_NTN:
		ret = ipa_uc_ntn_disconn_pipes(offload_ctx);
		break;

	default:
		IPA_UC_OFFLOAD_ERR("Invalid Proto :%d\n", clnt_hdl);
		ret = -EINVAL;
		break;
	}

	return ret;
}
EXPORT_SYMBOL(ipa_uc_offload_disconn_pipes);

static int ipa_uc_ntn_cleanup(struct ipa_uc_offload_ctx *ntn_ctx)
{
	int len, result = 0;
	struct ipa_ioc_del_hdr *hdr;

	if (ipa_rm_delete_resource(IPA_RM_RESOURCE_ODU_ADAPT_PROD)) {
		IPA_UC_OFFLOAD_ERR("fail to delete ODU_ADAPT_PROD resource\n");
		return -EFAULT;
	}

	if (ipa_rm_delete_resource(IPA_RM_RESOURCE_ODU_ADAPT_CONS)) {
		IPA_UC_OFFLOAD_ERR("fail to delete ODU_ADAPT_CONS resource\n");
		return -EFAULT;
	}

	len = sizeof(struct ipa_ioc_del_hdr) + 2 * sizeof(struct ipa_hdr_del);
	hdr = kzalloc(len, GFP_KERNEL);
	if (hdr == NULL) {
		IPA_UC_OFFLOAD_ERR("fail to alloc %d bytes\n", len);
		return -ENOMEM;
	}

	hdr->commit = 1;
	hdr->num_hdls = 2;
	hdr->hdl[0].hdl = ntn_ctx->partial_hdr_hdl[0];
	hdr->hdl[1].hdl = ntn_ctx->partial_hdr_hdl[1];

	if (ipa_del_hdr(hdr)) {
		IPA_UC_OFFLOAD_ERR("fail to delete partial header\n");
		result = -EFAULT;
		goto fail;
	}

	if (ipa_deregister_intf(ntn_ctx->netdev_name)) {
		IPA_UC_OFFLOAD_ERR("fail to delete interface prop\n");
		result = -EFAULT;
		goto fail;
	}

fail:
	kfree(hdr);
	return result;
}

int ipa_uc_offload_cleanup(u32 clnt_hdl)
{
	struct ipa_uc_offload_ctx *offload_ctx;
	int ret = 0;

	if (clnt_hdl <= IPA_UC_INVALID ||
		clnt_hdl >= IPA_UC_MAX_PROT_SIZE) {
		IPA_UC_OFFLOAD_ERR("Invalid client handle %d\n", clnt_hdl);
		return -EINVAL;
	}

	offload_ctx = ipa_uc_offload_ctx[clnt_hdl];
	if (!offload_ctx) {
		IPA_UC_OFFLOAD_ERR("Invalid client handle %d\n", clnt_hdl);
		return -EINVAL;
	}

	if (offload_ctx->state != IPA_UC_OFFLOAD_STATE_DOWN) {
		IPA_UC_OFFLOAD_ERR("Invalid State %d\n", offload_ctx->state);
		return -EINVAL;
	}

	switch (offload_ctx->proto) {
	case IPA_UC_NTN:
		ret = ipa_uc_ntn_cleanup(offload_ctx);
		break;

	default:
		IPA_UC_OFFLOAD_ERR("Invalid Proto :%d\n", clnt_hdl);
		ret = -EINVAL;
		break;
	}

	if (!ret) {
		kfree(offload_ctx);
		offload_ctx = NULL;
		ipa_uc_offload_ctx[clnt_hdl] = NULL;
	}

	return ret;
}
EXPORT_SYMBOL(ipa_uc_offload_cleanup);
