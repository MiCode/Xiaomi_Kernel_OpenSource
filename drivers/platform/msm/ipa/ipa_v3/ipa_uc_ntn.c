/* Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
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

static void ipa3_uc_ntn_event_log_info_handler(
struct IpaHwEventLogInfoData_t *uc_event_top_mmio)
{
	struct Ipa3HwEventInfoData_t *statsPtr = &uc_event_top_mmio->statsInfo;

	if ((uc_event_top_mmio->protocolMask &
		(1 << IPA_HW_PROTOCOL_ETH)) == 0) {
		IPAERR("NTN protocol missing 0x%x\n",
			uc_event_top_mmio->protocolMask);
		return;
	}

	if (statsPtr->featureInfo[IPA_HW_PROTOCOL_ETH].params.size !=
		sizeof(struct Ipa3HwStatsNTNInfoData_t)) {
		IPAERR("NTN stats sz invalid exp=%zu is=%u\n",
			sizeof(struct Ipa3HwStatsNTNInfoData_t),
			statsPtr->featureInfo[IPA_HW_PROTOCOL_ETH].params.size);
		return;
	}

	ipa3_ctx->uc_ntn_ctx.ntn_uc_stats_ofst =
		uc_event_top_mmio->statsInfo.baseAddrOffset +
		statsPtr->featureInfo[IPA_HW_PROTOCOL_ETH].params.offset;
	IPAERR("NTN stats ofst=0x%x\n", ipa3_ctx->uc_ntn_ctx.ntn_uc_stats_ofst);
	if (ipa3_ctx->uc_ntn_ctx.ntn_uc_stats_ofst +
		sizeof(struct Ipa3HwStatsNTNInfoData_t) >=
		ipa3_ctx->ctrl->ipa_reg_base_ofst +
		ipahal_get_reg_n_ofst(IPA_SW_AREA_RAM_DIRECT_ACCESS_n, 0) +
		ipa3_ctx->smem_sz) {
		IPAERR("uc_ntn_stats 0x%x outside SRAM\n",
			   ipa3_ctx->uc_ntn_ctx.ntn_uc_stats_ofst);
		return;
	}

	ipa3_ctx->uc_ntn_ctx.ntn_uc_stats_mmio =
		ioremap(ipa3_ctx->ipa_wrapper_base +
		ipa3_ctx->uc_ntn_ctx.ntn_uc_stats_ofst,
		sizeof(struct Ipa3HwStatsNTNInfoData_t));
	if (!ipa3_ctx->uc_ntn_ctx.ntn_uc_stats_mmio) {
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
int ipa3_get_ntn_stats(struct Ipa3HwStatsNTNInfoData_t *stats)
{
#define TX_STATS(x, y) stats->tx_ch_stats[x].y = \
	ipa3_ctx->uc_ntn_ctx.ntn_uc_stats_mmio->tx_ch_stats[x].y
#define RX_STATS(x, y) stats->rx_ch_stats[x].y = \
	ipa3_ctx->uc_ntn_ctx.ntn_uc_stats_mmio->rx_ch_stats[x].y

	int i = 0;

	if (unlikely(!ipa3_ctx)) {
		IPAERR("IPA driver was not initialized\n");
		return -EINVAL;
	}

	if (!stats || !ipa3_ctx->uc_ntn_ctx.ntn_uc_stats_mmio) {
		IPAERR("bad parms stats=%pK ntn_stats=%pK\n",
			stats,
			ipa3_ctx->uc_ntn_ctx.ntn_uc_stats_mmio);
		return -EINVAL;
	}

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	for (i = 0; i < IPA_UC_MAX_NTN_TX_CHANNELS; i++) {
		TX_STATS(i, num_pkts_processed);
		TX_STATS(i, ring_stats.ringFull);
		TX_STATS(i, ring_stats.ringEmpty);
		TX_STATS(i, ring_stats.ringUsageHigh);
		TX_STATS(i, ring_stats.ringUsageLow);
		TX_STATS(i, ring_stats.RingUtilCount);
		TX_STATS(i, gsi_stats.bamFifoFull);
		TX_STATS(i, gsi_stats.bamFifoEmpty);
		TX_STATS(i, gsi_stats.bamFifoUsageHigh);
		TX_STATS(i, gsi_stats.bamFifoUsageLow);
		TX_STATS(i, gsi_stats.bamUtilCount);
		TX_STATS(i, num_db);
		TX_STATS(i, num_qmb_int_handled);
		TX_STATS(i, ipa_pipe_number);
	}

	for (i = 0; i < IPA_UC_MAX_NTN_RX_CHANNELS; i++) {
		RX_STATS(i, num_pkts_processed);
		RX_STATS(i, ring_stats.ringFull);
		RX_STATS(i, ring_stats.ringEmpty);
		RX_STATS(i, ring_stats.ringUsageHigh);
		RX_STATS(i, ring_stats.ringUsageLow);
		RX_STATS(i, ring_stats.RingUtilCount);
		RX_STATS(i, gsi_stats.bamFifoFull);
		RX_STATS(i, gsi_stats.bamFifoEmpty);
		RX_STATS(i, gsi_stats.bamFifoUsageHigh);
		RX_STATS(i, gsi_stats.bamFifoUsageLow);
		RX_STATS(i, gsi_stats.bamUtilCount);
		RX_STATS(i, num_db);
		RX_STATS(i, num_qmb_int_handled);
		RX_STATS(i, ipa_pipe_number);
	}

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	return 0;
}


int ipa3_ntn_uc_reg_rdyCB(void (*ipa_ready_cb)(void *), void *user_data)
{
	int ret;

	if (!ipa3_ctx) {
		IPAERR("IPA ctx is null\n");
		return -ENXIO;
	}

	ret = ipa3_uc_state_check();
	if (ret) {
		ipa3_ctx->uc_ntn_ctx.uc_ready_cb = ipa_ready_cb;
		ipa3_ctx->uc_ntn_ctx.priv = user_data;
		return 0;
	}

	return -EEXIST;
}

void ipa3_ntn_uc_dereg_rdyCB(void)
{
	ipa3_ctx->uc_ntn_ctx.uc_ready_cb = NULL;
	ipa3_ctx->uc_ntn_ctx.priv = NULL;
}

static void ipa3_uc_ntn_loaded_handler(void)
{
	if (!ipa3_ctx) {
		IPAERR("IPA ctx is null\n");
		return;
	}

	if (ipa3_ctx->uc_ntn_ctx.uc_ready_cb) {
		ipa3_ctx->uc_ntn_ctx.uc_ready_cb(
			ipa3_ctx->uc_ntn_ctx.priv);

		ipa3_ctx->uc_ntn_ctx.uc_ready_cb =
			NULL;
		ipa3_ctx->uc_ntn_ctx.priv = NULL;
	}
}

int ipa3_ntn_init(void)
{
	struct ipa3_uc_hdlrs uc_ntn_cbs = { 0 };

	uc_ntn_cbs.ipa_uc_event_log_info_hdlr =
		ipa3_uc_ntn_event_log_info_handler;
	uc_ntn_cbs.ipa_uc_loaded_hdlr =
		ipa3_uc_ntn_loaded_handler;

	ipa3_uc_register_handlers(IPA_HW_FEATURE_NTN, &uc_ntn_cbs);

	/* ntn_init */
	ipa3_ctx->uc_ntn_ctx.uc_ready_cb = NULL;
	ipa3_ctx->uc_ntn_ctx.priv = NULL;
	ipa3_ctx->uc_ntn_ctx.ntn_reg_base_ptr_pa_rd = 0x0;
	ipa3_ctx->uc_ntn_ctx.smmu_mapped = 0;

	return 0;
}

static int ipa3_uc_send_ntn_setup_pipe_cmd(
	struct ipa_ntn_setup_info *ntn_info, u8 dir)
{
	int ipa_ep_idx;
	int result = 0;
	struct ipa_mem_buffer cmd;
	struct Ipa3HwNtnSetUpCmdData_t *Ntn_params;
	struct IpaHwOffloadSetUpCmdData_t *cmd_data;
	struct IpaHwOffloadSetUpCmdData_t_v4_0 *cmd_data_v4_0;

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
	IPADBG("ring_base_iova = 0x%pa\n",
			&ntn_info->ring_base_iova);
	IPADBG("ntn_ring_size = %d\n", ntn_info->ntn_ring_size);
	IPADBG("buff_pool_base_pa = 0x%pa\n", &ntn_info->buff_pool_base_pa);
	IPADBG("buff_pool_base_iova = 0x%pa\n", &ntn_info->buff_pool_base_iova);
	IPADBG("num_buffers = %d\n", ntn_info->num_buffers);
	IPADBG("data_buff_size = %d\n", ntn_info->data_buff_size);
	IPADBG("tail_ptr_base_pa = 0x%pa\n", &ntn_info->ntn_reg_base_ptr_pa);
	IPADBG("db_mode = %d\n", ntn_info->db_mode);
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0)
		cmd.size = sizeof(*cmd_data_v4_0);
	else
		cmd.size = sizeof(*cmd_data);
	cmd.base = dma_alloc_coherent(ipa3_ctx->uc_pdev, cmd.size,
			&cmd.phys_base, GFP_KERNEL);
	if (cmd.base == NULL) {
		IPAERR("fail to get DMA memory.\n");
		return -ENOMEM;
	}

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0) {
		cmd_data_v4_0 = (struct IpaHwOffloadSetUpCmdData_t_v4_0 *)
			cmd.base;
		cmd_data_v4_0->protocol = IPA_HW_PROTOCOL_ETH;
		Ntn_params = &cmd_data_v4_0->SetupCh_params.NtnSetupCh_params;
	} else {
		cmd_data = (struct IpaHwOffloadSetUpCmdData_t *)cmd.base;
		cmd_data->protocol = IPA_HW_PROTOCOL_ETH;
		Ntn_params = &cmd_data->SetupCh_params.NtnSetupCh_params;
	}

	if (ntn_info->smmu_enabled) {
		Ntn_params->ring_base_pa = (u32)ntn_info->ring_base_iova;
		Ntn_params->buff_pool_base_pa =
			(u32)ntn_info->buff_pool_base_iova;
	} else {
		Ntn_params->ring_base_pa = ntn_info->ring_base_pa;
		Ntn_params->buff_pool_base_pa = ntn_info->buff_pool_base_pa;
	}

	Ntn_params->ntn_ring_size = ntn_info->ntn_ring_size;
	Ntn_params->num_buffers = ntn_info->num_buffers;
	Ntn_params->ntn_reg_base_ptr_pa = ntn_info->ntn_reg_base_ptr_pa;
	Ntn_params->data_buff_size = ntn_info->data_buff_size;
	Ntn_params->db_mode = ntn_info->db_mode;
	Ntn_params->ipa_pipe_number = ipa_ep_idx;
	Ntn_params->dir = dir;

	result = ipa3_uc_send_cmd((u32)(cmd.phys_base),
				IPA_CPU_2_HW_CMD_OFFLOAD_CHANNEL_SET_UP,
				IPA_HW_2_CPU_OFFLOAD_CMD_STATUS_SUCCESS,
				false, 10*HZ);
	if (result)
		result = -EFAULT;

	dma_free_coherent(ipa3_ctx->uc_pdev, cmd.size, cmd.base, cmd.phys_base);
	return result;
}

static int ipa3_smmu_map_uc_ntn_pipes(struct ipa_ntn_setup_info *params,
	bool map)
{
	struct iommu_domain *smmu_domain;
	int result;
	int i;
	u64 iova;
	phys_addr_t pa;
	u64 iova_p;
	phys_addr_t pa_p;
	u32 size_p;
	bool map_unmap_once;

	if (params->data_buff_size > PAGE_SIZE) {
		IPAERR("invalid data buff size\n");
		return -EINVAL;
	}

	/* only map/unmap once the ntn_reg_base_ptr_pa */
	map_unmap_once = (map && ipa3_ctx->uc_ntn_ctx.smmu_mapped == 0)
	|| (!map && ipa3_ctx->uc_ntn_ctx.smmu_mapped == 1);

	IPADBG(" %s uC regs, smmu_mapped %d\n",
		map ? "map" : "unmap", ipa3_ctx->uc_ntn_ctx.smmu_mapped);

	if (map_unmap_once) {
		result = ipa3_smmu_map_peer_reg(rounddown(
					params->ntn_reg_base_ptr_pa, PAGE_SIZE),
					map, IPA_SMMU_CB_UC);
		if (result) {
			IPAERR("failed to %s uC regs %d\n",
					map ? "map" : "unmap", result);
			goto fail;
		}
		/* backup the ntn_reg_base_ptr_pa_r */
		ipa3_ctx->uc_ntn_ctx.ntn_reg_base_ptr_pa_rd =
			rounddown(params->ntn_reg_base_ptr_pa,
			PAGE_SIZE);
		IPADBG(" %s ntn_reg_base_ptr_pa regs 0X%0x smmu_mapped %d\n",
			map ? "map" : "unmap",
			(unsigned long long)
			ipa3_ctx->uc_ntn_ctx.ntn_reg_base_ptr_pa_rd,
			ipa3_ctx->uc_ntn_ctx.smmu_mapped);
	}
	/* update smmu_mapped reference count */
	if (map) {
		ipa3_ctx->uc_ntn_ctx.smmu_mapped++;
		IPADBG("uc_ntn_ctx.smmu_mapped %d\n",
			ipa3_ctx->uc_ntn_ctx.smmu_mapped);
	} else {
		if (ipa3_ctx->uc_ntn_ctx.smmu_mapped == 0) {
			IPAERR("Invalid smmu_mapped %d\n",
					ipa3_ctx->uc_ntn_ctx.smmu_mapped);
			goto fail;
		} else {
			ipa3_ctx->uc_ntn_ctx.smmu_mapped--;
			IPADBG("uc_ntn_ctx.smmu_mapped %d\n",
				ipa3_ctx->uc_ntn_ctx.smmu_mapped);
		}
	}

	if (params->smmu_enabled) {
		IPADBG("smmu is enabled on EMAC\n");
		result = ipa3_smmu_map_peer_buff((u64)params->ring_base_iova,
			params->ntn_ring_size, map, params->ring_base_sgt,
			IPA_SMMU_CB_UC);
		if (result) {
			IPAERR("failed to %s ntn ring %d\n",
				map ? "map" : "unmap", result);
			goto fail_map_ring;
		}
		result = ipa3_smmu_map_peer_buff(
			(u64)params->buff_pool_base_iova,
			params->num_buffers * 4, map,
			params->buff_pool_base_sgt, IPA_SMMU_CB_UC);
		if (result) {
			IPAERR("failed to %s pool buffs %d\n",
				map ? "map" : "unmap", result);
			goto fail_map_buffer_smmu_enabled;
		}
	} else {
		IPADBG("smmu is disabled on EMAC\n");
		result = ipa3_smmu_map_peer_buff((u64)params->ring_base_pa,
			params->ntn_ring_size, map, NULL, IPA_SMMU_CB_UC);
		if (result) {
			IPAERR("failed to %s ntn ring %d\n",
				map ? "map" : "unmap", result);
			goto fail_map_ring;
		}
		result = ipa3_smmu_map_peer_buff(params->buff_pool_base_pa,
			params->num_buffers * 4, map, NULL, IPA_SMMU_CB_UC);
		if (result) {
			IPAERR("failed to %s pool buffs %d\n",
				map ? "map" : "unmap", result);
			goto fail_map_buffer_smmu_disabled;
		}
	}

	if (ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_AP]) {
		IPADBG("AP SMMU is set to s1 bypass\n");
		return 0;
	}

	smmu_domain = ipa3_get_smmu_domain();
	if (!smmu_domain) {
		IPAERR("invalid smmu domain\n");
		return -EINVAL;
	}

	for (i = 0; i < params->num_buffers; i++) {
		iova = (u64)params->data_buff_list[i].iova;
		pa = (phys_addr_t)params->data_buff_list[i].pa;
		IPA_SMMU_ROUND_TO_PAGE(iova, pa, params->data_buff_size, iova_p,
			pa_p, size_p);
		IPADBG("%s 0x%llx to 0x%pa size %d\n", map ? "mapping" :
			"unmapping", iova_p, &pa_p, size_p);
		if (map) {
			result = ipa3_iommu_map(smmu_domain, iova_p, pa_p,
				size_p, IOMMU_READ | IOMMU_WRITE);
			if (result)
				IPAERR("Fail to map 0x%llx\n", iova);
		} else {
			result = iommu_unmap(smmu_domain, iova_p, size_p);
			if (result != size_p) {
				IPAERR("Fail to unmap 0x%llx\n", iova);
				if (params->smmu_enabled)
					goto fail_map_data_buff_smmu_enabled;
				else
					goto fail_map_data_buff_smmu_disabled;
			}
		}
	}
	return 0;

fail_map_data_buff_smmu_enabled:
	ipa3_smmu_map_peer_buff((u64)params->buff_pool_base_iova,
		params->num_buffers * 4, !map, NULL, IPA_SMMU_CB_UC);
	goto fail_map_buffer_smmu_enabled;
fail_map_data_buff_smmu_disabled:
	ipa3_smmu_map_peer_buff(params->buff_pool_base_pa,
		params->num_buffers * 4, !map, NULL, IPA_SMMU_CB_UC);
	goto fail_map_buffer_smmu_disabled;
fail_map_buffer_smmu_enabled:
	ipa3_smmu_map_peer_buff((u64)params->ring_base_iova,
		params->ntn_ring_size, !map, params->ring_base_sgt,
		IPA_SMMU_CB_UC);
	goto fail_map_ring;
fail_map_buffer_smmu_disabled:
	ipa3_smmu_map_peer_buff((u64)params->ring_base_pa,
			params->ntn_ring_size, !map, NULL, IPA_SMMU_CB_UC);
fail_map_ring:
	ipa3_smmu_map_peer_reg(rounddown(params->ntn_reg_base_ptr_pa,
		PAGE_SIZE), !map, IPA_SMMU_CB_UC);
fail:
	return result;
}

/**
 * ipa3_setup_uc_ntn_pipes() - setup uc offload pipes
 */
int ipa3_setup_uc_ntn_pipes(struct ipa_ntn_conn_in_params *in,
	ipa_notify_cb notify, void *priv, u8 hdr_len,
	struct ipa_ntn_conn_out_params *outp)
{
	struct ipa3_ep_context *ep_ul;
	struct ipa3_ep_context *ep_dl;
	int ipa_ep_idx_ul;
	int ipa_ep_idx_dl;
	int result = 0;

	if (in == NULL) {
		IPAERR("invalid input\n");
		return -EINVAL;
	}

	ipa_ep_idx_ul = ipa_get_ep_mapping(in->ul.client);
	if (ipa_ep_idx_ul == IPA_EP_NOT_ALLOCATED ||
		ipa_ep_idx_ul >= IPA3_MAX_NUM_PIPES) {
		IPAERR("fail to alloc UL EP ipa_ep_idx_ul=%d\n",
			ipa_ep_idx_ul);
		return -EFAULT;
	}

	ipa_ep_idx_dl = ipa_get_ep_mapping(in->dl.client);
	if (ipa_ep_idx_dl == IPA_EP_NOT_ALLOCATED ||
		ipa_ep_idx_dl >= IPA3_MAX_NUM_PIPES) {
		IPAERR("fail to alloc DL EP ipa_ep_idx_dl=%d\n",
			ipa_ep_idx_dl);
		return -EFAULT;
	}

	ep_ul = &ipa3_ctx->ep[ipa_ep_idx_ul];
	ep_dl = &ipa3_ctx->ep[ipa_ep_idx_dl];

	if (ep_ul->valid || ep_dl->valid) {
		IPAERR("EP already allocated ul:%d dl:%d\n",
			   ep_ul->valid, ep_dl->valid);
		return -EFAULT;
	}

	memset(ep_ul, 0, offsetof(struct ipa3_ep_context, sys));
	memset(ep_dl, 0, offsetof(struct ipa3_ep_context, sys));

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

	if (ipa3_cfg_ep(ipa_ep_idx_ul, &ep_ul->cfg)) {
		IPAERR("fail to setup ul pipe cfg\n");
		result = -EFAULT;
		goto fail;
	}

	result = ipa3_smmu_map_uc_ntn_pipes(&in->ul, true);
	if (result) {
		IPAERR("failed to map SMMU for UL %d\n", result);
		goto fail;
	}

	result = ipa3_enable_data_path(ipa_ep_idx_ul);
	if (result) {
		IPAERR("Enable data path failed res=%d pipe=%d.\n", result,
			ipa_ep_idx_ul);
		result = -EFAULT;
		goto fail_smmu_unmap_ul;
	}

	if (ipa3_uc_send_ntn_setup_pipe_cmd(&in->ul, IPA_NTN_RX_DIR)) {
		IPAERR("fail to send cmd to uc for ul pipe\n");
		result = -EFAULT;
		goto fail_disable_dp_ul;
	}
	ipa3_install_dflt_flt_rules(ipa_ep_idx_ul);
	/* Rx: IPA_UC_MAILBOX_m_n m = 1, n =3 mmio*/
	outp->ul_uc_db_iomem = ipa3_ctx->mmio +
		ipahal_get_reg_mn_ofst(IPA_UC_MAILBOX_m_n,
		1, 3);
	ep_ul->uc_offload_state |= IPA_UC_OFFLOAD_CONNECTED;
	IPADBG("client %d (ep: %d) connected\n", in->ul.client,
		ipa_ep_idx_ul);

	/* setup dl ep cfg */
	ep_dl->valid = 1;
	ep_dl->client = in->dl.client;
	memset(&ep_dl->cfg, 0, sizeof(ep_ul->cfg));
	ep_dl->cfg.nat.nat_en = IPA_BYPASS_NAT;
	ep_dl->cfg.hdr.hdr_len = hdr_len;
	ep_dl->cfg.mode.mode = IPA_BASIC;

	if (ipa3_cfg_ep(ipa_ep_idx_dl, &ep_dl->cfg)) {
		IPAERR("fail to setup dl pipe cfg\n");
		result = -EFAULT;
		goto fail_disable_dp_ul;
	}

	result = ipa3_smmu_map_uc_ntn_pipes(&in->dl, true);
	if (result) {
		IPAERR("failed to map SMMU for DL %d\n", result);
		goto fail_disable_dp_ul;
	}

	result = ipa3_enable_data_path(ipa_ep_idx_dl);
	if (result) {
		IPAERR("Enable data path failed res=%d pipe=%d.\n", result,
			ipa_ep_idx_dl);
		result = -EFAULT;
		goto fail_smmu_unmap_dl;
	}

	if (ipa3_uc_send_ntn_setup_pipe_cmd(&in->dl, IPA_NTN_TX_DIR)) {
		IPAERR("fail to send cmd to uc for dl pipe\n");
		result = -EFAULT;
		goto fail_disable_dp_dl;
	}
	/* Tx: IPA_UC_MAILBOX_m_n m = 1, n =4 mmio */
	outp->dl_uc_db_iomem = ipa3_ctx->mmio +
		ipahal_get_reg_mn_ofst(IPA_UC_MAILBOX_m_n,
		1, 4);
	ep_dl->uc_offload_state |= IPA_UC_OFFLOAD_CONNECTED;

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	IPADBG("client %d (ep: %d) connected\n", in->dl.client,
		ipa_ep_idx_dl);

	return 0;

fail_disable_dp_dl:
	ipa3_disable_data_path(ipa_ep_idx_dl);
fail_smmu_unmap_dl:
	ipa3_smmu_map_uc_ntn_pipes(&in->dl, false);
fail_disable_dp_ul:
	ipa3_disable_data_path(ipa_ep_idx_ul);
fail_smmu_unmap_ul:
	ipa3_smmu_map_uc_ntn_pipes(&in->ul, false);
fail:
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	return result;
}

/**
 * ipa3_tear_down_uc_offload_pipes() - tear down uc offload pipes
 */

int ipa3_tear_down_uc_offload_pipes(int ipa_ep_idx_ul,
		int ipa_ep_idx_dl, struct ipa_ntn_conn_in_params *params)
{
	struct ipa_mem_buffer cmd;
	struct ipa3_ep_context *ep_ul, *ep_dl;
	struct IpaHwOffloadCommonChCmdData_t *cmd_data;
	struct IpaHwOffloadCommonChCmdData_t_v4_0 *cmd_data_v4_0;
	union Ipa3HwNtnCommonChCmdData_t *tear;
	int result = 0;

	IPADBG("ep_ul = %d\n", ipa_ep_idx_ul);
	IPADBG("ep_dl = %d\n", ipa_ep_idx_dl);

	if (ipa_ep_idx_ul == IPA_EP_NOT_ALLOCATED ||
		ipa_ep_idx_ul >= IPA3_MAX_NUM_PIPES) {
		IPAERR("ipa_ep_idx_ul %d invalid\n",
			ipa_ep_idx_ul);
		return -EFAULT;
	}

	if (ipa_ep_idx_dl == IPA_EP_NOT_ALLOCATED ||
		ipa_ep_idx_dl >= IPA3_MAX_NUM_PIPES) {
		IPAERR("ep ipa_ep_idx_dl %d invalid\n",
			ipa_ep_idx_dl);
		return -EFAULT;
	}

	ep_ul = &ipa3_ctx->ep[ipa_ep_idx_ul];
	ep_dl = &ipa3_ctx->ep[ipa_ep_idx_dl];

	if (ep_ul->uc_offload_state != IPA_UC_OFFLOAD_CONNECTED ||
		ep_dl->uc_offload_state != IPA_UC_OFFLOAD_CONNECTED) {
		IPAERR("channel bad state: ul %d dl %d\n",
			ep_ul->uc_offload_state, ep_dl->uc_offload_state);
		return -EFAULT;
	}

	atomic_set(&ep_ul->disconnect_in_progress, 1);
	atomic_set(&ep_dl->disconnect_in_progress, 1);

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0)
		cmd.size = sizeof(*cmd_data_v4_0);
	else
		cmd.size = sizeof(*cmd_data);
	cmd.base = dma_alloc_coherent(ipa3_ctx->uc_pdev, cmd.size,
		&cmd.phys_base, GFP_KERNEL);
	if (cmd.base == NULL) {
		IPAERR("fail to get DMA memory.\n");
		return -ENOMEM;
	}

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0) {
		cmd_data_v4_0 = (struct IpaHwOffloadCommonChCmdData_t_v4_0 *)
			cmd.base;
		cmd_data_v4_0->protocol = IPA_HW_PROTOCOL_ETH;
		tear = &cmd_data_v4_0->CommonCh_params.NtnCommonCh_params;
	} else {
		cmd_data = (struct IpaHwOffloadCommonChCmdData_t *)cmd.base;
		cmd_data->protocol = IPA_HW_PROTOCOL_ETH;
		tear = &cmd_data->CommonCh_params.NtnCommonCh_params;
	}

	/* teardown the DL pipe */
	ipa3_disable_data_path(ipa_ep_idx_dl);
	/*
	 * Reset ep before sending cmd otherwise disconnect
	 * during data transfer will result into
	 * enormous suspend interrupts
	 */
	memset(&ipa3_ctx->ep[ipa_ep_idx_dl], 0, sizeof(struct ipa3_ep_context));
	IPADBG("dl client (ep: %d) disconnected\n", ipa_ep_idx_dl);
	tear->params.ipa_pipe_number = ipa_ep_idx_dl;
	result = ipa3_uc_send_cmd((u32)(cmd.phys_base),
				IPA_CPU_2_HW_CMD_OFFLOAD_TEAR_DOWN,
				IPA_HW_2_CPU_OFFLOAD_CMD_STATUS_SUCCESS,
				false, 10*HZ);
	if (result) {
		IPAERR("fail to tear down dl pipe\n");
		result = -EFAULT;
		goto fail;
	}

	/* unmap the DL pipe */
	result = ipa3_smmu_map_uc_ntn_pipes(&params->dl, false);
	if (result) {
		IPAERR("failed to unmap SMMU for DL %d\n", result);
		goto fail;
	}

	/* teardown the UL pipe */
	ipa3_disable_data_path(ipa_ep_idx_ul);

	tear->params.ipa_pipe_number = ipa_ep_idx_ul;
	result = ipa3_uc_send_cmd((u32)(cmd.phys_base),
				IPA_CPU_2_HW_CMD_OFFLOAD_TEAR_DOWN,
				IPA_HW_2_CPU_OFFLOAD_CMD_STATUS_SUCCESS,
				false, 10*HZ);
	if (result) {
		IPAERR("fail to tear down ul pipe\n");
		result = -EFAULT;
		goto fail;
	}

	/* unmap the UL pipe */
	result = ipa3_smmu_map_uc_ntn_pipes(&params->ul, false);
	if (result) {
		IPAERR("failed to unmap SMMU for UL %d\n", result);
		goto fail;
	}

	ipa3_delete_dflt_flt_rules(ipa_ep_idx_ul);
	memset(&ipa3_ctx->ep[ipa_ep_idx_ul], 0, sizeof(struct ipa3_ep_context));
	IPADBG("ul client (ep: %d) disconnected\n", ipa_ep_idx_ul);

fail:
	dma_free_coherent(ipa3_ctx->uc_pdev, cmd.size, cmd.base, cmd.phys_base);
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	return result;
}
