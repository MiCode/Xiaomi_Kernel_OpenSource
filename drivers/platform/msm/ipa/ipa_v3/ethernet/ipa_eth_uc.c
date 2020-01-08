/* Copyright (c) 2019 The Linux Foundation. All rights reserved.
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

#include "../ipa_i.h"
#include "../ipa_uc_offload_i.h"

#include "ipa_eth_i.h"

#define IPA_ETH_UC_RESPONSE_SUCCESS \
	FEATURE_ENUM_VAL(IPA_HW_FEATURE_OFFLOAD, IPA_ETH_UC_RSP_SUCCESS)

struct ipa_eth_uc_cmd_param {
	u32 protocol;
	u8 protocol_data[];
} __packed;

struct ipa_eth_uc_cmd_param_mem {
	struct ipa_eth_uc_cmd_param *param;
	dma_addr_t dma_addr;
	size_t size;
};

struct ipa_eth_uc_cmd_ctx {
	u8 cmd_op;
	u32 protocol;
	const void *protocol_data;
	size_t data_size;

	struct ipa_eth_uc_cmd_param_mem pmem;
};

static int ipa_eth_uc_init_cmd_ctx(struct ipa_eth_uc_cmd_ctx *cmd_ctx,
				   enum ipa_eth_uc_op op, u32 protocol,
				   const void *prot_data, size_t datasz)
{
	struct ipa_eth_uc_cmd_param_mem *pmem = &cmd_ctx->pmem;

	if (op == IPA_ETH_UC_OP_NOP || op >= IPA_ETH_UC_OP_MAX) {
		ipa_eth_err("Invalid uC op code");
		return -EINVAL;
	}

	cmd_ctx->cmd_op = FEATURE_ENUM_VAL(IPA_HW_FEATURE_OFFLOAD, op);
	cmd_ctx->protocol = protocol;
	cmd_ctx->protocol_data = prot_data;
	cmd_ctx->data_size = datasz;

	pmem->size = sizeof(*pmem->param) + datasz;
	pmem->param = dma_alloc_coherent(ipa3_ctx->uc_pdev, pmem->size,
				&pmem->dma_addr, GFP_KERNEL);
	if (!pmem->param) {
		ipa_eth_err("Failed to alloc uC command DMA buffer of size %u",
			    pmem->size);
		return -ENOMEM;
	}

	pmem->param->protocol = protocol;
	memcpy(pmem->param->protocol_data, prot_data, datasz);

	return 0;
}

static void ipa_eth_uc_deinit_cmd_ctx(struct ipa_eth_uc_cmd_ctx *cmd_ctx)
{
	struct ipa_eth_uc_cmd_param_mem *pmem = &cmd_ctx->pmem;

	dma_free_coherent(ipa3_ctx->uc_pdev, pmem->size,
		pmem->param, pmem->dma_addr);
}

/**
 * ipa_eth_uc_send_cmd() - Send an offload command to IPA uC
 * @op: uC offload op code
 * @protocol: uC offload protocol value
 * @prot_data: uC offload command data, specific to the protocol
 * @datasz: size of command data
 *
 * Return: 0 on success, negative errno otherwise
 */
int ipa_eth_uc_send_cmd(enum ipa_eth_uc_op op, u32 protocol,
			const void *prot_data, size_t datasz)
{
	int rc = 0;
	struct ipa_eth_uc_cmd_ctx cmd_ctx;

	rc = ipa_eth_uc_init_cmd_ctx(&cmd_ctx, op, protocol, prot_data, datasz);
	if (rc) {
		ipa_eth_err("Failed to init command context for op=%u", op);
		return rc;
	}

	ipa_eth_log("Sending uC command, op=%u, prot=%u, data size=%u",
		    cmd_ctx.cmd_op, cmd_ctx.protocol, cmd_ctx.data_size);

	rc = ipa3_uc_send_cmd((u32)cmd_ctx.pmem.dma_addr, cmd_ctx.cmd_op,
			IPA_ETH_UC_RESPONSE_SUCCESS, false, 10*HZ);

	ipa_eth_uc_deinit_cmd_ctx(&cmd_ctx);

	return rc;
}
EXPORT_SYMBOL(ipa_eth_uc_send_cmd);

/* Define IPA hardware constants not available from IPA header files */

#define IPA_HW_DIR_PRODUCER 0
#define IPA_HW_DIR_CONSUMER 1

#define IPA_HW_CH_ID_INVALID 0xFF
#define IPA_HW_PROTOCOL_INVALID 0xFFFFFFFF

static u32 find_uc_protocol(struct ipa_eth_device *eth_dev)
{
	int protocol;
	struct ipa_eth_channel *ch;

	list_for_each_entry(ch, &eth_dev->rx_channels, channel_list) {
		protocol = ipa_get_prot_id(ch->ipa_client);
		if (protocol >= 0)
			return lower_32_bits(protocol);
	}

	list_for_each_entry(ch, &eth_dev->tx_channels, channel_list) {
		protocol = ipa_get_prot_id(ch->ipa_client);
		if (protocol >= 0)
			return lower_32_bits(protocol);
	}

	return IPA_HW_PROTOCOL_INVALID;
}

static int find_client_channel(enum ipa_client_type client)
{
	const struct ipa_gsi_ep_config *gsi_ep_cfg;

	gsi_ep_cfg = ipa3_get_gsi_ep_info(client);
	if (!gsi_ep_cfg)
		return -EFAULT;

	return gsi_ep_cfg->ipa_gsi_chan_num;
}

int ipa_eth_uc_stats_init(struct ipa_eth_device *eth_dev)
{
	return 0;
}

int ipa_eth_uc_stats_deinit(struct ipa_eth_device *eth_dev)
{
	u32 protocol = find_uc_protocol(eth_dev);

	if (protocol == IPA_HW_PROTOCOL_INVALID)
		return -EFAULT;

	return ipa_uc_debug_stats_dealloc(protocol);
}

static void __fill_stats_info(
	struct ipa_eth_channel *ch,
	struct IpaOffloadStatschannel_info *ch_info,
	bool start)
{
	ch_info->dir = IPA_ETH_CH_IS_RX(ch) ?
			IPA_HW_DIR_CONSUMER : IPA_HW_DIR_PRODUCER;

	if (start) {
		int gsi_ch = find_client_channel(ch->ipa_client);

		if (gsi_ch < 0) {
			ipa_eth_dev_err(ch->eth_dev,
				"Failed to determine GSI channel for client %d",
				ch->ipa_client);
			gsi_ch = IPA_HW_CH_ID_INVALID;
		}

		ch_info->ch_id = (u8) gsi_ch;
	} else {
		ch_info->ch_id = IPA_HW_CH_ID_INVALID;
	}
}

static int ipa_eth_uc_stats_control(struct ipa_eth_device *eth_dev, bool start)
{
	int stats_idx = 0;
	struct ipa_eth_channel *ch;
	u32 protocol = find_uc_protocol(eth_dev);
	struct IpaHwOffloadStatsAllocCmdData_t stats_info;

	if (protocol == IPA_HW_PROTOCOL_INVALID) {
		ipa_eth_dev_err(eth_dev, "Failed find to uC protocol");
		return -EFAULT;
	}

	memset(&stats_info, 0, sizeof(stats_info));

	stats_info.protocol = protocol;

	list_for_each_entry(ch, &eth_dev->rx_channels, channel_list) {
		if (stats_idx == IPA_MAX_CH_STATS_SUPPORTED)
			break;

		__fill_stats_info(ch,
			&stats_info.ch_id_info[stats_idx++], start);
	}

	list_for_each_entry(ch, &eth_dev->tx_channels, channel_list) {
		if (stats_idx == IPA_MAX_CH_STATS_SUPPORTED)
			break;

		__fill_stats_info(ch,
			&stats_info.ch_id_info[stats_idx++], start);
	}

	return ipa_uc_debug_stats_alloc(stats_info);
}

int ipa_eth_uc_stats_start(struct ipa_eth_device *eth_dev)
{
	return ipa_eth_uc_stats_control(eth_dev, true);
}

int ipa_eth_uc_stats_stop(struct ipa_eth_device *eth_dev)
{
	return ipa_eth_uc_stats_control(eth_dev, false);
}
