// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2019, 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/ipa_uc_offload.h>
#include <linux/msm_ipa.h>
#include <linux/if_vlan.h>
#include "../ipa_common_i.h"
#include "../ipa_v3/ipa_pm.h"

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
	u32 pm_hdl;
	struct ipa_ntn_conn_in_params conn;
};

static struct ipa_uc_offload_ctx *ipa_uc_offload_ctx[IPA_UC_MAX_PROT_SIZE];


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

static void ipa_uc_offload_ntn_pm_cb(void *p, enum ipa_pm_cb_event event)
{
	/* suspend/resume is not supported */
	IPA_UC_OFFLOAD_DBG("event = %d\n", event);
}

static int ipa_uc_offload_ntn_register_pm_client(
	struct ipa_uc_offload_ctx *ntn_ctx)
{
	int res;
	struct ipa_pm_register_params params;

	memset(&params, 0, sizeof(params));
	params.name = "ETH";
	params.callback = ipa_uc_offload_ntn_pm_cb;
	params.user_data = ntn_ctx;
	params.group = IPA_PM_GROUP_DEFAULT;
	res = ipa_pm_register(&params, &ntn_ctx->pm_hdl);
	if (res) {
		IPA_UC_OFFLOAD_ERR("fail to register with PM %d\n", res);
		return res;
	}

	res = ipa_pm_associate_ipa_cons_to_client(ntn_ctx->pm_hdl,
		IPA_CLIENT_ETHERNET_CONS);
	if (res) {
		IPA_UC_OFFLOAD_ERR("fail to associate cons with PM %d\n", res);
		ipa_pm_deregister(ntn_ctx->pm_hdl);
		ntn_ctx->pm_hdl = ~0;
		return res;
	}

	return 0;
}

static void ipa_uc_offload_ntn_deregister_pm_client(
	struct ipa_uc_offload_ctx *ntn_ctx)
{
	ipa_pm_deactivate_sync(ntn_ctx->pm_hdl);
	ipa_pm_deregister(ntn_ctx->pm_hdl);
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
	int ret = 0;
	u32 len;
	bool is_vlan_mode;

	IPA_UC_OFFLOAD_DBG("register interface for netdev %s\n",
					 inp->netdev_name);
	ret = ipa_uc_offload_ntn_register_pm_client(ntn_ctx);
	if (ret) {
		IPA_UC_OFFLOAD_ERR("fail to register PM client\n");
		return -EFAULT;
	}
	memcpy(ntn_ctx->netdev_name, inp->netdev_name, IPA_RESOURCE_NAME_MAX);
	ntn_ctx->hdr_len = inp->hdr_info[0].hdr_len;
	ntn_ctx->notify = inp->notify;
	ntn_ctx->priv = inp->priv;

	/* add partial header */
	len = sizeof(struct ipa_ioc_add_hdr) + 2 * sizeof(struct ipa_hdr_add);
	hdr = kzalloc(len, GFP_KERNEL);
	if (hdr == NULL) {
		ret = -ENOMEM;
		goto fail_alloc;
	}

	ret = ipa_is_vlan_mode(IPA_VLAN_IF_ETH, &is_vlan_mode);
	if (ret) {
		IPA_UC_OFFLOAD_ERR("get vlan mode failed\n");
		goto fail;
	}

	if (is_vlan_mode) {
		if ((inp->hdr_info[0].hdr_type != IPA_HDR_L2_802_1Q) ||
			(inp->hdr_info[1].hdr_type != IPA_HDR_L2_802_1Q)) {
			IPA_UC_OFFLOAD_ERR(
				"hdr_type mismatch in vlan mode\n");
			WARN_ON_RATELIMIT_IPA(1);
			ret = -EFAULT;
			goto fail;
		}
		IPA_UC_OFFLOAD_DBG("vlan HEADER type compatible\n");

		if ((inp->hdr_info[0].hdr_len <
			(ETH_HLEN + VLAN_HLEN)) ||
			(inp->hdr_info[1].hdr_len <
			(ETH_HLEN + VLAN_HLEN))) {
			IPA_UC_OFFLOAD_ERR(
				"hdr_len shorter than vlan len (%u) (%u)\n"
				, inp->hdr_info[0].hdr_len
				, inp->hdr_info[1].hdr_len);
			WARN_ON_RATELIMIT_IPA(1);
			ret = -EFAULT;
			goto fail;
		}

		IPA_UC_OFFLOAD_DBG("vlan HEADER len compatible (%u) (%u)\n",
			inp->hdr_info[0].hdr_len,
			inp->hdr_info[1].hdr_len);
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
	tx_prop[0].dst_pipe = IPA_CLIENT_ETHERNET_CONS;
	tx_prop[0].hdr_l2_type = inp->hdr_info[0].hdr_type;
	memcpy(tx_prop[0].hdr_name, hdr->hdr[IPA_IP_v4].name,
		sizeof(tx_prop[0].hdr_name));

	tx_prop[1].ip = IPA_IP_v6;
	tx_prop[1].dst_pipe = IPA_CLIENT_ETHERNET_CONS;
	tx_prop[1].hdr_l2_type = inp->hdr_info[1].hdr_type;
	memcpy(tx_prop[1].hdr_name, hdr->hdr[IPA_IP_v6].name,
		sizeof(tx_prop[1].hdr_name));

	/* populate rx prop */
	rx.num_props = 2;
	rx.prop = rx_prop;

	memset(rx_prop, 0, sizeof(rx_prop));
	rx_prop[0].ip = IPA_IP_v4;
	rx_prop[0].src_pipe = IPA_CLIENT_ETHERNET_PROD;
	rx_prop[0].hdr_l2_type = inp->hdr_info[0].hdr_type;
	if (inp->is_meta_data_valid) {
		rx_prop[0].attrib.attrib_mask |= IPA_FLT_META_DATA;
		rx_prop[0].attrib.meta_data = inp->meta_data;
		rx_prop[0].attrib.meta_data_mask = inp->meta_data_mask;
	}

	rx_prop[1].ip = IPA_IP_v6;
	rx_prop[1].src_pipe = IPA_CLIENT_ETHERNET_PROD;
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
	ipa_uc_offload_ntn_deregister_pm_client(ntn_ctx);
	return ret;
}

int ipa_uc_offload_reg_intf(
	struct ipa_uc_offload_intf_params *inp,
	struct ipa_uc_offload_out_params *outp)
{
	struct ipa_uc_offload_ctx *ctx;
	int ret = 0;

	if (inp == NULL || outp == NULL) {
		IPA_UC_OFFLOAD_ERR("invalid params in=%pK out=%pK\n",
			inp, outp);
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


static int ipa_uc_ntn_alloc_conn_smmu_info(struct ipa_ntn_setup_info *dest,
	struct ipa_ntn_setup_info *source)
{
	int result;

	IPA_UC_OFFLOAD_DBG("Allocating smmu info\n");

	memcpy(dest, source, sizeof(struct ipa_ntn_setup_info));

	dest->data_buff_list =
		kcalloc(dest->num_buffers, sizeof(struct ntn_buff_smmu_map),
			GFP_KERNEL);
	if (dest->data_buff_list == NULL) {
		IPA_UC_OFFLOAD_ERR("failed to alloc smmu info\n");
		return -ENOMEM;
	}

	memcpy(dest->data_buff_list, source->data_buff_list,
		sizeof(struct ntn_buff_smmu_map) * dest->num_buffers);

	result = ipa_smmu_store_sgt(&dest->buff_pool_base_sgt,
		source->buff_pool_base_sgt);
	if (result) {
		kfree(dest->data_buff_list);
		return result;
	}

	result = ipa_smmu_store_sgt(&dest->ring_base_sgt,
		source->ring_base_sgt);
	if (result) {
		kfree(dest->data_buff_list);
		ipa_smmu_free_sgt(&dest->buff_pool_base_sgt);
		return result;
	}

	return 0;
}

static void ipa_uc_ntn_free_conn_smmu_info(struct ipa_ntn_setup_info *params)
{
	kfree(params->data_buff_list);
	ipa_smmu_free_sgt(&params->buff_pool_base_sgt);
	ipa_smmu_free_sgt(&params->ring_base_sgt);
}

int ipa_uc_ntn_conn_pipes(struct ipa_ntn_conn_in_params *inp,
			struct ipa_ntn_conn_out_params *outp,
			struct ipa_uc_offload_ctx *ntn_ctx)
{
	int result = 0;
	enum ipa_uc_offload_state prev_state;

	if (ntn_ctx->conn.dl.smmu_enabled != ntn_ctx->conn.ul.smmu_enabled) {
		IPA_UC_OFFLOAD_ERR("ul and dl smmu enablement do not match\n");
		return -EINVAL;
	}

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

	result = ipa_pm_activate_sync(ntn_ctx->pm_hdl);
	if (result) {
		IPA_UC_OFFLOAD_ERR("fail to activate: %d\n", result);
		return result;
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

	if (ntn_ctx->conn.dl.smmu_enabled) {
		result = ipa_uc_ntn_alloc_conn_smmu_info(&ntn_ctx->conn.dl,
			&inp->dl);
		if (result) {
			IPA_UC_OFFLOAD_ERR("alloc failure on TX\n");
			goto fail;
		}
		result = ipa_uc_ntn_alloc_conn_smmu_info(&ntn_ctx->conn.ul,
			&inp->ul);
		if (result) {
			ipa_uc_ntn_free_conn_smmu_info(&ntn_ctx->conn.dl);
			IPA_UC_OFFLOAD_ERR("alloc failure on RX\n");
			goto fail;
		}
	}

fail:
	return result;
}

int ipa_uc_offload_conn_pipes(struct ipa_uc_offload_conn_in_params *inp,
			struct ipa_uc_offload_conn_out_params *outp)
{
	int ret = 0;
	struct ipa_uc_offload_ctx *offload_ctx;

	if (!(inp && outp)) {
		IPA_UC_OFFLOAD_ERR("bad parm. in=%pK out=%pK\n", inp, outp);
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

	if (offload_ctx->state != IPA_UC_OFFLOAD_STATE_INITIALIZED) {
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
	if (!profile) {
		IPA_UC_OFFLOAD_ERR("Invalid input\n");
		return -EINVAL;
	}

	if (profile->client != IPA_CLIENT_ETHERNET_PROD &&
		profile->client != IPA_CLIENT_ETHERNET_CONS) {
		IPA_UC_OFFLOAD_ERR("not supported\n");
		return -EINVAL;
	}

	IPA_UC_OFFLOAD_DBG("setting throughput to %d\n",
		profile->max_supported_bw_mbps);

	return ipa_pm_set_throughput(
		ipa_uc_offload_ctx[IPA_UC_NTN]->pm_hdl,
		profile->max_supported_bw_mbps);
}
EXPORT_SYMBOL(ipa_set_perf_profile);

static int ipa_uc_ntn_disconn_pipes(struct ipa_uc_offload_ctx *ntn_ctx)
{
	int ipa_ep_idx_ul, ipa_ep_idx_dl;
	int ret = 0;

	if (ntn_ctx->conn.dl.smmu_enabled != ntn_ctx->conn.ul.smmu_enabled) {
		IPA_UC_OFFLOAD_ERR("ul and dl smmu enablement do not match\n");
		return -EINVAL;
	}

	ntn_ctx->state = IPA_UC_OFFLOAD_STATE_INITIALIZED;
	ret = ipa_pm_deactivate_sync(ntn_ctx->pm_hdl);
	if (ret) {
		IPA_UC_OFFLOAD_ERR("fail to deactivate res: %d\n",
			ret);
		return -EFAULT;
	}

	ipa_ep_idx_ul = ipa_get_ep_mapping(IPA_CLIENT_ETHERNET_PROD);
	ipa_ep_idx_dl = ipa_get_ep_mapping(IPA_CLIENT_ETHERNET_CONS);
	ret = ipa_tear_down_uc_offload_pipes(ipa_ep_idx_ul, ipa_ep_idx_dl,
		&ntn_ctx->conn);
	if (ret) {
		IPA_UC_OFFLOAD_ERR("fail to tear down ntn offload pipes, %d\n",
			ret);
		return -EFAULT;
	}
	if (ntn_ctx->conn.dl.smmu_enabled) {
		ipa_uc_ntn_free_conn_smmu_info(&ntn_ctx->conn.dl);
		ipa_uc_ntn_free_conn_smmu_info(&ntn_ctx->conn.ul);
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

	ipa_uc_offload_ntn_deregister_pm_client(ntn_ctx);

	len = sizeof(struct ipa_ioc_del_hdr) + 2 * sizeof(struct ipa_hdr_del);
	hdr = kzalloc(len, GFP_KERNEL);
	if (hdr == NULL)
		return -ENOMEM;

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

	if (offload_ctx->state != IPA_UC_OFFLOAD_STATE_INITIALIZED) {
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

/**
 * ipa_uc_offload_uc_rdyCB() - To register uC ready CB if uC not
 * ready
 * @inout:	[in/out] input/output parameters
 * from/to client
 *
 * Returns:	0 on success, negative on failure
 *
 */
int ipa_uc_offload_reg_rdyCB(struct ipa_uc_ready_params *inp)
{
	int ret = 0;

	if (!inp) {
		IPA_UC_OFFLOAD_ERR("Invalid input\n");
		return -EINVAL;
	}

	if (inp->proto == IPA_UC_NTN)
		ret = ipa_ntn_uc_reg_rdyCB(inp->notify, inp->priv);

	if (ret == -EEXIST) {
		inp->is_uC_ready = true;
		ret = 0;
	} else
		inp->is_uC_ready = false;

	return ret;
}
EXPORT_SYMBOL(ipa_uc_offload_reg_rdyCB);

void ipa_uc_offload_dereg_rdyCB(enum ipa_uc_offload_proto proto)
{
	if (proto == IPA_UC_NTN)
		ipa_ntn_uc_dereg_rdyCB();
}
EXPORT_SYMBOL(ipa_uc_offload_dereg_rdyCB);
