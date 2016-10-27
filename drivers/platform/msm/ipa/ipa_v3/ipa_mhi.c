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

#include <linux/debugfs.h>
#include <linux/export.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/ipa.h>
#include <linux/msm_gsi.h>
#include <linux/ipa_mhi.h>
#include "../ipa_common_i.h"
#include "ipa_i.h"
#include "ipa_qmi_service.h"

#define IPA_MHI_DRV_NAME "ipa_mhi"


#define IPA_MHI_DBG(fmt, args...) \
	do { \
		pr_debug(IPA_MHI_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			IPA_MHI_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPA_MHI_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_MHI_DBG_LOW(fmt, args...) \
	do { \
		pr_debug(IPA_MHI_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPA_MHI_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)


#define IPA_MHI_ERR(fmt, args...) \
	do { \
		pr_err(IPA_MHI_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
				IPA_MHI_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
				IPA_MHI_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)


#define IPA_MHI_FUNC_ENTRY() \
	IPA_MHI_DBG_LOW("ENTRY\n")
#define IPA_MHI_FUNC_EXIT() \
	IPA_MHI_DBG_LOW("EXIT\n")

#define IPA_MHI_MAX_UL_CHANNELS 1
#define IPA_MHI_MAX_DL_CHANNELS 1

/* bit #40 in address should be asserted for MHI transfers over pcie */
#define IPA_MHI_HOST_ADDR_COND(addr) \
		((params->assert_bit40)?(IPA_MHI_HOST_ADDR(addr)):(addr))

enum ipa3_mhi_polling_mode {
	IPA_MHI_POLLING_MODE_DB_MODE,
	IPA_MHI_POLLING_MODE_POLL_MODE,
};

bool ipa3_mhi_stop_gsi_channel(enum ipa_client_type client)
{
	int res;
	int ipa_ep_idx;
	struct ipa3_ep_context *ep;

	IPA_MHI_FUNC_ENTRY();
	ipa_ep_idx = ipa3_get_ep_mapping(client);
	if (ipa_ep_idx == -1) {
		IPA_MHI_ERR("Invalid client.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];
	IPA_MHI_DBG_LOW("Stopping GSI channel %ld\n", ep->gsi_chan_hdl);
	res = gsi_stop_channel(ep->gsi_chan_hdl);
	if (res != 0 &&
		res != -GSI_STATUS_AGAIN &&
		res != -GSI_STATUS_TIMED_OUT) {
		IPA_MHI_ERR("GSI stop channel failed %d\n",
			res);
		WARN_ON(1);
		return false;
	}

	if (res == 0) {
		IPA_MHI_DBG_LOW("GSI channel %ld STOP\n",
			ep->gsi_chan_hdl);
		return true;
	}

	return false;
}

static int ipa3_mhi_reset_gsi_channel(enum ipa_client_type client)
{
	int res;
	int clnt_hdl;

	IPA_MHI_FUNC_ENTRY();

	clnt_hdl = ipa3_get_ep_mapping(client);
	if (clnt_hdl < 0)
		return -EFAULT;

	res = ipa3_reset_gsi_channel(clnt_hdl);
	if (res) {
		IPA_MHI_ERR("ipa3_reset_gsi_channel failed %d\n", res);
		return -EFAULT;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

int ipa3_mhi_reset_channel_internal(enum ipa_client_type client)
{
	int res;

	IPA_MHI_FUNC_ENTRY();

	res = ipa3_mhi_reset_gsi_channel(client);
	if (res) {
		IPAERR("ipa3_mhi_reset_gsi_channel failed\n");
		ipa_assert();
		return res;
	}

	res = ipa3_disable_data_path(ipa3_get_ep_mapping(client));
	if (res) {
		IPA_MHI_ERR("ipa3_disable_data_path failed %d\n", res);
		return res;
	}
	IPA_MHI_FUNC_EXIT();

	return 0;
}

int ipa3_mhi_start_channel_internal(enum ipa_client_type client)
{
	int res;

	IPA_MHI_FUNC_ENTRY();

	res = ipa3_enable_data_path(ipa3_get_ep_mapping(client));
	if (res) {
		IPA_MHI_ERR("ipa3_enable_data_path failed %d\n", res);
		return res;
	}
	IPA_MHI_FUNC_EXIT();

	return 0;
}

static int ipa3_mhi_get_ch_poll_cfg(enum ipa_client_type client,
		struct ipa_mhi_ch_ctx *ch_ctx_host, int ring_size)
{
	switch (ch_ctx_host->pollcfg) {
	case 0:
	/*set default polling configuration according to MHI spec*/
		if (IPA_CLIENT_IS_PROD(client))
			return 7;
		else
			return (ring_size/2)/8;
		break;
	default:
		return ch_ctx_host->pollcfg;
	}
}

static int ipa_mhi_start_gsi_channel(enum ipa_client_type client,
	int ipa_ep_idx, struct start_gsi_channel *params)
{
	int res;
	struct gsi_evt_ring_props ev_props;
	struct ipa_mhi_msi_info *msi;
	struct gsi_chan_props ch_props;
	union __packed gsi_channel_scratch ch_scratch;
	struct ipa3_ep_context *ep;
	struct ipa_gsi_ep_config *ep_cfg;

	IPA_MHI_FUNC_ENTRY();

	ep = &ipa3_ctx->ep[ipa_ep_idx];

	msi = params->msi;
	ep_cfg = ipa_get_gsi_ep_info(ipa_ep_idx);
	if (!ep_cfg) {
		IPA_MHI_ERR("Wrong parameter, ep_cfg is NULL\n");
		return -EPERM;
	}

	/* allocate event ring only for the first time pipe is connected */
	if (params->state == IPA_HW_MHI_CHANNEL_STATE_INVALID) {
		memset(&ev_props, 0, sizeof(ev_props));
		ev_props.intf = GSI_EVT_CHTYPE_MHI_EV;
		ev_props.intr = GSI_INTR_MSI;
		ev_props.re_size = GSI_EVT_RING_RE_SIZE_16B;
		ev_props.ring_len = params->ev_ctx_host->rlen;
		ev_props.ring_base_addr = IPA_MHI_HOST_ADDR_COND(
				params->ev_ctx_host->rbase);
		ev_props.int_modt = params->ev_ctx_host->intmodt *
				IPA_SLEEP_CLK_RATE_KHZ;
		ev_props.int_modc = params->ev_ctx_host->intmodc;
		ev_props.intvec = ((msi->data & ~msi->mask) |
				(params->ev_ctx_host->msivec & msi->mask));
		ev_props.msi_addr = IPA_MHI_HOST_ADDR_COND(
				(((u64)msi->addr_hi << 32) | msi->addr_low));
		ev_props.rp_update_addr = IPA_MHI_HOST_ADDR_COND(
				params->event_context_addr +
				offsetof(struct ipa_mhi_ev_ctx, rp));
		ev_props.exclusive = true;
		ev_props.err_cb = params->ev_err_cb;
		ev_props.user_data = params->channel;
		ev_props.evchid_valid = true;
		ev_props.evchid = params->evchid;
		IPA_MHI_DBG("allocating event ring ep:%u evchid:%u\n",
			ipa_ep_idx, ev_props.evchid);
		res = gsi_alloc_evt_ring(&ev_props, ipa3_ctx->gsi_dev_hdl,
			&ep->gsi_evt_ring_hdl);
		if (res) {
			IPA_MHI_ERR("gsi_alloc_evt_ring failed %d\n", res);
			goto fail_alloc_evt;
			return res;
		}
		IPA_MHI_DBG("client %d, caching event ring hdl %lu\n",
				client,
				ep->gsi_evt_ring_hdl);
		*params->cached_gsi_evt_ring_hdl =
			ep->gsi_evt_ring_hdl;

	} else {
		IPA_MHI_DBG("event ring already exists: evt_ring_hdl=%lu\n",
			*params->cached_gsi_evt_ring_hdl);
		ep->gsi_evt_ring_hdl = *params->cached_gsi_evt_ring_hdl;
	}

	memset(&ch_props, 0, sizeof(ch_props));
	ch_props.prot = GSI_CHAN_PROT_MHI;
	ch_props.dir = IPA_CLIENT_IS_PROD(client) ?
		GSI_CHAN_DIR_TO_GSI : GSI_CHAN_DIR_FROM_GSI;
	ch_props.ch_id = ep_cfg->ipa_gsi_chan_num;
	ch_props.evt_ring_hdl = *params->cached_gsi_evt_ring_hdl;
	ch_props.re_size = GSI_CHAN_RE_SIZE_16B;
	ch_props.ring_len = params->ch_ctx_host->rlen;
	ch_props.ring_base_addr = IPA_MHI_HOST_ADDR_COND(
			params->ch_ctx_host->rbase);
	ch_props.use_db_eng = GSI_CHAN_DB_MODE;
	ch_props.max_prefetch = GSI_ONE_PREFETCH_SEG;
	ch_props.low_weight = 1;
	ch_props.err_cb = params->ch_err_cb;
	ch_props.chan_user_data = params->channel;
	res = gsi_alloc_channel(&ch_props, ipa3_ctx->gsi_dev_hdl,
		&ep->gsi_chan_hdl);
	if (res) {
		IPA_MHI_ERR("gsi_alloc_channel failed %d\n",
			res);
		goto fail_alloc_ch;
	}

	memset(&ch_scratch, 0, sizeof(ch_scratch));
	ch_scratch.mhi.mhi_host_wp_addr = IPA_MHI_HOST_ADDR_COND(
			params->channel_context_addr +
			offsetof(struct ipa_mhi_ch_ctx, wp));
	ch_scratch.mhi.assert_bit40 = params->assert_bit40;
	ch_scratch.mhi.max_outstanding_tre =
		ep_cfg->ipa_if_tlv * ch_props.re_size;
	ch_scratch.mhi.outstanding_threshold =
		min(ep_cfg->ipa_if_tlv / 2, 8) * ch_props.re_size;
	ch_scratch.mhi.oob_mod_threshold = 4;
	if (params->ch_ctx_host->brstmode == IPA_MHI_BURST_MODE_DEFAULT ||
		params->ch_ctx_host->brstmode == IPA_MHI_BURST_MODE_ENABLE) {
		ch_scratch.mhi.burst_mode_enabled = true;
		ch_scratch.mhi.polling_configuration =
			ipa3_mhi_get_ch_poll_cfg(client, params->ch_ctx_host,
				(ch_props.ring_len / ch_props.re_size));
		ch_scratch.mhi.polling_mode = IPA_MHI_POLLING_MODE_DB_MODE;
	} else {
		ch_scratch.mhi.burst_mode_enabled = false;
	}
	res = gsi_write_channel_scratch(ep->gsi_chan_hdl,
		ch_scratch);
	if (res) {
		IPA_MHI_ERR("gsi_write_channel_scratch failed %d\n",
			res);
		goto fail_ch_scratch;
	}

	*params->mhi = ch_scratch.mhi;

	IPA_MHI_DBG("Starting channel\n");
	res = gsi_start_channel(ep->gsi_chan_hdl);
	if (res) {
		IPA_MHI_ERR("gsi_start_channel failed %d\n", res);
		goto fail_ch_start;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;

fail_ch_start:
fail_ch_scratch:
	gsi_dealloc_channel(ep->gsi_chan_hdl);
fail_alloc_ch:
	gsi_dealloc_evt_ring(ep->gsi_evt_ring_hdl);
	ep->gsi_evt_ring_hdl = ~0;
fail_alloc_evt:
	return res;
}

int ipa3_mhi_init_engine(struct ipa_mhi_init_engine *params)
{
	int res;
	struct gsi_device_scratch gsi_scratch;
	struct ipa_gsi_ep_config *gsi_ep_info;

	IPA_MHI_FUNC_ENTRY();

	if (!params) {
		IPA_MHI_ERR("null args\n");
		return -EINVAL;
	}

	/* Initialize IPA MHI engine */
	gsi_ep_info = ipa_get_gsi_ep_info(
		ipa_get_ep_mapping(IPA_CLIENT_MHI_PROD));
	if (!gsi_ep_info) {
		IPAERR("MHI PROD has no ep allocated\n");
		ipa_assert();
	}
	memset(&gsi_scratch, 0, sizeof(gsi_scratch));
	gsi_scratch.mhi_base_chan_idx_valid = true;
	gsi_scratch.mhi_base_chan_idx = gsi_ep_info->ipa_gsi_chan_num +
		params->gsi.first_ch_idx;
	res = gsi_write_device_scratch(ipa3_ctx->gsi_dev_hdl,
		&gsi_scratch);
	if (res) {
		IPA_MHI_ERR("failed to write device scratch %d\n", res);
		goto fail_init_engine;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;

fail_init_engine:
	return res;
}

/**
 * ipa3_connect_mhi_pipe() - Connect pipe to IPA and start corresponding
 * MHI channel
 * @in: connect parameters
 * @clnt_hdl: [out] client handle for this pipe
 *
 * This function is called by IPA MHI client driver on MHI channel start.
 * This function is called after MHI engine was started.
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa3_connect_mhi_pipe(struct ipa_mhi_connect_params_internal *in,
		u32 *clnt_hdl)
{
	struct ipa3_ep_context *ep;
	int ipa_ep_idx;
	int res;
	enum ipa_client_type client;

	IPA_MHI_FUNC_ENTRY();

	if (!in || !clnt_hdl) {
		IPA_MHI_ERR("NULL args\n");
		return -EINVAL;
	}

	client = in->sys->client;
	ipa_ep_idx = ipa3_get_ep_mapping(client);
	if (ipa_ep_idx == -1) {
		IPA_MHI_ERR("Invalid client.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];

	if (ep->valid == 1) {
		IPA_MHI_ERR("EP already allocated.\n");
		return -EPERM;
	}

	memset(ep, 0, offsetof(struct ipa3_ep_context, sys));
	ep->valid = 1;
	ep->skip_ep_cfg = in->sys->skip_ep_cfg;
	ep->client = client;
	ep->client_notify = in->sys->notify;
	ep->priv = in->sys->priv;
	ep->keep_ipa_awake = in->sys->keep_ipa_awake;

	res = ipa_mhi_start_gsi_channel(client,
					ipa_ep_idx, &in->start.gsi);
	if (res) {
		IPA_MHI_ERR("ipa_mhi_start_gsi_channel failed %d\n",
			res);
		goto fail_start_channel;
	}

	res = ipa3_enable_data_path(ipa_ep_idx);
	if (res) {
		IPA_MHI_ERR("enable data path failed res=%d clnt=%d.\n", res,
			ipa_ep_idx);
		goto fail_ep_cfg;
	}

	if (!ep->skip_ep_cfg) {
		if (ipa3_cfg_ep(ipa_ep_idx, &in->sys->ipa_ep_cfg)) {
			IPAERR("fail to configure EP.\n");
			goto fail_ep_cfg;
		}
		if (ipa3_cfg_ep_status(ipa_ep_idx, &ep->status)) {
			IPAERR("fail to configure status of EP.\n");
			goto fail_ep_cfg;
		}
		IPA_MHI_DBG("ep configuration successful\n");
	} else {
		IPA_MHI_DBG("skipping ep configuration\n");
	}

	*clnt_hdl = ipa_ep_idx;

	if (!ep->skip_ep_cfg && IPA_CLIENT_IS_PROD(client))
		ipa3_install_dflt_flt_rules(ipa_ep_idx);

	ipa3_ctx->skip_ep_cfg_shadow[ipa_ep_idx] = ep->skip_ep_cfg;
	IPA_MHI_DBG("client %d (ep: %d) connected\n", client,
		ipa_ep_idx);

	IPA_MHI_FUNC_EXIT();

	return 0;

fail_ep_cfg:
	ipa3_disable_data_path(ipa_ep_idx);
fail_start_channel:
	memset(ep, 0, offsetof(struct ipa3_ep_context, sys));
	return -EPERM;
}

/**
 * ipa3_disconnect_mhi_pipe() - Disconnect pipe from IPA and reset corresponding
 * MHI channel
 * @clnt_hdl: client handle for this pipe
 *
 * This function is called by IPA MHI client driver on MHI channel reset.
 * This function is called after MHI channel was started.
 * This function is doing the following:
 *	- Send command to uC/GSI to reset corresponding MHI channel
 *	- Configure IPA EP control
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa3_disconnect_mhi_pipe(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep;
	int res;

	IPA_MHI_FUNC_ENTRY();

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes) {
		IPAERR("invalid handle %d\n", clnt_hdl);
		return -EINVAL;
	}

	if (ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("pipe was not connected %d\n", clnt_hdl);
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI) {
		res = gsi_dealloc_channel(ep->gsi_chan_hdl);
		if (res) {
			IPAERR("gsi_dealloc_channel failed %d\n", res);
			goto fail_reset_channel;
		}
	}

	ep->valid = 0;
	ipa3_delete_dflt_flt_rules(clnt_hdl);

	IPA_MHI_DBG("client (ep: %d) disconnected\n", clnt_hdl);
	IPA_MHI_FUNC_EXIT();
	return 0;

fail_reset_channel:
	return res;
}

int ipa3_mhi_resume_channels_internal(enum ipa_client_type client,
		bool LPTransitionRejected, bool brstmode_enabled,
		union __packed gsi_channel_scratch ch_scratch, u8 index)
{
	int res;
	int ipa_ep_idx;
	struct ipa3_ep_context *ep;

	IPA_MHI_FUNC_ENTRY();

	ipa_ep_idx = ipa3_get_ep_mapping(client);
	ep = &ipa3_ctx->ep[ipa_ep_idx];

	if (brstmode_enabled && !LPTransitionRejected) {
		/*
		 * set polling mode bit to DB mode before
		 * resuming the channel
		 */
		res = gsi_write_channel_scratch(
			ep->gsi_chan_hdl, ch_scratch);
		if (res) {
			IPA_MHI_ERR("write ch scratch fail %d\n"
				, res);
			return res;
		}
	}

	res = gsi_start_channel(ep->gsi_chan_hdl);
	if (res) {
		IPA_MHI_ERR("failed to resume channel error %d\n", res);
		return res;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

int ipa3_mhi_query_ch_info(enum ipa_client_type client,
		struct gsi_chan_info *ch_info)
{
	int ipa_ep_idx;
	int res;
	struct ipa3_ep_context *ep;

	IPA_MHI_FUNC_ENTRY();

	ipa_ep_idx = ipa3_get_ep_mapping(client);

	ep = &ipa3_ctx->ep[ipa_ep_idx];
	res = gsi_query_channel_info(ep->gsi_chan_hdl, ch_info);
	if (res) {
		IPAERR("gsi_query_channel_info failed\n");
		return res;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

bool ipa3_has_open_aggr_frame(enum ipa_client_type client)
{
	u32 aggr_state_active;
	int ipa_ep_idx;

	aggr_state_active = ipahal_read_reg(IPA_STATE_AGGR_ACTIVE);
	IPA_MHI_DBG_LOW("IPA_STATE_AGGR_ACTIVE_OFST 0x%x\n", aggr_state_active);

	ipa_ep_idx = ipa_get_ep_mapping(client);
	if (ipa_ep_idx == -1) {
		ipa_assert();
		return false;
	}

	if ((1 << ipa_ep_idx) & aggr_state_active)
		return true;

	return false;
}

int ipa3_mhi_destroy_channel(enum ipa_client_type client)
{
	int res;
	int ipa_ep_idx;
	struct ipa3_ep_context *ep;

	ipa_ep_idx = ipa3_get_ep_mapping(client);

	ep = &ipa3_ctx->ep[ipa_ep_idx];

	IPA_MHI_DBG("reset event ring (hdl: %lu, ep: %d)\n",
		ep->gsi_evt_ring_hdl, ipa_ep_idx);

	res = gsi_reset_evt_ring(ep->gsi_evt_ring_hdl);
	if (res) {
		IPAERR(" failed to reset evt ring %lu, err %d\n"
			, ep->gsi_evt_ring_hdl, res);
		goto fail;
	}

	IPA_MHI_DBG("dealloc event ring (hdl: %lu, ep: %d)\n",
		ep->gsi_evt_ring_hdl, ipa_ep_idx);

	res = gsi_dealloc_evt_ring(
		ep->gsi_evt_ring_hdl);
	if (res) {
		IPAERR("dealloc evt ring %lu failed, err %d\n"
			, ep->gsi_evt_ring_hdl, res);
		goto fail;
	}

	return 0;
fail:
	return res;
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("IPA MHI driver");
