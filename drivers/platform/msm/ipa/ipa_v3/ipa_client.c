/* Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
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
#include <asm/barrier.h>
#include <linux/delay.h>
#include <linux/device.h>
#include "ipa_i.h"
#include "linux/msm_gsi.h"

/*
 * These values were determined empirically and shows good E2E bi-
 * directional throughputs
 */
#define IPA_POLL_AGGR_STATE_RETRIES_NUM 3
#define IPA_POLL_AGGR_STATE_SLEEP_MSEC 1

#define IPA_PKT_FLUSH_TO_US 100

#define IPA_POLL_FOR_EMPTINESS_NUM 50
#define IPA_POLL_FOR_EMPTINESS_SLEEP_USEC 20
#define IPA_CHANNEL_STOP_IN_PROC_TO_MSEC 5
#define IPA_CHANNEL_STOP_IN_PROC_SLEEP_USEC 200

/* xfer_rsc_idx should be 7 bits */
#define IPA_XFER_RSC_IDX_MAX 127

static int ipa3_is_xdci_channel_empty(struct ipa3_ep_context *ep,
	bool *is_empty);
static void ipa3_start_gsi_debug_monitor(u32 clnt_hdl);

int ipa3_enable_data_path(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep = &ipa3_ctx->ep[clnt_hdl];
	struct ipa_ep_cfg_holb holb_cfg;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;
	int res = 0;
	struct ipahal_reg_endp_init_rsrc_grp rsrc_grp;

	/* Assign the resource group for pipe */
	memset(&rsrc_grp, 0, sizeof(rsrc_grp));
	rsrc_grp.rsrc_grp = ipa_get_ep_group(ep->client);
	if (rsrc_grp.rsrc_grp == -1) {
		IPAERR("invalid group for client %d\n", ep->client);
		WARN_ON(1);
		return -EFAULT;
	}

	IPADBG("Setting group %d for pipe %d\n",
		rsrc_grp.rsrc_grp, clnt_hdl);
	ipahal_write_reg_n_fields(IPA_ENDP_INIT_RSRC_GRP_n, clnt_hdl,
		&rsrc_grp);

	IPADBG("Enabling data path\n");
	if (IPA_CLIENT_IS_CONS(ep->client)) {
		memset(&holb_cfg, 0, sizeof(holb_cfg));
		/*
		 * Set HOLB on USB DPL CONS to avoid IPA stall
		 * if DPL client is not pulling the data
		 * on other end from IPA hw.
		 */
		if ((ep->client == IPA_CLIENT_USB_DPL_CONS) ||
				(ep->client == IPA_CLIENT_MHI_DPL_CONS) ||
				(ep->client == IPA_CLIENT_MHI_QDSS_CONS)) {
			holb_cfg.tmr_val = 0;
			holb_cfg.en = IPA_HOLB_TMR_EN;
		} else if ((ipa3_ctx->ipa_hw_type == IPA_HW_v4_2) &&
			(ep->client == IPA_CLIENT_WLAN1_CONS ||
				ep->client == IPA_CLIENT_USB_CONS)) {
			holb_cfg.en = IPA_HOLB_TMR_EN;
			holb_cfg.tmr_val = IPA_HOLB_TMR_VAL;
		} else {
			holb_cfg.tmr_val = 0;
			holb_cfg.en = IPA_HOLB_TMR_DIS;
		}
		res = ipa3_cfg_ep_holb(clnt_hdl, &holb_cfg);
	}

	/* Enable the pipe */
	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0) {
		if (IPA_CLIENT_IS_CONS(ep->client) &&
		    (ep->keep_ipa_awake ||
		    ipa3_ctx->resume_on_connect[ep->client] ||
		    !ipa3_should_pipe_be_suspended(ep->client))) {
			memset(&ep_cfg_ctrl, 0, sizeof(ep_cfg_ctrl));
			ep_cfg_ctrl.ipa_ep_suspend = false;
			res = ipa3_cfg_ep_ctrl(clnt_hdl, &ep_cfg_ctrl);
		}
	}

	return res;
}

int ipa3_disable_data_path(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep = &ipa3_ctx->ep[clnt_hdl];
	struct ipa_ep_cfg_holb holb_cfg;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;
	struct ipa_ep_cfg_aggr ep_aggr;
	int res = 0;

	IPADBG("Disabling data path\n");
	if (IPA_CLIENT_IS_CONS(ep->client)) {
		memset(&holb_cfg, 0, sizeof(holb_cfg));
		holb_cfg.en = IPA_HOLB_TMR_EN;
		holb_cfg.tmr_val = 0;
		res = ipa3_cfg_ep_holb(clnt_hdl, &holb_cfg);
	}

	/*
	 * for IPA 4.0 and above aggregation frame is closed together with
	 * channel STOP
	 */
	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0) {
		/* Suspend the pipe */
		if (IPA_CLIENT_IS_CONS(ep->client)) {
			memset(&ep_cfg_ctrl, 0, sizeof(struct ipa_ep_cfg_ctrl));
			ep_cfg_ctrl.ipa_ep_suspend = true;
			res = ipa3_cfg_ep_ctrl(clnt_hdl, &ep_cfg_ctrl);
		}

		udelay(IPA_PKT_FLUSH_TO_US);
		ipahal_read_reg_n_fields(IPA_ENDP_INIT_AGGR_n, clnt_hdl,
			&ep_aggr);
		if (ep_aggr.aggr_en) {
			res = ipa3_tag_aggr_force_close(clnt_hdl);
			if (res) {
				IPAERR("tag process timeout client:%d err:%d\n",
					clnt_hdl, res);
				ipa_assert();
			}
		}
	}

	return res;
}

int ipa3_reset_gsi_channel(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep;
	int result = -EFAULT;
	enum gsi_status gsi_res;
	bool undo_aggr_value = false;
	struct ipahal_reg_clkon_cfg fields;

	IPADBG("entry\n");
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("Bad parameter\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	/*
	 * IPAv4.0 HW has a limitation where WSEQ in MBIM NTH header is not
	 * reset to 0 when MBIM pipe is reset. Workaround is to disable
	 * HW clock gating for AGGR block using IPA_CLKON_CFG reg. undo flag to
	 * disable the bit after reset is finished
	 */
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0) {
		if (ep->cfg.aggr.aggr == IPA_MBIM_16 &&
			ep->cfg.aggr.aggr_en != IPA_BYPASS_AGGR) {
			ipahal_read_reg_fields(IPA_CLKON_CFG, &fields);
			if (fields.open_aggr_wrapper == true) {
				undo_aggr_value = true;
				fields.open_aggr_wrapper = false;
				ipahal_write_reg_fields(IPA_CLKON_CFG, &fields);
			}
		}
	}

	/*
	 * Reset channel
	 * If the reset called after stop, need to wait 1ms
	 */
	msleep(IPA_POLL_AGGR_STATE_SLEEP_MSEC);
	gsi_res = gsi_reset_channel(ep->gsi_chan_hdl);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error resetting channel: %d\n", gsi_res);
		result = -EFAULT;
		goto reset_chan_fail;
	}

	/* undo the aggr value if flag was set above*/
	if (undo_aggr_value) {
		fields.open_aggr_wrapper = false;
		ipahal_write_reg_fields(IPA_CLKON_CFG, &fields);
	}

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	IPADBG("exit\n");
	return 0;

reset_chan_fail:
	/* undo the aggr value if flag was set above*/
	if (undo_aggr_value) {
		fields.open_aggr_wrapper = false;
		ipahal_write_reg_fields(IPA_CLKON_CFG, &fields);
	}

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	return result;
}

int ipa3_reset_gsi_event_ring(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep;
	int result = -EFAULT;
	enum gsi_status gsi_res;

	IPADBG("entry\n");
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("Bad parameter.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));
	/* Reset event ring */
	gsi_res = gsi_reset_evt_ring(ep->gsi_evt_ring_hdl);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error resetting event: %d\n", gsi_res);
		result = -EFAULT;
		goto reset_evt_fail;
	}

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	IPADBG("exit\n");
	return 0;

reset_evt_fail:
	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	return result;
}

static bool ipa3_is_legal_params(struct ipa_request_gsi_channel_params *params)
{
	if (params->client >= IPA_CLIENT_MAX)
		return false;
	else
		return true;
}

static void ipa3_start_gsi_debug_monitor(u32 clnt_hdl)
{
	struct IpaHwOffloadStatsAllocCmdData_t *gsi_info;
	struct ipa3_ep_context *ep;
	enum ipa_client_type client_type;

	IPADBG("entry\n");
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("Bad parameters.\n");
		return;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];
	client_type = ipa3_get_client_mapping(clnt_hdl);

	/* start uC gsi dbg stats monitor */
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_5 ||
		(ipa3_ctx->ipa_hw_type == IPA_HW_v4_1 &&
		ipa3_ctx->platform_type == IPA_PLAT_TYPE_APQ)) {
		switch (client_type) {
		case IPA_CLIENT_MHI_PRIME_TETH_PROD:
			gsi_info = &ipa3_ctx->gsi_info[IPA_HW_PROTOCOL_MHIP];
			gsi_info->ch_id_info[0].ch_id = ep->gsi_chan_hdl;
			gsi_info->ch_id_info[0].dir = DIR_PRODUCER;
			ipa3_uc_debug_stats_alloc(*gsi_info);
			break;
		case IPA_CLIENT_MHI_PRIME_TETH_CONS:
			gsi_info = &ipa3_ctx->gsi_info[IPA_HW_PROTOCOL_MHIP];
			gsi_info->ch_id_info[1].ch_id = ep->gsi_chan_hdl;
			gsi_info->ch_id_info[1].dir = DIR_CONSUMER;
			ipa3_uc_debug_stats_alloc(*gsi_info);
			break;
		case IPA_CLIENT_MHI_PRIME_RMNET_PROD:
			gsi_info = &ipa3_ctx->gsi_info[IPA_HW_PROTOCOL_MHIP];
			gsi_info->ch_id_info[2].ch_id = ep->gsi_chan_hdl;
			gsi_info->ch_id_info[2].dir = DIR_PRODUCER;
			ipa3_uc_debug_stats_alloc(*gsi_info);
			break;
		case IPA_CLIENT_MHI_PRIME_RMNET_CONS:
			gsi_info = &ipa3_ctx->gsi_info[IPA_HW_PROTOCOL_MHIP];
			gsi_info->ch_id_info[3].ch_id = ep->gsi_chan_hdl;
			gsi_info->ch_id_info[3].dir = DIR_CONSUMER;
			ipa3_uc_debug_stats_alloc(*gsi_info);
			break;
		case IPA_CLIENT_USB_PROD:
			gsi_info = &ipa3_ctx->gsi_info[IPA_HW_PROTOCOL_USB];
			gsi_info->ch_id_info[0].ch_id = ep->gsi_chan_hdl;
			gsi_info->ch_id_info[0].dir = DIR_PRODUCER;
			ipa3_uc_debug_stats_alloc(*gsi_info);
			break;
		case IPA_CLIENT_USB_CONS:
			gsi_info = &ipa3_ctx->gsi_info[IPA_HW_PROTOCOL_USB];
			gsi_info->ch_id_info[1].ch_id = ep->gsi_chan_hdl;
			gsi_info->ch_id_info[1].dir = DIR_CONSUMER;
			ipa3_uc_debug_stats_alloc(*gsi_info);
			break;
		default:
			IPADBG("client_type %d not supported\n",
				client_type);
		}
	}
}

int ipa3_smmu_map_peer_reg(phys_addr_t phys_addr, bool map,
	enum ipa_smmu_cb_type cb_type)
{
	struct iommu_domain *smmu_domain;
	int res;

	if (cb_type >= IPA_SMMU_CB_MAX) {
		IPAERR("invalid cb_type\n");
		return -EINVAL;
	}

	if (ipa3_ctx->s1_bypass_arr[cb_type]) {
		IPADBG("CB# %d is set to s1 bypass\n", cb_type);
		return 0;
	}

	smmu_domain = ipa3_get_smmu_domain_by_type(cb_type);
	if (!smmu_domain) {
		IPAERR("invalid smmu domain\n");
		return -EINVAL;
	}

	if (map) {
		res = ipa3_iommu_map(smmu_domain, phys_addr, phys_addr,
			PAGE_SIZE, IOMMU_READ | IOMMU_WRITE | IOMMU_MMIO);
	} else {
		res = iommu_unmap(smmu_domain, phys_addr, PAGE_SIZE);
		res = (res != PAGE_SIZE);
	}
	if (res) {
		IPAERR("Fail to %s reg 0x%pa\n", map ? "map" : "unmap",
			&phys_addr);
		return -EINVAL;
	}

	IPADBG("Peer reg 0x%pa %s\n", &phys_addr, map ? "map" : "unmap");

	return 0;
}

int ipa3_smmu_map_peer_buff(u64 iova, u32 size, bool map, struct sg_table *sgt,
	enum ipa_smmu_cb_type cb_type)
{
	struct iommu_domain *smmu_domain;
	int res;
	phys_addr_t phys;
	unsigned long va;
	struct scatterlist *sg;
	int count = 0;
	size_t len;
	int i;
	struct page *page;

	if (cb_type >= IPA_SMMU_CB_MAX) {
		IPAERR("invalid cb_type\n");
		return -EINVAL;
	}

	if (ipa3_ctx->s1_bypass_arr[cb_type]) {
		IPADBG("CB# %d is set to s1 bypass\n", cb_type);
		return 0;
	}

	smmu_domain = ipa3_get_smmu_domain_by_type(cb_type);
	if (!smmu_domain) {
		IPAERR("invalid smmu domain\n");
		return -EINVAL;
	}

	/*
	 * USB GSI driver would update sgt irrespective of USB S1
	 * is enable or bypass.
	 * If USB S1 is enabled using IOMMU, iova != pa.
	 * If USB S1 is bypass, iova == pa.
	 */
	if (map) {
		if (sgt != NULL) {
			va = rounddown(iova, PAGE_SIZE);
			for_each_sg(sgt->sgl, sg, sgt->nents, i) {
				page = sg_page(sg);
				phys = page_to_phys(page);
				len = PAGE_ALIGN(sg->offset + sg->length);
				res = ipa3_iommu_map(smmu_domain, va, phys,
					len, IOMMU_READ | IOMMU_WRITE);
				if (res) {
					IPAERR("Fail to map pa=%pa\n", &phys);
					return -EINVAL;
				}
				va += len;
				count++;
			}
		} else {
			res = ipa3_iommu_map(smmu_domain,
				rounddown(iova, PAGE_SIZE),
				rounddown(iova, PAGE_SIZE),
				roundup(size + iova -
					rounddown(iova, PAGE_SIZE),
				PAGE_SIZE),
				IOMMU_READ | IOMMU_WRITE);
			if (res) {
				IPAERR("Fail to map 0x%llx\n", iova);
				return -EINVAL;
			}
		}
	} else {
		res = iommu_unmap(smmu_domain,
		rounddown(iova, PAGE_SIZE),
		roundup(size + iova - rounddown(iova, PAGE_SIZE),
		PAGE_SIZE));
		if (res != roundup(size + iova - rounddown(iova, PAGE_SIZE),
			PAGE_SIZE)) {
			IPAERR("Fail to unmap 0x%llx\n", iova);
			return -EINVAL;
		}
	}
	IPADBG("Peer buff %s 0x%llx\n", map ? "map" : "unmap", iova);
	return 0;
}

static enum ipa_client_cb_type ipa_get_client_cb_type(
					enum ipa_client_type client_type)
{
	enum ipa_client_cb_type client_cb;

	if (client_type == IPA_CLIENT_USB_PROD ||
			client_type == IPA_CLIENT_USB_CONS) {
		IPADBG("USB Client registered\n");
		client_cb = IPA_USB_CLNT;
	} else if (client_type == IPA_CLIENT_MHI_PROD ||
			client_type == IPA_CLIENT_MHI_CONS) {
		IPADBG("MHI Client registered\n");
		client_cb = IPA_MHI_CLNT;
	} else {
		IPAERR("Invalid IPA client\n");
		client_cb = IPA_MAX_CLNT;
	}

	return client_cb;
}
void ipa3_register_client_callback(int (*client_cb)(bool is_lock),
				bool (*teth_port_state)(void),
				enum ipa_client_type client_type)
{
	enum ipa_client_cb_type client;

	IPADBG("entry\n");

	client = ipa_get_client_cb_type(client_type);
	if (client == IPA_MAX_CLNT)
		return;

	if (client_cb == NULL) {
		IPAERR("Bad Param");
		return;
	}

	if (!ipa3_ctx->client_lock_unlock[client])
		ipa3_ctx->client_lock_unlock[client] = client_cb;
	if (!ipa3_ctx->get_teth_port_state[client])
		ipa3_ctx->get_teth_port_state[client] = teth_port_state;
	IPADBG("exit\n");
}

void ipa3_deregister_client_callback(enum ipa_client_type client_type)
{
	enum ipa_client_cb_type client_cb;

	IPADBG("entry\n");

	client_cb = ipa_get_client_cb_type(client_type);
	if (client_cb == IPA_MAX_CLNT)
		return;

	if (ipa3_ctx->client_lock_unlock[client_cb] == NULL &&
		ipa3_ctx->get_teth_port_state[client_cb] == NULL) {
		IPAERR("client_lock_unlock is already NULL");
		return;
	}

	ipa3_ctx->client_lock_unlock[client_cb] = NULL;
	ipa3_ctx->get_teth_port_state[client_cb] = NULL;
	IPADBG("exit\n");
}

static void client_lock_unlock_cb(enum ipa_client_type client, bool is_lock)
{
	enum ipa_client_cb_type client_cb;

	IPADBG("entry\n");

	client_cb = ipa_get_client_cb_type(client);
	if (client_cb == IPA_MAX_CLNT)
		return;

	if (ipa3_ctx->client_lock_unlock[client_cb])
		ipa3_ctx->client_lock_unlock[client_cb](is_lock);

	IPADBG("exit\n");
}

int ipa3_request_gsi_channel(struct ipa_request_gsi_channel_params *params,
			     struct ipa_req_chan_out_params *out_params)
{
	int ipa_ep_idx;
	int result = -EFAULT;
	struct ipa3_ep_context *ep;
	struct ipahal_reg_ep_cfg_status ep_status;
	unsigned long gsi_dev_hdl;
	enum gsi_status gsi_res;
	const struct ipa_gsi_ep_config *gsi_ep_cfg_ptr;

	IPADBG("entry\n");
	if (params == NULL || out_params == NULL ||
		!ipa3_is_legal_params(params)) {
		IPAERR("bad parameters\n");
		return -EINVAL;
	}

	ipa_ep_idx = ipa3_get_ep_mapping(params->client);
	if (ipa_ep_idx == -1) {
		IPAERR("fail to alloc EP.\n");
		goto fail;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];

	if (ep->valid) {
		IPAERR("EP already allocated.\n");
		goto fail;
	}

	memset(&ipa3_ctx->ep[ipa_ep_idx], 0, sizeof(struct ipa3_ep_context));
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	ep->skip_ep_cfg = params->skip_ep_cfg;
	ep->valid = 1;
	ep->client = params->client;
	ep->client_notify = params->notify;
	ep->priv = params->priv;
	ep->keep_ipa_awake = params->keep_ipa_awake;


	/* Config QMB for USB_CONS ep */
	if (!IPA_CLIENT_IS_PROD(ep->client)) {
		IPADBG("Configuring QMB on USB CONS pipe\n");
		if (ipa_ep_idx >= ipa3_ctx->ipa_num_pipes ||
			ipa3_ctx->ep[ipa_ep_idx].valid == 0) {
			IPAERR("bad parm.\n");
			return -EINVAL;
		}
		result = ipa3_cfg_ep_cfg(ipa_ep_idx, &params->ipa_ep_cfg.cfg);
		if (result) {
			IPAERR("fail to configure QMB.\n");
			return result;
		}
	}

	if (!ep->skip_ep_cfg) {
		if (ipa3_cfg_ep(ipa_ep_idx, &params->ipa_ep_cfg)) {
			IPAERR("fail to configure EP.\n");
			goto ipa_cfg_ep_fail;
		}
		/* Setting EP status 0 */
		memset(&ep_status, 0, sizeof(ep_status));
		if (ipa3_cfg_ep_status(ipa_ep_idx, &ep_status)) {
			IPAERR("fail to configure status of EP.\n");
			goto ipa_cfg_ep_fail;
		}
		IPADBG("ep configuration successful\n");
	} else {
		IPADBG("Skipping endpoint configuration.\n");
		if (IPA_CLIENT_IS_PROD(ipa3_ctx->ep[ipa_ep_idx].client) &&
			ipa3_ctx->ep[ipa_ep_idx].client == IPA_CLIENT_USB_PROD
			&& !ipa3_is_mhip_offload_enabled()) {
			if (ipa3_cfg_ep_seq(ipa_ep_idx,
						&params->ipa_ep_cfg.seq)) {
				IPAERR("fail to configure USB pipe seq\n");
				goto ipa_cfg_ep_fail;
			}
		}
	}

	out_params->clnt_hdl = ipa_ep_idx;

	result = ipa3_enable_data_path(out_params->clnt_hdl);
	if (result) {
		IPAERR("enable data path failed res=%d clnt=%d.\n", result,
				out_params->clnt_hdl);
		goto ipa_cfg_ep_fail;
	}

	gsi_dev_hdl = ipa3_ctx->gsi_dev_hdl;
	gsi_res = gsi_alloc_evt_ring(&params->evt_ring_params, gsi_dev_hdl,
		&ep->gsi_evt_ring_hdl);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error allocating event ring: %d\n", gsi_res);
		result = -EFAULT;
		goto ipa_cfg_ep_fail;
	}

	gsi_res = gsi_write_evt_ring_scratch(ep->gsi_evt_ring_hdl,
		params->evt_scratch);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error writing event ring scratch: %d\n", gsi_res);
		result = -EFAULT;
		goto write_evt_scratch_fail;
	}

	gsi_ep_cfg_ptr = ipa3_get_gsi_ep_info(ep->client);
	if (gsi_ep_cfg_ptr == NULL) {
		IPAERR("Error ipa3_get_gsi_ep_info ret NULL\n");
		result = -EFAULT;
		goto write_evt_scratch_fail;
	}

	params->chan_params.evt_ring_hdl = ep->gsi_evt_ring_hdl;
	params->chan_params.ch_id = gsi_ep_cfg_ptr->ipa_gsi_chan_num;
	params->chan_params.prefetch_mode = gsi_ep_cfg_ptr->prefetch_mode;
	params->chan_params.empty_lvl_threshold =
		gsi_ep_cfg_ptr->prefetch_threshold;
	gsi_res = gsi_alloc_channel(&params->chan_params, gsi_dev_hdl,
		&ep->gsi_chan_hdl);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error allocating channel: %d, chan_id: %d\n", gsi_res,
			params->chan_params.ch_id);
		result = -EFAULT;
		goto write_evt_scratch_fail;
	}

	memcpy(&ep->chan_scratch, &params->chan_scratch,
		sizeof(union __packed gsi_channel_scratch));

	/*
	 * Update scratch for MCS smart prefetch:
	 * Starting IPA4.5, smart prefetch implemented by H/W.
	 * At IPA 4.0/4.1/4.2, we do not use MCS smart prefetch
	 *  so keep the fields zero.
	 */
	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0) {
		ep->chan_scratch.xdci.max_outstanding_tre =
		params->chan_params.re_size * gsi_ep_cfg_ptr->ipa_if_tlv;
	}

	gsi_res = gsi_write_channel_scratch(ep->gsi_chan_hdl,
		params->chan_scratch);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error writing channel scratch: %d\n", gsi_res);
		result = -EFAULT;
		goto write_chan_scratch_fail;
	}

	gsi_res = gsi_query_channel_db_addr(ep->gsi_chan_hdl,
		&out_params->db_reg_phs_addr_lsb,
		&out_params->db_reg_phs_addr_msb);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error querying channel DB registers addresses: %d\n",
			gsi_res);
		result = -EFAULT;
		goto write_chan_scratch_fail;
	}

	ep->gsi_mem_info.evt_ring_len = params->evt_ring_params.ring_len;
	ep->gsi_mem_info.evt_ring_base_addr =
		params->evt_ring_params.ring_base_addr;
	ep->gsi_mem_info.evt_ring_base_vaddr =
		params->evt_ring_params.ring_base_vaddr;
	ep->gsi_mem_info.chan_ring_len = params->chan_params.ring_len;
	ep->gsi_mem_info.chan_ring_base_addr =
		params->chan_params.ring_base_addr;
	ep->gsi_mem_info.chan_ring_base_vaddr =
		params->chan_params.ring_base_vaddr;

	ipa3_ctx->skip_ep_cfg_shadow[ipa_ep_idx] = ep->skip_ep_cfg;
	if (!ep->skip_ep_cfg && IPA_CLIENT_IS_PROD(params->client))
		ipa3_install_dflt_flt_rules(ipa_ep_idx);

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	IPADBG("client %d (ep: %d) connected\n", params->client, ipa_ep_idx);
	IPADBG("exit\n");

	return 0;

write_chan_scratch_fail:
	gsi_dealloc_channel(ep->gsi_chan_hdl);
write_evt_scratch_fail:
	gsi_dealloc_evt_ring(ep->gsi_evt_ring_hdl);
ipa_cfg_ep_fail:
	memset(&ipa3_ctx->ep[ipa_ep_idx], 0, sizeof(struct ipa3_ep_context));
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
fail:
	return result;
}

int ipa3_set_usb_max_packet_size(
	enum ipa_usb_max_usb_packet_size usb_max_packet_size)
{
	struct gsi_device_scratch dev_scratch;
	enum gsi_status gsi_res;

	IPADBG("entry\n");

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	memset(&dev_scratch, 0, sizeof(struct gsi_device_scratch));
	dev_scratch.mhi_base_chan_idx_valid = false;
	dev_scratch.max_usb_pkt_size_valid = true;
	dev_scratch.max_usb_pkt_size = usb_max_packet_size;

	gsi_res = gsi_write_device_scratch(ipa3_ctx->gsi_dev_hdl,
		&dev_scratch);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error writing device scratch: %d\n", gsi_res);
		return -EFAULT;
	}
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	IPADBG("exit\n");
	return 0;
}

/* This function called as part of usb pipe resume */
int ipa3_xdci_connect(u32 clnt_hdl)
{
	int result;
	struct ipa3_ep_context *ep;

	IPADBG("entry\n");

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("Bad parameter.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];
	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	result = ipa3_start_gsi_channel(clnt_hdl);
	if (result) {
		IPAERR("failed to start gsi channel clnt_hdl=%u\n", clnt_hdl);
		goto exit;
	}

	result = ipa3_enable_data_path(clnt_hdl);
	if (result) {
		IPAERR("enable data path failed res=%d clnt_hdl=%d.\n", result,
			clnt_hdl);
		goto stop_ch;
	}

	IPADBG("exit\n");
	goto exit;

stop_ch:
	(void)ipa3_stop_gsi_channel(clnt_hdl);
exit:
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	return result;
}


/* This function called as part of usb pipe connect */
int ipa3_xdci_start(u32 clnt_hdl, u8 xferrscidx, bool xferrscidx_valid)
{
	struct ipa3_ep_context *ep;
	int result = -EFAULT;
	enum gsi_status gsi_res;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;
	int code = 0;

	IPADBG("entry\n");
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes  ||
		ipa3_ctx->ep[clnt_hdl].valid == 0 ||
		xferrscidx > IPA_XFER_RSC_IDX_MAX) {
		IPAERR("Bad parameters.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];
	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	if (xferrscidx_valid) {
		ep->chan_scratch.xdci.xferrscidx = xferrscidx;
		gsi_res = gsi_write_channel_scratch(ep->gsi_chan_hdl,
			ep->chan_scratch);
		if (gsi_res != GSI_STATUS_SUCCESS) {
			IPAERR("Error writing channel scratch: %d\n", gsi_res);
			goto write_chan_scratch_fail;
		}
	}

	if (IPA_CLIENT_IS_PROD(ep->client) && ep->skip_ep_cfg) {
		memset(&ep_cfg_ctrl, 0, sizeof(struct ipa_ep_cfg_ctrl));
		ep_cfg_ctrl.ipa_ep_delay = true;
		ep->ep_delay_set = true;

		result = ipa3_cfg_ep_ctrl(clnt_hdl, &ep_cfg_ctrl);
		if (result)
			IPAERR("client (ep: %d) failed result=%d\n",
			clnt_hdl, result);
		else
			IPADBG("client (ep: %d) success\n", clnt_hdl);
	} else {
		ep->ep_delay_set = false;
	}

	gsi_res = gsi_start_channel(ep->gsi_chan_hdl);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error starting channel: %d\n", gsi_res);
		goto write_chan_scratch_fail;
	}

	if (IPA_CLIENT_IS_PROD(ep->client) && ep->skip_ep_cfg &&
			ipa3_ctx->ipa_endp_delay_wa &&
			!ipa3_is_mhip_offload_enabled()) {
		gsi_res = gsi_enable_flow_control_ee(ep->gsi_chan_hdl, 0,
									&code);
		if (gsi_res == GSI_STATUS_SUCCESS) {
			IPADBG("flow control sussess gsi ch %d with code %d\n",
					ep->gsi_chan_hdl, code);
		} else {
			IPADBG("failed to flow control gsi ch %d code %d\n",
					ep->gsi_chan_hdl, code);
		}
	}
	ipa3_start_gsi_debug_monitor(clnt_hdl);
	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	IPADBG("exit\n");
	return 0;

write_chan_scratch_fail:
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	return result;
}

int ipa3_get_gsi_chan_info(struct gsi_chan_info *gsi_chan_info,
	unsigned long chan_hdl)
{
	enum gsi_status gsi_res;

	memset(gsi_chan_info, 0, sizeof(struct gsi_chan_info));
	gsi_res = gsi_query_channel_info(chan_hdl, gsi_chan_info);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error querying channel info: %d\n", gsi_res);
		return -EFAULT;
	}
	if (!gsi_chan_info->evt_valid) {
		IPAERR("Event info invalid\n");
		return -EFAULT;
	}

	return 0;
}

static bool ipa3_is_xdci_channel_with_given_info_empty(
	struct ipa3_ep_context *ep, struct gsi_chan_info *chan_info)
{
	bool is_empty = false;

	if (!IPA_CLIENT_IS_CONS(ep->client)) {
		/* For UL channel: chan.RP == chan.WP */
		is_empty = (chan_info->rp == chan_info->wp);
	} else {
		/* For DL channel: */
		if (chan_info->wp !=
		    (ep->gsi_mem_info.chan_ring_base_addr +
		     ep->gsi_mem_info.chan_ring_len -
		     GSI_CHAN_RE_SIZE_16B)) {
			/*  if chan.WP != LINK TRB: chan.WP == evt.RP */
			is_empty = (chan_info->wp == chan_info->evt_rp);
		} else {
			/*
			 * if chan.WP == LINK TRB: chan.base_xfer_ring_addr
			 * == evt.RP
			 */
			is_empty = (ep->gsi_mem_info.chan_ring_base_addr ==
				chan_info->evt_rp);
		}
	}

	return is_empty;
}

static int ipa3_is_xdci_channel_empty(struct ipa3_ep_context *ep,
	bool *is_empty)
{
	struct gsi_chan_info chan_info;
	int res;

	if (!ep || !is_empty || !ep->valid) {
		IPAERR("Input Error\n");
		return -EFAULT;
	}

	res = ipa3_get_gsi_chan_info(&chan_info, ep->gsi_chan_hdl);
	if (res) {
		IPAERR("Failed to get GSI channel info\n");
		return -EFAULT;
	}

	*is_empty = ipa3_is_xdci_channel_with_given_info_empty(ep, &chan_info);

	return 0;
}

int ipa3_enable_force_clear(u32 request_id, bool throttle_source,
	u32 source_pipe_bitmask)
{
	struct ipa_enable_force_clear_datapath_req_msg_v01 req;
	int result;

	if (ipa3_ctx->platform_type == IPA_PLAT_TYPE_APQ) {
		IPADBG("APQ platform - ignore force clear\n");
		return 0;
	}

	memset(&req, 0, sizeof(req));
	req.request_id = request_id;
	req.source_pipe_bitmask = source_pipe_bitmask;
	if (throttle_source) {
		req.throttle_source_valid = 1;
		req.throttle_source = 1;
	}
	result = ipa3_qmi_enable_force_clear_datapath_send(&req);
	if (result) {
		IPAERR("ipa3_qmi_enable_force_clear_datapath_send failed %d\n",
			result);
		return result;
	}

	return 0;
}

int ipa3_disable_force_clear(u32 request_id)
{
	struct ipa_disable_force_clear_datapath_req_msg_v01 req;
	int result;

	if (ipa3_ctx->platform_type == IPA_PLAT_TYPE_APQ) {
		IPADBG("APQ platform - ignore force clear\n");
		return 0;
	}

	memset(&req, 0, sizeof(req));
	req.request_id = request_id;
	result = ipa3_qmi_disable_force_clear_datapath_send(&req);
	if (result) {
		IPAERR("ipa3_qmi_disable_force_clear_datapath_send failed %d\n",
			result);
		return result;
	}

	return 0;
}

/* Clocks should be voted before invoking this function */
static int ipa3_xdci_stop_gsi_channel(u32 clnt_hdl, bool *stop_in_proc)
{
	int res;

	IPADBG("entry\n");
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0 ||
		!stop_in_proc) {
		IPAERR("Bad parameter.\n");
		return -EINVAL;
	}

	res = ipa3_stop_gsi_channel(clnt_hdl);
	if (res != 0 && res != -GSI_STATUS_AGAIN &&
		res != -GSI_STATUS_TIMED_OUT) {
		IPAERR("xDCI stop channel failed res=%d\n", res);
		return -EFAULT;
	}

	if (res)
		*stop_in_proc = true;
	else
		*stop_in_proc = false;

	IPADBG("xDCI channel is %s (result=%d)\n",
		res ? "STOP_IN_PROC/TimeOut" : "STOP", res);

	IPADBG("exit\n");
	return 0;
}

/* Clocks should be voted before invoking this function */
static int ipa3_xdci_stop_gsi_ch_brute_force(u32 clnt_hdl,
	bool *stop_in_proc)
{
	unsigned long jiffies_start;
	unsigned long jiffies_timeout =
		msecs_to_jiffies(IPA_CHANNEL_STOP_IN_PROC_TO_MSEC);
	int res;

	IPADBG("entry\n");
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0 ||
		!stop_in_proc) {
		IPAERR("Bad parameter.\n");
		return -EINVAL;
	}

	jiffies_start = jiffies;
	while (1) {
		res = ipa3_xdci_stop_gsi_channel(clnt_hdl,
			stop_in_proc);
		if (res) {
			IPAERR("failed to stop xDCI channel hdl=%d\n",
				clnt_hdl);
			return res;
		}

		if (!*stop_in_proc) {
			IPADBG("xDCI channel STOP hdl=%d\n", clnt_hdl);
			return res;
		}

		/*
		 * Give chance to the previous stop request to be accomplished
		 * before the retry
		 */
		udelay(IPA_CHANNEL_STOP_IN_PROC_SLEEP_USEC);

		if (time_after(jiffies, jiffies_start + jiffies_timeout)) {
			IPADBG("timeout waiting for xDCI channel emptiness\n");
			return res;
		}
	}
}

/* Clocks should be voted for before invoking this function */
static int ipa3_stop_ul_chan_with_data_drain(u32 qmi_req_id,
		u32 source_pipe_bitmask, bool should_force_clear, u32 clnt_hdl,
		bool remove_delay)
{
	int result;
	bool is_empty = false;
	int i;
	bool stop_in_proc;
	struct ipa3_ep_context *ep;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;

	IPADBG("entry\n");

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("Bad parameter.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];

	/* first try to stop the channel */
	result = ipa3_xdci_stop_gsi_ch_brute_force(clnt_hdl,
			&stop_in_proc);
	if (result) {
		IPAERR("fail to stop UL channel - hdl=%d clnt=%d\n",
			clnt_hdl, ep->client);
		goto exit;
	}
	if (!stop_in_proc)
		goto exit;

	/* Remove delay only if stop channel success*/
	if (remove_delay && ep->ep_delay_set == true && !stop_in_proc) {
		memset(&ep_cfg_ctrl, 0, sizeof(struct ipa_ep_cfg_ctrl));
		ep_cfg_ctrl.ipa_ep_delay = false;
		result = ipa3_cfg_ep_ctrl(clnt_hdl,
			&ep_cfg_ctrl);
		if (result) {
			IPAERR
			("client (ep: %d) failed to remove delay result=%d\n",
				clnt_hdl, result);
		} else {
			IPADBG("client (ep: %d) delay removed\n",
				clnt_hdl);
			ep->ep_delay_set = false;
		}
	}

	/* if stop_in_proc, lets wait for emptiness */
	for (i = 0; i < IPA_POLL_FOR_EMPTINESS_NUM; i++) {
		result = ipa3_is_xdci_channel_empty(ep, &is_empty);
		if (result)
			goto exit;
		if (is_empty)
			break;
		udelay(IPA_POLL_FOR_EMPTINESS_SLEEP_USEC);
	}
	/* In case of empty, lets try to stop the channel again */
	if (is_empty) {
		result = ipa3_xdci_stop_gsi_ch_brute_force(clnt_hdl,
			&stop_in_proc);
		if (result) {
			IPAERR("fail to stop UL channel - hdl=%d clnt=%d\n",
				clnt_hdl, ep->client);
			goto exit;
		}
		if (!stop_in_proc)
			goto exit;
	}
	/* if still stop_in_proc or not empty, activate force clear */
	if (should_force_clear) {
		result = ipa3_enable_force_clear(qmi_req_id, false,
			source_pipe_bitmask);
		if (result) {
			struct ipahal_ep_cfg_ctrl_scnd ep_ctrl_scnd = { 0 };

			/*
			 * assuming here modem SSR\shutdown, AP can remove
			 * the delay in this case
			 */
			IPAERR(
				"failed to force clear %d, remove delay from SCND reg\n"
				, result);
			ep_ctrl_scnd.endp_delay = false;
			ipahal_write_reg_n_fields(
				IPA_ENDP_INIT_CTRL_SCND_n, clnt_hdl,
				&ep_ctrl_scnd);
		}
	}
	/* with force clear, wait for emptiness */
	for (i = 0; i < IPA_POLL_FOR_EMPTINESS_NUM; i++) {
		result = ipa3_is_xdci_channel_empty(ep, &is_empty);
		if (result)
			goto disable_force_clear_and_exit;
		if (is_empty)
			break;

		udelay(IPA_POLL_FOR_EMPTINESS_SLEEP_USEC);
	}
	/* try to stop for the last time */
	result = ipa3_xdci_stop_gsi_ch_brute_force(clnt_hdl,
		&stop_in_proc);
	if (result) {
		IPAERR("fail to stop UL channel - hdl=%d clnt=%d\n",
			clnt_hdl, ep->client);
		goto disable_force_clear_and_exit;
	}
	result = stop_in_proc ? -EFAULT : 0;

disable_force_clear_and_exit:
	if (should_force_clear)
		ipa3_disable_force_clear(qmi_req_id);
exit:
	if (remove_delay && ep->ep_delay_set == true && !stop_in_proc) {
		memset(&ep_cfg_ctrl, 0, sizeof(struct ipa_ep_cfg_ctrl));
		ep_cfg_ctrl.ipa_ep_delay = false;
		result = ipa3_cfg_ep_ctrl(clnt_hdl,
			&ep_cfg_ctrl);
		if (result) {
			IPAERR
			("client (ep: %d) failed to remove delay result=%d\n",
				clnt_hdl, result);
		} else {
			IPADBG("client (ep: %d) delay removed\n",
				clnt_hdl);
			ep->ep_delay_set = false;
		}
	}
	IPADBG("exit\n");
	return result;
}

/*
 * Set reset ep_delay for CLIENT PROD pipe
 * Clocks, should be voted before calling this API
 * locks should be taken before calling this API
 */

int ipa3_set_reset_client_prod_pipe_delay(bool set_reset,
		enum ipa_client_type client)
{
	int result = 0;
	int pipe_idx;
	struct ipa3_ep_context *ep;
	struct ipa_ep_cfg_ctrl ep_ctrl;

	memset(&ep_ctrl, 0, sizeof(struct ipa_ep_cfg_ctrl));
	ep_ctrl.ipa_ep_delay = set_reset;

	if (IPA_CLIENT_IS_CONS(client)) {
		IPAERR("client (%d) not PROD\n", client);
		return -EINVAL;
	}

	pipe_idx = ipa3_get_ep_mapping(client);

	if (pipe_idx == IPA_EP_NOT_ALLOCATED) {
		IPAERR("client (%d) not valid\n", client);
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[pipe_idx];

	/* Setting delay on USB_PROD with skip_ep_cfg */
	client_lock_unlock_cb(client, true);
	if (ep->valid && ep->skip_ep_cfg) {
		ep->ep_delay_set = ep_ctrl.ipa_ep_delay;
		result = ipa3_cfg_ep_ctrl(pipe_idx, &ep_ctrl);
		if (result)
			IPAERR("client (ep: %d) failed result=%d\n",
				pipe_idx, result);
		else
			IPADBG("client (ep: %d) success\n", pipe_idx);
	}
	client_lock_unlock_cb(client, false);
	return result;
}

static bool ipa3_get_teth_port_status(enum ipa_client_type client)
{
	enum ipa_client_cb_type client_cb;

	client_cb = ipa_get_client_cb_type(client);
	if (client_cb == IPA_MAX_CLNT)
		return false;
	if (ipa3_ctx->get_teth_port_state[client_cb])
		return ipa3_ctx->get_teth_port_state[client_cb]();
	return false;
}

/*
 * Start/stop the CLIENT PROD pipes in SSR scenarios
 */

int ipa3_start_stop_client_prod_gsi_chnl(enum ipa_client_type client,
		bool start_chnl)
{
	int result = 0;
	int pipe_idx;
	struct ipa3_ep_context *ep;
	int code = 0;

	if (IPA_CLIENT_IS_CONS(client)) {
		IPAERR("client (%d) not PROD\n", client);
		return -EINVAL;
	}

	pipe_idx = ipa3_get_ep_mapping(client);

	if (pipe_idx == IPA_EP_NOT_ALLOCATED) {
		IPAERR("client (%d) not valid\n", client);
		return -EINVAL;
	}

	client_lock_unlock_cb(client, true);
	ep = &ipa3_ctx->ep[pipe_idx];
	if (ep->valid && ep->skip_ep_cfg && ipa3_get_teth_port_status(client)
		&& !ipa3_is_mhip_offload_enabled()) {
		if (start_chnl) {
			result = ipa3_start_gsi_channel(pipe_idx);
			result = gsi_enable_flow_control_ee(ep->gsi_chan_hdl,
								0, &code);
			if (result == GSI_STATUS_SUCCESS) {
				IPADBG("flow control sussess ch %d code %d\n",
						ep->gsi_chan_hdl, code);
			} else {
				IPADBG("failed to flow control ch %d code %d\n",
						ep->gsi_chan_hdl, code);
			}
		} else
			result = ipa3_stop_gsi_channel(pipe_idx);
	}
	client_lock_unlock_cb(client, false);
	return result;
}
int ipa3_set_reset_client_cons_pipe_sus_holb(bool set_reset,
		enum ipa_client_type client)
{
	int pipe_idx;
	struct ipa3_ep_context *ep;
	struct ipa_ep_cfg_ctrl ep_suspend;
	struct ipa_ep_cfg_holb ep_holb;

	memset(&ep_suspend, 0, sizeof(ep_suspend));
	memset(&ep_holb, 0, sizeof(ep_holb));

	ep_suspend.ipa_ep_suspend = set_reset;
	ep_holb.tmr_val = 0;
	ep_holb.en = set_reset;

	if (IPA_CLIENT_IS_PROD(client)) {
		IPAERR("client (%d) not CONS\n", client);
		return -EINVAL;
	}

	pipe_idx = ipa3_get_ep_mapping(client);

	if (pipe_idx == IPA_EP_NOT_ALLOCATED) {
		IPAERR("client (%d) not valid\n", client);
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[pipe_idx];
	/* Setting sus/holb on MHI_CONS with skip_ep_cfg */
	client_lock_unlock_cb(client, true);
	if (ep->valid && ep->skip_ep_cfg) {
		if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0)
			ipahal_write_reg_n_fields(
					IPA_ENDP_INIT_CTRL_n,
					pipe_idx, &ep_suspend);
		/*
		 * ipa3_cfg_ep_holb is not used here because we are
		 * setting HOLB on Q6 pipes, and from APPS perspective
		 * they are not valid, therefore, the above function
		 * will fail.
		 */
		ipahal_write_reg_n_fields(
			IPA_ENDP_INIT_HOL_BLOCK_TIMER_n,
			pipe_idx, &ep_holb);
		ipahal_write_reg_n_fields(
			IPA_ENDP_INIT_HOL_BLOCK_EN_n,
			pipe_idx, &ep_holb);

		/* IPA4.5 issue requires HOLB_EN to be written twice */
		if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_5)
			ipahal_write_reg_n_fields(
				IPA_ENDP_INIT_HOL_BLOCK_EN_n,
				pipe_idx, &ep_holb);
	}
	client_lock_unlock_cb(client, false);
	return 0;
}

void ipa3_xdci_ep_delay_rm(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;
	int result;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm.\n");
		return;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (ep->ep_delay_set == true) {

		memset(&ep_cfg_ctrl, 0, sizeof(struct ipa_ep_cfg_ctrl));
		ep_cfg_ctrl.ipa_ep_delay = false;

		if (!ep->keep_ipa_awake)
			IPA_ACTIVE_CLIENTS_INC_EP
				(ipa3_get_client_mapping(clnt_hdl));

		result = ipa3_cfg_ep_ctrl(clnt_hdl,
			&ep_cfg_ctrl);

		if (!ep->keep_ipa_awake)
			IPA_ACTIVE_CLIENTS_DEC_EP
				(ipa3_get_client_mapping(clnt_hdl));

		if (result) {
			IPAERR
			("client (ep: %d) failed to remove delay result=%d\n",
				clnt_hdl, result);
		} else {
			IPADBG("client (ep: %d) delay removed\n",
				clnt_hdl);
			ep->ep_delay_set = false;
		}
	}
}

int ipa3_xdci_disconnect(u32 clnt_hdl, bool should_force_clear, u32 qmi_req_id)
{
	struct ipa3_ep_context *ep;
	int result;
	u32 source_pipe_bitmask = 0;

	IPADBG("entry\n");
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("Bad parameter.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	if (!IPA_CLIENT_IS_CONS(ep->client)) {
		IPADBG("Stopping PROD channel - hdl=%d clnt=%d\n",
			clnt_hdl, ep->client);
		source_pipe_bitmask = 1 <<
			ipa3_get_ep_mapping(ep->client);
		result = ipa3_stop_ul_chan_with_data_drain(qmi_req_id,
			source_pipe_bitmask, should_force_clear, clnt_hdl,
			true);
		if (result) {
			IPAERR("Fail to stop UL channel with data drain\n");
			WARN_ON(1);
			goto stop_chan_fail;
		}
	} else {
		IPADBG("Stopping CONS channel - hdl=%d clnt=%d\n",
			clnt_hdl, ep->client);
		result = ipa3_stop_gsi_channel(clnt_hdl);
		if (result) {
			IPAERR("Error stopping channel (CONS client): %d\n",
				result);
			goto stop_chan_fail;
		}
	}

	ipa3_disable_data_path(clnt_hdl);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	IPADBG("exit\n");
	return 0;

stop_chan_fail:
	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	return result;
}

int ipa3_release_gsi_channel(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep;
	int result = -EFAULT;
	enum gsi_status gsi_res;

	IPADBG("entry\n");
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("Bad parameter.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	/* Set the disconnect in progress flag to avoid calling cb.*/
	spin_lock(&ipa3_ctx->disconnect_lock);
	atomic_set(&ep->disconnect_in_progress, 1);
	spin_unlock(&ipa3_ctx->disconnect_lock);


	gsi_res = gsi_dealloc_channel(ep->gsi_chan_hdl);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error deallocating channel: %d\n", gsi_res);
		goto dealloc_chan_fail;
	}

	gsi_res = gsi_dealloc_evt_ring(ep->gsi_evt_ring_hdl);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error deallocating event: %d\n", gsi_res);
		goto dealloc_chan_fail;
	}

	if (!ep->skip_ep_cfg && IPA_CLIENT_IS_PROD(ep->client))
		ipa3_delete_dflt_flt_rules(clnt_hdl);

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	spin_lock(&ipa3_ctx->disconnect_lock);
	memset(&ipa3_ctx->ep[clnt_hdl], 0, sizeof(struct ipa3_ep_context));
	spin_unlock(&ipa3_ctx->disconnect_lock);

	IPADBG("exit\n");
	return 0;

dealloc_chan_fail:
	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	return result;
}

int ipa3_xdci_suspend(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
	bool should_force_clear, u32 qmi_req_id, bool is_dpl)
{
	struct ipa3_ep_context *ul_ep = NULL;
	struct ipa3_ep_context *dl_ep;
	int result = -EFAULT;
	u32 source_pipe_bitmask = 0;
	bool dl_data_pending = true;
	bool ul_data_pending = true;
	int i;
	bool is_empty = false;
	struct gsi_chan_info ul_gsi_chan_info, dl_gsi_chan_info;
	int aggr_active_bitmap = 0;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;

	/* In case of DPL, dl is the DPL channel/client */

	IPADBG("entry\n");
	if (dl_clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[dl_clnt_hdl].valid == 0 ||
		(!is_dpl && (ul_clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[ul_clnt_hdl].valid == 0))) {
		IPAERR("Bad parameter.\n");
		return -EINVAL;
	}

	dl_ep = &ipa3_ctx->ep[dl_clnt_hdl];
	if (!is_dpl)
		ul_ep = &ipa3_ctx->ep[ul_clnt_hdl];
	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(dl_clnt_hdl));

	result = ipa3_get_gsi_chan_info(&dl_gsi_chan_info,
		dl_ep->gsi_chan_hdl);
	if (result)
		goto disable_clk_and_exit;

	if (!is_dpl) {
		result = ipa3_get_gsi_chan_info(&ul_gsi_chan_info,
			ul_ep->gsi_chan_hdl);
		if (result)
			goto disable_clk_and_exit;
	}

	for (i = 0; i < IPA_POLL_FOR_EMPTINESS_NUM; i++) {
		if (!dl_data_pending && !ul_data_pending)
			break;
		result = ipa3_is_xdci_channel_empty(dl_ep, &is_empty);
		if (result)
			goto disable_clk_and_exit;
		if (!is_empty) {
			dl_data_pending = true;
			break;
		}
		dl_data_pending = false;
		if (!is_dpl) {
			result = ipa3_is_xdci_channel_empty(ul_ep, &is_empty);
			if (result)
				goto disable_clk_and_exit;
			ul_data_pending = !is_empty;
		} else {
			ul_data_pending = false;
		}

		udelay(IPA_POLL_FOR_EMPTINESS_SLEEP_USEC);
	}

	if (!dl_data_pending) {
		aggr_active_bitmap = ipahal_read_reg(IPA_STATE_AGGR_ACTIVE);
		if (aggr_active_bitmap & (1 << dl_clnt_hdl)) {
			IPADBG("DL/DPL data pending due to open aggr. frame\n");
			dl_data_pending = true;
		}
	}
	if (dl_data_pending) {
		IPAERR("DL/DPL data pending, can't suspend\n");
		result = -EFAULT;
		goto disable_clk_and_exit;
	}

	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0) {
		/* Suspend the DL/DPL EP */
		memset(&ep_cfg_ctrl, 0, sizeof(struct ipa_ep_cfg_ctrl));
		ep_cfg_ctrl.ipa_ep_suspend = true;
		ipa3_cfg_ep_ctrl(dl_clnt_hdl, &ep_cfg_ctrl);
	}

	/*
	 * Check if DL/DPL channel is empty again, data could enter the channel
	 * before its IPA EP was suspended
	 */
	result = ipa3_is_xdci_channel_empty(dl_ep, &is_empty);
	if (result)
		goto unsuspend_dl_and_exit;
	if (!is_empty) {
		IPAERR("DL/DPL data pending, can't suspend\n");
		result = -EFAULT;
		goto unsuspend_dl_and_exit;
	}

	/* Stop DL channel */
	result = ipa3_stop_gsi_channel(dl_clnt_hdl);
	if (result) {
		IPAERR("Error stopping DL/DPL channel: %d\n", result);
		result = -EFAULT;
		goto unsuspend_dl_and_exit;
	}

	/* STOP UL channel */
	if (!is_dpl) {
		source_pipe_bitmask = 1 << ipa3_get_ep_mapping(ul_ep->client);
		result = ipa3_stop_ul_chan_with_data_drain(qmi_req_id,
			source_pipe_bitmask, should_force_clear, ul_clnt_hdl,
			false);
		if (result) {
			IPAERR("Error stopping UL channel: result = %d\n",
				result);
			goto start_dl_and_exit;
		}
	}

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(dl_clnt_hdl));

	IPADBG("exit\n");
	return 0;

start_dl_and_exit:
	gsi_start_channel(dl_ep->gsi_chan_hdl);
	ipa3_start_gsi_debug_monitor(dl_clnt_hdl);
unsuspend_dl_and_exit:
	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0) {
		/* Unsuspend the DL EP */
		memset(&ep_cfg_ctrl, 0, sizeof(struct ipa_ep_cfg_ctrl));
		ep_cfg_ctrl.ipa_ep_suspend = false;
		ipa3_cfg_ep_ctrl(dl_clnt_hdl, &ep_cfg_ctrl);
	}
disable_clk_and_exit:
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(dl_clnt_hdl));
	return result;
}

int ipa3_start_gsi_channel(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep;
	int result = -EFAULT;
	enum gsi_status gsi_res;
	enum ipa_client_type client_type;

	IPADBG("entry\n");
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes  ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("Bad parameters.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];
	client_type = ipa3_get_client_mapping(clnt_hdl);
	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_INC_EP(client_type);

	gsi_res = gsi_start_channel(ep->gsi_chan_hdl);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error starting channel: %d\n", gsi_res);
		goto start_chan_fail;
	}
	ipa3_start_gsi_debug_monitor(clnt_hdl);

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(client_type);

	IPADBG("exit\n");
	return 0;

start_chan_fail:
	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	return result;
}

int ipa3_xdci_resume(u32 ul_clnt_hdl, u32 dl_clnt_hdl, bool is_dpl)
{
	struct ipa3_ep_context *ul_ep = NULL;
	struct ipa3_ep_context *dl_ep = NULL;
	enum gsi_status gsi_res;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;

	/* In case of DPL, dl is the DPL channel/client */

	IPADBG("entry\n");
	if (dl_clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[dl_clnt_hdl].valid == 0 ||
		(!is_dpl && (ul_clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[ul_clnt_hdl].valid == 0))) {
		IPAERR("Bad parameter.\n");
		return -EINVAL;
	}

	dl_ep = &ipa3_ctx->ep[dl_clnt_hdl];
	if (!is_dpl)
		ul_ep = &ipa3_ctx->ep[ul_clnt_hdl];
	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(dl_clnt_hdl));

	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0) {
		/* Unsuspend the DL/DPL EP */
		memset(&ep_cfg_ctrl, 0, sizeof(struct ipa_ep_cfg_ctrl));
		ep_cfg_ctrl.ipa_ep_suspend = false;
		ipa3_cfg_ep_ctrl(dl_clnt_hdl, &ep_cfg_ctrl);
	}

	/* Start DL channel */
	gsi_res = gsi_start_channel(dl_ep->gsi_chan_hdl);
	if (gsi_res != GSI_STATUS_SUCCESS)
		IPAERR("Error starting DL channel: %d\n", gsi_res);
	ipa3_start_gsi_debug_monitor(dl_clnt_hdl);

	/* Start UL channel */
	if (!is_dpl) {
		gsi_res = gsi_start_channel(ul_ep->gsi_chan_hdl);
		if (gsi_res != GSI_STATUS_SUCCESS)
			IPAERR("Error starting UL channel: %d\n", gsi_res);
		ipa3_start_gsi_debug_monitor(ul_clnt_hdl);
	}

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(dl_clnt_hdl));

	IPADBG("exit\n");
	return 0;
}
/**
 * ipa3_clear_endpoint_delay() - Remove ep delay set on the IPA pipe before
 * client disconnect.
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Should be called by the driver of the peripheral that wants to remove
 * ep delay on IPA consumer ipe before disconnect in non GPI mode. this api
 * expects caller to take responsibility to free any needed headers, routing
 * and filtering tables and rules as needed.
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa3_clear_endpoint_delay(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep;
	struct ipa_ep_cfg_ctrl ep_ctrl = {0};
	struct ipa_enable_force_clear_datapath_req_msg_v01 req = {0};
	int res;

	if (unlikely(!ipa3_ctx)) {
		IPAERR("IPA driver was not initialized\n");
		return -EINVAL;
	}

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (!ipa3_ctx->tethered_flow_control) {
		IPADBG("APPS flow control is not enabled\n");
		/* Send a message to modem to disable flow control honoring. */
		req.request_id = clnt_hdl;
		req.source_pipe_bitmask = 1 << clnt_hdl;
		res = ipa3_qmi_enable_force_clear_datapath_send(&req);
		if (res) {
			IPADBG("enable_force_clear_datapath failed %d\n",
				res);
		}
		ep->qmi_request_sent = true;
	}

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));
	/* Set disconnect in progress flag so further flow control events are
	 * not honored.
	 */
	atomic_set(&ep->disconnect_in_progress, 1);

	/* If flow is disabled at this point, restore the ep state.*/
	ep_ctrl.ipa_ep_delay = false;
	ep_ctrl.ipa_ep_suspend = false;
	ipa3_cfg_ep_ctrl(clnt_hdl, &ep_ctrl);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	IPADBG("client (ep: %d) removed ep delay\n", clnt_hdl);

	return 0;
}

/**
 * ipa3_get_aqc_gsi_stats() - Query AQC gsi stats from uc
 * @stats:	[inout] stats blob from client populated by driver
 *
 * Returns:	0 on success, negative on failure
 *
 * @note Cannot be called from atomic context
 *
 */
int ipa3_get_aqc_gsi_stats(struct ipa_uc_dbg_ring_stats *stats)
{
	int i;

	if (!ipa3_ctx->aqc_ctx.dbg_stats.uc_dbg_stats_mmio) {
		IPAERR("bad parms NULL aqc_gsi_stats_mmio\n");
		return -EINVAL;
	}
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	for (i = 0; i < MAX_AQC_CHANNELS; i++) {
		stats->ring[i].ringFull = ioread32(
			ipa3_ctx->aqc_ctx.dbg_stats.uc_dbg_stats_mmio
			+ i * IPA3_UC_DEBUG_STATS_OFF +
			IPA3_UC_DEBUG_STATS_RINGFULL_OFF);
		stats->ring[i].ringEmpty = ioread32(
			ipa3_ctx->aqc_ctx.dbg_stats.uc_dbg_stats_mmio
			+ i * IPA3_UC_DEBUG_STATS_OFF +
			IPA3_UC_DEBUG_STATS_RINGEMPTY_OFF);
		stats->ring[i].ringUsageHigh = ioread32(
			ipa3_ctx->aqc_ctx.dbg_stats.uc_dbg_stats_mmio
			+ i * IPA3_UC_DEBUG_STATS_OFF +
			IPA3_UC_DEBUG_STATS_RINGUSAGEHIGH_OFF);
		stats->ring[i].ringUsageLow = ioread32(
			ipa3_ctx->aqc_ctx.dbg_stats.uc_dbg_stats_mmio
			+ i * IPA3_UC_DEBUG_STATS_OFF +
			IPA3_UC_DEBUG_STATS_RINGUSAGELOW_OFF);
		stats->ring[i].RingUtilCount = ioread32(
			ipa3_ctx->aqc_ctx.dbg_stats.uc_dbg_stats_mmio
			+ i * IPA3_UC_DEBUG_STATS_OFF +
			IPA3_UC_DEBUG_STATS_RINGUTILCOUNT_OFF);
	}
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();


	return 0;
}
