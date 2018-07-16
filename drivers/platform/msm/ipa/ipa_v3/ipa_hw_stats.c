/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/delay.h>
#include "ipa_i.h"
#include "ipahal/ipahal.h"
#include "ipahal/ipahal_hw_stats.h"

#define IPA_CLIENT_BIT_32(client) \
	((ipa3_get_ep_mapping(client) >= 0 && \
		ipa3_get_ep_mapping(client) < IPA_STATS_MAX_PIPE_BIT) ? \
		(1 << ipa3_get_ep_mapping(client)) : 0)

int ipa_hw_stats_init(void)
{
	int ret = 0, ep_index;
	struct ipa_teth_stats_endpoints *teth_stats_init;

	if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0)
		return 0;

	/* initialize stats here */
	ipa3_ctx->hw_stats.enabled = true;

	teth_stats_init = kzalloc(sizeof(*teth_stats_init), GFP_KERNEL);
	if (!teth_stats_init) {
		IPAERR("mem allocated failed!\n");
		return -ENOMEM;
	}
	/* enable prod mask */
	teth_stats_init->prod_mask = (
		IPA_CLIENT_BIT_32(IPA_CLIENT_Q6_WAN_PROD) |
		IPA_CLIENT_BIT_32(IPA_CLIENT_USB_PROD) |
		IPA_CLIENT_BIT_32(IPA_CLIENT_WLAN1_PROD));

	if (IPA_CLIENT_BIT_32(IPA_CLIENT_Q6_WAN_PROD)) {
		ep_index = ipa3_get_ep_mapping(IPA_CLIENT_Q6_WAN_PROD);
		if (ep_index == -1) {
			IPAERR("Invalid client.\n");
			kfree(teth_stats_init);
			return -EINVAL;
		}
		teth_stats_init->dst_ep_mask[ep_index] =
			(IPA_CLIENT_BIT_32(IPA_CLIENT_WLAN1_CONS) |
			IPA_CLIENT_BIT_32(IPA_CLIENT_USB_CONS));
	}

	if (IPA_CLIENT_BIT_32(IPA_CLIENT_USB_PROD)) {
		ep_index = ipa3_get_ep_mapping(IPA_CLIENT_USB_PROD);
		if (ep_index == -1) {
			IPAERR("Invalid client.\n");
			kfree(teth_stats_init);
			return -EINVAL;
		}
		teth_stats_init->dst_ep_mask[ep_index] =
			IPA_CLIENT_BIT_32(IPA_CLIENT_Q6_WAN_CONS);
	}

	if (IPA_CLIENT_BIT_32(IPA_CLIENT_WLAN1_PROD)) {
		ep_index = ipa3_get_ep_mapping(IPA_CLIENT_WLAN1_PROD);
		if (ep_index == -1) {
			IPAERR("Invalid client.\n");
			kfree(teth_stats_init);
			return -EINVAL;
		}
		teth_stats_init->dst_ep_mask[ep_index] =
			IPA_CLIENT_BIT_32(IPA_CLIENT_Q6_WAN_CONS);
	}

	ret = ipa_init_teth_stats(teth_stats_init);
	kfree(teth_stats_init);
	return ret;
}

int ipa_init_quota_stats(u32 pipe_bitmask)
{
	struct ipahal_stats_init_pyld *pyld;
	struct ipahal_imm_cmd_dma_shared_mem cmd = { 0 };
	struct ipahal_imm_cmd_pyld *cmd_pyld;
	struct ipahal_imm_cmd_register_write quota_base = {0};
	struct ipahal_imm_cmd_pyld *quota_base_pyld;
	struct ipahal_imm_cmd_register_write quota_mask = {0};
	struct ipahal_imm_cmd_pyld *quota_mask_pyld;
	struct ipa3_desc desc[3] = { {0} };
	dma_addr_t dma_address;
	int ret;

	if (!ipa3_ctx->hw_stats.enabled)
		return 0;

	/* reset driver's cache */
	memset(&ipa3_ctx->hw_stats.quota, 0, sizeof(ipa3_ctx->hw_stats.quota));
	ipa3_ctx->hw_stats.quota.init.enabled_bitmask = pipe_bitmask;
	IPADBG_LOW("pipe_bitmask=0x%x\n", pipe_bitmask);

	pyld = ipahal_stats_generate_init_pyld(IPAHAL_HW_STATS_QUOTA,
		&ipa3_ctx->hw_stats.quota.init, false);
	if (!pyld) {
		IPAERR("failed to generate pyld\n");
		return -EPERM;
	}

	if (pyld->len > IPA_MEM_PART(stats_quota_size)) {
		IPAERR("SRAM partition too small: %d needed %d\n",
			IPA_MEM_PART(stats_quota_size), pyld->len);
		ret = -EPERM;
		goto destroy_init_pyld;
	}

	dma_address = dma_map_single(ipa3_ctx->pdev,
		pyld->data,
		pyld->len,
		DMA_TO_DEVICE);
	if (dma_mapping_error(ipa3_ctx->pdev, dma_address)) {
		IPAERR("failed to DMA map\n");
		ret = -EPERM;
		goto destroy_init_pyld;
	}

	/* setting the registers and init the stats pyld are done atomically */
	quota_mask.skip_pipeline_clear = false;
	quota_mask.pipeline_clear_options = IPAHAL_FULL_PIPELINE_CLEAR;
	quota_mask.offset = ipahal_get_reg_n_ofst(IPA_STAT_QUOTA_MASK_n,
		ipa3_ctx->ee);
	quota_mask.value = pipe_bitmask;
	quota_mask.value_mask = ~0;
	quota_mask_pyld = ipahal_construct_imm_cmd(IPA_IMM_CMD_REGISTER_WRITE,
		&quota_mask, false);
	if (!quota_mask_pyld) {
		IPAERR("failed to construct register_write imm cmd\n");
		ret = -ENOMEM;
		goto unmap;
	}
	desc[0].opcode = quota_mask_pyld->opcode;
	desc[0].pyld = quota_mask_pyld->data;
	desc[0].len = quota_mask_pyld->len;
	desc[0].type = IPA_IMM_CMD_DESC;

	quota_base.skip_pipeline_clear = false;
	quota_base.pipeline_clear_options = IPAHAL_FULL_PIPELINE_CLEAR;
	quota_base.offset = ipahal_get_reg_n_ofst(IPA_STAT_QUOTA_BASE_n,
		ipa3_ctx->ee);
	quota_base.value = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(stats_quota_ofst);
	quota_base.value_mask = ~0;
	quota_base_pyld = ipahal_construct_imm_cmd(IPA_IMM_CMD_REGISTER_WRITE,
		&quota_base, false);
	if (!quota_base_pyld) {
		IPAERR("failed to construct register_write imm cmd\n");
		ret = -ENOMEM;
		goto destroy_quota_mask;
	}
	desc[1].opcode = quota_base_pyld->opcode;
	desc[1].pyld = quota_base_pyld->data;
	desc[1].len = quota_base_pyld->len;
	desc[1].type = IPA_IMM_CMD_DESC;

	cmd.is_read = false;
	cmd.skip_pipeline_clear = false;
	cmd.pipeline_clear_options = IPAHAL_FULL_PIPELINE_CLEAR;
	cmd.size = pyld->len;
	cmd.system_addr = dma_address;
	cmd.local_addr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(stats_quota_ofst);
	cmd_pyld = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_DMA_SHARED_MEM, &cmd, false);
	if (!cmd_pyld) {
		IPAERR("failed to construct dma_shared_mem imm cmd\n");
		ret = -ENOMEM;
		goto destroy_quota_base;
	}
	desc[2].opcode = cmd_pyld->opcode;
	desc[2].pyld = cmd_pyld->data;
	desc[2].len = cmd_pyld->len;
	desc[2].type = IPA_IMM_CMD_DESC;

	ret = ipa3_send_cmd(3, desc);
	if (ret) {
		IPAERR("failed to send immediate command (error %d)\n", ret);
		goto destroy_imm;
	}

	ret = 0;

destroy_imm:
	ipahal_destroy_imm_cmd(cmd_pyld);
destroy_quota_base:
	ipahal_destroy_imm_cmd(quota_base_pyld);
destroy_quota_mask:
	ipahal_destroy_imm_cmd(quota_mask_pyld);
unmap:
	dma_unmap_single(ipa3_ctx->pdev, dma_address, pyld->len, DMA_TO_DEVICE);
destroy_init_pyld:
	ipahal_destroy_stats_init_pyld(pyld);
	return ret;
}

int ipa_get_quota_stats(struct ipa_quota_stats_all *out)
{
	int i;
	int ret;
	struct ipahal_stats_get_offset_quota get_offset = { { 0 } };
	struct ipahal_stats_offset offset = { 0 };
	struct ipahal_imm_cmd_dma_shared_mem cmd = { 0 };
	struct ipahal_imm_cmd_pyld *cmd_pyld;
	struct ipa_mem_buffer mem;
	struct ipa3_desc desc = { 0 };
	struct ipahal_stats_quota_all *stats;

	if (!ipa3_ctx->hw_stats.enabled)
		return 0;

	get_offset.init = ipa3_ctx->hw_stats.quota.init;
	ret = ipahal_stats_get_offset(IPAHAL_HW_STATS_QUOTA, &get_offset,
		&offset);
	if (ret) {
		IPAERR("failed to get offset from hal %d\n", ret);
		return ret;
	}

	IPADBG_LOW("offset = %d size = %d\n", offset.offset, offset.size);

	if (offset.size == 0)
		return 0;

	mem.size = offset.size;
	mem.base = dma_alloc_coherent(ipa3_ctx->pdev,
		mem.size,
		&mem.phys_base,
		GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA memory");
		return ret;
	}

	cmd.is_read = true;
	cmd.clear_after_read = true;
	cmd.skip_pipeline_clear = false;
	cmd.pipeline_clear_options = IPAHAL_HPS_CLEAR;
	cmd.size = mem.size;
	cmd.system_addr = mem.phys_base;
	cmd.local_addr = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(stats_quota_ofst) + offset.offset;
	cmd_pyld = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_DMA_SHARED_MEM, &cmd, false);
	if (!cmd_pyld) {
		IPAERR("failed to construct dma_shared_mem imm cmd\n");
		ret = -ENOMEM;
		goto free_dma_mem;
	}
	desc.opcode = cmd_pyld->opcode;
	desc.pyld = cmd_pyld->data;
	desc.len = cmd_pyld->len;
	desc.type = IPA_IMM_CMD_DESC;

	ret = ipa3_send_cmd(1, &desc);
	if (ret) {
		IPAERR("failed to send immediate command (error %d)\n", ret);
		goto destroy_imm;
	}

	stats = kzalloc(sizeof(*stats), GFP_KERNEL);
	if (!stats) {
		ret = -ENOMEM;
		goto destroy_imm;
	}

	ret = ipahal_parse_stats(IPAHAL_HW_STATS_QUOTA,
		&ipa3_ctx->hw_stats.quota.init, mem.base, stats);
	if (ret) {
		IPAERR("failed to parse stats (error %d)\n", ret);
		goto free_stats;
	}

	/*
	 * update driver cache.
	 * the stats were read from hardware with clear_after_read meaning
	 * hardware stats are 0 now
	 */
	for (i = 0; i < IPA_CLIENT_MAX; i++) {
		int ep_idx = ipa3_get_ep_mapping(i);

		if (ep_idx == -1 || ep_idx >= IPA3_MAX_NUM_PIPES)
			continue;

		if (ipa3_ctx->ep[ep_idx].client != i)
			continue;

		ipa3_ctx->hw_stats.quota.stats.client[i].num_ipv4_bytes +=
			stats->stats[ep_idx].num_ipv4_bytes;
		ipa3_ctx->hw_stats.quota.stats.client[i].num_ipv4_pkts +=
			stats->stats[ep_idx].num_ipv4_pkts;
		ipa3_ctx->hw_stats.quota.stats.client[i].num_ipv6_bytes +=
			stats->stats[ep_idx].num_ipv6_bytes;
		ipa3_ctx->hw_stats.quota.stats.client[i].num_ipv6_pkts +=
			stats->stats[ep_idx].num_ipv6_pkts;
	}

	/* copy results to out parameter */
	if (out)
		*out = ipa3_ctx->hw_stats.quota.stats;
	ret = 0;
free_stats:
	kfree(stats);
destroy_imm:
	ipahal_destroy_imm_cmd(cmd_pyld);
free_dma_mem:
	dma_free_coherent(ipa3_ctx->pdev, mem.size, mem.base, mem.phys_base);
	return ret;

}

int ipa_reset_quota_stats(enum ipa_client_type client)
{
	int ret;
	struct ipa_quota_stats *stats;

	if (!ipa3_ctx->hw_stats.enabled)
		return 0;

	if (client >= IPA_CLIENT_MAX) {
		IPAERR("invalid client %d\n", client);
		return -EINVAL;
	}

	/* reading stats will reset them in hardware */
	ret = ipa_get_quota_stats(NULL);
	if (ret) {
		IPAERR("ipa_get_quota_stats failed %d\n", ret);
		return ret;
	}

	/* reset driver's cache */
	stats = &ipa3_ctx->hw_stats.quota.stats.client[client];
	memset(stats, 0, sizeof(*stats));
	return 0;
}

int ipa_reset_all_quota_stats(void)
{
	int ret;
	struct ipa_quota_stats_all *stats;

	if (!ipa3_ctx->hw_stats.enabled)
		return 0;

	/* reading stats will reset them in hardware */
	ret = ipa_get_quota_stats(NULL);
	if (ret) {
		IPAERR("ipa_get_quota_stats failed %d\n", ret);
		return ret;
	}

	/* reset driver's cache */
	stats = &ipa3_ctx->hw_stats.quota.stats;
	memset(stats, 0, sizeof(*stats));
	return 0;
}

int ipa_init_teth_stats(struct ipa_teth_stats_endpoints *in)
{
	struct ipahal_stats_init_pyld *pyld;
	struct ipahal_imm_cmd_dma_shared_mem cmd = { 0 };
	struct ipahal_imm_cmd_pyld *cmd_pyld;
	struct ipahal_imm_cmd_register_write teth_base = {0};
	struct ipahal_imm_cmd_pyld *teth_base_pyld;
	struct ipahal_imm_cmd_register_write teth_mask = { 0 };
	struct ipahal_imm_cmd_pyld *teth_mask_pyld;
	struct ipa3_desc desc[3] = { {0} };
	dma_addr_t dma_address;
	int ret;
	int i;

	if (!ipa3_ctx->hw_stats.enabled)
		return 0;

	if (!in || !in->prod_mask) {
		IPAERR("invalid params\n");
		return -EINVAL;
	}

	for (i = 0; i < IPA_STATS_MAX_PIPE_BIT; i++) {
		if ((in->prod_mask & (1 << i)) && !in->dst_ep_mask[i]) {
			IPAERR("prod %d doesn't have cons\n", i);
			return -EINVAL;
		}
	}
	IPADBG_LOW("prod_mask=0x%x\n", in->prod_mask);

	/* reset driver's cache */
	memset(&ipa3_ctx->hw_stats.teth.init, 0,
		sizeof(ipa3_ctx->hw_stats.teth.init));
	for (i = 0; i < IPA_CLIENT_MAX; i++) {
		memset(&ipa3_ctx->hw_stats.teth.prod_stats_sum[i], 0,
			sizeof(ipa3_ctx->hw_stats.teth.prod_stats_sum[i]));
		memset(&ipa3_ctx->hw_stats.teth.prod_stats[i], 0,
			sizeof(ipa3_ctx->hw_stats.teth.prod_stats[i]));
	}
	ipa3_ctx->hw_stats.teth.init.prod_bitmask = in->prod_mask;
	memcpy(ipa3_ctx->hw_stats.teth.init.cons_bitmask, in->dst_ep_mask,
		sizeof(ipa3_ctx->hw_stats.teth.init.cons_bitmask));


	pyld = ipahal_stats_generate_init_pyld(IPAHAL_HW_STATS_TETHERING,
		&ipa3_ctx->hw_stats.teth.init, false);
	if (!pyld) {
		IPAERR("failed to generate pyld\n");
		return -EPERM;
	}

	if (pyld->len > IPA_MEM_PART(stats_tethering_size)) {
		IPAERR("SRAM partition too small: %d needed %d\n",
			IPA_MEM_PART(stats_tethering_size), pyld->len);
		ret = -EPERM;
		goto destroy_init_pyld;
	}

	dma_address = dma_map_single(ipa3_ctx->pdev,
		pyld->data,
		pyld->len,
		DMA_TO_DEVICE);
	if (dma_mapping_error(ipa3_ctx->pdev, dma_address)) {
		IPAERR("failed to DMA map\n");
		ret = -EPERM;
		goto destroy_init_pyld;
	}

	/* setting the registers and init the stats pyld are done atomically */
	teth_mask.skip_pipeline_clear = false;
	teth_mask.pipeline_clear_options = IPAHAL_FULL_PIPELINE_CLEAR;
	teth_mask.offset = ipahal_get_reg_n_ofst(IPA_STAT_TETHERING_MASK_n,
		ipa3_ctx->ee);
	teth_mask.value = in->prod_mask;
	teth_mask.value_mask = ~0;
	teth_mask_pyld = ipahal_construct_imm_cmd(IPA_IMM_CMD_REGISTER_WRITE,
		&teth_mask, false);
	if (!teth_mask_pyld) {
		IPAERR("failed to construct register_write imm cmd\n");
		ret = -ENOMEM;
		goto unmap;
	}
	desc[0].opcode = teth_mask_pyld->opcode;
	desc[0].pyld = teth_mask_pyld->data;
	desc[0].len = teth_mask_pyld->len;
	desc[0].type = IPA_IMM_CMD_DESC;

	teth_base.skip_pipeline_clear = false;
	teth_base.pipeline_clear_options = IPAHAL_FULL_PIPELINE_CLEAR;
	teth_base.offset = ipahal_get_reg_n_ofst(IPA_STAT_TETHERING_BASE_n,
		ipa3_ctx->ee);
	teth_base.value = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(stats_tethering_ofst);
	teth_base.value_mask = ~0;
	teth_base_pyld = ipahal_construct_imm_cmd(IPA_IMM_CMD_REGISTER_WRITE,
		&teth_base, false);
	if (!teth_base_pyld) {
		IPAERR("failed to construct register_write imm cmd\n");
		ret = -ENOMEM;
		goto destroy_teth_mask;
	}
	desc[1].opcode = teth_base_pyld->opcode;
	desc[1].pyld = teth_base_pyld->data;
	desc[1].len = teth_base_pyld->len;
	desc[1].type = IPA_IMM_CMD_DESC;

	cmd.is_read = false;
	cmd.skip_pipeline_clear = false;
	cmd.pipeline_clear_options = IPAHAL_FULL_PIPELINE_CLEAR;
	cmd.size = pyld->len;
	cmd.system_addr = dma_address;
	cmd.local_addr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(stats_tethering_ofst);
	cmd_pyld = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_DMA_SHARED_MEM, &cmd, false);
	if (!cmd_pyld) {
		IPAERR("failed to construct dma_shared_mem imm cmd\n");
		ret = -ENOMEM;
		goto destroy_teth_base;
	}
	desc[2].opcode = cmd_pyld->opcode;
	desc[2].pyld = cmd_pyld->data;
	desc[2].len = cmd_pyld->len;
	desc[2].type = IPA_IMM_CMD_DESC;

	ret = ipa3_send_cmd(3, desc);
	if (ret) {
		IPAERR("failed to send immediate command (error %d)\n", ret);
		goto destroy_imm;
	}

	ret = 0;

destroy_imm:
	ipahal_destroy_imm_cmd(cmd_pyld);
destroy_teth_base:
	ipahal_destroy_imm_cmd(teth_base_pyld);
destroy_teth_mask:
	ipahal_destroy_imm_cmd(teth_mask_pyld);
unmap:
	dma_unmap_single(ipa3_ctx->pdev, dma_address, pyld->len, DMA_TO_DEVICE);
destroy_init_pyld:
	ipahal_destroy_stats_init_pyld(pyld);
	return ret;
}

int ipa_get_teth_stats(void)
{
	int i, j;
	int ret;
	struct ipahal_stats_get_offset_tethering get_offset = { { 0 } };
	struct ipahal_stats_offset offset = {0};
	struct ipahal_imm_cmd_dma_shared_mem cmd = { 0 };
	struct ipahal_imm_cmd_pyld *cmd_pyld;
	struct ipa_mem_buffer mem;
	struct ipa3_desc desc = { 0 };
	struct ipahal_stats_tethering_all *stats_all;
	struct ipa_hw_stats_teth *sw_stats = &ipa3_ctx->hw_stats.teth;
	struct ipahal_stats_tethering *stats;
	struct ipa_quota_stats *quota_stats;
	struct ipahal_stats_init_tethering *init =
		(struct ipahal_stats_init_tethering *)
			&ipa3_ctx->hw_stats.teth.init;

	if (!ipa3_ctx->hw_stats.enabled)
		return 0;

	get_offset.init = ipa3_ctx->hw_stats.teth.init;
	ret = ipahal_stats_get_offset(IPAHAL_HW_STATS_TETHERING, &get_offset,
		&offset);
	if (ret) {
		IPAERR("failed to get offset from hal %d\n", ret);
		return ret;
	}

	IPADBG_LOW("offset = %d size = %d\n", offset.offset, offset.size);

	if (offset.size == 0)
		return 0;

	mem.size = offset.size;
	mem.base = dma_alloc_coherent(ipa3_ctx->pdev,
		mem.size,
		&mem.phys_base,
		GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA memory\n");
		return ret;
	}

	cmd.is_read = true;
	cmd.clear_after_read = true;
	cmd.skip_pipeline_clear = false;
	cmd.pipeline_clear_options = IPAHAL_HPS_CLEAR;
	cmd.size = mem.size;
	cmd.system_addr = mem.phys_base;
	cmd.local_addr = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(stats_tethering_ofst) + offset.offset;
	cmd_pyld = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_DMA_SHARED_MEM, &cmd, false);
	if (!cmd_pyld) {
		IPAERR("failed to construct dma_shared_mem imm cmd\n");
		ret = -ENOMEM;
		goto free_dma_mem;
	}
	desc.opcode = cmd_pyld->opcode;
	desc.pyld = cmd_pyld->data;
	desc.len = cmd_pyld->len;
	desc.type = IPA_IMM_CMD_DESC;

	ret = ipa3_send_cmd(1, &desc);
	if (ret) {
		IPAERR("failed to send immediate command (error %d)\n", ret);
		goto destroy_imm;
	}

	stats_all = kzalloc(sizeof(*stats_all), GFP_KERNEL);
	if (!stats_all) {
		IPADBG("failed to alloc memory\n");
		ret = -ENOMEM;
		goto destroy_imm;
	}

	ret = ipahal_parse_stats(IPAHAL_HW_STATS_TETHERING,
		&ipa3_ctx->hw_stats.teth.init, mem.base, stats_all);
	if (ret) {
		IPAERR("failed to parse stats_all (error %d)\n", ret);
		goto free_stats;
	}

	/* reset prod_stats cache */
	for (i = 0; i < IPA_CLIENT_MAX; i++) {
		memset(&ipa3_ctx->hw_stats.teth.prod_stats[i], 0,
			sizeof(ipa3_ctx->hw_stats.teth.prod_stats[i]));
	}

	/*
	 * update driver cache.
	 * the stats were read from hardware with clear_after_read meaning
	 * hardware stats are 0 now
	 */
	for (i = 0; i < IPA_CLIENT_MAX; i++) {
		for (j = 0; j < IPA_CLIENT_MAX; j++) {
			int prod_idx = ipa3_get_ep_mapping(i);
			int cons_idx = ipa3_get_ep_mapping(j);

			if (prod_idx == -1 || prod_idx >= IPA3_MAX_NUM_PIPES)
				continue;

			if (cons_idx == -1 || cons_idx >= IPA3_MAX_NUM_PIPES)
				continue;

			/* save hw-query result */
			if ((init->prod_bitmask & (1 << prod_idx)) &&
				(init->cons_bitmask[prod_idx]
					& (1 << cons_idx))) {
				IPADBG_LOW("prod %d cons %d\n",
					prod_idx, cons_idx);
				stats = &stats_all->stats[prod_idx][cons_idx];
				IPADBG_LOW("num_ipv4_bytes %lld\n",
					stats->num_ipv4_bytes);
				IPADBG_LOW("num_ipv4_pkts %lld\n",
					stats->num_ipv4_pkts);
				IPADBG_LOW("num_ipv6_pkts %lld\n",
					stats->num_ipv6_pkts);
				IPADBG_LOW("num_ipv6_bytes %lld\n",
					stats->num_ipv6_bytes);

				/* update stats*/
				quota_stats =
					&sw_stats->prod_stats[i].client[j];
				quota_stats->num_ipv4_bytes =
					stats->num_ipv4_bytes;
				quota_stats->num_ipv4_pkts =
					stats->num_ipv4_pkts;
				quota_stats->num_ipv6_bytes =
					stats->num_ipv6_bytes;
				quota_stats->num_ipv6_pkts =
					stats->num_ipv6_pkts;

				/* Accumulated stats */
				quota_stats =
					&sw_stats->prod_stats_sum[i].client[j];
				quota_stats->num_ipv4_bytes +=
					stats->num_ipv4_bytes;
				quota_stats->num_ipv4_pkts +=
					stats->num_ipv4_pkts;
				quota_stats->num_ipv6_bytes +=
					stats->num_ipv6_bytes;
				quota_stats->num_ipv6_pkts +=
					stats->num_ipv6_pkts;
			}
		}
	}

	ret = 0;
free_stats:
	kfree(stats_all);
	stats = NULL;
destroy_imm:
	ipahal_destroy_imm_cmd(cmd_pyld);
free_dma_mem:
	dma_free_coherent(ipa3_ctx->pdev, mem.size, mem.base, mem.phys_base);
	return ret;

}

int ipa_query_teth_stats(enum ipa_client_type prod,
	struct ipa_quota_stats_all *out, bool reset)
{
	if (!IPA_CLIENT_IS_PROD(prod) || ipa3_get_ep_mapping(prod) == -1) {
		IPAERR("invalid prod %d\n", prod);
		return -EINVAL;
	}

	/* copy results to out parameter */
	if (reset)
		*out = ipa3_ctx->hw_stats.teth.prod_stats[prod];
	else
		*out = ipa3_ctx->hw_stats.teth.prod_stats_sum[prod];
	return 0;
}

int ipa_reset_teth_stats(enum ipa_client_type prod, enum ipa_client_type cons)
{
	int ret;
	struct ipa_quota_stats *stats;

	if (!ipa3_ctx->hw_stats.enabled)
		return 0;

	if (!IPA_CLIENT_IS_PROD(prod) || !IPA_CLIENT_IS_CONS(cons)) {
		IPAERR("invalid prod %d or cons %d\n", prod, cons);
		return -EINVAL;
	}

	/* reading stats will reset them in hardware */
	ret = ipa_get_teth_stats();
	if (ret) {
		IPAERR("ipa_get_teth_stats failed %d\n", ret);
		return ret;
	}

	/* reset driver's cache */
	stats = &ipa3_ctx->hw_stats.teth.prod_stats_sum[prod].client[cons];
	memset(stats, 0, sizeof(*stats));
	return 0;
}

int ipa_reset_all_cons_teth_stats(enum ipa_client_type prod)
{
	int ret;
	int i;
	struct ipa_quota_stats *stats;

	if (!ipa3_ctx->hw_stats.enabled)
		return 0;

	if (!IPA_CLIENT_IS_PROD(prod)) {
		IPAERR("invalid prod %d\n", prod);
		return -EINVAL;
	}

	/* reading stats will reset them in hardware */
	ret = ipa_get_teth_stats();
	if (ret) {
		IPAERR("ipa_get_teth_stats failed %d\n", ret);
		return ret;
	}

	/* reset driver's cache */
	for (i = 0; i < IPA_CLIENT_MAX; i++) {
		stats = &ipa3_ctx->hw_stats.teth.prod_stats_sum[prod].client[i];
		memset(stats, 0, sizeof(*stats));
	}

	return 0;
}

int ipa_reset_all_teth_stats(void)
{
	int i;
	int ret;
	struct ipa_quota_stats_all *stats;

	if (!ipa3_ctx->hw_stats.enabled)
		return 0;

	/* reading stats will reset them in hardware */
	for (i = 0; i < IPA_CLIENT_MAX; i++) {
		if (IPA_CLIENT_IS_PROD(i) && ipa3_get_ep_mapping(i) != -1) {
			ret = ipa_get_teth_stats();
			if (ret) {
				IPAERR("ipa_get_teth_stats failed %d\n", ret);
				return ret;
			}
			/* a single iteration will reset all hardware stats */
			break;
		}
	}

	/* reset driver's cache */
	for (i = 0; i < IPA_CLIENT_MAX; i++) {
		stats = &ipa3_ctx->hw_stats.teth.prod_stats_sum[i];
		memset(stats, 0, sizeof(*stats));
	}

	return 0;
}

int ipa_flt_rt_stats_add_rule_id(enum ipa_ip_type ip, bool filtering,
	u16 rule_id)
{
	int rule_idx, rule_bit;
	u32 *bmsk_ptr;

	if (!ipa3_ctx->hw_stats.enabled)
		return 0;

	if (ip < 0 || ip >= IPA_IP_MAX) {
		IPAERR("wrong ip type %d\n", ip);
		return -EINVAL;
	}

	rule_idx = rule_id / 32;
	rule_bit = rule_id % 32;

	if (rule_idx >= IPAHAL_MAX_RULE_ID_32) {
		IPAERR("invalid rule_id %d\n", rule_id);
		return -EINVAL;
	}

	if (ip == IPA_IP_v4 && filtering)
		bmsk_ptr =
			ipa3_ctx->hw_stats.flt_rt.flt_v4_init.rule_id_bitmask;
	else if (ip == IPA_IP_v4)
		bmsk_ptr =
			ipa3_ctx->hw_stats.flt_rt.rt_v4_init.rule_id_bitmask;
	else if (ip == IPA_IP_v6 && filtering)
		bmsk_ptr =
			ipa3_ctx->hw_stats.flt_rt.flt_v6_init.rule_id_bitmask;
	else
		bmsk_ptr =
			ipa3_ctx->hw_stats.flt_rt.rt_v6_init.rule_id_bitmask;

	bmsk_ptr[rule_idx] |= (1 << rule_bit);

	return 0;
}

int ipa_flt_rt_stats_start(enum ipa_ip_type ip, bool filtering)
{
	struct ipahal_stats_init_pyld *pyld;
	int smem_ofst, smem_size, stats_base, start_id_ofst, end_id_ofst;
	int start_id, end_id;
	struct ipahal_stats_init_flt_rt *init;
	struct ipahal_imm_cmd_dma_shared_mem cmd = { 0 };
	struct ipahal_imm_cmd_pyld *cmd_pyld;
	struct ipahal_imm_cmd_register_write flt_rt_base = {0};
	struct ipahal_imm_cmd_pyld *flt_rt_base_pyld;
	struct ipahal_imm_cmd_register_write flt_rt_start_id = {0};
	struct ipahal_imm_cmd_pyld *flt_rt_start_id_pyld;
	struct ipahal_imm_cmd_register_write flt_rt_end_id = { 0 };
	struct ipahal_imm_cmd_pyld *flt_rt_end_id_pyld;
	struct ipa3_desc desc[4] = { {0} };
	dma_addr_t dma_address;
	int ret;

	if (!ipa3_ctx->hw_stats.enabled)
		return 0;

	if (ip == IPA_IP_v4 && filtering) {
		init = &ipa3_ctx->hw_stats.flt_rt.flt_v4_init;
		smem_ofst = IPA_MEM_PART(stats_flt_v4_ofst);
		smem_size = IPA_MEM_PART(stats_flt_v4_size);
		stats_base = ipahal_get_reg_ofst(IPA_STAT_FILTER_IPV4_BASE);
		start_id_ofst =
			ipahal_get_reg_ofst(IPA_STAT_FILTER_IPV4_START_ID);
		end_id_ofst = ipahal_get_reg_ofst(IPA_STAT_FILTER_IPV4_END_ID);
	} else if (ip == IPA_IP_v4) {
		init = &ipa3_ctx->hw_stats.flt_rt.rt_v4_init;
		smem_ofst = IPA_MEM_PART(stats_rt_v4_ofst);
		smem_size = IPA_MEM_PART(stats_rt_v4_size);
		stats_base = ipahal_get_reg_ofst(IPA_STAT_ROUTER_IPV4_BASE);
		start_id_ofst =
			ipahal_get_reg_ofst(IPA_STAT_ROUTER_IPV4_START_ID);
		end_id_ofst = ipahal_get_reg_ofst(IPA_STAT_ROUTER_IPV4_END_ID);
	} else if (ip == IPA_IP_v6 && filtering) {
		init = &ipa3_ctx->hw_stats.flt_rt.flt_v6_init;
		smem_ofst = IPA_MEM_PART(stats_flt_v6_ofst);
		smem_size = IPA_MEM_PART(stats_flt_v6_size);
		stats_base = ipahal_get_reg_ofst(IPA_STAT_FILTER_IPV6_BASE);
		start_id_ofst =
			ipahal_get_reg_ofst(IPA_STAT_FILTER_IPV6_START_ID);
		end_id_ofst = ipahal_get_reg_ofst(IPA_STAT_FILTER_IPV6_END_ID);
	} else {
		init = &ipa3_ctx->hw_stats.flt_rt.rt_v6_init;
		smem_ofst = IPA_MEM_PART(stats_rt_v6_ofst);
		smem_size = IPA_MEM_PART(stats_rt_v6_size);
		stats_base = ipahal_get_reg_ofst(IPA_STAT_ROUTER_IPV6_BASE);
		start_id_ofst =
			ipahal_get_reg_ofst(IPA_STAT_ROUTER_IPV6_START_ID);
		end_id_ofst = ipahal_get_reg_ofst(IPA_STAT_ROUTER_IPV6_END_ID);
	}

	for (start_id = 0; start_id < IPAHAL_MAX_RULE_ID_32; start_id++) {
		if (init->rule_id_bitmask[start_id])
			break;
	}

	if (start_id == IPAHAL_MAX_RULE_ID_32) {
		IPAERR("empty rule ids\n");
		return -EINVAL;
	}

	/* every rule_id_bitmask contains 32 rules */
	start_id *= 32;

	for (end_id = IPAHAL_MAX_RULE_ID_32 - 1; end_id >= 0; end_id--) {
		if (init->rule_id_bitmask[end_id])
			break;
	}
	end_id = (end_id + 1) * 32 - 1;

	pyld = ipahal_stats_generate_init_pyld(IPAHAL_HW_STATS_FNR, init,
		false);
	if (!pyld) {
		IPAERR("failed to generate pyld\n");
		return -EPERM;
	}

	if (pyld->len > smem_size) {
		IPAERR("SRAM partition too small: %d needed %d\n",
			smem_size, pyld->len);
		ret = -EPERM;
		goto destroy_init_pyld;
	}

	dma_address = dma_map_single(ipa3_ctx->pdev,
		pyld->data,
		pyld->len,
		DMA_TO_DEVICE);
	if (dma_mapping_error(ipa3_ctx->pdev, dma_address)) {
		IPAERR("failed to DMA map\n");
		ret = -EPERM;
		goto destroy_init_pyld;
	}

	/* setting the registers and init the stats pyld are done atomically */
	flt_rt_start_id.skip_pipeline_clear = false;
	flt_rt_start_id.pipeline_clear_options = IPAHAL_FULL_PIPELINE_CLEAR;
	flt_rt_start_id.offset = start_id_ofst;
	flt_rt_start_id.value = start_id;
	flt_rt_start_id.value_mask = 0x3FF;
	flt_rt_start_id_pyld = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_REGISTER_WRITE, &flt_rt_start_id, false);
	if (!flt_rt_start_id_pyld) {
		IPAERR("failed to construct register_write imm cmd\n");
		ret = -ENOMEM;
		goto unmap;
	}
	desc[0].opcode = flt_rt_start_id_pyld->opcode;
	desc[0].pyld = flt_rt_start_id_pyld->data;
	desc[0].len = flt_rt_start_id_pyld->len;
	desc[0].type = IPA_IMM_CMD_DESC;

	flt_rt_end_id.skip_pipeline_clear = false;
	flt_rt_end_id.pipeline_clear_options = IPAHAL_FULL_PIPELINE_CLEAR;
	flt_rt_end_id.offset = end_id_ofst;
	flt_rt_end_id.value = end_id;
	flt_rt_end_id.value_mask = 0x3FF;
	flt_rt_end_id_pyld = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_REGISTER_WRITE, &flt_rt_end_id, false);
	if (!flt_rt_end_id_pyld) {
		IPAERR("failed to construct register_write imm cmd\n");
		ret = -ENOMEM;
		goto destroy_flt_rt_start_id;
	}
	desc[1].opcode = flt_rt_end_id_pyld->opcode;
	desc[1].pyld = flt_rt_end_id_pyld->data;
	desc[1].len = flt_rt_end_id_pyld->len;
	desc[1].type = IPA_IMM_CMD_DESC;

	flt_rt_base.skip_pipeline_clear = false;
	flt_rt_base.pipeline_clear_options = IPAHAL_FULL_PIPELINE_CLEAR;
	flt_rt_base.offset = stats_base;
	flt_rt_base.value = ipa3_ctx->smem_restricted_bytes +
		smem_ofst;
	flt_rt_base.value_mask = ~0;
	flt_rt_base_pyld = ipahal_construct_imm_cmd(IPA_IMM_CMD_REGISTER_WRITE,
		&flt_rt_base, false);
	if (!flt_rt_base_pyld) {
		IPAERR("failed to construct register_write imm cmd\n");
		ret = -ENOMEM;
		goto destroy_flt_rt_end_id;
	}
	desc[2].opcode = flt_rt_base_pyld->opcode;
	desc[2].pyld = flt_rt_base_pyld->data;
	desc[2].len = flt_rt_base_pyld->len;
	desc[2].type = IPA_IMM_CMD_DESC;

	cmd.is_read = false;
	cmd.skip_pipeline_clear = false;
	cmd.pipeline_clear_options = IPAHAL_FULL_PIPELINE_CLEAR;
	cmd.size = pyld->len;
	cmd.system_addr = dma_address;
	cmd.local_addr = ipa3_ctx->smem_restricted_bytes +
			smem_ofst;
	cmd_pyld = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_DMA_SHARED_MEM, &cmd, false);
	if (!cmd_pyld) {
		IPAERR("failed to construct dma_shared_mem imm cmd\n");
		ret = -ENOMEM;
		goto destroy_flt_rt_base;
	}
	desc[3].opcode = cmd_pyld->opcode;
	desc[3].pyld = cmd_pyld->data;
	desc[3].len = cmd_pyld->len;
	desc[3].type = IPA_IMM_CMD_DESC;

	ret = ipa3_send_cmd(4, desc);
	if (ret) {
		IPAERR("failed to send immediate command (error %d)\n", ret);
		goto destroy_imm;
	}

	ret = 0;

destroy_imm:
	ipahal_destroy_imm_cmd(cmd_pyld);
destroy_flt_rt_base:
	ipahal_destroy_imm_cmd(flt_rt_base_pyld);
destroy_flt_rt_end_id:
	ipahal_destroy_imm_cmd(flt_rt_end_id_pyld);
destroy_flt_rt_start_id:
	ipahal_destroy_imm_cmd(flt_rt_start_id_pyld);
unmap:
	dma_unmap_single(ipa3_ctx->pdev, dma_address, pyld->len, DMA_TO_DEVICE);
destroy_init_pyld:
	ipahal_destroy_stats_init_pyld(pyld);
	return ret;
}

int ipa_flt_rt_stats_clear_rule_ids(enum ipa_ip_type ip, bool filtering)
{
	struct ipahal_stats_init_flt_rt *init;
	int i;

	if (!ipa3_ctx->hw_stats.enabled)
		return 0;

	if (ip < 0 || ip >= IPA_IP_MAX) {
		IPAERR("wrong ip type %d\n", ip);
		return -EINVAL;
	}

	if (ip == IPA_IP_v4 && filtering)
		init = &ipa3_ctx->hw_stats.flt_rt.flt_v4_init;
	else if (ip == IPA_IP_v4)
		init = &ipa3_ctx->hw_stats.flt_rt.rt_v4_init;
	else if (ip == IPA_IP_v6 && filtering)
		init = &ipa3_ctx->hw_stats.flt_rt.flt_v6_init;
	else
		init = &ipa3_ctx->hw_stats.flt_rt.rt_v6_init;

	for (i = 0; i < IPAHAL_MAX_RULE_ID_32; i++)
		init->rule_id_bitmask[i] = 0;

	return 0;
}

static int __ipa_get_flt_rt_stats(enum ipa_ip_type ip, bool filtering,
	u16 rule_id, struct ipa_flt_rt_stats *out)
{
	int ret;
	int smem_ofst;
	bool clear = false;
	struct ipahal_stats_get_offset_flt_rt *get_offset;
	struct ipahal_stats_offset offset = { 0 };
	struct ipahal_imm_cmd_dma_shared_mem cmd = { 0 };
	struct ipahal_imm_cmd_pyld *cmd_pyld;
	struct ipa_mem_buffer mem;
	struct ipa3_desc desc = { 0 };
	struct ipahal_stats_flt_rt stats;

	if (rule_id >= IPAHAL_MAX_RULE_ID_32 * 32) {
		IPAERR("invalid rule_id %d\n", rule_id);
		return -EINVAL;
	}

	if (out == NULL)
		clear = true;

	get_offset = kzalloc(sizeof(*get_offset), GFP_KERNEL);
	if (!get_offset) {
		IPADBG("no mem\n");
		return -ENOMEM;
	}

	if (ip == IPA_IP_v4 && filtering) {
		get_offset->init = ipa3_ctx->hw_stats.flt_rt.flt_v4_init;
		smem_ofst = IPA_MEM_PART(stats_flt_v4_ofst);
	} else if (ip == IPA_IP_v4) {
		get_offset->init = ipa3_ctx->hw_stats.flt_rt.rt_v4_init;
		smem_ofst = IPA_MEM_PART(stats_rt_v4_ofst);
	} else if (ip == IPA_IP_v6 && filtering) {
		get_offset->init = ipa3_ctx->hw_stats.flt_rt.flt_v6_init;
		smem_ofst = IPA_MEM_PART(stats_flt_v6_ofst);
	} else {
		get_offset->init = ipa3_ctx->hw_stats.flt_rt.rt_v6_init;
		smem_ofst = IPA_MEM_PART(stats_rt_v6_ofst);
	}

	get_offset->rule_id = rule_id;

	ret = ipahal_stats_get_offset(IPAHAL_HW_STATS_FNR, get_offset,
		&offset);
	if (ret) {
		IPAERR("failed to get offset from hal %d\n", ret);
		goto free_offset;
	}

	IPADBG_LOW("offset = %d size = %d\n", offset.offset, offset.size);

	if (offset.size == 0) {
		ret = 0;
		goto free_offset;
	}

	mem.size = offset.size;
	mem.base = dma_alloc_coherent(ipa3_ctx->pdev,
		mem.size,
		&mem.phys_base,
		GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA memory\n");
		goto free_offset;
	}

	cmd.is_read = true;
	cmd.clear_after_read = clear;
	cmd.skip_pipeline_clear = false;
	cmd.pipeline_clear_options = IPAHAL_HPS_CLEAR;
	cmd.size = mem.size;
	cmd.system_addr = mem.phys_base;
	cmd.local_addr = ipa3_ctx->smem_restricted_bytes +
		smem_ofst + offset.offset;
	cmd_pyld = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_DMA_SHARED_MEM, &cmd, false);
	if (!cmd_pyld) {
		IPAERR("failed to construct dma_shared_mem imm cmd\n");
		ret = -ENOMEM;
		goto free_dma_mem;
	}
	desc.opcode = cmd_pyld->opcode;
	desc.pyld = cmd_pyld->data;
	desc.len = cmd_pyld->len;
	desc.type = IPA_IMM_CMD_DESC;

	ret = ipa3_send_cmd(1, &desc);
	if (ret) {
		IPAERR("failed to send immediate command (error %d)\n", ret);
		goto destroy_imm;
	}

	ret = ipahal_parse_stats(IPAHAL_HW_STATS_FNR,
		&get_offset->init, mem.base, &stats);
	if (ret) {
		IPAERR("failed to parse stats (error %d)\n", ret);
		goto destroy_imm;
	}

	if (out) {
		out->num_pkts = stats.num_packets;
		out->num_pkts_hash = stats.num_packets_hash;
	}

	ret = 0;

destroy_imm:
	ipahal_destroy_imm_cmd(cmd_pyld);
free_dma_mem:
	dma_free_coherent(ipa3_ctx->pdev, mem.size, mem.base, mem.phys_base);
free_offset:
	kfree(get_offset);
	return ret;

}


int ipa_get_flt_rt_stats(enum ipa_ip_type ip, bool filtering, u16 rule_id,
	struct ipa_flt_rt_stats *out)
{
	if (!ipa3_ctx->hw_stats.enabled)
		return 0;

	if (ip < 0 || ip >= IPA_IP_MAX) {
		IPAERR("wrong ip type %d\n", ip);
		return -EINVAL;
	}

	return __ipa_get_flt_rt_stats(ip, filtering, rule_id, out);
}

int ipa_reset_flt_rt_stats(enum ipa_ip_type ip, bool filtering, u16 rule_id)
{
	if (!ipa3_ctx->hw_stats.enabled)
		return 0;

	if (ip < 0 || ip >= IPA_IP_MAX) {
		IPAERR("wrong ip type %d\n", ip);
		return -EINVAL;
	}

	return __ipa_get_flt_rt_stats(ip, filtering, rule_id, NULL);
}

int ipa_reset_all_flt_rt_stats(enum ipa_ip_type ip, bool filtering)
{
	struct ipahal_stats_init_flt_rt *init;
	int i;

	if (!ipa3_ctx->hw_stats.enabled)
		return 0;

	if (ip < 0 || ip >= IPA_IP_MAX) {
		IPAERR("wrong ip type %d\n", ip);
		return -EINVAL;
	}

	if (ip == IPA_IP_v4 && filtering)
		init = &ipa3_ctx->hw_stats.flt_rt.flt_v4_init;
	else if (ip == IPA_IP_v4)
		init = &ipa3_ctx->hw_stats.flt_rt.rt_v4_init;
	else if (ip == IPA_IP_v6 && filtering)
		init = &ipa3_ctx->hw_stats.flt_rt.flt_v6_init;
	else
		init = &ipa3_ctx->hw_stats.flt_rt.rt_v6_init;

	for (i = 0; i < IPAHAL_MAX_RULE_ID_32 * 32; i++) {
		int idx = i / 32;
		int bit = i % 32;

		if (init->rule_id_bitmask[idx] & (1 << bit))
			__ipa_get_flt_rt_stats(ip, filtering, i, NULL);
	}

	return 0;
}

int ipa_init_drop_stats(u32 pipe_bitmask)
{
	struct ipahal_stats_init_pyld *pyld;
	struct ipahal_imm_cmd_dma_shared_mem cmd = { 0 };
	struct ipahal_imm_cmd_pyld *cmd_pyld;
	struct ipahal_imm_cmd_register_write drop_base = {0};
	struct ipahal_imm_cmd_pyld *drop_base_pyld;
	struct ipahal_imm_cmd_register_write drop_mask = {0};
	struct ipahal_imm_cmd_pyld *drop_mask_pyld;
	struct ipa3_desc desc[3] = { {0} };
	dma_addr_t dma_address;
	int ret;

	if (!ipa3_ctx->hw_stats.enabled)
		return 0;

	/* reset driver's cache */
	memset(&ipa3_ctx->hw_stats.drop, 0, sizeof(ipa3_ctx->hw_stats.drop));
	ipa3_ctx->hw_stats.drop.init.enabled_bitmask = pipe_bitmask;
	IPADBG_LOW("pipe_bitmask=0x%x\n", pipe_bitmask);

	pyld = ipahal_stats_generate_init_pyld(IPAHAL_HW_STATS_DROP,
		&ipa3_ctx->hw_stats.drop.init, false);
	if (!pyld) {
		IPAERR("failed to generate pyld\n");
		return -EPERM;
	}

	if (pyld->len > IPA_MEM_PART(stats_drop_size)) {
		IPAERR("SRAM partition too small: %d needed %d\n",
			IPA_MEM_PART(stats_drop_size), pyld->len);
		ret = -EPERM;
		goto destroy_init_pyld;
	}

	dma_address = dma_map_single(ipa3_ctx->pdev,
		pyld->data,
		pyld->len,
		DMA_TO_DEVICE);
	if (dma_mapping_error(ipa3_ctx->pdev, dma_address)) {
		IPAERR("failed to DMA map\n");
		ret = -EPERM;
		goto destroy_init_pyld;
	}

	/* setting the registers and init the stats pyld are done atomically */
	drop_mask.skip_pipeline_clear = false;
	drop_mask.pipeline_clear_options = IPAHAL_FULL_PIPELINE_CLEAR;
	drop_mask.offset = ipahal_get_reg_n_ofst(IPA_STAT_DROP_CNT_MASK_n,
		ipa3_ctx->ee);
	drop_mask.value = pipe_bitmask;
	drop_mask.value_mask = ~0;
	drop_mask_pyld = ipahal_construct_imm_cmd(IPA_IMM_CMD_REGISTER_WRITE,
		&drop_mask, false);
	if (!drop_mask_pyld) {
		IPAERR("failed to construct register_write imm cmd\n");
		ret = -ENOMEM;
		goto unmap;
	}
	desc[0].opcode = drop_mask_pyld->opcode;
	desc[0].pyld = drop_mask_pyld->data;
	desc[0].len = drop_mask_pyld->len;
	desc[0].type = IPA_IMM_CMD_DESC;

	drop_base.skip_pipeline_clear = false;
	drop_base.pipeline_clear_options = IPAHAL_FULL_PIPELINE_CLEAR;
	drop_base.offset = ipahal_get_reg_n_ofst(IPA_STAT_DROP_CNT_BASE_n,
		ipa3_ctx->ee);
	drop_base.value = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(stats_drop_ofst);
	drop_base.value_mask = ~0;
	drop_base_pyld = ipahal_construct_imm_cmd(IPA_IMM_CMD_REGISTER_WRITE,
		&drop_base, false);
	if (!drop_base_pyld) {
		IPAERR("failed to construct register_write imm cmd\n");
		ret = -ENOMEM;
		goto destroy_drop_mask;
	}
	desc[1].opcode = drop_base_pyld->opcode;
	desc[1].pyld = drop_base_pyld->data;
	desc[1].len = drop_base_pyld->len;
	desc[1].type = IPA_IMM_CMD_DESC;

	cmd.is_read = false;
	cmd.skip_pipeline_clear = false;
	cmd.pipeline_clear_options = IPAHAL_FULL_PIPELINE_CLEAR;
	cmd.size = pyld->len;
	cmd.system_addr = dma_address;
	cmd.local_addr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(stats_drop_ofst);
	cmd_pyld = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_DMA_SHARED_MEM, &cmd, false);
	if (!cmd_pyld) {
		IPAERR("failed to construct dma_shared_mem imm cmd\n");
		ret = -ENOMEM;
		goto destroy_drop_base;
	}
	desc[2].opcode = cmd_pyld->opcode;
	desc[2].pyld = cmd_pyld->data;
	desc[2].len = cmd_pyld->len;
	desc[2].type = IPA_IMM_CMD_DESC;

	ret = ipa3_send_cmd(3, desc);
	if (ret) {
		IPAERR("failed to send immediate command (error %d)\n", ret);
		goto destroy_imm;
	}

	ret = 0;

destroy_imm:
	ipahal_destroy_imm_cmd(cmd_pyld);
destroy_drop_base:
	ipahal_destroy_imm_cmd(drop_base_pyld);
destroy_drop_mask:
	ipahal_destroy_imm_cmd(drop_mask_pyld);
unmap:
	dma_unmap_single(ipa3_ctx->pdev, dma_address, pyld->len, DMA_TO_DEVICE);
destroy_init_pyld:
	ipahal_destroy_stats_init_pyld(pyld);
	return ret;
}

int ipa_get_drop_stats(struct ipa_drop_stats_all *out)
{
	int i;
	int ret;
	struct ipahal_stats_get_offset_drop get_offset = { { 0 } };
	struct ipahal_stats_offset offset = { 0 };
	struct ipahal_imm_cmd_dma_shared_mem cmd = { 0 };
	struct ipahal_imm_cmd_pyld *cmd_pyld;
	struct ipa_mem_buffer mem;
	struct ipa3_desc desc = { 0 };
	struct ipahal_stats_drop_all *stats;

	if (!ipa3_ctx->hw_stats.enabled)
		return 0;

	get_offset.init = ipa3_ctx->hw_stats.drop.init;
	ret = ipahal_stats_get_offset(IPAHAL_HW_STATS_DROP, &get_offset,
		&offset);
	if (ret) {
		IPAERR("failed to get offset from hal %d\n", ret);
		return ret;
	}

	IPADBG_LOW("offset = %d size = %d\n", offset.offset, offset.size);

	if (offset.size == 0)
		return 0;

	mem.size = offset.size;
	mem.base = dma_alloc_coherent(ipa3_ctx->pdev,
		mem.size,
		&mem.phys_base,
		GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA memory\n");
		return ret;
	}

	cmd.is_read = true;
	cmd.clear_after_read = true;
	cmd.skip_pipeline_clear = false;
	cmd.pipeline_clear_options = IPAHAL_HPS_CLEAR;
	cmd.size = mem.size;
	cmd.system_addr = mem.phys_base;
	cmd.local_addr = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(stats_drop_ofst) + offset.offset;
	cmd_pyld = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_DMA_SHARED_MEM, &cmd, false);
	if (!cmd_pyld) {
		IPAERR("failed to construct dma_shared_mem imm cmd\n");
		ret = -ENOMEM;
		goto free_dma_mem;
	}
	desc.opcode = cmd_pyld->opcode;
	desc.pyld = cmd_pyld->data;
	desc.len = cmd_pyld->len;
	desc.type = IPA_IMM_CMD_DESC;

	ret = ipa3_send_cmd(1, &desc);
	if (ret) {
		IPAERR("failed to send immediate command (error %d)\n", ret);
		goto destroy_imm;
	}

	stats = kzalloc(sizeof(*stats), GFP_KERNEL);
	if (!stats) {
		ret = -ENOMEM;
		goto destroy_imm;
	}

	ret = ipahal_parse_stats(IPAHAL_HW_STATS_DROP,
		&ipa3_ctx->hw_stats.drop.init, mem.base, stats);
	if (ret) {
		IPAERR("failed to parse stats (error %d)\n", ret);
		goto free_stats;
	}

	/*
	 * update driver cache.
	 * the stats were read from hardware with clear_after_read meaning
	 * hardware stats are 0 now
	 */
	for (i = 0; i < IPA_CLIENT_MAX; i++) {
		int ep_idx = ipa3_get_ep_mapping(i);

		if (ep_idx == -1 || ep_idx >= IPA3_MAX_NUM_PIPES)
			continue;

		if (ipa3_ctx->ep[ep_idx].client != i)
			continue;

		ipa3_ctx->hw_stats.drop.stats.client[i].drop_byte_cnt +=
			stats->stats[ep_idx].drop_byte_cnt;
		ipa3_ctx->hw_stats.drop.stats.client[i].drop_packet_cnt +=
			stats->stats[ep_idx].drop_packet_cnt;
	}


	if (!out) {
		ret = 0;
		goto free_stats;
	}

	/* copy results to out parameter */
	*out = ipa3_ctx->hw_stats.drop.stats;

	ret = 0;
free_stats:
	kfree(stats);
destroy_imm:
	ipahal_destroy_imm_cmd(cmd_pyld);
free_dma_mem:
	dma_free_coherent(ipa3_ctx->pdev, mem.size, mem.base, mem.phys_base);
	return ret;

}

int ipa_reset_drop_stats(enum ipa_client_type client)
{
	int ret;
	struct ipa_drop_stats *stats;

	if (client >= IPA_CLIENT_MAX) {
		IPAERR("invalid client %d\n", client);
		return -EINVAL;
	}

	/* reading stats will reset them in hardware */
	ret = ipa_get_drop_stats(NULL);
	if (ret) {
		IPAERR("ipa_get_drop_stats failed %d\n", ret);
		return ret;
	}

	/* reset driver's cache */
	stats = &ipa3_ctx->hw_stats.drop.stats.client[client];
	memset(stats, 0, sizeof(*stats));
	return 0;
}

int ipa_reset_all_drop_stats(void)
{
	int ret;
	struct ipa_drop_stats_all *stats;

	if (!ipa3_ctx->hw_stats.enabled)
		return 0;

	/* reading stats will reset them in hardware */
	ret = ipa_get_drop_stats(NULL);
	if (ret) {
		IPAERR("ipa_get_drop_stats failed %d\n", ret);
		return ret;
	}

	/* reset driver's cache */
	stats = &ipa3_ctx->hw_stats.drop.stats;
	memset(stats, 0, sizeof(*stats));
	return 0;
}


#ifndef CONFIG_DEBUG_FS
int ipa_debugfs_init_stats(struct dentry *parent) { return 0; }
#else
#define IPA_MAX_MSG_LEN 4096
static char dbg_buff[IPA_MAX_MSG_LEN];

static ssize_t ipa_debugfs_reset_quota_stats(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	unsigned long missing;
	s8 client = 0;
	int ret;

	mutex_lock(&ipa3_ctx->lock);
	if (sizeof(dbg_buff) < count + 1) {
		ret = -EFAULT;
		goto bail;
	}

	missing = copy_from_user(dbg_buff, ubuf, count);
	if (missing) {
		ret = -EFAULT;
		goto bail;
	}

	dbg_buff[count] = '\0';
	if (kstrtos8(dbg_buff, 0, &client)) {
		ret = -EFAULT;
		goto bail;
	}

	if (client == -1)
		ipa_reset_all_quota_stats();
	else
		ipa_reset_quota_stats(client);

	ret = count;
bail:
	mutex_unlock(&ipa3_ctx->lock);
	return ret;
}

static ssize_t ipa_debugfs_print_quota_stats(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos)
{
	int nbytes = 0;
	struct ipa_quota_stats_all *out;
	int i;
	int res;

	out = kzalloc(sizeof(*out), GFP_KERNEL);
	if (!out)
		return -ENOMEM;

	mutex_lock(&ipa3_ctx->lock);
	res = ipa_get_quota_stats(out);
	if (res) {
		mutex_unlock(&ipa3_ctx->lock);
		kfree(out);
		return res;
	}
	for (i = 0; i < IPA_CLIENT_MAX; i++) {
		int ep_idx = ipa3_get_ep_mapping(i);

		if (ep_idx == -1)
			continue;

		if (IPA_CLIENT_IS_TEST(i))
			continue;

		if (!(ipa3_ctx->hw_stats.quota.init.enabled_bitmask &
			(1 << ep_idx)))
			continue;

		nbytes += scnprintf(dbg_buff + nbytes,
			IPA_MAX_MSG_LEN - nbytes,
			"%s:\n",
			ipa_clients_strings[i]);
		nbytes += scnprintf(dbg_buff + nbytes,
			IPA_MAX_MSG_LEN - nbytes,
			"num_ipv4_bytes=%llu\n",
			out->client[i].num_ipv4_bytes);
		nbytes += scnprintf(dbg_buff + nbytes,
			IPA_MAX_MSG_LEN - nbytes,
			"num_ipv6_bytes=%llu\n",
			out->client[i].num_ipv6_bytes);
		nbytes += scnprintf(dbg_buff + nbytes,
			IPA_MAX_MSG_LEN - nbytes,
			"num_ipv4_pkts=%u\n",
			out->client[i].num_ipv4_pkts);
		nbytes += scnprintf(dbg_buff + nbytes,
			IPA_MAX_MSG_LEN - nbytes,
			"num_ipv6_pkts=%u\n",
			out->client[i].num_ipv6_pkts);
		nbytes += scnprintf(dbg_buff + nbytes,
			IPA_MAX_MSG_LEN - nbytes,
			"\n");

	}
	mutex_unlock(&ipa3_ctx->lock);
	kfree(out);

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t ipa_debugfs_reset_tethering_stats(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	unsigned long missing;
	s8 client = 0;
	int ret;

	mutex_lock(&ipa3_ctx->lock);
	if (sizeof(dbg_buff) < count + 1) {
		ret = -EFAULT;
		goto bail;
	}

	missing = copy_from_user(dbg_buff, ubuf, count);
	if (missing) {
		ret = -EFAULT;
		goto bail;
	}

	dbg_buff[count] = '\0';
	if (kstrtos8(dbg_buff, 0, &client)) {
		ret = -EFAULT;
		goto bail;
	}

	if (client == -1)
		ipa_reset_all_teth_stats();
	else
		ipa_reset_all_cons_teth_stats(client);

	ret = count;
bail:
	mutex_unlock(&ipa3_ctx->lock);
	return ret;
}

static ssize_t ipa_debugfs_print_tethering_stats(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos)
{
	int nbytes = 0;
	struct ipa_quota_stats_all *out;
	int i, j;
	int res;

	out = kzalloc(sizeof(*out), GFP_KERNEL);
	if (!out)
		return -ENOMEM;

	mutex_lock(&ipa3_ctx->lock);
	for (i = 0; i < IPA_CLIENT_MAX; i++) {
		int ep_idx = ipa3_get_ep_mapping(i);

		if (ep_idx == -1)
			continue;

		if (!IPA_CLIENT_IS_PROD(i))
			continue;

		if (IPA_CLIENT_IS_TEST(i))
			continue;

		if (!(ipa3_ctx->hw_stats.teth.init.prod_bitmask &
			(1 << ep_idx)))
			continue;

		res = ipa_get_teth_stats();
		if (res) {
			mutex_unlock(&ipa3_ctx->lock);
			kfree(out);
			return res;
		}

		for (j = 0; j < IPA_CLIENT_MAX; j++) {
			int cons_idx = ipa3_get_ep_mapping(j);

			if (cons_idx == -1)
				continue;

			if (IPA_CLIENT_IS_TEST(j))
				continue;

			if (!(ipa3_ctx->hw_stats.teth.init.cons_bitmask[ep_idx]
				& (1 << cons_idx)))
				continue;

			nbytes += scnprintf(dbg_buff + nbytes,
				IPA_MAX_MSG_LEN - nbytes,
				"%s->%s:\n",
				ipa_clients_strings[i],
				ipa_clients_strings[j]);
			nbytes += scnprintf(dbg_buff + nbytes,
				IPA_MAX_MSG_LEN - nbytes,
				"num_ipv4_bytes=%llu\n",
				out->client[j].num_ipv4_bytes);
			nbytes += scnprintf(dbg_buff + nbytes,
				IPA_MAX_MSG_LEN - nbytes,
				"num_ipv6_bytes=%llu\n",
				out->client[j].num_ipv6_bytes);
			nbytes += scnprintf(dbg_buff + nbytes,
				IPA_MAX_MSG_LEN - nbytes,
				"num_ipv4_pkts=%u\n",
				out->client[j].num_ipv4_pkts);
			nbytes += scnprintf(dbg_buff + nbytes,
				IPA_MAX_MSG_LEN - nbytes,
				"num_ipv6_pkts=%u\n",
				out->client[j].num_ipv6_pkts);
			nbytes += scnprintf(dbg_buff + nbytes,
				IPA_MAX_MSG_LEN - nbytes,
				"\n");
		}
	}
	mutex_unlock(&ipa3_ctx->lock);
	kfree(out);

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t ipa_debugfs_control_flt_rt_stats(enum ipa_ip_type ip,
	bool filtering, struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	unsigned long missing;
	u16 rule_id = 0;
	int ret;

	mutex_lock(&ipa3_ctx->lock);
	if (sizeof(dbg_buff) < count + 1) {
		ret = -EFAULT;
		goto bail;
	}

	missing = copy_from_user(dbg_buff, ubuf, count);
	if (missing) {
		ret = -EFAULT;
		goto bail;
	}

	dbg_buff[count] = '\0';
	if (strcmp(dbg_buff, "start\n") == 0) {
		ipa_flt_rt_stats_start(ip, filtering);
	} else if (strcmp(dbg_buff, "clear\n") == 0) {
		ipa_flt_rt_stats_clear_rule_ids(ip, filtering);
	} else if (strcmp(dbg_buff, "reset\n") == 0) {
		ipa_reset_all_flt_rt_stats(ip, filtering);
	} else {
		if (kstrtou16(dbg_buff, 0, &rule_id)) {
			ret = -EFAULT;
			goto bail;
		}
		ipa_flt_rt_stats_add_rule_id(ip, filtering, rule_id);
	}

	ret = count;
bail:
	mutex_unlock(&ipa3_ctx->lock);
	return ret;
}

static ssize_t ipa_debugfs_print_flt_rt_stats(enum ipa_ip_type ip,
	bool filtering, struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos)
{
	int nbytes = 0;
	struct ipahal_stats_init_flt_rt *init;
	struct ipa_flt_rt_stats out;
	int i;
	int res;

	if (ip == IPA_IP_v4 && filtering)
		init = &ipa3_ctx->hw_stats.flt_rt.flt_v4_init;
	else if (ip == IPA_IP_v4)
		init = &ipa3_ctx->hw_stats.flt_rt.rt_v4_init;
	else if (ip == IPA_IP_v6 && filtering)
		init = &ipa3_ctx->hw_stats.flt_rt.flt_v6_init;
	else
		init = &ipa3_ctx->hw_stats.flt_rt.rt_v6_init;

	mutex_lock(&ipa3_ctx->lock);
	for (i = 0; i < IPAHAL_MAX_RULE_ID_32 * 32; i++) {
		int idx = i / 32;
		int bit = i % 32;

		if (init->rule_id_bitmask[idx] & (1 << bit)) {
			res = ipa_get_flt_rt_stats(ip, filtering, i, &out);
			if (res) {
				mutex_unlock(&ipa3_ctx->lock);
				return res;
			}

			nbytes += scnprintf(dbg_buff + nbytes,
				IPA_MAX_MSG_LEN - nbytes,
				"rule_id: %d\n", i);
			nbytes += scnprintf(dbg_buff + nbytes,
				IPA_MAX_MSG_LEN - nbytes,
				"num_pkts: %d\n",
				out.num_pkts);
			nbytes += scnprintf(dbg_buff + nbytes,
				IPA_MAX_MSG_LEN - nbytes,
				"num_pkts_hash: %d\n",
				out.num_pkts_hash);
			nbytes += scnprintf(dbg_buff + nbytes,
				IPA_MAX_MSG_LEN - nbytes,
				"\n");
		}
	}

	mutex_unlock(&ipa3_ctx->lock);

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t ipa_debugfs_reset_drop_stats(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	unsigned long missing;
	s8 client = 0;
	int ret;

	mutex_lock(&ipa3_ctx->lock);
	if (sizeof(dbg_buff) < count + 1) {
		ret = -EFAULT;
		goto bail;
	}

	missing = copy_from_user(dbg_buff, ubuf, count);
	if (missing) {
		ret = -EFAULT;
		goto bail;
	}

	dbg_buff[count] = '\0';
	if (kstrtos8(dbg_buff, 0, &client)) {
		ret = -EFAULT;
		goto bail;
	}

	if (client == -1)
		ipa_reset_all_drop_stats();
	else
		ipa_reset_drop_stats(client);

	ret = count;
bail:
	mutex_unlock(&ipa3_ctx->lock);
	return count;
}

static ssize_t ipa_debugfs_print_drop_stats(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos)
{
	int nbytes = 0;
	struct ipa_drop_stats_all *out;
	int i;
	int res;

	out = kzalloc(sizeof(*out), GFP_KERNEL);
	if (!out)
		return -ENOMEM;

	mutex_lock(&ipa3_ctx->lock);
	res = ipa_get_drop_stats(out);
	if (res) {
		mutex_unlock(&ipa3_ctx->lock);
		kfree(out);
		return res;
	}

	for (i = 0; i < IPA_CLIENT_MAX; i++) {
		int ep_idx = ipa3_get_ep_mapping(i);

		if (ep_idx == -1)
			continue;

		if (!IPA_CLIENT_IS_CONS(i))
			continue;

		if (IPA_CLIENT_IS_TEST(i))
			continue;

		if (!(ipa3_ctx->hw_stats.drop.init.enabled_bitmask &
			(1 << ep_idx)))
			continue;


		nbytes += scnprintf(dbg_buff + nbytes,
			IPA_MAX_MSG_LEN - nbytes,
			"%s:\n",
			ipa_clients_strings[i]);

		nbytes += scnprintf(dbg_buff + nbytes,
			IPA_MAX_MSG_LEN - nbytes,
			"drop_byte_cnt=%u\n",
			out->client[i].drop_byte_cnt);

		nbytes += scnprintf(dbg_buff + nbytes,
			IPA_MAX_MSG_LEN - nbytes,
			"drop_packet_cnt=%u\n",
			out->client[i].drop_packet_cnt);
		nbytes += scnprintf(dbg_buff + nbytes,
			IPA_MAX_MSG_LEN - nbytes,
			"\n");
	}
	mutex_unlock(&ipa3_ctx->lock);
	kfree(out);

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t ipa_debugfs_control_flt_v4_stats(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	return ipa_debugfs_control_flt_rt_stats(IPA_IP_v4, true, file, ubuf,
		count, ppos);
}

static ssize_t ipa_debugfs_control_flt_v6_stats(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	return ipa_debugfs_control_flt_rt_stats(IPA_IP_v6, true, file, ubuf,
		count, ppos);
}

static ssize_t ipa_debugfs_control_rt_v4_stats(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	return ipa_debugfs_control_flt_rt_stats(IPA_IP_v4, false, file, ubuf,
		count, ppos);
}

static ssize_t ipa_debugfs_control_rt_v6_stats(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	return ipa_debugfs_control_flt_rt_stats(IPA_IP_v6, false, file, ubuf,
		count, ppos);
}

static ssize_t ipa_debugfs_print_flt_v4_stats(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos)
{
	return ipa_debugfs_print_flt_rt_stats(IPA_IP_v4, true, file, ubuf,
		count, ppos);
}

static ssize_t ipa_debugfs_print_flt_v6_stats(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos)
{
	return ipa_debugfs_print_flt_rt_stats(IPA_IP_v6, true, file, ubuf,
		count, ppos);
}

static ssize_t ipa_debugfs_print_rt_v4_stats(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos)
{
	return ipa_debugfs_print_flt_rt_stats(IPA_IP_v4, false, file, ubuf,
		count, ppos);
}

static ssize_t ipa_debugfs_print_rt_v6_stats(struct file *file,
	char __user *ubuf, size_t count, loff_t *ppos)
{
	return ipa_debugfs_print_flt_rt_stats(IPA_IP_v6, false, file, ubuf,
		count, ppos);
}

static const struct file_operations ipa3_quota_ops = {
	.read = ipa_debugfs_print_quota_stats,
	.write = ipa_debugfs_reset_quota_stats,
};

static const struct file_operations ipa3_tethering_ops = {
	.read = ipa_debugfs_print_tethering_stats,
	.write = ipa_debugfs_reset_tethering_stats,
};

static const struct file_operations ipa3_flt_v4_ops = {
	.read = ipa_debugfs_print_flt_v4_stats,
	.write = ipa_debugfs_control_flt_v4_stats,
};

static const struct file_operations ipa3_flt_v6_ops = {
	.read = ipa_debugfs_print_flt_v6_stats,
	.write = ipa_debugfs_control_flt_v6_stats,
};

static const struct file_operations ipa3_rt_v4_ops = {
	.read = ipa_debugfs_print_rt_v4_stats,
	.write = ipa_debugfs_control_rt_v4_stats,
};

static const struct file_operations ipa3_rt_v6_ops = {
	.read = ipa_debugfs_print_rt_v6_stats,
	.write = ipa_debugfs_control_rt_v6_stats,
};

static const struct file_operations ipa3_drop_ops = {
	.read = ipa_debugfs_print_drop_stats,
	.write = ipa_debugfs_reset_drop_stats,
};


int ipa_debugfs_init_stats(struct dentry *parent)
{
	const mode_t read_write_mode = 0664;
	struct dentry *file;
	struct dentry *dent;

	if (!ipa3_ctx->hw_stats.enabled)
		return 0;

	dent = debugfs_create_dir("hw_stats", parent);
	if (IS_ERR_OR_NULL(dent)) {
		IPAERR("fail to create folder in debug_fs\n");
		return -EFAULT;
	}

	file = debugfs_create_file("quota", read_write_mode, dent, NULL,
		&ipa3_quota_ops);
	if (IS_ERR_OR_NULL(file)) {
		IPAERR("fail to create file %s\n", "quota");
		goto fail;
	}

	file = debugfs_create_file("drop", read_write_mode, dent, NULL,
		&ipa3_drop_ops);
	if (IS_ERR_OR_NULL(file)) {
		IPAERR("fail to create file %s\n", "drop");
		goto fail;
	}

	file = debugfs_create_file("tethering", read_write_mode, dent, NULL,
		&ipa3_tethering_ops);
	if (IS_ERR_OR_NULL(file)) {
		IPAERR("fail to create file %s\n", "tethering");
		goto fail;
	}

	file = debugfs_create_file("flt_v4", read_write_mode, dent, NULL,
		&ipa3_flt_v4_ops);
	if (IS_ERR_OR_NULL(file)) {
		IPAERR("fail to create file %s\n", "flt_v4");
		goto fail;
	}

	file = debugfs_create_file("flt_v6", read_write_mode, dent, NULL,
		&ipa3_flt_v6_ops);
	if (IS_ERR_OR_NULL(file)) {
		IPAERR("fail to create file %s\n", "flt_v6");
		goto fail;
	}

	file = debugfs_create_file("rt_v4", read_write_mode, dent, NULL,
		&ipa3_rt_v4_ops);
	if (IS_ERR_OR_NULL(file)) {
		IPAERR("fail to create file %s\n", "rt_v4");
		goto fail;
	}

	file = debugfs_create_file("rt_v6", read_write_mode, dent, NULL,
		&ipa3_rt_v6_ops);
	if (IS_ERR_OR_NULL(file)) {
		IPAERR("fail to create file %s\n", "rt_v6");
		goto fail;
	}

	return 0;
fail:
	debugfs_remove_recursive(dent);
	return -EFAULT;
}
#endif
