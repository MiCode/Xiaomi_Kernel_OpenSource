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
#include "../ipa_v3/ipa_pm.h"

#define OFFLOAD_DRV_NAME "ipa_wdi"
#define IPA_WDI_DBG(fmt, args...) \
	do { \
		pr_debug(OFFLOAD_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_WDI_DBG_LOW(fmt, args...) \
	do { \
		pr_debug(OFFLOAD_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_WDI_ERR(fmt, args...) \
	do { \
		pr_err(OFFLOAD_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

struct ipa_wdi_intf_info {
	char netdev_name[IPA_RESOURCE_NAME_MAX];
	u8 hdr_len;
	u32 partial_hdr_hdl[IPA_IP_MAX];
	struct list_head link;
};

struct ipa_wdi_context {
	struct list_head head_intf_list;
	struct completion wdi_completion;
	struct mutex lock;
	enum ipa_wdi_version wdi_version;
	u8 is_smmu_enabled;
	u32 tx_pipe_hdl;
	u32 rx_pipe_hdl;
	u8 num_sys_pipe_needed;
	u32 sys_pipe_hdl[IPA_WDI_MAX_SUPPORTED_SYS_PIPE];
	u32 ipa_pm_hdl;
#ifdef IPA_WAN_MSG_IPv6_ADDR_GW_LEN
	ipa_wdi_meter_notifier_cb wdi_notify;
#endif
};

static struct ipa_wdi_context *ipa_wdi_ctx;

int ipa_wdi_init(struct ipa_wdi_init_in_params *in,
	struct ipa_wdi_init_out_params *out)
{
	struct ipa_wdi_uc_ready_params uc_ready_params;
	struct ipa_smmu_in_params smmu_in;
	struct ipa_smmu_out_params smmu_out;

	if (ipa_wdi_ctx) {
		IPA_WDI_ERR("ipa_wdi_ctx was initialized before\n");
		return -EFAULT;
	}

	if (in->wdi_version > IPA_WDI_3 || in->wdi_version < IPA_WDI_1) {
		IPA_WDI_ERR("wrong wdi version: %d\n", in->wdi_version);
		return -EFAULT;
	}

	ipa_wdi_ctx = kzalloc(sizeof(*ipa_wdi_ctx), GFP_KERNEL);
	if (ipa_wdi_ctx == NULL) {
		IPA_WDI_ERR("fail to alloc wdi ctx\n");
		return -ENOMEM;
	}
	mutex_init(&ipa_wdi_ctx->lock);
	init_completion(&ipa_wdi_ctx->wdi_completion);
	INIT_LIST_HEAD(&ipa_wdi_ctx->head_intf_list);

	ipa_wdi_ctx->wdi_version = in->wdi_version;
	uc_ready_params.notify = in->notify;
	uc_ready_params.priv = in->priv;
#ifdef IPA_WAN_MSG_IPv6_ADDR_GW_LEN
	ipa_wdi_ctx->wdi_notify = in->wdi_notify;
#endif

	if (ipa_uc_reg_rdyCB(&uc_ready_params) != 0) {
		mutex_destroy(&ipa_wdi_ctx->lock);
		kfree(ipa_wdi_ctx);
		ipa_wdi_ctx = NULL;
		return -EFAULT;
	}

	out->is_uC_ready = uc_ready_params.is_uC_ready;

	smmu_in.smmu_client = IPA_SMMU_WLAN_CLIENT;
	if (ipa_get_smmu_params(&smmu_in, &smmu_out))
		out->is_smmu_enabled = false;
	else
		out->is_smmu_enabled = smmu_out.smmu_enable;

	ipa_wdi_ctx->is_smmu_enabled = out->is_smmu_enabled;

	return 0;
}
EXPORT_SYMBOL(ipa_wdi_init);

int ipa_wdi_cleanup(void)
{
	struct ipa_wdi_intf_info *entry;
	struct ipa_wdi_intf_info *next;

	/* clear interface list */
	list_for_each_entry_safe(entry, next,
		&ipa_wdi_ctx->head_intf_list, link) {
		list_del(&entry->link);
		kfree(entry);
	}
	mutex_destroy(&ipa_wdi_ctx->lock);
	kfree(ipa_wdi_ctx);
	ipa_wdi_ctx = NULL;
	return 0;
}
EXPORT_SYMBOL(ipa_wdi_cleanup);

static int ipa_wdi_commit_partial_hdr(
	struct ipa_ioc_add_hdr *hdr,
	const char *netdev_name,
	struct ipa_wdi_hdr_info *hdr_info)
{
	int i;

	if (!hdr || !hdr_info || !netdev_name) {
		IPA_WDI_ERR("Invalid input\n");
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
		IPA_WDI_ERR("fail to add partial headers\n");
		return -EFAULT;
	}

	return 0;
}

int ipa_wdi_reg_intf(struct ipa_wdi_reg_intf_in_params *in)
{
	struct ipa_ioc_add_hdr *hdr;
	struct ipa_wdi_intf_info *new_intf;
	struct ipa_wdi_intf_info *entry;
	struct ipa_tx_intf tx;
	struct ipa_rx_intf rx;
	struct ipa_ioc_tx_intf_prop tx_prop[2];
	struct ipa_ioc_rx_intf_prop rx_prop[2];
	u32 len;
	int ret = 0;

	if (in == NULL) {
		IPA_WDI_ERR("invalid params in=%pK\n", in);
		return -EINVAL;
	}

	if (!ipa_wdi_ctx) {
		IPA_WDI_ERR("wdi ctx is not initialized\n");
		return -EPERM;
	}

	IPA_WDI_DBG("register interface for netdev %s\n",
		in->netdev_name);

	mutex_lock(&ipa_wdi_ctx->lock);
	list_for_each_entry(entry, &ipa_wdi_ctx->head_intf_list, link)
		if (strcmp(entry->netdev_name, in->netdev_name) == 0) {
			IPA_WDI_DBG("intf was added before.\n");
			mutex_unlock(&ipa_wdi_ctx->lock);
			return 0;
		}

	IPA_WDI_DBG("intf was not added before, proceed.\n");
	new_intf = kzalloc(sizeof(*new_intf), GFP_KERNEL);
	if (new_intf == NULL) {
		IPA_WDI_ERR("fail to alloc new intf\n");
		mutex_unlock(&ipa_wdi_ctx->lock);
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
		IPA_WDI_ERR("fail to alloc %d bytes\n", len);
		ret = -EFAULT;
		goto fail_alloc_hdr;
	}

	if (ipa_wdi_commit_partial_hdr(hdr, in->netdev_name, in->hdr_info)) {
		IPA_WDI_ERR("fail to commit partial headers\n");
		ret = -EFAULT;
		goto fail_commit_hdr;
	}

	new_intf->partial_hdr_hdl[IPA_IP_v4] = hdr->hdr[IPA_IP_v4].hdr_hdl;
	new_intf->partial_hdr_hdl[IPA_IP_v6] = hdr->hdr[IPA_IP_v6].hdr_hdl;
	IPA_WDI_DBG("IPv4 hdr hdl: %d IPv6 hdr hdl: %d\n",
		hdr->hdr[IPA_IP_v4].hdr_hdl, hdr->hdr[IPA_IP_v6].hdr_hdl);

	/* populate tx prop */
	tx.num_props = 2;
	tx.prop = tx_prop;

	memset(tx_prop, 0, sizeof(tx_prop));
	tx_prop[0].ip = IPA_IP_v4;
	tx_prop[0].dst_pipe = IPA_CLIENT_WLAN1_CONS;
	tx_prop[0].alt_dst_pipe = in->alt_dst_pipe;
	tx_prop[0].hdr_l2_type = in->hdr_info[0].hdr_type;
	strlcpy(tx_prop[0].hdr_name, hdr->hdr[IPA_IP_v4].name,
		sizeof(tx_prop[0].hdr_name));

	tx_prop[1].ip = IPA_IP_v6;
	tx_prop[1].dst_pipe = IPA_CLIENT_WLAN1_CONS;
	tx_prop[1].alt_dst_pipe = in->alt_dst_pipe;
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
		IPA_WDI_ERR("fail to add interface prop\n");
		ret = -EFAULT;
		goto fail_commit_hdr;
	}

	list_add(&new_intf->link, &ipa_wdi_ctx->head_intf_list);
	init_completion(&ipa_wdi_ctx->wdi_completion);

	kfree(hdr);
	mutex_unlock(&ipa_wdi_ctx->lock);
	return 0;

fail_commit_hdr:
	kfree(hdr);
fail_alloc_hdr:
	kfree(new_intf);
	mutex_unlock(&ipa_wdi_ctx->lock);
	return ret;
}
EXPORT_SYMBOL(ipa_wdi_reg_intf);

int ipa_wdi_dereg_intf(const char *netdev_name)
{
	int len, ret = 0;
	struct ipa_ioc_del_hdr *hdr = NULL;
	struct ipa_wdi_intf_info *entry;
	struct ipa_wdi_intf_info *next;

	if (!netdev_name) {
		IPA_WDI_ERR("no netdev name.\n");
		return -EINVAL;
	}

	if (!ipa_wdi_ctx) {
		IPA_WDI_ERR("wdi ctx is not initialized.\n");
		return -EPERM;
	}

	mutex_lock(&ipa_wdi_ctx->lock);
	list_for_each_entry_safe(entry, next, &ipa_wdi_ctx->head_intf_list,
		link)
		if (strcmp(entry->netdev_name, netdev_name) == 0) {
			len = sizeof(struct ipa_ioc_del_hdr) +
				2 * sizeof(struct ipa_hdr_del);
			hdr = kzalloc(len, GFP_KERNEL);
			if (hdr == NULL) {
				IPA_WDI_ERR("fail to alloc %d bytes\n", len);
				mutex_unlock(&ipa_wdi_ctx->lock);
				return -ENOMEM;
			}

			hdr->commit = 1;
			hdr->num_hdls = 2;
			hdr->hdl[0].hdl = entry->partial_hdr_hdl[0];
			hdr->hdl[1].hdl = entry->partial_hdr_hdl[1];
			IPA_WDI_DBG("IPv4 hdr hdl: %d IPv6 hdr hdl: %d\n",
				hdr->hdl[0].hdl, hdr->hdl[1].hdl);

			if (ipa_del_hdr(hdr)) {
				IPA_WDI_ERR("fail to delete partial header\n");
				ret = -EFAULT;
				goto fail;
			}

			if (ipa_deregister_intf(entry->netdev_name)) {
				IPA_WDI_ERR("fail to del interface props\n");
				ret = -EFAULT;
				goto fail;
			}

			list_del(&entry->link);
			kfree(entry);

			break;
		}

fail:
	kfree(hdr);
	mutex_unlock(&ipa_wdi_ctx->lock);
	return ret;
}
EXPORT_SYMBOL(ipa_wdi_dereg_intf);

static void ipa_wdi_rm_notify(void *user_data, enum ipa_rm_event event,
		unsigned long data)
{
	if (!ipa_wdi_ctx) {
		IPA_WDI_ERR("Invalid context\n");
		return;
	}

	switch (event) {
	case IPA_RM_RESOURCE_GRANTED:
		complete_all(&ipa_wdi_ctx->wdi_completion);
		break;

	case IPA_RM_RESOURCE_RELEASED:
		break;

	default:
		IPA_WDI_ERR("Invalid RM Evt: %d", event);
		break;
	}
}

static int ipa_wdi_cons_release(void)
{
	return 0;
}

static int ipa_wdi_cons_request(void)
{
	int ret = 0;

	if (!ipa_wdi_ctx) {
		IPA_WDI_ERR("wdi ctx is not initialized\n");
		ret = -EFAULT;
	}

	return ret;
}

static void ipa_wdi_pm_cb(void *p, enum ipa_pm_cb_event event)
{
	IPA_WDI_DBG("received pm event %d\n", event);
}

int ipa_wdi_conn_pipes(struct ipa_wdi_conn_in_params *in,
			struct ipa_wdi_conn_out_params *out)
{
	int i, j, ret = 0;
	struct ipa_rm_create_params param;
	struct ipa_pm_register_params pm_params;
	struct ipa_wdi_in_params in_tx;
	struct ipa_wdi_in_params in_rx;
	struct ipa_wdi_out_params out_tx;
	struct ipa_wdi_out_params out_rx;

	if (!(in && out)) {
		IPA_WDI_ERR("empty parameters. in=%pK out=%pK\n", in, out);
		return -EINVAL;
	}

	if (!ipa_wdi_ctx) {
		IPA_WDI_ERR("wdi ctx is not initialized\n");
		return -EPERM;
	}

	if (in->num_sys_pipe_needed > IPA_WDI_MAX_SUPPORTED_SYS_PIPE) {
		IPA_WDI_ERR("ipa can only support up to %d sys pipe\n",
			IPA_WDI_MAX_SUPPORTED_SYS_PIPE);
		return -EINVAL;
	}
	ipa_wdi_ctx->num_sys_pipe_needed = in->num_sys_pipe_needed;
	IPA_WDI_DBG("number of sys pipe %d\n", in->num_sys_pipe_needed);

	/* setup sys pipe when needed */
	for (i = 0; i < ipa_wdi_ctx->num_sys_pipe_needed; i++) {
		ret = ipa_setup_sys_pipe(&in->sys_in[i],
			&ipa_wdi_ctx->sys_pipe_hdl[i]);
		if (ret) {
			IPA_WDI_ERR("fail to setup sys pipe %d\n", i);
			ret = -EFAULT;
			goto fail_setup_sys_pipe;
		}
	}

	if (!ipa_pm_is_used()) {
		memset(&param, 0, sizeof(param));
		param.name = IPA_RM_RESOURCE_WLAN_PROD;
		param.reg_params.user_data = ipa_wdi_ctx;
		param.reg_params.notify_cb = ipa_wdi_rm_notify;
		param.floor_voltage = IPA_VOLTAGE_SVS;
		ret = ipa_rm_create_resource(&param);
		if (ret) {
			IPA_WDI_ERR("fail to create WLAN_PROD resource\n");
			ret = -EFAULT;
			goto fail_setup_sys_pipe;
		}

		memset(&param, 0, sizeof(param));
		param.name = IPA_RM_RESOURCE_WLAN_CONS;
		param.request_resource = ipa_wdi_cons_request;
		param.release_resource = ipa_wdi_cons_release;
		ret = ipa_rm_create_resource(&param);
		if (ret) {
			IPA_WDI_ERR("fail to create WLAN_CONS resource\n");
			goto fail_create_rm_cons;
		}

		if (ipa_rm_add_dependency(IPA_RM_RESOURCE_WLAN_PROD,
			IPA_RM_RESOURCE_APPS_CONS)) {
			IPA_WDI_ERR("fail to add rm dependency\n");
			ret = -EFAULT;
			goto fail_add_dependency;
		}
	} else {
		memset(&pm_params, 0, sizeof(pm_params));
		pm_params.name = "wdi";
		pm_params.callback = ipa_wdi_pm_cb;
		pm_params.user_data = NULL;
		pm_params.group = IPA_PM_GROUP_DEFAULT;
		if (ipa_pm_register(&pm_params, &ipa_wdi_ctx->ipa_pm_hdl)) {
			IPA_WDI_ERR("fail to register ipa pm\n");
			ret = -EFAULT;
			goto fail_setup_sys_pipe;
		}
	}

	if (ipa_wdi_ctx->wdi_version == IPA_WDI_3) {
		if (ipa_conn_wdi_pipes(in, out, ipa_wdi_ctx->wdi_notify)) {
			IPA_WDI_ERR("fail to setup wdi pipes\n");
			ret = -EFAULT;
			goto fail_connect_pipe;
		}
	} else {
		memset(&in_tx, 0, sizeof(in_tx));
		memset(&in_rx, 0, sizeof(in_rx));
		memset(&out_tx, 0, sizeof(out_tx));
		memset(&out_rx, 0, sizeof(out_rx));
#ifdef IPA_WAN_MSG_IPv6_ADDR_GW_LEN
		in_rx.wdi_notify = ipa_wdi_ctx->wdi_notify;
#endif
		if (in->is_smmu_enabled == false) {
			/* firsr setup rx pipe */
			in_rx.sys.ipa_ep_cfg = in->u_rx.rx.ipa_ep_cfg;
			in_rx.sys.client = in->u_rx.rx.client;
			in_rx.sys.notify = in->notify;
			in_rx.sys.priv = in->priv;
			in_rx.smmu_enabled = in->is_smmu_enabled;
			in_rx.u.ul.rdy_ring_base_pa =
				in->u_rx.rx.transfer_ring_base_pa;
			in_rx.u.ul.rdy_ring_size =
				in->u_rx.rx.transfer_ring_size;
			in_rx.u.ul.rdy_ring_rp_pa =
				in->u_rx.rx.transfer_ring_doorbell_pa;
			in_rx.u.ul.rdy_comp_ring_base_pa =
				in->u_rx.rx.event_ring_base_pa;
			in_rx.u.ul.rdy_comp_ring_wp_pa =
				in->u_rx.rx.event_ring_doorbell_pa;
			in_rx.u.ul.rdy_comp_ring_size =
				in->u_rx.rx.event_ring_size;
			if (ipa_connect_wdi_pipe(&in_rx, &out_rx)) {
				IPA_WDI_ERR("fail to setup rx pipe\n");
				ret = -EFAULT;
				goto fail_connect_pipe;
			}
			ipa_wdi_ctx->rx_pipe_hdl = out_rx.clnt_hdl;
			out->rx_uc_db_pa = out_rx.uc_door_bell_pa;
			IPA_WDI_DBG("rx uc db pa: 0x%pad\n", &out->rx_uc_db_pa);

			/* then setup tx pipe */
			in_tx.sys.ipa_ep_cfg = in->u_tx.tx.ipa_ep_cfg;
			in_tx.sys.client = in->u_tx.tx.client;
			in_tx.smmu_enabled = in->is_smmu_enabled;
			in_tx.u.dl.comp_ring_base_pa =
				in->u_tx.tx.transfer_ring_base_pa;
			in_tx.u.dl.comp_ring_size =
				in->u_tx.tx.transfer_ring_size;
			in_tx.u.dl.ce_ring_base_pa =
				in->u_tx.tx.event_ring_base_pa;
			in_tx.u.dl.ce_door_bell_pa =
				in->u_tx.tx.event_ring_doorbell_pa;
			in_tx.u.dl.ce_ring_size =
				in->u_tx.tx.event_ring_size;
			in_tx.u.dl.num_tx_buffers =
				in->u_tx.tx.num_pkt_buffers;
			if (ipa_connect_wdi_pipe(&in_tx, &out_tx)) {
				IPA_WDI_ERR("fail to setup tx pipe\n");
				ret = -EFAULT;
				goto fail;
			}
			ipa_wdi_ctx->tx_pipe_hdl = out_tx.clnt_hdl;
			out->tx_uc_db_pa = out_tx.uc_door_bell_pa;
			IPA_WDI_DBG("tx uc db pa: 0x%pad\n", &out->tx_uc_db_pa);
		} else { /* smmu is enabled */
			/* firsr setup rx pipe */
			in_rx.sys.ipa_ep_cfg = in->u_rx.rx_smmu.ipa_ep_cfg;
			in_rx.sys.client = in->u_rx.rx_smmu.client;
			in_rx.sys.notify = in->notify;
			in_rx.sys.priv = in->priv;
			in_rx.smmu_enabled = in->is_smmu_enabled;
			in_rx.u.ul_smmu.rdy_ring =
				in->u_rx.rx_smmu.transfer_ring_base;
			in_rx.u.ul_smmu.rdy_ring_size =
				in->u_rx.rx_smmu.transfer_ring_size;
			in_rx.u.ul_smmu.rdy_ring_rp_pa =
				in->u_rx.rx_smmu.transfer_ring_doorbell_pa;
			in_rx.u.ul_smmu.rdy_comp_ring =
				in->u_rx.rx_smmu.event_ring_base;
			in_rx.u.ul_smmu.rdy_comp_ring_wp_pa =
				in->u_rx.rx_smmu.event_ring_doorbell_pa;
			in_rx.u.ul_smmu.rdy_comp_ring_size =
				in->u_rx.rx_smmu.event_ring_size;
			if (ipa_connect_wdi_pipe(&in_rx, &out_rx)) {
				IPA_WDI_ERR("fail to setup rx pipe\n");
				ret = -EFAULT;
				goto fail_connect_pipe;
			}
			ipa_wdi_ctx->rx_pipe_hdl = out_rx.clnt_hdl;
			out->rx_uc_db_pa = out_rx.uc_door_bell_pa;
			IPA_WDI_DBG("rx uc db pa: 0x%pad\n", &out->rx_uc_db_pa);

			/* then setup tx pipe */
			in_tx.sys.ipa_ep_cfg = in->u_tx.tx_smmu.ipa_ep_cfg;
			in_tx.sys.client = in->u_tx.tx_smmu.client;
			in_tx.smmu_enabled = in->is_smmu_enabled;
			in_tx.u.dl_smmu.comp_ring =
				in->u_tx.tx_smmu.transfer_ring_base;
			in_tx.u.dl_smmu.comp_ring_size =
				in->u_tx.tx_smmu.transfer_ring_size;
			in_tx.u.dl_smmu.ce_ring =
				in->u_tx.tx_smmu.event_ring_base;
			in_tx.u.dl_smmu.ce_door_bell_pa =
				in->u_tx.tx_smmu.event_ring_doorbell_pa;
			in_tx.u.dl_smmu.ce_ring_size =
				in->u_tx.tx_smmu.event_ring_size;
			in_tx.u.dl_smmu.num_tx_buffers =
				in->u_tx.tx_smmu.num_pkt_buffers;
			if (ipa_connect_wdi_pipe(&in_tx, &out_tx)) {
				IPA_WDI_ERR("fail to setup tx pipe\n");
				ret = -EFAULT;
				goto fail;
			}
			ipa_wdi_ctx->tx_pipe_hdl = out_tx.clnt_hdl;
			out->tx_uc_db_pa = out_tx.uc_door_bell_pa;
			IPA_WDI_DBG("tx uc db pa: 0x%pad\n", &out->tx_uc_db_pa);
		}
	}

	return 0;

fail:
	ipa_disconnect_wdi_pipe(ipa_wdi_ctx->rx_pipe_hdl);
fail_connect_pipe:
	if (!ipa_pm_is_used())
		ipa_rm_delete_dependency(IPA_RM_RESOURCE_WLAN_PROD,
			IPA_RM_RESOURCE_APPS_CONS);
	else
		ipa_pm_deregister(ipa_wdi_ctx->ipa_pm_hdl);
fail_add_dependency:
	if (!ipa_pm_is_used())
		ipa_rm_delete_resource(IPA_RM_RESOURCE_WLAN_CONS);
fail_create_rm_cons:
	if (!ipa_pm_is_used())
		ipa_rm_delete_resource(IPA_RM_RESOURCE_WLAN_PROD);
fail_setup_sys_pipe:
	for (j = 0; j < i; j++)
		ipa_teardown_sys_pipe(ipa_wdi_ctx->sys_pipe_hdl[j]);
	return ret;
}
EXPORT_SYMBOL(ipa_wdi_conn_pipes);

int ipa_wdi_disconn_pipes(void)
{
	int i, ipa_ep_idx_rx, ipa_ep_idx_tx;

	if (!ipa_wdi_ctx) {
		IPA_WDI_ERR("wdi ctx is not initialized\n");
		return -EPERM;
	}

	/* tear down sys pipe if needed */
	for (i = 0; i < ipa_wdi_ctx->num_sys_pipe_needed; i++) {
		if (ipa_teardown_sys_pipe(ipa_wdi_ctx->sys_pipe_hdl[i])) {
			IPA_WDI_ERR("fail to tear down sys pipe %d\n", i);
			return -EFAULT;
		}
	}

	ipa_ep_idx_rx = ipa_get_ep_mapping(IPA_CLIENT_WLAN1_PROD);
	ipa_ep_idx_tx = ipa_get_ep_mapping(IPA_CLIENT_WLAN1_CONS);

	if (ipa_wdi_ctx->wdi_version == IPA_WDI_3) {
		if (ipa_disconn_wdi_pipes(ipa_ep_idx_rx, ipa_ep_idx_tx)) {
			IPA_WDI_ERR("fail to tear down wdi pipes\n");
			return -EFAULT;
		}
	} else {
		if (ipa_disconnect_wdi_pipe(ipa_wdi_ctx->tx_pipe_hdl)) {
			IPA_WDI_ERR("fail to tear down wdi tx pipes\n");
			return -EFAULT;
		}
		if (ipa_disconnect_wdi_pipe(ipa_wdi_ctx->rx_pipe_hdl)) {
			IPA_WDI_ERR("fail to tear down wdi rx pipes\n");
			return -EFAULT;
		}
	}

	if (!ipa_pm_is_used()) {
		if (ipa_rm_delete_dependency(IPA_RM_RESOURCE_WLAN_PROD,
					IPA_RM_RESOURCE_APPS_CONS)) {
			IPA_WDI_ERR("fail to delete rm dependency\n");
			return -EFAULT;
		}

		if (ipa_rm_delete_resource(IPA_RM_RESOURCE_WLAN_PROD)) {
			IPA_WDI_ERR("fail to delete WLAN_PROD resource\n");
			return -EFAULT;
		}

		if (ipa_rm_delete_resource(IPA_RM_RESOURCE_WLAN_CONS)) {
			IPA_WDI_ERR("fail to delete WLAN_CONS resource\n");
			return -EFAULT;
		}
	} else {
		if (ipa_pm_deregister(ipa_wdi_ctx->ipa_pm_hdl)) {
			IPA_WDI_ERR("fail to deregister ipa pm\n");
			return -EFAULT;
		}
	}

	return 0;
}
EXPORT_SYMBOL(ipa_wdi_disconn_pipes);

int ipa_wdi_enable_pipes(void)
{
	int ret;
	int ipa_ep_idx_tx, ipa_ep_idx_rx;

	if (!ipa_wdi_ctx) {
		IPA_WDI_ERR("wdi ctx is not initialized.\n");
		return -EPERM;
	}

	ipa_ep_idx_rx = ipa_get_ep_mapping(IPA_CLIENT_WLAN1_PROD);
	ipa_ep_idx_tx = ipa_get_ep_mapping(IPA_CLIENT_WLAN1_CONS);

	if (ipa_wdi_ctx->wdi_version == IPA_WDI_3) {
		if (ipa_enable_wdi_pipes(ipa_ep_idx_tx, ipa_ep_idx_rx)) {
			IPA_WDI_ERR("fail to enable wdi pipes\n");
			return -EFAULT;
		}
	} else {
		if (ipa_enable_wdi_pipe(ipa_wdi_ctx->tx_pipe_hdl)) {
			IPA_WDI_ERR("fail to enable wdi tx pipe\n");
			return -EFAULT;
		}
		if (ipa_resume_wdi_pipe(ipa_wdi_ctx->tx_pipe_hdl)) {
			IPA_WDI_ERR("fail to resume wdi tx pipe\n");
			return -EFAULT;
		}
		if (ipa_enable_wdi_pipe(ipa_wdi_ctx->rx_pipe_hdl)) {
			IPA_WDI_ERR("fail to enable wdi rx pipe\n");
			return -EFAULT;
		}
		if (ipa_resume_wdi_pipe(ipa_wdi_ctx->rx_pipe_hdl)) {
			IPA_WDI_ERR("fail to resume wdi rx pipe\n");
			return -EFAULT;
		}
	}

	if (!ipa_pm_is_used()) {
		ret = ipa_rm_request_resource(IPA_RM_RESOURCE_WLAN_PROD);
		if (ret == -EINPROGRESS) {
			if (wait_for_completion_timeout(
				&ipa_wdi_ctx->wdi_completion, 10*HZ) == 0) {
				IPA_WDI_ERR("WLAN_PROD res req time out\n");
				return -EFAULT;
			}
		} else if (ret != 0) {
			IPA_WDI_ERR("fail to request resource\n");
			return -EFAULT;
		}
	} else {
		ret = ipa_pm_activate_sync(ipa_wdi_ctx->ipa_pm_hdl);
		if (ret) {
			IPA_WDI_ERR("fail to activate ipa pm\n");
			return -EFAULT;
		}
	}

	return 0;
}
EXPORT_SYMBOL(ipa_wdi_enable_pipes);

int ipa_wdi_disable_pipes(void)
{
	int ret;
	int ipa_ep_idx_tx, ipa_ep_idx_rx;

	if (!ipa_wdi_ctx) {
		IPA_WDI_ERR("wdi ctx is not initialized.\n");
		return -EPERM;
	}

	ipa_ep_idx_rx = ipa_get_ep_mapping(IPA_CLIENT_WLAN1_PROD);
	ipa_ep_idx_tx = ipa_get_ep_mapping(IPA_CLIENT_WLAN1_CONS);

	if (ipa_wdi_ctx->wdi_version == IPA_WDI_3) {
		if (ipa_disable_wdi_pipes(ipa_ep_idx_tx, ipa_ep_idx_rx)) {
			IPA_WDI_ERR("fail to disable wdi pipes\n");
			return -EFAULT;
		}
	} else {
		if (ipa_suspend_wdi_pipe(ipa_wdi_ctx->tx_pipe_hdl)) {
			IPA_WDI_ERR("fail to suspend wdi tx pipe\n");
			return -EFAULT;
		}
		if (ipa_disable_wdi_pipe(ipa_wdi_ctx->tx_pipe_hdl)) {
			IPA_WDI_ERR("fail to disable wdi tx pipe\n");
			return -EFAULT;
		}
		if (ipa_suspend_wdi_pipe(ipa_wdi_ctx->rx_pipe_hdl)) {
			IPA_WDI_ERR("fail to suspend wdi rx pipe\n");
			return -EFAULT;
		}
		if (ipa_disable_wdi_pipe(ipa_wdi_ctx->rx_pipe_hdl)) {
			IPA_WDI_ERR("fail to disable wdi rx pipe\n");
			return -EFAULT;
		}
	}

	if (!ipa_pm_is_used()) {
		ret = ipa_rm_release_resource(IPA_RM_RESOURCE_WLAN_PROD);
		if (ret != 0) {
			IPA_WDI_ERR("fail to release resource\n");
			return -EFAULT;
		}
	} else {
		ret = ipa_pm_deactivate_sync(ipa_wdi_ctx->ipa_pm_hdl);
		if (ret) {
			IPA_WDI_ERR("fail to deactivate ipa pm\n");
			return -EFAULT;
		}
	}

	return 0;
}
EXPORT_SYMBOL(ipa_wdi_disable_pipes);

int ipa_wdi_set_perf_profile(struct ipa_wdi_perf_profile *profile)
{
	struct ipa_rm_perf_profile rm_profile;
	enum ipa_rm_resource_name resource_name;

	if (profile == NULL) {
		IPA_WDI_ERR("Invalid input\n");
		return -EINVAL;
	}

	if (!ipa_pm_is_used()) {
		rm_profile.max_supported_bandwidth_mbps =
			profile->max_supported_bw_mbps;

		if (profile->client == IPA_CLIENT_WLAN1_PROD) {
			resource_name = IPA_RM_RESOURCE_WLAN_PROD;
		} else if (profile->client == IPA_CLIENT_WLAN1_CONS) {
			resource_name = IPA_RM_RESOURCE_WLAN_CONS;
		} else {
			IPA_WDI_ERR("not supported\n");
			return -EINVAL;
		}

		if (ipa_rm_set_perf_profile(resource_name, &rm_profile)) {
			IPA_WDI_ERR("fail to setup rm perf profile\n");
			return -EFAULT;
		}
	} else {
		if (ipa_pm_set_throughput(ipa_wdi_ctx->ipa_pm_hdl,
			profile->max_supported_bw_mbps)) {
			IPA_WDI_ERR("fail to setup pm perf profile\n");
			return -EFAULT;
		}
	}

	return 0;
}
EXPORT_SYMBOL(ipa_wdi_set_perf_profile);

int ipa_wdi_create_smmu_mapping(u32 num_buffers,
	struct ipa_wdi_buffer_info *info)
{
	return ipa_create_wdi_mapping(num_buffers, info);
}
EXPORT_SYMBOL(ipa_wdi_create_smmu_mapping);

int ipa_wdi_release_smmu_mapping(u32 num_buffers,
	struct ipa_wdi_buffer_info *info)
{
	return ipa_release_wdi_mapping(num_buffers, info);
}
EXPORT_SYMBOL(ipa_wdi_release_smmu_mapping);

int ipa_wdi_get_stats(struct IpaHwStatsWDIInfoData_t *stats)
{
	return ipa_get_wdi_stats(stats);
}
EXPORT_SYMBOL(ipa_wdi_get_stats);
