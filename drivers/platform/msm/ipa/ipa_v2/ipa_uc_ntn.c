/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#include "ipa_i.h"

#define IPA_UC_NTN_DB_PA_TX 0x79620DC
#define IPA_UC_NTN_DB_PA_RX 0x79620D8

static void ipa_uc_ntn_event_handler(
		struct IpaHwSharedMemCommonMapping_t *uc_sram_mmio)
{
	union IpaHwNTNErrorEventData_t ntn_evt;

	if (uc_sram_mmio->eventOp == IPA_HW_2_CPU_EVENT_NTN_ERROR) {
		ntn_evt.raw32b = uc_sram_mmio->eventParams;
		IPADBG("uC NTN evt errType=%u pipe=%d cherrType=%u\n",
			ntn_evt.params.ntn_error_type,
			ntn_evt.params.ipa_pipe_number,
			ntn_evt.params.ntn_ch_err_type);
	}
}

static void ipa_uc_ntn_event_log_info_handler(
		struct IpaHwEventLogInfoData_t *uc_event_top_mmio)
{
	if ((uc_event_top_mmio->featureMask & (1 << IPA_HW_FEATURE_NTN)) == 0) {
		IPAERR("NTN feature missing 0x%x\n",
			uc_event_top_mmio->featureMask);
		return;
	}

	if (uc_event_top_mmio->statsInfo.featureInfo[IPA_HW_FEATURE_NTN].
		params.size != sizeof(struct IpaHwStatsNTNInfoData_t)) {
		IPAERR("NTN stats sz invalid exp=%zu is=%u\n",
			sizeof(struct IpaHwStatsNTNInfoData_t),
			uc_event_top_mmio->statsInfo.
			featureInfo[IPA_HW_FEATURE_NTN].params.size);
		return;
	}

	ipa_ctx->uc_ntn_ctx.ntn_uc_stats_ofst = uc_event_top_mmio->
		statsInfo.baseAddrOffset + uc_event_top_mmio->statsInfo.
		featureInfo[IPA_HW_FEATURE_NTN].params.offset;
	IPAERR("NTN stats ofst=0x%x\n", ipa_ctx->uc_ntn_ctx.ntn_uc_stats_ofst);
	if (ipa_ctx->uc_ntn_ctx.ntn_uc_stats_ofst +
		sizeof(struct IpaHwStatsNTNInfoData_t) >=
		ipa_ctx->ctrl->ipa_reg_base_ofst +
		IPA_SRAM_DIRECT_ACCESS_N_OFST_v2_0(0) +
		ipa_ctx->smem_sz) {
		IPAERR("uc_ntn_stats 0x%x outside SRAM\n",
			ipa_ctx->uc_ntn_ctx.ntn_uc_stats_ofst);
		return;
	}

	ipa_ctx->uc_ntn_ctx.ntn_uc_stats_mmio =
		ioremap(ipa_ctx->ipa_wrapper_base +
		ipa_ctx->uc_ntn_ctx.ntn_uc_stats_ofst,
		sizeof(struct IpaHwStatsNTNInfoData_t));
	if (!ipa_ctx->uc_ntn_ctx.ntn_uc_stats_mmio) {
		IPAERR("fail to ioremap uc ntn stats\n");
		return;
	}
}

/**
 * ipa2_get_wdi_stats() - Query WDI statistics from uc
 * @stats:	[inout] stats blob from client populated by driver
 *
 * Returns:	0 on success, negative on failure
 *
 * @note Cannot be called from atomic context
 *
 */
int ipa2_get_ntn_stats(struct IpaHwStatsNTNInfoData_t *stats)
{
#define TX_STATS(y) stats->tx_ch_stats[0].y = \
	ipa_ctx->uc_ntn_ctx.ntn_uc_stats_mmio->tx_ch_stats[0].y
#define RX_STATS(y) stats->rx_ch_stats[0].y = \
	ipa_ctx->uc_ntn_ctx.ntn_uc_stats_mmio->rx_ch_stats[0].y

	if (unlikely(!ipa_ctx)) {
		IPAERR("IPA driver was not initialized\n");
		return -EINVAL;
	}

	if (!stats || !ipa_ctx->uc_ntn_ctx.ntn_uc_stats_mmio) {
		IPAERR("bad parms stats=%p ntn_stats=%p\n",
			stats,
			ipa_ctx->uc_ntn_ctx.ntn_uc_stats_mmio);
		return -EINVAL;
	}

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	TX_STATS(num_pkts_processed);
	TX_STATS(tail_ptr_val);
	TX_STATS(num_db_fired);
	TX_STATS(tx_comp_ring_stats.ringFull);
	TX_STATS(tx_comp_ring_stats.ringEmpty);
	TX_STATS(tx_comp_ring_stats.ringUsageHigh);
	TX_STATS(tx_comp_ring_stats.ringUsageLow);
	TX_STATS(tx_comp_ring_stats.RingUtilCount);
	TX_STATS(bam_stats.bamFifoFull);
	TX_STATS(bam_stats.bamFifoEmpty);
	TX_STATS(bam_stats.bamFifoUsageHigh);
	TX_STATS(bam_stats.bamFifoUsageLow);
	TX_STATS(bam_stats.bamUtilCount);
	TX_STATS(num_db);
	TX_STATS(num_unexpected_db);
	TX_STATS(num_bam_int_handled);
	TX_STATS(num_bam_int_in_non_running_state);
	TX_STATS(num_qmb_int_handled);
	TX_STATS(num_bam_int_handled_while_wait_for_bam);
	TX_STATS(num_bam_int_handled_while_not_in_bam);

	RX_STATS(max_outstanding_pkts);
	RX_STATS(num_pkts_processed);
	RX_STATS(rx_ring_rp_value);
	RX_STATS(rx_ind_ring_stats.ringFull);
	RX_STATS(rx_ind_ring_stats.ringEmpty);
	RX_STATS(rx_ind_ring_stats.ringUsageHigh);
	RX_STATS(rx_ind_ring_stats.ringUsageLow);
	RX_STATS(rx_ind_ring_stats.RingUtilCount);
	RX_STATS(bam_stats.bamFifoFull);
	RX_STATS(bam_stats.bamFifoEmpty);
	RX_STATS(bam_stats.bamFifoUsageHigh);
	RX_STATS(bam_stats.bamFifoUsageLow);
	RX_STATS(bam_stats.bamUtilCount);
	RX_STATS(num_bam_int_handled);
	RX_STATS(num_db);
	RX_STATS(num_unexpected_db);
	RX_STATS(num_pkts_in_dis_uninit_state);
	RX_STATS(num_bam_int_handled_while_not_in_bam);
	RX_STATS(num_bam_int_handled_while_in_bam_state);

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	return 0;
}

int ipa2_register_ipa_ready_cb(void (*ipa_ready_cb)(void *), void *user_data)
{
	int ret;

	if (!ipa_ctx) {
		IPAERR("IPA ctx is null\n");
		return -ENXIO;
	}

	ret = ipa2_uc_state_check();
	if (ret) {
		ipa_ctx->uc_ntn_ctx.uc_ready_cb = ipa_ready_cb;
		ipa_ctx->uc_ntn_ctx.priv = user_data;
		return 0;
	}

	return -EEXIST;
}

static void ipa_uc_ntn_loaded_handler(void)
{
	if (!ipa_ctx) {
		IPAERR("IPA ctx is null\n");
		return;
	}

	if (ipa_ctx->uc_ntn_ctx.uc_ready_cb) {
		ipa_ctx->uc_ntn_ctx.uc_ready_cb(
			ipa_ctx->uc_ntn_ctx.priv);

		ipa_ctx->uc_ntn_ctx.uc_ready_cb =
			NULL;
		ipa_ctx->uc_ntn_ctx.priv = NULL;
	}
}

int ipa_ntn_init(void)
{
	struct ipa_uc_hdlrs uc_ntn_cbs = { 0 };

	uc_ntn_cbs.ipa_uc_event_hdlr = ipa_uc_ntn_event_handler;
	uc_ntn_cbs.ipa_uc_event_log_info_hdlr =
		ipa_uc_ntn_event_log_info_handler;
	uc_ntn_cbs.ipa_uc_loaded_hdlr =
		ipa_uc_ntn_loaded_handler;

	ipa_uc_register_handlers(IPA_HW_FEATURE_NTN, &uc_ntn_cbs);

	return 0;
}

static int ipa2_uc_send_ntn_setup_pipe_cmd(
	struct ipa_ntn_setup_info *ntn_info, u8 dir)
{
	int ipa_ep_idx;
	int result = 0;
	struct ipa_mem_buffer cmd;
	struct IpaHwNtnSetUpCmdData_t *Ntn_params;
	struct IpaHwOffloadSetUpCmdData_t *cmd_data;

	if (ntn_info == NULL) {
		IPAERR("invalid input\n");
		return -EINVAL;
	}

	ipa_ep_idx = ipa_get_ep_mapping(ntn_info->client);
	if (ipa_ep_idx == -1) {
		IPAERR("fail to get ep idx.\n");
		return -EFAULT;
	}

	IPADBG("client=%d ep=%d\n", ntn_info->client, ipa_ep_idx);

	IPADBG("ring_base_pa = 0x%pa\n",
			&ntn_info->ring_base_pa);
	IPADBG("ntn_ring_size = %d\n", ntn_info->ntn_ring_size);
	IPADBG("buff_pool_base_pa = 0x%pa\n", &ntn_info->buff_pool_base_pa);
	IPADBG("num_buffers = %d\n", ntn_info->num_buffers);
	IPADBG("data_buff_size = %d\n", ntn_info->data_buff_size);
	IPADBG("tail_ptr_base_pa = 0x%pa\n", &ntn_info->ntn_reg_base_ptr_pa);

	cmd.size = sizeof(*cmd_data);
	cmd.base = dma_alloc_coherent(ipa_ctx->uc_pdev, cmd.size,
			&cmd.phys_base, GFP_KERNEL);
	if (cmd.base == NULL) {
		IPAERR("fail to get DMA memory.\n");
		return -ENOMEM;
	}

	cmd_data = (struct IpaHwOffloadSetUpCmdData_t *)cmd.base;
	cmd_data->protocol = IPA_HW_FEATURE_NTN;

	Ntn_params = &cmd_data->SetupCh_params.NtnSetupCh_params;
	Ntn_params->ring_base_pa = ntn_info->ring_base_pa;
	Ntn_params->buff_pool_base_pa = ntn_info->buff_pool_base_pa;
	Ntn_params->ntn_ring_size = ntn_info->ntn_ring_size;
	Ntn_params->num_buffers = ntn_info->num_buffers;
	Ntn_params->ntn_reg_base_ptr_pa = ntn_info->ntn_reg_base_ptr_pa;
	Ntn_params->data_buff_size = ntn_info->data_buff_size;
	Ntn_params->ipa_pipe_number = ipa_ep_idx;
	Ntn_params->dir = dir;

	result = ipa_uc_send_cmd((u32)(cmd.phys_base),
				IPA_CPU_2_HW_CMD_OFFLOAD_CHANNEL_SET_UP,
				IPA_HW_2_CPU_OFFLOAD_CMD_STATUS_SUCCESS,
				false, 10*HZ);
	if (result)
		result = -EFAULT;

	dma_free_coherent(ipa_ctx->uc_pdev, cmd.size, cmd.base, cmd.phys_base);
	return result;
}

/**
 * ipa2_setup_uc_ntn_pipes() - setup uc offload pipes
 */
int ipa2_setup_uc_ntn_pipes(struct ipa_ntn_conn_in_params *in,
	ipa_notify_cb notify, void *priv, u8 hdr_len,
	struct ipa_ntn_conn_out_params *outp)
{
	int ipa_ep_idx_ul, ipa_ep_idx_dl;
	struct ipa_ep_context *ep_ul, *ep_dl;
	int result = 0;

	if (in == NULL) {
		IPAERR("invalid input\n");
		return -EINVAL;
	}

	ipa_ep_idx_ul = ipa_get_ep_mapping(in->ul.client);
	ipa_ep_idx_dl = ipa_get_ep_mapping(in->dl.client);
	if (ipa_ep_idx_ul == -1 || ipa_ep_idx_dl == -1) {
		IPAERR("fail to alloc EP.\n");
		return -EFAULT;
	}

	ep_ul = &ipa_ctx->ep[ipa_ep_idx_ul];
	ep_dl = &ipa_ctx->ep[ipa_ep_idx_dl];

	if (ep_ul->valid || ep_dl->valid) {
		IPAERR("EP already allocated ul:%d dl:%d\n",
			   ep_ul->valid, ep_dl->valid);
		return -EFAULT;
	}

	memset(ep_ul, 0, offsetof(struct ipa_ep_context, sys));
	memset(ep_dl, 0, offsetof(struct ipa_ep_context, sys));

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	/* setup ul ep cfg */
	ep_ul->valid = 1;
	ep_ul->client = in->ul.client;
	ep_ul->client_notify = notify;
	ep_ul->priv = priv;

	memset(&ep_ul->cfg, 0, sizeof(ep_ul->cfg));
	ep_ul->cfg.nat.nat_en = IPA_SRC_NAT;
	ep_ul->cfg.hdr.hdr_len = hdr_len;
	ep_ul->cfg.mode.mode = IPA_BASIC;

	if (ipa2_cfg_ep(ipa_ep_idx_ul, &ep_ul->cfg)) {
		IPAERR("fail to setup ul pipe cfg\n");
		result = -EFAULT;
		goto fail;
	}

	if (ipa2_uc_send_ntn_setup_pipe_cmd(&in->ul, IPA_NTN_RX_DIR)) {
		IPAERR("fail to send cmd to uc for ul pipe\n");
		result = -EFAULT;
		goto fail;
	}
	ipa_install_dflt_flt_rules(ipa_ep_idx_ul);
	outp->ul_uc_db_pa = IPA_UC_NTN_DB_PA_RX;
	ep_ul->uc_offload_state |= IPA_UC_OFFLOAD_CONNECTED;
	IPAERR("client %d (ep: %d) connected\n", in->ul.client,
		ipa_ep_idx_ul);

	/* setup dl ep cfg */
	ep_dl->valid = 1;
	ep_dl->client = in->dl.client;
	memset(&ep_dl->cfg, 0, sizeof(ep_ul->cfg));
	ep_dl->cfg.nat.nat_en = IPA_BYPASS_NAT;
	ep_dl->cfg.hdr.hdr_len = hdr_len;
	ep_dl->cfg.mode.mode = IPA_BASIC;

	if (ipa2_cfg_ep(ipa_ep_idx_dl, &ep_dl->cfg)) {
		IPAERR("fail to setup dl pipe cfg\n");
		result = -EFAULT;
		goto fail;
	}

	if (ipa2_uc_send_ntn_setup_pipe_cmd(&in->dl, IPA_NTN_TX_DIR)) {
		IPAERR("fail to send cmd to uc for dl pipe\n");
		result = -EFAULT;
		goto fail;
	}
	outp->dl_uc_db_pa = IPA_UC_NTN_DB_PA_TX;
	ep_dl->uc_offload_state |= IPA_UC_OFFLOAD_CONNECTED;

	result = ipa_enable_data_path(ipa_ep_idx_dl);
	if (result) {
		IPAERR("Enable data path failed res=%d clnt=%d.\n", result,
			ipa_ep_idx_dl);
		result = -EFAULT;
		goto fail;
	}
	IPAERR("client %d (ep: %d) connected\n", in->dl.client,
		ipa_ep_idx_dl);

fail:
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	return result;
}

/**
 * ipa2_tear_down_uc_offload_pipes() - tear down uc offload pipes
 */

int ipa2_tear_down_uc_offload_pipes(int ipa_ep_idx_ul,
		int ipa_ep_idx_dl)
{
	struct ipa_mem_buffer cmd;
	struct ipa_ep_context *ep_ul, *ep_dl;
	struct IpaHwOffloadCommonChCmdData_t *cmd_data;
	union IpaHwNtnCommonChCmdData_t *tear;
	int result = 0;

	IPADBG("ep_ul = %d\n", ipa_ep_idx_ul);
	IPADBG("ep_dl = %d\n", ipa_ep_idx_dl);

	ep_ul = &ipa_ctx->ep[ipa_ep_idx_ul];
	ep_dl = &ipa_ctx->ep[ipa_ep_idx_dl];

	if (ep_ul->uc_offload_state != IPA_UC_OFFLOAD_CONNECTED ||
		ep_dl->uc_offload_state != IPA_UC_OFFLOAD_CONNECTED) {
		IPAERR("channel bad state: ul %d dl %d\n",
			ep_ul->uc_offload_state, ep_dl->uc_offload_state);
		return -EFAULT;
	}

	cmd.size = sizeof(*cmd_data);
	cmd.base = dma_alloc_coherent(ipa_ctx->uc_pdev, cmd.size,
		&cmd.phys_base, GFP_KERNEL);
	if (cmd.base == NULL) {
		IPAERR("fail to get DMA memory.\n");
		return -ENOMEM;
	}

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	cmd_data = (struct IpaHwOffloadCommonChCmdData_t *)cmd.base;
	cmd_data->protocol = IPA_HW_FEATURE_NTN;
	tear = &cmd_data->CommonCh_params.NtnCommonCh_params;

	/* teardown the DL pipe */
	ipa_disable_data_path(ipa_ep_idx_dl);
	/*
	 * Reset ep before sending cmd otherwise disconnect
	 * during data transfer will result into
	 * enormous suspend interrupts
	*/
	memset(&ipa_ctx->ep[ipa_ep_idx_dl], 0, sizeof(struct ipa_ep_context));
	IPADBG("dl client (ep: %d) disconnected\n", ipa_ep_idx_dl);
	tear->params.ipa_pipe_number = ipa_ep_idx_dl;
	result = ipa_uc_send_cmd((u32)(cmd.phys_base),
				IPA_CPU_2_HW_CMD_OFFLOAD_TEAR_DOWN,
				IPA_HW_2_CPU_OFFLOAD_CMD_STATUS_SUCCESS,
				false, 10*HZ);
	if (result) {
		IPAERR("fail to tear down dl pipe\n");
		result = -EFAULT;
		goto fail;
	}

	/* teardown the UL pipe */
	tear->params.ipa_pipe_number = ipa_ep_idx_ul;
	result = ipa_uc_send_cmd((u32)(cmd.phys_base),
				IPA_CPU_2_HW_CMD_OFFLOAD_TEAR_DOWN,
				IPA_HW_2_CPU_OFFLOAD_CMD_STATUS_SUCCESS,
				false, 10*HZ);
	if (result) {
		IPAERR("fail to tear down ul pipe\n");
		result = -EFAULT;
		goto fail;
	}

	ipa_delete_dflt_flt_rules(ipa_ep_idx_ul);
	memset(&ipa_ctx->ep[ipa_ep_idx_ul], 0, sizeof(struct ipa_ep_context));
	IPADBG("ul client (ep: %d) disconnected\n", ipa_ep_idx_ul);

fail:
	dma_free_coherent(ipa_ctx->uc_pdev, cmd.size, cmd.base, cmd.phys_base);
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	return result;
}
