/* Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
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

#include <linux/ipa_qdss.h>
#include <linux/msm_ipa.h>
#include <linux/string.h>
#include <linux/ipa_qdss.h>
#include "ipa_i.h"

#define IPA_HOLB_TMR_VALUE 0
#define OFFLOAD_DRV_NAME "ipa_qdss"
#define IPA_QDSS_DBG(fmt, args...) \
	do { \
		pr_debug(OFFLOAD_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_QDSS_ERR(fmt, args...) \
	do { \
		pr_err(OFFLOAD_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			OFFLOAD_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

static void ipa3_qdss_gsi_chan_err_cb(struct gsi_chan_err_notify *notify)
{
	switch (notify->evt_id) {
	case GSI_CHAN_INVALID_TRE_ERR:
			IPAERR("Got GSI_CHAN_INVALID_TRE_ERR\n");
			break;
	case GSI_CHAN_NON_ALLOCATED_EVT_ACCESS_ERR:
			IPAERR("Got GSI_CHAN_NON_ALLOCATED_EVT_ACCESS_ERR\n");
			break;
	case GSI_CHAN_OUT_OF_BUFFERS_ERR:
			IPAERR("Got GSI_CHAN_OUT_OF_BUFFERS_ERR\n");
			break;
	case GSI_CHAN_OUT_OF_RESOURCES_ERR:
			IPAERR("Got GSI_CHAN_OUT_OF_RESOURCES_ERR\n");
			break;
	case GSI_CHAN_UNSUPPORTED_INTER_EE_OP_ERR:
			IPAERR("Got GSI_CHAN_UNSUPPORTED_INTER_EE_OP_ERR\n");
			break;
	case GSI_CHAN_HWO_1_ERR:
			IPAERR("Got GSI_CHAN_HWO_1_ERR\n");
			break;
	default:
		IPAERR("Unexpected err evt: %d\n", notify->evt_id);
	}
	ipa_assert();
}

int ipa3_conn_qdss_pipes(struct ipa_qdss_conn_in_params *in,
	struct ipa_qdss_conn_out_params *out)
{
	struct gsi_chan_props gsi_channel_props;
	struct ipa3_ep_context *ep_rx;
	const struct ipa_gsi_ep_config *gsi_ep_info;
	union __packed gsi_channel_scratch ch_scratch;
	u32 gsi_db_addr_low, gsi_db_addr_high;
	struct ipa_ep_cfg ep_cfg = { { 0 } };
	int ipa_ep_idx_rx, ipa_ep_idx_tx;
	int result = 0;
	struct ipa_ep_cfg_holb holb_cfg;

	if (!(in && out)) {
		IPA_QDSS_ERR("Empty parameters. in=%pK out=%pK\n", in, out);
		return -IPA_QDSS_PIPE_CONN_FAILURE;
	}

	ipa_ep_idx_tx = ipa3_get_ep_mapping(IPA_CLIENT_MHI_QDSS_CONS);
	if ((ipa_ep_idx_tx) < 0 || (!ipa3_ctx->ipa_config_is_mhi)) {
		IPA_QDSS_ERR("getting EP map failed\n");
		return -IPA_QDSS_PIPE_CONN_FAILURE;
	}

	ipa_ep_idx_rx = ipa3_get_ep_mapping(IPA_CLIENT_QDSS_PROD);
	if ((ipa_ep_idx_rx == -1) ||
		(ipa_ep_idx_rx >= IPA3_MAX_NUM_PIPES)) {
		IPA_QDSS_ERR("out of range ipa_ep_idx_rx = %d\n",
			ipa_ep_idx_rx);
		return -IPA_QDSS_PIPE_CONN_FAILURE;
	}

	ep_rx = &ipa3_ctx->ep[ipa_ep_idx_rx];

	if (ep_rx->valid) {
		IPA_QDSS_ERR("EP already allocated.\n");
		return IPA_QDSS_SUCCESS;
	}

	memset(ep_rx, 0, offsetof(struct ipa3_ep_context, sys));

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	ep_rx->valid = 1;
	ep_rx->client = IPA_CLIENT_QDSS_PROD;
	if (ipa3_cfg_ep(ipa_ep_idx_rx, &ep_rx->cfg)) {
		IPAERR("fail to setup rx pipe cfg\n");
		goto fail;
	}

	/* setup channel ring */
	memset(&gsi_channel_props, 0, sizeof(gsi_channel_props));
	gsi_channel_props.prot = GSI_CHAN_PROT_QDSS;
	gsi_channel_props.dir = GSI_CHAN_DIR_TO_GSI;

	gsi_ep_info = ipa3_get_gsi_ep_info(ep_rx->client);
	if (!gsi_ep_info) {
		IPA_QDSS_ERR("Failed getting GSI EP info for client=%d\n",
			ep_rx->client);
		goto fail;
	}

	gsi_channel_props.ch_id = gsi_ep_info->ipa_gsi_chan_num;
	gsi_channel_props.re_size = GSI_CHAN_RE_SIZE_8B;
	gsi_channel_props.use_db_eng = GSI_CHAN_DB_MODE;
	gsi_channel_props.err_cb = ipa3_qdss_gsi_chan_err_cb;
	gsi_channel_props.ring_len = in->desc_fifo_size;
	gsi_channel_props.ring_base_addr =
			in->desc_fifo_base_addr;
	result = gsi_alloc_channel(&gsi_channel_props, ipa3_ctx->gsi_dev_hdl,
				&ep_rx->gsi_chan_hdl);
	if (result != GSI_STATUS_SUCCESS) {
		IPA_QDSS_ERR("Failed allocating gsi_chan_hdl=%d\n",
				&ep_rx->gsi_chan_hdl);
		goto fail;
	}

	ep_rx->gsi_mem_info.chan_ring_len = gsi_channel_props.ring_len;
	ep_rx->gsi_mem_info.chan_ring_base_addr =
				gsi_channel_props.ring_base_addr;

	/* write channel scratch, do we need this? */
	memset(&ch_scratch, 0, sizeof(ch_scratch));
	ch_scratch.qdss.bam_p_evt_dest_addr = in->bam_p_evt_dest_addr;
	ch_scratch.qdss.data_fifo_base_addr = in->data_fifo_base_addr;
	ch_scratch.qdss.data_fifo_size = in->data_fifo_size;
	ch_scratch.qdss.bam_p_evt_threshold = in->bam_p_evt_threshold;
	ch_scratch.qdss.override_eot = in->override_eot;
	result = gsi_write_channel_scratch(
				ep_rx->gsi_chan_hdl, ch_scratch);
	if (result != GSI_STATUS_SUCCESS) {
		IPA_QDSS_ERR("failed to write channel scratch\n");
		goto fail_write_scratch;
	}

	/* query channel db address */
	if (gsi_query_channel_db_addr(ep_rx->gsi_chan_hdl,
		&gsi_db_addr_low, &gsi_db_addr_high)) {
		IPA_QDSS_ERR("failed to query gsi rx db addr\n");
		goto fail_write_scratch;
	}
	out->ipa_rx_db_pa = (phys_addr_t)(gsi_db_addr_low);
	IPA_QDSS_DBG("QDSS out->ipa_rx_db_pa %llu\n", out->ipa_rx_db_pa);

	/* Configuring HOLB on MHI endpoint */
	memset(&holb_cfg, 0, sizeof(holb_cfg));
	holb_cfg.en = IPA_HOLB_TMR_EN;
	holb_cfg.tmr_val = IPA_HOLB_TMR_VALUE;
	result = ipa3_force_cfg_ep_holb(ipa_ep_idx_tx, &holb_cfg);
	if (result)
		IPA_QDSS_ERR("Configuring HOLB failed client_type =%d\n",
			IPA_CLIENT_MHI_QDSS_CONS);

	/* Set DMA */
	IPA_QDSS_DBG("DMA from %d to %d", IPA_CLIENT_QDSS_PROD,
		IPA_CLIENT_MHI_QDSS_CONS);
	ep_cfg.mode.mode = IPA_DMA;
	ep_cfg.mode.dst = IPA_CLIENT_MHI_QDSS_CONS;
	ep_cfg.seq.set_dynamic = true;
	if (ipa3_cfg_ep(ipa3_get_ep_mapping(IPA_CLIENT_QDSS_PROD),
		&ep_cfg)) {
		IPA_QDSS_ERR("Setting DMA mode failed\n");
		goto fail_write_scratch;
	}

	/* Start QDSS_rx gsi channel */
	result = ipa3_start_gsi_channel(ipa_ep_idx_rx);
	if (result) {
		IPA_QDSS_ERR("Failed starting QDSS gsi channel\n");
		goto fail_write_scratch;
	}

	IPA_QDSS_DBG("QDSS connect pipe success");

	return IPA_QDSS_SUCCESS;

fail_write_scratch:
	gsi_dealloc_channel(ep_rx->gsi_chan_hdl);
	memset(ep_rx, 0, sizeof(struct ipa3_ep_context));
fail:
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	return -IPA_QDSS_PIPE_CONN_FAILURE;
}

int ipa3_disconn_qdss_pipes(void)
{
	int result = 0;
	int ipa_ep_idx_rx;
	struct ipa3_ep_context *ep_rx;
	struct ipa_ep_cfg ep_cfg = { { 0 } };

	ipa_ep_idx_rx = ipa_get_ep_mapping(IPA_CLIENT_QDSS_PROD);
	if (ipa_ep_idx_rx == -1) {
		IPA_QDSS_ERR("fail to get ep mapping\n");
		return -IPA_QDSS_PIPE_DISCONN_FAILURE;
	}

	if (ipa_ep_idx_rx >= IPA3_MAX_NUM_PIPES) {
		IPA_QDSS_ERR("ep out of range.\n");
		return -IPA_QDSS_PIPE_DISCONN_FAILURE;
	}

	/* Stop QDSS_rx gsi channel / release channel */
	result = ipa3_stop_gsi_channel(ipa_ep_idx_rx);
	if (result) {
		IPA_QDSS_ERR("Failed stopping QDSS gsi channel\n");
		goto fail;
	}

	/* Resetting gsi channel */
	result = ipa3_reset_gsi_channel(ipa_ep_idx_rx);
	if (result) {
		IPA_QDSS_ERR("Failed resetting QDSS gsi channel\n");
		goto fail;
	}

	/* Reset DMA */
	IPA_QDSS_ERR("Resetting DMA %d to %d",
		IPA_CLIENT_QDSS_PROD, IPA_CLIENT_MHI_QDSS_CONS);
	ep_cfg.mode.mode = IPA_BASIC;
	ep_cfg.mode.dst = IPA_CLIENT_MHI_QDSS_CONS;
	ep_cfg.seq.set_dynamic = true;
	if (ipa3_cfg_ep(ipa3_get_ep_mapping(IPA_CLIENT_QDSS_PROD),
		&ep_cfg)) {
		IPAERR("Resetting DMA mode failed\n");
	}

	/* Deallocating and Clearing ep config */
	ep_rx = &ipa3_ctx->ep[ipa_ep_idx_rx];
	gsi_dealloc_channel(ep_rx->gsi_chan_hdl);
	memset(ep_rx, 0, sizeof(struct ipa3_ep_context));

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	IPA_QDSS_DBG("QDSS disconnect pipe success");

	return IPA_QDSS_SUCCESS;
fail:
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	return -IPA_QDSS_PIPE_DISCONN_FAILURE;
}
