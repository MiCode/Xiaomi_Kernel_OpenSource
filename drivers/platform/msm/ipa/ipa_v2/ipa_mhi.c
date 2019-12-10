/* Copyright (c) 2015-2016, 2020, The Linux Foundation. All rights reserved.
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
#include <linux/ipa_mhi.h>
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


bool ipa2_mhi_sps_channel_empty(enum ipa_client_type client)
{
	u32 pipe_idx;
	bool pending;

	pipe_idx = ipa2_get_ep_mapping(client);
	if (sps_pipe_pending_desc(ipa_ctx->bam_handle,
		pipe_idx, &pending)) {
		IPA_MHI_ERR("sps_pipe_pending_desc failed\n");
		WARN_ON(1);
		return false;
	}

	return !pending;
}

int ipa2_disable_sps_pipe(enum ipa_client_type client)
{
	int ipa_ep_index;
	int res;

	ipa_ep_index = ipa2_get_ep_mapping(client);

	res = sps_pipe_disable(ipa_ctx->bam_handle, ipa_ep_index);
	if (res) {
		IPA_MHI_ERR("sps_pipe_disable fail %d\n", res);
		return res;
	}

	return 0;
}

int ipa2_mhi_reset_channel_internal(enum ipa_client_type client)
{
	int res;

	IPA_MHI_FUNC_ENTRY();

	res = ipa_disable_data_path(ipa2_get_ep_mapping(client));
	if (res) {
		IPA_MHI_ERR("ipa_disable_data_path failed %d\n", res);
		return res;
	}
	IPA_MHI_FUNC_EXIT();

	return 0;
}

int ipa2_mhi_start_channel_internal(enum ipa_client_type client)
{
	int res;

	IPA_MHI_FUNC_ENTRY();

	res = ipa_enable_data_path(ipa2_get_ep_mapping(client));
	if (res) {
		IPA_MHI_ERR("ipa_enable_data_path failed %d\n", res);
		return res;
	}
	IPA_MHI_FUNC_EXIT();

	return 0;
}

int ipa2_mhi_init_engine(struct ipa_mhi_init_engine *params)
{
	int res;

	IPA_MHI_FUNC_ENTRY();

	if (!params) {
		IPA_MHI_ERR("null args\n");
		return -EINVAL;
	}

	if (ipa2_uc_state_check()) {
		IPA_MHI_ERR("IPA uc is not loaded\n");
		return -EAGAIN;
	}

	/* Initialize IPA MHI engine */
	res = ipa_uc_mhi_init_engine(params->uC.msi, params->uC.mmio_addr,
		params->uC.host_ctrl_addr, params->uC.host_data_addr,
		params->uC.first_ch_idx, params->uC.first_er_idx);
	if (res) {
		IPA_MHI_ERR("failed to start MHI engine %d\n", res);
		goto fail_init_engine;
	}

	/* Update UL/DL sync if valid */
	res = ipa2_uc_mhi_send_dl_ul_sync_info(
		params->uC.ipa_cached_dl_ul_sync_info);
	if (res) {
		IPA_MHI_ERR("failed to update ul/dl sync %d\n", res);
		goto fail_init_engine;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;

fail_init_engine:
	return res;
}

/**
 * ipa2_connect_mhi_pipe() - Connect pipe to IPA and start corresponding
 * MHI channel
 * @in: connect parameters
 * @clnt_hdl: [out] client handle for this pipe
 *
 * This function is called by IPA MHI client driver on MHI channel start.
 * This function is called after MHI engine was started.
 * This function is doing the following:
 *	- Send command to uC to start corresponding MHI channel
 *	- Configure IPA EP control
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa2_connect_mhi_pipe(struct ipa_mhi_connect_params_internal *in,
		u32 *clnt_hdl)
{
	struct ipa_ep_context *ep;
	int ipa_ep_idx;
	int res;

	IPA_MHI_FUNC_ENTRY();

	if (!in || !clnt_hdl) {
		IPA_MHI_ERR("NULL args\n");
		return -EINVAL;
	}

	if (in->sys->client >= IPA_CLIENT_MAX) {
		IPA_MHI_ERR("bad parm client:%d\n", in->sys->client);
		return -EINVAL;
	}

	ipa_ep_idx = ipa2_get_ep_mapping(in->sys->client);
	if (ipa_ep_idx == -1) {
		IPA_MHI_ERR("Invalid client.\n");
		return -EINVAL;
	}

	ep = &ipa_ctx->ep[ipa_ep_idx];

	IPA_MHI_DBG("client %d channelHandle %d channelIndex %d\n",
		in->sys->client, in->start.uC.index, in->start.uC.id);

	if (ep->valid == 1) {
		IPA_MHI_ERR("EP already allocated.\n");
		goto fail_ep_exists;
	}

	memset(ep, 0, offsetof(struct ipa_ep_context, sys));
	ep->valid = 1;
	ep->skip_ep_cfg = in->sys->skip_ep_cfg;
	ep->client = in->sys->client;
	ep->client_notify = in->sys->notify;
	ep->priv = in->sys->priv;
	ep->keep_ipa_awake = in->sys->keep_ipa_awake;

	/* start channel in uC */
	if (in->start.uC.state == IPA_HW_MHI_CHANNEL_STATE_INVALID) {
		IPA_MHI_DBG("Initializing channel\n");
		res = ipa_uc_mhi_init_channel(ipa_ep_idx, in->start.uC.index,
			in->start.uC.id,
			(IPA_CLIENT_IS_PROD(ep->client) ? 1 : 2));
		if (res) {
			IPA_MHI_ERR("init_channel failed %d\n", res);
			goto fail_init_channel;
		}
	} else if (in->start.uC.state == IPA_HW_MHI_CHANNEL_STATE_DISABLE) {
		IPA_MHI_DBG("Starting channel\n");
		res = ipa_uc_mhi_resume_channel(in->start.uC.index, false);
		if (res) {
			IPA_MHI_ERR("init_channel failed %d\n", res);
			goto fail_init_channel;
		}
	} else {
		IPA_MHI_ERR("Invalid channel state %d\n", in->start.uC.state);
		goto fail_init_channel;
	}

	res = ipa_enable_data_path(ipa_ep_idx);
	if (res) {
		IPA_MHI_ERR("enable data path failed res=%d clnt=%d.\n", res,
			ipa_ep_idx);
		goto fail_enable_dp;
	}

	if (!ep->skip_ep_cfg) {
		if (ipa2_cfg_ep(ipa_ep_idx, &in->sys->ipa_ep_cfg)) {
			IPAERR("fail to configure EP.\n");
			goto fail_ep_cfg;
		}
		if (ipa2_cfg_ep_status(ipa_ep_idx, &ep->status)) {
			IPAERR("fail to configure status of EP.\n");
			goto fail_ep_cfg;
		}
		IPA_MHI_DBG("ep configuration successful\n");
	} else {
		IPA_MHI_DBG("skipping ep configuration\n");
	}

	*clnt_hdl = ipa_ep_idx;

	if (!ep->skip_ep_cfg && IPA_CLIENT_IS_PROD(in->sys->client))
		ipa_install_dflt_flt_rules(ipa_ep_idx);

	ipa_ctx->skip_ep_cfg_shadow[ipa_ep_idx] = ep->skip_ep_cfg;
	IPA_MHI_DBG("client %d (ep: %d) connected\n", in->sys->client,
		ipa_ep_idx);

	IPA_MHI_FUNC_EXIT();

	return 0;

fail_ep_cfg:
	ipa_disable_data_path(ipa_ep_idx);
fail_enable_dp:
	ipa_uc_mhi_reset_channel(in->start.uC.index);
fail_init_channel:
	memset(ep, 0, offsetof(struct ipa_ep_context, sys));
fail_ep_exists:
	return -EPERM;
}

/**
 * ipa2_disconnect_mhi_pipe() - Disconnect pipe from IPA and reset corresponding
 * MHI channel
 * @in: connect parameters
 * @clnt_hdl: [out] client handle for this pipe
 *
 * This function is called by IPA MHI client driver on MHI channel reset.
 * This function is called after MHI channel was started.
 * This function is doing the following:
 *	- Send command to uC to reset corresponding MHI channel
 *	- Configure IPA EP control
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa2_disconnect_mhi_pipe(u32 clnt_hdl)
{
	IPA_MHI_FUNC_ENTRY();

	if (clnt_hdl >= ipa_ctx->ipa_num_pipes) {
		IPAERR("invalid handle %d\n", clnt_hdl);
		return -EINVAL;
	}

	if (ipa_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("pipe was not connected %d\n", clnt_hdl);
		return -EINVAL;
	}

	ipa_ctx->ep[clnt_hdl].valid = 0;

	ipa_delete_dflt_flt_rules(clnt_hdl);

	IPA_MHI_DBG("client (ep: %d) disconnected\n", clnt_hdl);
	IPA_MHI_FUNC_EXIT();
	return 0;
}

int ipa2_mhi_resume_channels_internal(enum ipa_client_type client,
		bool LPTransitionRejected, bool brstmode_enabled,
		union __packed gsi_channel_scratch ch_scratch, u8 index)
{
	int res;

	IPA_MHI_FUNC_ENTRY();
	res = ipa_uc_mhi_resume_channel(index, LPTransitionRejected);
	if (res) {
		IPA_MHI_ERR("failed to suspend channel %u error %d\n",
			index, res);
		return res;
	}

	IPA_MHI_FUNC_EXIT();
	return 0;
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("IPA MHI driver");
