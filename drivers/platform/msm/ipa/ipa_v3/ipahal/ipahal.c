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

#include <linux/debugfs.h>
#include "ipahal.h"
#include "ipahal_i.h"
#include "ipahal_reg_i.h"

struct ipahal_context *ipahal_ctx;

static const char *ipahal_imm_cmd_name_to_str[IPA_IMM_CMD_MAX] = {
	__stringify(IPA_IMM_CMD_IP_V4_FILTER_INIT),
	__stringify(IPA_IMM_CMD_IP_V6_FILTER_INIT),
	__stringify(IPA_IMM_CMD_IP_V4_NAT_INIT),
	__stringify(IPA_IMM_CMD_IP_V4_ROUTING_INIT),
	__stringify(IPA_IMM_CMD_IP_V6_ROUTING_INIT),
	__stringify(IPA_IMM_CMD_HDR_INIT_LOCAL),
	__stringify(IPA_IMM_CMD_HDR_INIT_SYSTEM),
	__stringify(IPA_IMM_CMD_REGISTER_WRITE),
	__stringify(IPA_IMM_CMD_NAT_DMA),
	__stringify(IPA_IMM_CMD_IP_PACKET_INIT),
	__stringify(IPA_IMM_CMD_DMA_SHARED_MEM),
	__stringify(IPA_IMM_CMD_IP_PACKET_TAG_STATUS),
	__stringify(IPA_IMM_CMD_DMA_TASK_32B_ADDR),
};

static const char *ipahal_pkt_status_exception_to_str
	[IPAHAL_PKT_STATUS_EXCEPTION_MAX] = {
	__stringify(IPAHAL_PKT_STATUS_EXCEPTION_NONE),
	__stringify(IPAHAL_PKT_STATUS_EXCEPTION_DEAGGR),
	__stringify(IPAHAL_PKT_STATUS_EXCEPTION_IPTYPE),
	__stringify(IPAHAL_PKT_STATUS_EXCEPTION_PACKET_LENGTH),
	__stringify(IPAHAL_PKT_STATUS_EXCEPTION_PACKET_THRESHOLD),
	__stringify(IPAHAL_PKT_STATUS_EXCEPTION_FRAG_RULE_MISS),
	__stringify(IPAHAL_PKT_STATUS_EXCEPTION_SW_FILT),
	__stringify(IPAHAL_PKT_STATUS_EXCEPTION_NAT),
};

#define IPAHAL_MEM_ALLOC(__size, __is_atomic_ctx) \
		(kzalloc((__size), ((__is_atomic_ctx)?GFP_ATOMIC:GFP_KERNEL)))


static struct ipahal_imm_cmd_pyld *ipa_imm_cmd_construct_dma_task_32b_addr(
	enum ipahal_imm_cmd_name cmd, const void *params, bool is_atomic_ctx)
{
	struct ipahal_imm_cmd_pyld *pyld;
	struct ipa_imm_cmd_hw_dma_task_32b_addr *data;
	struct ipahal_imm_cmd_dma_task_32b_addr *dma_params =
		(struct ipahal_imm_cmd_dma_task_32b_addr *)params;

	pyld = IPAHAL_MEM_ALLOC(sizeof(*pyld) + sizeof(*data), is_atomic_ctx);
	if (unlikely(!pyld)) {
		IPAHAL_ERR("kzalloc err\n");
		return pyld;
	}
	pyld->len = sizeof(*data);
	data = (struct ipa_imm_cmd_hw_dma_task_32b_addr *)pyld->data;

	if (unlikely(dma_params->size1 & ~0xFFFF)) {
		IPAHAL_ERR("Size1 is bigger than 16bit width 0x%x\n",
			dma_params->size1);
		WARN_ON(1);
	}
	if (unlikely(dma_params->packet_size & ~0xFFFF)) {
		IPAHAL_ERR("Pkt size is bigger than 16bit width 0x%x\n",
			dma_params->packet_size);
		WARN_ON(1);
	}
	data->cmplt = dma_params->cmplt ? 1 : 0;
	data->eof = dma_params->eof ? 1 : 0;
	data->flsh = dma_params->flsh ? 1 : 0;
	data->lock = dma_params->lock ? 1 : 0;
	data->unlock = dma_params->unlock ? 1 : 0;
	data->size1 = dma_params->size1;
	data->addr1 = dma_params->addr1;
	data->packet_size = dma_params->packet_size;

	return pyld;
}

static struct ipahal_imm_cmd_pyld *ipa_imm_cmd_construct_ip_packet_tag_status(
	enum ipahal_imm_cmd_name cmd, const void *params, bool is_atomic_ctx)
{
	struct ipahal_imm_cmd_pyld *pyld;
	struct ipa_imm_cmd_hw_ip_packet_tag_status *data;
	struct ipahal_imm_cmd_ip_packet_tag_status *tag_params =
		(struct ipahal_imm_cmd_ip_packet_tag_status *)params;

	pyld = IPAHAL_MEM_ALLOC(sizeof(*pyld) + sizeof(*data), is_atomic_ctx);
	if (unlikely(!pyld)) {
		IPAHAL_ERR("kzalloc err\n");
		return pyld;
	}
	pyld->len = sizeof(*data);
	data = (struct ipa_imm_cmd_hw_ip_packet_tag_status *)pyld->data;

	if (unlikely(tag_params->tag & ~0xFFFFFFFFFFFF)) {
		IPAHAL_ERR("tag is bigger than 48bit width 0x%llx\n",
			tag_params->tag);
		WARN_ON(1);
	}
	data->tag = tag_params->tag;

	return pyld;
}

static struct ipahal_imm_cmd_pyld *ipa_imm_cmd_construct_dma_shared_mem(
	enum ipahal_imm_cmd_name cmd, const void *params, bool is_atomic_ctx)
{
	struct ipahal_imm_cmd_pyld *pyld;
	struct ipa_imm_cmd_hw_dma_shared_mem *data;
	struct ipahal_imm_cmd_dma_shared_mem *mem_params =
		(struct ipahal_imm_cmd_dma_shared_mem *)params;

	pyld = IPAHAL_MEM_ALLOC(sizeof(*pyld) + sizeof(*data), is_atomic_ctx);
	if (unlikely(!pyld)) {
		IPAHAL_ERR("kzalloc err\n");
		return pyld;
	}
	pyld->len = sizeof(*data);
	data = (struct ipa_imm_cmd_hw_dma_shared_mem *)pyld->data;

	if (unlikely(mem_params->size & ~0xFFFF)) {
		IPAHAL_ERR("Size is bigger than 16bit width 0x%x\n",
			mem_params->size);
		WARN_ON(1);
	}
	if (unlikely(mem_params->local_addr & ~0xFFFF)) {
		IPAHAL_ERR("Local addr is bigger than 16bit width 0x%x\n",
			mem_params->local_addr);
		WARN_ON(1);
	}
	data->direction = mem_params->is_read ? 1 : 0;
	data->size = mem_params->size;
	data->local_addr = mem_params->local_addr;
	data->system_addr = mem_params->system_addr;
	data->skip_pipeline_clear = mem_params->skip_pipeline_clear ? 1 : 0;
	switch (mem_params->pipeline_clear_options) {
	case IPAHAL_HPS_CLEAR:
		data->pipeline_clear_options = 0;
		break;
	case IPAHAL_SRC_GRP_CLEAR:
		data->pipeline_clear_options = 1;
		break;
	case IPAHAL_FULL_PIPELINE_CLEAR:
		data->pipeline_clear_options = 2;
		break;
	default:
		IPAHAL_ERR("unsupported pipline clear option %d\n",
			mem_params->pipeline_clear_options);
		WARN_ON(1);
	};

	return pyld;
}

static struct ipahal_imm_cmd_pyld *ipa_imm_cmd_construct_register_write(
	enum ipahal_imm_cmd_name cmd, const void *params, bool is_atomic_ctx)
{
	struct ipahal_imm_cmd_pyld *pyld;
	struct ipa_imm_cmd_hw_register_write *data;
	struct ipahal_imm_cmd_register_write *regwrt_params =
		(struct ipahal_imm_cmd_register_write *)params;

	pyld = IPAHAL_MEM_ALLOC(sizeof(*pyld) + sizeof(*data), is_atomic_ctx);
	if (unlikely(!pyld)) {
		IPAHAL_ERR("kzalloc err\n");
		return pyld;
	}
	pyld->len = sizeof(*data);
	data = (struct ipa_imm_cmd_hw_register_write *)pyld->data;

	if (unlikely(regwrt_params->offset & ~0xFFFF)) {
		IPAHAL_ERR("Offset is bigger than 16bit width 0x%x\n",
			regwrt_params->offset);
		WARN_ON(1);
	}
	data->offset = regwrt_params->offset;
	data->value = regwrt_params->value;
	data->value_mask = regwrt_params->value_mask;

	data->skip_pipeline_clear = regwrt_params->skip_pipeline_clear ? 1 : 0;
	switch (regwrt_params->pipeline_clear_options) {
	case IPAHAL_HPS_CLEAR:
		data->pipeline_clear_options = 0;
		break;
	case IPAHAL_SRC_GRP_CLEAR:
		data->pipeline_clear_options = 1;
		break;
	case IPAHAL_FULL_PIPELINE_CLEAR:
		data->pipeline_clear_options = 2;
		break;
	default:
		IPAHAL_ERR("unsupported pipline clear option %d\n",
			regwrt_params->pipeline_clear_options);
		WARN_ON(1);
	};

	return pyld;
}

static struct ipahal_imm_cmd_pyld *ipa_imm_cmd_construct_ip_packet_init(
	enum ipahal_imm_cmd_name cmd, const void *params, bool is_atomic_ctx)
{
	struct ipahal_imm_cmd_pyld *pyld;
	struct ipa_imm_cmd_hw_ip_packet_init *data;
	struct ipahal_imm_cmd_ip_packet_init *pktinit_params =
		(struct ipahal_imm_cmd_ip_packet_init *)params;

	pyld = IPAHAL_MEM_ALLOC(sizeof(*pyld) + sizeof(*data), is_atomic_ctx);
	if (unlikely(!pyld)) {
		IPAHAL_ERR("kzalloc err\n");
		return pyld;
	}
	pyld->len = sizeof(*data);
	data = (struct ipa_imm_cmd_hw_ip_packet_init *)pyld->data;

	if (unlikely(pktinit_params->destination_pipe_index & ~0x1F)) {
		IPAHAL_ERR("Dst pipe idx is bigger than 5bit width 0x%x\n",
			pktinit_params->destination_pipe_index);
		WARN_ON(1);
	}
	data->destination_pipe_index = pktinit_params->destination_pipe_index;

	return pyld;
}

static struct ipahal_imm_cmd_pyld *ipa_imm_cmd_construct_nat_dma(
	enum ipahal_imm_cmd_name cmd, const void *params, bool is_atomic_ctx)
{
	struct ipahal_imm_cmd_pyld *pyld;
	struct ipa_imm_cmd_hw_nat_dma *data;
	struct ipahal_imm_cmd_nat_dma *nat_params =
		(struct ipahal_imm_cmd_nat_dma *)params;

	pyld = IPAHAL_MEM_ALLOC(sizeof(*pyld) + sizeof(*data), is_atomic_ctx);
	if (unlikely(!pyld)) {
		IPAHAL_ERR("kzalloc err\n");
		return pyld;
	}
	pyld->len = sizeof(*data);
	data = (struct ipa_imm_cmd_hw_nat_dma *)pyld->data;

	data->table_index = nat_params->table_index;
	data->base_addr = nat_params->base_addr;
	data->offset = nat_params->offset;
	data->data = nat_params->data;

	return pyld;
}

static struct ipahal_imm_cmd_pyld *ipa_imm_cmd_construct_hdr_init_system(
	enum ipahal_imm_cmd_name cmd, const void *params, bool is_atomic_ctx)
{
	struct ipahal_imm_cmd_pyld *pyld;
	struct ipa_imm_cmd_hw_hdr_init_system *data;
	struct ipahal_imm_cmd_hdr_init_system *syshdr_params =
		(struct ipahal_imm_cmd_hdr_init_system *)params;

	pyld = IPAHAL_MEM_ALLOC(sizeof(*pyld) + sizeof(*data), is_atomic_ctx);
	if (unlikely(!pyld)) {
		IPAHAL_ERR("kzalloc err\n");
		return pyld;
	}
	pyld->len = sizeof(*data);
	data = (struct ipa_imm_cmd_hw_hdr_init_system *)pyld->data;

	data->hdr_table_addr = syshdr_params->hdr_table_addr;

	return pyld;
}

static struct ipahal_imm_cmd_pyld *ipa_imm_cmd_construct_hdr_init_local(
	enum ipahal_imm_cmd_name cmd, const void *params, bool is_atomic_ctx)
{
	struct ipahal_imm_cmd_pyld *pyld;
	struct ipa_imm_cmd_hw_hdr_init_local *data;
	struct ipahal_imm_cmd_hdr_init_local *lclhdr_params =
		(struct ipahal_imm_cmd_hdr_init_local *)params;

	pyld = IPAHAL_MEM_ALLOC(sizeof(*pyld) + sizeof(*data), is_atomic_ctx);
	if (unlikely(!pyld)) {
		IPAHAL_ERR("kzalloc err\n");
		return pyld;
	}
	pyld->len = sizeof(*data);
	data = (struct ipa_imm_cmd_hw_hdr_init_local *)pyld->data;

	if (unlikely(lclhdr_params->size_hdr_table & ~0xFFF)) {
		IPAHAL_ERR("Hdr tble size is bigger than 12bit width 0x%x\n",
			lclhdr_params->size_hdr_table);
		WARN_ON(1);
	}
	data->hdr_table_addr = lclhdr_params->hdr_table_addr;
	data->size_hdr_table = lclhdr_params->size_hdr_table;
	data->hdr_addr = lclhdr_params->hdr_addr;

	return pyld;
}

static struct ipahal_imm_cmd_pyld *ipa_imm_cmd_construct_ip_v6_routing_init(
	enum ipahal_imm_cmd_name cmd, const void *params, bool is_atomic_ctx)
{
	struct ipahal_imm_cmd_pyld *pyld;
	struct ipa_imm_cmd_hw_ip_v6_routing_init *data;
	struct ipahal_imm_cmd_ip_v6_routing_init *rt6_params =
		(struct ipahal_imm_cmd_ip_v6_routing_init *)params;

	pyld = IPAHAL_MEM_ALLOC(sizeof(*pyld) + sizeof(*data), is_atomic_ctx);
	if (unlikely(!pyld)) {
		IPAHAL_ERR("kzalloc err\n");
		return pyld;
	}
	pyld->len = sizeof(*data);
	data = (struct ipa_imm_cmd_hw_ip_v6_routing_init *)pyld->data;

	data->hash_rules_addr = rt6_params->hash_rules_addr;
	data->hash_rules_size = rt6_params->hash_rules_size;
	data->hash_local_addr = rt6_params->hash_local_addr;
	data->nhash_rules_addr = rt6_params->nhash_rules_addr;
	data->nhash_rules_size = rt6_params->nhash_rules_size;
	data->nhash_local_addr = rt6_params->nhash_local_addr;

	return pyld;
}

static struct ipahal_imm_cmd_pyld *ipa_imm_cmd_construct_ip_v4_routing_init(
	enum ipahal_imm_cmd_name cmd, const void *params, bool is_atomic_ctx)
{
	struct ipahal_imm_cmd_pyld *pyld;
	struct ipa_imm_cmd_hw_ip_v4_routing_init *data;
	struct ipahal_imm_cmd_ip_v4_routing_init *rt4_params =
		(struct ipahal_imm_cmd_ip_v4_routing_init *)params;

	pyld = IPAHAL_MEM_ALLOC(sizeof(*pyld) + sizeof(*data), is_atomic_ctx);
	if (unlikely(!pyld)) {
		IPAHAL_ERR("kzalloc err\n");
		return pyld;
	}
	pyld->len = sizeof(*data);
	data = (struct ipa_imm_cmd_hw_ip_v4_routing_init *)pyld->data;

	data->hash_rules_addr = rt4_params->hash_rules_addr;
	data->hash_rules_size = rt4_params->hash_rules_size;
	data->hash_local_addr = rt4_params->hash_local_addr;
	data->nhash_rules_addr = rt4_params->nhash_rules_addr;
	data->nhash_rules_size = rt4_params->nhash_rules_size;
	data->nhash_local_addr = rt4_params->nhash_local_addr;

	return pyld;
}

static struct ipahal_imm_cmd_pyld *ipa_imm_cmd_construct_ip_v4_nat_init(
	enum ipahal_imm_cmd_name cmd, const void *params, bool is_atomic_ctx)
{
	struct ipahal_imm_cmd_pyld *pyld;
	struct ipa_imm_cmd_hw_ip_v4_nat_init *data;
	struct ipahal_imm_cmd_ip_v4_nat_init *nat4_params =
		(struct ipahal_imm_cmd_ip_v4_nat_init *)params;

	pyld = IPAHAL_MEM_ALLOC(sizeof(*pyld) + sizeof(*data), is_atomic_ctx);
	if (unlikely(!pyld)) {
		IPAHAL_ERR("kzalloc err\n");
		return pyld;
	}
	pyld->len = sizeof(*data);
	data = (struct ipa_imm_cmd_hw_ip_v4_nat_init *)pyld->data;

	data->ipv4_rules_addr = nat4_params->ipv4_rules_addr;
	data->ipv4_expansion_rules_addr =
		nat4_params->ipv4_expansion_rules_addr;
	data->index_table_addr = nat4_params->index_table_addr;
	data->index_table_expansion_addr =
		nat4_params->index_table_expansion_addr;
	data->table_index = nat4_params->table_index;
	data->ipv4_rules_addr_type =
		nat4_params->ipv4_rules_addr_shared ? 1 : 0;
	data->ipv4_expansion_rules_addr_type =
		nat4_params->ipv4_expansion_rules_addr_shared ? 1 : 0;
	data->index_table_addr_type =
		nat4_params->index_table_addr_shared ? 1 : 0;
	data->index_table_expansion_addr_type =
		nat4_params->index_table_expansion_addr_shared ? 1 : 0;
	data->size_base_tables = nat4_params->size_base_tables;
	data->size_expansion_tables = nat4_params->size_expansion_tables;
	data->public_ip_addr = nat4_params->public_ip_addr;

	return pyld;
}

static struct ipahal_imm_cmd_pyld *ipa_imm_cmd_construct_ip_v6_filter_init(
	enum ipahal_imm_cmd_name cmd, const void *params, bool is_atomic_ctx)
{
	struct ipahal_imm_cmd_pyld *pyld;
	struct ipa_imm_cmd_hw_ip_v6_filter_init *data;
	struct ipahal_imm_cmd_ip_v6_filter_init *flt6_params =
		(struct ipahal_imm_cmd_ip_v6_filter_init *)params;

	pyld = IPAHAL_MEM_ALLOC(sizeof(*pyld) + sizeof(*data), is_atomic_ctx);
	if (unlikely(!pyld)) {
		IPAHAL_ERR("kzalloc err\n");
		return pyld;
	}
	pyld->len = sizeof(*data);
	data = (struct ipa_imm_cmd_hw_ip_v6_filter_init *)pyld->data;

	data->hash_rules_addr = flt6_params->hash_rules_addr;
	data->hash_rules_size = flt6_params->hash_rules_size;
	data->hash_local_addr = flt6_params->hash_local_addr;
	data->nhash_rules_addr = flt6_params->nhash_rules_addr;
	data->nhash_rules_size = flt6_params->nhash_rules_size;
	data->nhash_local_addr = flt6_params->nhash_local_addr;

	return pyld;
}

static struct ipahal_imm_cmd_pyld *ipa_imm_cmd_construct_ip_v4_filter_init(
	enum ipahal_imm_cmd_name cmd, const void *params, bool is_atomic_ctx)
{
	struct ipahal_imm_cmd_pyld *pyld;
	struct ipa_imm_cmd_hw_ip_v4_filter_init *data;
	struct ipahal_imm_cmd_ip_v4_filter_init *flt4_params =
		(struct ipahal_imm_cmd_ip_v4_filter_init *)params;

	pyld = IPAHAL_MEM_ALLOC(sizeof(*pyld) + sizeof(*data), is_atomic_ctx);
	if (unlikely(!pyld)) {
		IPAHAL_ERR("kzalloc err\n");
		return pyld;
	}
	pyld->len = sizeof(*data);
	data = (struct ipa_imm_cmd_hw_ip_v4_filter_init *)pyld->data;

	data->hash_rules_addr = flt4_params->hash_rules_addr;
	data->hash_rules_size = flt4_params->hash_rules_size;
	data->hash_local_addr = flt4_params->hash_local_addr;
	data->nhash_rules_addr = flt4_params->nhash_rules_addr;
	data->nhash_rules_size = flt4_params->nhash_rules_size;
	data->nhash_local_addr = flt4_params->nhash_local_addr;

	return pyld;
}

/*
 * struct ipahal_imm_cmd_obj - immediate command H/W information for
 *  specific IPA version
 * @construct - CB to construct imm command payload from abstracted structure
 * @opcode - Immediate command OpCode
 * @dyn_op - Does this command supports Dynamic opcode?
 *  Some commands opcode are dynamic where the part of the opcode is
 *  supplied as param. This flag indicates if the specific command supports it
 *  or not.
 */
struct ipahal_imm_cmd_obj {
	struct ipahal_imm_cmd_pyld *(*construct)(enum ipahal_imm_cmd_name cmd,
		const void *params, bool is_atomic_ctx);
	u16 opcode;
	bool dyn_op;
};

/*
 * This table contains the info regard each immediate command for IPAv3
 *  and later.
 * Information like: opcode and construct functions.
 * All the information on the IMM on IPAv3 are statically defined below.
 * If information is missing regard some IMM on some IPA version,
 *  the init function will fill it with the information from the previous
 *  IPA version.
 * Information is considered missing if all of the fields are 0
 * If opcode is -1, this means that the IMM is removed on the
 *  specific version
 */
static struct ipahal_imm_cmd_obj
		ipahal_imm_cmd_objs[IPA_HW_MAX][IPA_IMM_CMD_MAX] = {
	/* IPAv3 */
	[IPA_HW_v3_0][IPA_IMM_CMD_IP_V4_FILTER_INIT] = {
		ipa_imm_cmd_construct_ip_v4_filter_init,
		3, false},
	[IPA_HW_v3_0][IPA_IMM_CMD_IP_V6_FILTER_INIT] = {
		ipa_imm_cmd_construct_ip_v6_filter_init,
		4, false},
	[IPA_HW_v3_0][IPA_IMM_CMD_IP_V4_NAT_INIT] = {
		ipa_imm_cmd_construct_ip_v4_nat_init,
		5, false},
	[IPA_HW_v3_0][IPA_IMM_CMD_IP_V4_ROUTING_INIT] = {
		ipa_imm_cmd_construct_ip_v4_routing_init,
		7, false},
	[IPA_HW_v3_0][IPA_IMM_CMD_IP_V6_ROUTING_INIT] = {
		ipa_imm_cmd_construct_ip_v6_routing_init,
		8, false},
	[IPA_HW_v3_0][IPA_IMM_CMD_HDR_INIT_LOCAL] = {
		ipa_imm_cmd_construct_hdr_init_local,
		9, false},
	[IPA_HW_v3_0][IPA_IMM_CMD_HDR_INIT_SYSTEM] = {
		ipa_imm_cmd_construct_hdr_init_system,
		10, false},
	[IPA_HW_v3_0][IPA_IMM_CMD_REGISTER_WRITE] = {
		ipa_imm_cmd_construct_register_write,
		12, false},
	[IPA_HW_v3_0][IPA_IMM_CMD_NAT_DMA] = {
		ipa_imm_cmd_construct_nat_dma,
		14, false},
	[IPA_HW_v3_0][IPA_IMM_CMD_IP_PACKET_INIT] = {
		ipa_imm_cmd_construct_ip_packet_init,
		16, false},
	[IPA_HW_v3_0][IPA_IMM_CMD_DMA_TASK_32B_ADDR] = {
		ipa_imm_cmd_construct_dma_task_32b_addr,
		17, true},
	[IPA_HW_v3_0][IPA_IMM_CMD_DMA_SHARED_MEM] = {
		ipa_imm_cmd_construct_dma_shared_mem,
		19, false},
	[IPA_HW_v3_0][IPA_IMM_CMD_IP_PACKET_TAG_STATUS] = {
		ipa_imm_cmd_construct_ip_packet_tag_status,
		20, false},
};

/*
 * ipahal_imm_cmd_init() - Build the Immediate command information table
 *  See ipahal_imm_cmd_objs[][] comments
 */
static int ipahal_imm_cmd_init(enum ipa_hw_type ipa_hw_type)
{
	int i;
	int j;
	struct ipahal_imm_cmd_obj zero_obj;

	IPAHAL_DBG_LOW("Entry - HW_TYPE=%d\n", ipa_hw_type);

	if ((ipa_hw_type < 0) || (ipa_hw_type >= IPA_HW_MAX)) {
		IPAHAL_ERR("invalid IPA HW type (%d)\n", ipa_hw_type);
		return -EINVAL;
	}

	memset(&zero_obj, 0, sizeof(zero_obj));
	for (i = IPA_HW_v3_0 ; i < ipa_hw_type ; i++) {
		for (j = 0; j < IPA_IMM_CMD_MAX ; j++) {
			if (!memcmp(&ipahal_imm_cmd_objs[i+1][j], &zero_obj,
				sizeof(struct ipahal_imm_cmd_obj))) {
				memcpy(&ipahal_imm_cmd_objs[i+1][j],
					&ipahal_imm_cmd_objs[i][j],
					sizeof(struct ipahal_imm_cmd_obj));
			} else {
				/*
				 * explicitly overridden immediate command.
				 * Check validity
				 */
				 if (!ipahal_imm_cmd_objs[i+1][j].opcode) {
					IPAHAL_ERR(
					  "imm_cmd=%s with zero opcode ipa_ver=%d\n",
					  ipahal_imm_cmd_name_str(j), i+1);
					WARN_ON(1);
				 }
				 if (!ipahal_imm_cmd_objs[i+1][j].construct) {
					IPAHAL_ERR(
					  "imm_cmd=%s with NULL construct func ipa_ver=%d\n",
					  ipahal_imm_cmd_name_str(j), i+1);
					WARN_ON(1);
				 }
			}
		}
	}

	return 0;
}

/*
 * ipahal_imm_cmd_name_str() - returns string that represent the imm cmd
 * @cmd_name: [in] Immediate command name
 */
const char *ipahal_imm_cmd_name_str(enum ipahal_imm_cmd_name cmd_name)
{
	if (cmd_name < 0 || cmd_name >= IPA_IMM_CMD_MAX) {
		IPAHAL_ERR("requested name of invalid imm_cmd=%d\n", cmd_name);
		return "Invalid IMM_CMD";
	}

	return ipahal_imm_cmd_name_to_str[cmd_name];
}

/*
 * ipahal_imm_cmd_get_opcode() - Get the fixed opcode of the immediate command
 */
u16 ipahal_imm_cmd_get_opcode(enum ipahal_imm_cmd_name cmd)
{
	u32 opcode;

	if (cmd >= IPA_IMM_CMD_MAX) {
		IPAHAL_ERR("Invalid immediate command imm_cmd=%u\n", cmd);
		ipa_assert();
		return -EFAULT;
	}

	IPAHAL_DBG_LOW("Get opcode of IMM_CMD=%s\n",
		ipahal_imm_cmd_name_str(cmd));
	opcode = ipahal_imm_cmd_objs[ipahal_ctx->hw_type][cmd].opcode;
	if (opcode == -1) {
		IPAHAL_ERR("Try to get opcode of obsolete IMM_CMD=%s\n",
			ipahal_imm_cmd_name_str(cmd));
		ipa_assert();
		return -EFAULT;
	}

	return opcode;
}

/*
 * ipahal_imm_cmd_get_opcode_param() - Get the opcode of an immediate command
 *  that supports dynamic opcode
 * Some commands opcode are not totaly fixed, but part of it is
 *  a supplied parameter. E.g. Low-Byte is fixed and Hi-Byte
 *  is a given parameter.
 * This API will return the composed opcode of the command given
 *  the parameter
 * Note: Use this API only for immediate comamnds that support Dynamic Opcode
 */
u16 ipahal_imm_cmd_get_opcode_param(enum ipahal_imm_cmd_name cmd, int param)
{
	u32 opcode;

	if (cmd >= IPA_IMM_CMD_MAX) {
		IPAHAL_ERR("Invalid immediate command IMM_CMD=%u\n", cmd);
		ipa_assert();
		return -EFAULT;
	}

	IPAHAL_DBG_LOW("Get opcode of IMM_CMD=%s\n",
		ipahal_imm_cmd_name_str(cmd));

	if (!ipahal_imm_cmd_objs[ipahal_ctx->hw_type][cmd].dyn_op) {
		IPAHAL_ERR("IMM_CMD=%s does not support dynamic opcode\n",
			ipahal_imm_cmd_name_str(cmd));
		ipa_assert();
		return -EFAULT;
	}

	/* Currently, dynamic opcode commands uses params to be set
	 *  on the Opcode hi-byte (lo-byte is fixed).
	 * If this to be changed in the future, make the opcode calculation
	 *  a CB per command
	 */
	if (param & ~0xFFFF) {
		IPAHAL_ERR("IMM_CMD=%s opcode param is invalid\n",
			ipahal_imm_cmd_name_str(cmd));
		ipa_assert();
		return -EFAULT;
	}
	opcode = ipahal_imm_cmd_objs[ipahal_ctx->hw_type][cmd].opcode;
	if (opcode == -1) {
		IPAHAL_ERR("Try to get opcode of obsolete IMM_CMD=%s\n",
			ipahal_imm_cmd_name_str(cmd));
		ipa_assert();
		return -EFAULT;
	}
	if (opcode & ~0xFFFF) {
		IPAHAL_ERR("IMM_CMD=%s opcode will be overridden\n",
			ipahal_imm_cmd_name_str(cmd));
		ipa_assert();
		return -EFAULT;
	}
	return (opcode + (param<<8));
}

/*
 * ipahal_construct_imm_cmd() - Construct immdiate command
 * This function builds imm cmd bulk that can be be sent to IPA
 * The command will be allocated dynamically.
 * After done using it, call ipahal_destroy_imm_cmd() to release it
 */
struct ipahal_imm_cmd_pyld *ipahal_construct_imm_cmd(
	enum ipahal_imm_cmd_name cmd, const void *params, bool is_atomic_ctx)
{
	if (!params) {
		IPAHAL_ERR("Input error: params=%p\n", params);
		ipa_assert();
		return NULL;
	}

	if (cmd >= IPA_IMM_CMD_MAX) {
		IPAHAL_ERR("Invalid immediate command %u\n", cmd);
		ipa_assert();
		return NULL;
	}

	IPAHAL_DBG_LOW("construct IMM_CMD:%s\n", ipahal_imm_cmd_name_str(cmd));
	return ipahal_imm_cmd_objs[ipahal_ctx->hw_type][cmd].construct(
		cmd, params, is_atomic_ctx);
}

/*
 * ipahal_construct_nop_imm_cmd() - Construct immediate comamnd for NO-Op
 * Core driver may want functionality to inject NOP commands to IPA
 *  to ensure e.g., PIPLINE clear before someother operation.
 * The functionality given by this function can be reached by
 *  ipahal_construct_imm_cmd(). This function is helper to the core driver
 *  to reach this NOP functionlity easily.
 * @skip_pipline_clear: if to skip pipeline clear waiting (don't wait)
 * @pipline_clr_opt: options for pipeline clear waiting
 * @is_atomic_ctx: is called in atomic context or can sleep?
 */
struct ipahal_imm_cmd_pyld *ipahal_construct_nop_imm_cmd(
	bool skip_pipline_clear,
	enum ipahal_pipeline_clear_option pipline_clr_opt,
	bool is_atomic_ctx)
{
	struct ipahal_imm_cmd_register_write cmd;
	struct ipahal_imm_cmd_pyld *cmd_pyld;

	memset(&cmd, 0, sizeof(cmd));
	cmd.skip_pipeline_clear = skip_pipline_clear;
	cmd.pipeline_clear_options = pipline_clr_opt;
	cmd.value_mask = 0x0;

	cmd_pyld = ipahal_construct_imm_cmd(IPA_IMM_CMD_REGISTER_WRITE,
		&cmd, is_atomic_ctx);

	if (!cmd_pyld)
		IPAHAL_ERR("failed to construct register_write imm cmd\n");

	return cmd_pyld;
}


/* IPA Packet Status Logic */

#define IPA_PKT_STATUS_SET_MSK(__hw_bit_msk, __shft) \
	(status->status_mask |= \
		((hw_status->status_mask & (__hw_bit_msk) ? 1 : 0) << (__shft)))

static void ipa_pkt_status_parse(
	const void *unparsed_status, struct ipahal_pkt_status *status)
{
	enum ipahal_pkt_status_opcode opcode = 0;
	enum ipahal_pkt_status_exception exception_type = 0;

	struct ipa_pkt_status_hw *hw_status =
		(struct ipa_pkt_status_hw *)unparsed_status;

	status->pkt_len = hw_status->pkt_len;
	status->endp_src_idx = hw_status->endp_src_idx;
	status->endp_dest_idx = hw_status->endp_dest_idx;
	status->metadata = hw_status->metadata;
	status->flt_local = hw_status->flt_local;
	status->flt_hash = hw_status->flt_hash;
	status->flt_global = hw_status->flt_hash;
	status->flt_ret_hdr = hw_status->flt_ret_hdr;
	status->flt_miss = ~(hw_status->flt_rule_id) ? false : true;
	status->flt_rule_id = hw_status->flt_rule_id;
	status->rt_local = hw_status->rt_local;
	status->rt_hash = hw_status->rt_hash;
	status->ucp = hw_status->ucp;
	status->rt_tbl_idx = hw_status->rt_tbl_idx;
	status->rt_miss = ~(hw_status->rt_rule_id) ? false : true;
	status->rt_rule_id = hw_status->rt_rule_id;
	status->nat_hit = hw_status->nat_hit;
	status->nat_entry_idx = hw_status->nat_entry_idx;
	status->tag_info = hw_status->tag_info;
	status->seq_num = hw_status->seq_num;
	status->time_of_day_ctr = hw_status->time_of_day_ctr;
	status->hdr_local = hw_status->hdr_local;
	status->hdr_offset = hw_status->hdr_offset;
	status->frag_hit = hw_status->frag_hit;
	status->frag_rule = hw_status->frag_rule;

	switch (hw_status->status_opcode) {
	case 0x1:
		opcode = IPAHAL_PKT_STATUS_OPCODE_PACKET;
		break;
	case 0x2:
		opcode = IPAHAL_PKT_STATUS_OPCODE_NEW_FRAG_RULE;
		break;
	case 0x4:
		opcode = IPAHAL_PKT_STATUS_OPCODE_DROPPED_PACKET;
		break;
	case 0x8:
		opcode = IPAHAL_PKT_STATUS_OPCODE_SUSPENDED_PACKET;
		break;
	case 0x10:
		opcode = IPAHAL_PKT_STATUS_OPCODE_LOG;
		break;
	case 0x20:
		opcode = IPAHAL_PKT_STATUS_OPCODE_DCMP;
		break;
	case 0x40:
		opcode = IPAHAL_PKT_STATUS_OPCODE_PACKET_2ND_PASS;
		break;
	default:
		IPAHAL_ERR("unsupported Status Opcode 0x%x\n",
			hw_status->status_opcode);
		WARN_ON(1);
	};
	status->status_opcode = opcode;

	switch (hw_status->nat_type) {
	case 0:
		status->nat_type = IPAHAL_PKT_STATUS_NAT_NONE;
		break;
	case 1:
		status->nat_type = IPAHAL_PKT_STATUS_NAT_SRC;
		break;
	case 2:
		status->nat_type = IPAHAL_PKT_STATUS_NAT_DST;
		break;
	default:
		IPAHAL_ERR("unsupported Status NAT type 0x%x\n",
			hw_status->nat_type);
		WARN_ON(1);
	};

	switch (hw_status->exception) {
	case 0:
		exception_type = IPAHAL_PKT_STATUS_EXCEPTION_NONE;
		break;
	case 1:
		exception_type = IPAHAL_PKT_STATUS_EXCEPTION_DEAGGR;
		break;
	case 4:
		exception_type = IPAHAL_PKT_STATUS_EXCEPTION_IPTYPE;
		break;
	case 8:
		exception_type = IPAHAL_PKT_STATUS_EXCEPTION_PACKET_LENGTH;
		break;
	case 16:
		exception_type = IPAHAL_PKT_STATUS_EXCEPTION_FRAG_RULE_MISS;
		break;
	case 32:
		exception_type = IPAHAL_PKT_STATUS_EXCEPTION_SW_FILT;
		break;
	case 64:
		exception_type = IPAHAL_PKT_STATUS_EXCEPTION_NAT;
		break;
	default:
		IPAHAL_ERR("unsupported Status Exception type 0x%x\n",
			hw_status->exception);
		WARN_ON(1);
	};
	status->exception = exception_type;

	IPA_PKT_STATUS_SET_MSK(0x1, IPAHAL_PKT_STATUS_MASK_FRAG_PROCESS_SHFT);
	IPA_PKT_STATUS_SET_MSK(0x2, IPAHAL_PKT_STATUS_MASK_FILT_PROCESS_SHFT);
	IPA_PKT_STATUS_SET_MSK(0x4, IPAHAL_PKT_STATUS_MASK_NAT_PROCESS_SHFT);
	IPA_PKT_STATUS_SET_MSK(0x8, IPAHAL_PKT_STATUS_MASK_ROUTE_PROCESS_SHFT);
	IPA_PKT_STATUS_SET_MSK(0x10, IPAHAL_PKT_STATUS_MASK_TAG_VALID_SHFT);
	IPA_PKT_STATUS_SET_MSK(0x20, IPAHAL_PKT_STATUS_MASK_FRAGMENT_SHFT);
	IPA_PKT_STATUS_SET_MSK(0x40,
		IPAHAL_PKT_STATUS_MASK_FIRST_FRAGMENT_SHFT);
	IPA_PKT_STATUS_SET_MSK(0x80, IPAHAL_PKT_STATUS_MASK_V4_SHFT);
	IPA_PKT_STATUS_SET_MSK(0x100,
		IPAHAL_PKT_STATUS_MASK_CKSUM_PROCESS_SHFT);
	IPA_PKT_STATUS_SET_MSK(0x200, IPAHAL_PKT_STATUS_MASK_AGGR_PROCESS_SHFT);
	IPA_PKT_STATUS_SET_MSK(0x400, IPAHAL_PKT_STATUS_MASK_DEST_EOT_SHFT);
	IPA_PKT_STATUS_SET_MSK(0x800,
		IPAHAL_PKT_STATUS_MASK_DEAGGR_PROCESS_SHFT);
	IPA_PKT_STATUS_SET_MSK(0x1000, IPAHAL_PKT_STATUS_MASK_DEAGG_FIRST_SHFT);
	IPA_PKT_STATUS_SET_MSK(0x2000, IPAHAL_PKT_STATUS_MASK_SRC_EOT_SHFT);
	IPA_PKT_STATUS_SET_MSK(0x4000, IPAHAL_PKT_STATUS_MASK_PREV_EOT_SHFT);
	IPA_PKT_STATUS_SET_MSK(0x8000, IPAHAL_PKT_STATUS_MASK_BYTE_LIMIT_SHFT);
	status->status_mask &= 0xFFFF;
}

/*
 * struct ipahal_pkt_status_obj - Pakcet Status H/W information for
 *  specific IPA version
 * @size: H/W size of the status packet
 * @parse: CB that parses the H/W packet status into the abstracted structure
 */
struct ipahal_pkt_status_obj {
	u32 size;
	void (*parse)(const void *unparsed_status,
		struct ipahal_pkt_status *status);
};

/*
 * This table contains the info regard packet status for IPAv3 and later
 * Information like: size of packet status and parsing function
 * All the information on the pkt Status on IPAv3 are statically defined below.
 * If information is missing regard some IPA version, the init function
 *  will fill it with the information from the previous IPA version.
 * Information is considered missing if all of the fields are 0
 */
static struct ipahal_pkt_status_obj ipahal_pkt_status_objs[IPA_HW_MAX] = {
	/* IPAv3 */
	[IPA_HW_v3_0] = {
		IPA3_0_PKT_STATUS_SIZE,
		ipa_pkt_status_parse,
		},
};

/*
 * ipahal_pkt_status_init() - Build the packet status information array
 *  for the different IPA versions
 *  See ipahal_pkt_status_objs[] comments
 */
static int ipahal_pkt_status_init(enum ipa_hw_type ipa_hw_type)
{
	int i;
	struct ipahal_pkt_status_obj zero_obj;

	IPAHAL_DBG_LOW("Entry - HW_TYPE=%d\n", ipa_hw_type);

	if ((ipa_hw_type < 0) || (ipa_hw_type >= IPA_HW_MAX)) {
		IPAHAL_ERR("invalid IPA HW type (%d)\n", ipa_hw_type);
		return -EINVAL;
	}

	/*
	 * Since structure alignment is implementation dependent,
	 * add test to avoid different and incompatible data layouts.
	 *
	 * In case new H/W has different size or structure of status packet,
	 * add a compile time validty check for it like below (as well as
	 * the new defines and/or the new strucutre in the internal header).
	 */
	BUILD_BUG_ON(sizeof(struct ipa_pkt_status_hw) !=
		IPA3_0_PKT_STATUS_SIZE);

	memset(&zero_obj, 0, sizeof(zero_obj));
	for (i = IPA_HW_v3_0 ; i < ipa_hw_type ; i++) {
		if (!memcmp(&ipahal_pkt_status_objs[i+1], &zero_obj,
			sizeof(struct ipahal_pkt_status_obj))) {
			memcpy(&ipahal_pkt_status_objs[i+1],
				&ipahal_pkt_status_objs[i],
				sizeof(struct ipahal_pkt_status_obj));
		} else {
			/*
			 * explicitly overridden Packet Status info
			 * Check validity
			 */
			 if (!ipahal_pkt_status_objs[i+1].size) {
				IPAHAL_ERR(
				  "Packet Status with zero size ipa_ver=%d\n",
				  i+1);
				WARN_ON(1);
			 }
			  if (!ipahal_pkt_status_objs[i+1].parse) {
				IPAHAL_ERR(
				  "Packet Status without Parse func ipa_ver=%d\n",
				  i+1);
				WARN_ON(1);
			 }
		}
	}

	return 0;
}

/*
 * ipahal_pkt_status_get_size() - Get H/W size of packet status
 */
u32 ipahal_pkt_status_get_size(void)
{
	return ipahal_pkt_status_objs[ipahal_ctx->hw_type].size;
}

/*
 * ipahal_pkt_status_parse() - Parse Packet Status payload to abstracted form
 * @unparsed_status: Pointer to H/W format of the packet status as read from H/W
 * @status: Pointer to pre-allocated buffer where the parsed info will be stored
 */
void ipahal_pkt_status_parse(const void *unparsed_status,
	struct ipahal_pkt_status *status)
{
	if (!unparsed_status || !status) {
		IPAHAL_ERR("Input Error: unparsed_status=%p status=%p\n",
			unparsed_status, status);
		return;
	}

	IPAHAL_DBG_LOW("Parse Status Packet\n");
	memset(status, 0, sizeof(*status));
	ipahal_pkt_status_objs[ipahal_ctx->hw_type].parse(unparsed_status,
		status);
}

/*
 * ipahal_pkt_status_exception_str() - returns string represents exception type
 * @exception: [in] The exception type
 */
const char *ipahal_pkt_status_exception_str(
	enum ipahal_pkt_status_exception exception)
{
	if (exception < 0 || exception >= IPAHAL_PKT_STATUS_EXCEPTION_MAX) {
		IPAHAL_ERR(
			"requested string of invalid pkt_status exception=%d\n",
			exception);
		return "Invalid PKT_STATUS_EXCEPTION";
	}

	return ipahal_pkt_status_exception_to_str[exception];
}

#ifdef CONFIG_DEBUG_FS
static void ipahal_debugfs_init(void)
{
	ipahal_ctx->dent = debugfs_create_dir("ipahal", 0);
	if (!ipahal_ctx->dent || IS_ERR(ipahal_ctx->dent)) {
		IPAHAL_ERR("fail to create ipahal debugfs folder\n");
		goto fail;
	}

	return;
fail:
	debugfs_remove_recursive(ipahal_ctx->dent);
	ipahal_ctx->dent = NULL;
}

static void ipahal_debugfs_remove(void)
{
	if (!ipahal_ctx)
		return;

	if (IS_ERR(ipahal_ctx->dent)) {
		IPAHAL_ERR("ipahal debugfs folder was not created\n");
		return;
	}

	debugfs_remove_recursive(ipahal_ctx->dent);
}
#else /* CONFIG_DEBUG_FS */
static void ipahal_debugfs_init(void) {}
static void ipahal_debugfs_remove(void) {}
#endif /* CONFIG_DEBUG_FS */

/*
 * ipahal_cp_hdr_to_hw_buff_v3() - copy header to hardware buffer according to
 * base address and offset given.
 * @base: dma base address
 * @offset: offset from base address where the data will be copied
 * @hdr: the header to be copied
 * @hdr_len: the length of the header
 */
static void ipahal_cp_hdr_to_hw_buff_v3(void *const base, u32 offset,
		u8 *const hdr, u32 hdr_len)
{
	memcpy(base + offset, hdr, hdr_len);
}

/*
 * ipahal_cp_proc_ctx_to_hw_buff_v3() - copy processing context to
 * base address and offset given.
 * @type: header processing context type (no processing context,
 *	IPA_HDR_PROC_ETHII_TO_ETHII etc.)
 * @base: dma base address
 * @offset: offset from base address where the data will be copied
 * @hdr_len: the length of the header
 * @is_hdr_proc_ctx: header is located in phys_base (true) or hdr_base_addr
 * @phys_base: memory location in DDR
 * @hdr_base_addr: base address in table
 * @offset_entry: offset from hdr_base_addr in table
 */
static void ipahal_cp_proc_ctx_to_hw_buff_v3(enum ipa_hdr_proc_type type,
		void *const base, u32 offset,
		u32 hdr_len, bool is_hdr_proc_ctx,
		dma_addr_t phys_base, u32 hdr_base_addr,
		struct ipa_hdr_offset_entry *offset_entry){
	if (type == IPA_HDR_PROC_NONE) {
		struct ipa_hw_hdr_proc_ctx_add_hdr_seq *ctx;

		ctx = (struct ipa_hw_hdr_proc_ctx_add_hdr_seq *)
			(base + offset);
		ctx->hdr_add.tlv.type = IPA_PROC_CTX_TLV_TYPE_HDR_ADD;
		ctx->hdr_add.tlv.length = 1;
		ctx->hdr_add.tlv.value = hdr_len;
		ctx->hdr_add.hdr_addr = is_hdr_proc_ctx ? phys_base :
			hdr_base_addr + offset_entry->offset;
		IPAHAL_DBG("header address 0x%x\n",
			ctx->hdr_add.hdr_addr);
		ctx->end.type = IPA_PROC_CTX_TLV_TYPE_END;
		ctx->end.length = 0;
		ctx->end.value = 0;
	} else {
		struct ipa_hw_hdr_proc_ctx_add_hdr_cmd_seq *ctx;

		ctx = (struct ipa_hw_hdr_proc_ctx_add_hdr_cmd_seq *)
			(base + offset);
		ctx->hdr_add.tlv.type = IPA_PROC_CTX_TLV_TYPE_HDR_ADD;
		ctx->hdr_add.tlv.length = 1;
		ctx->hdr_add.tlv.value = hdr_len;
		ctx->hdr_add.hdr_addr = is_hdr_proc_ctx ? phys_base :
			hdr_base_addr + offset_entry->offset;
		IPAHAL_DBG("header address 0x%x\n",
			ctx->hdr_add.hdr_addr);
		ctx->cmd.type = IPA_PROC_CTX_TLV_TYPE_PROC_CMD;
		ctx->cmd.length = 0;
		switch (type) {
		case IPA_HDR_PROC_ETHII_TO_ETHII:
			ctx->cmd.value = IPA_HDR_UCP_ETHII_TO_ETHII;
		break;
		case IPA_HDR_PROC_ETHII_TO_802_3:
			ctx->cmd.value = IPA_HDR_UCP_ETHII_TO_802_3;
		break;
		case IPA_HDR_PROC_802_3_TO_ETHII:
			ctx->cmd.value = IPA_HDR_UCP_802_3_TO_ETHII;
		break;
		case IPA_HDR_PROC_802_3_TO_802_3:
			ctx->cmd.value = IPA_HDR_UCP_802_3_TO_802_3;
		break;
		default:
			IPAHAL_ERR("unknown ipa_hdr_proc_type %d", type);
			BUG();
		}
		IPAHAL_DBG("command id %d\n", ctx->cmd.value);
		ctx->end.type = IPA_PROC_CTX_TLV_TYPE_END;
		ctx->end.length = 0;
		ctx->end.value = 0;
	}
}

/*
 * ipahal_get_proc_ctx_needed_len_v3() - calculates the needed length for
 * addition of header processing context according to the type of processing
 * context.
 * @type: header processing context type (no processing context,
 *	IPA_HDR_PROC_ETHII_TO_ETHII etc.)
 */
static int ipahal_get_proc_ctx_needed_len_v3(enum ipa_hdr_proc_type type)
{
	return (type == IPA_HDR_PROC_NONE) ?
			sizeof(struct ipa_hw_hdr_proc_ctx_add_hdr_seq) :
			sizeof(struct ipa_hw_hdr_proc_ctx_add_hdr_cmd_seq);
}

/*
 * struct ipahal_hdr_funcs - headers handling functions for specific IPA
 * version
 * @ipahal_cp_hdr_to_hw_buff - copy function for regular headers
 */
struct ipahal_hdr_funcs {
	void (*ipahal_cp_hdr_to_hw_buff)(void *const base, u32 offset,
			u8 *const hdr, u32 hdr_len);

	void (*ipahal_cp_proc_ctx_to_hw_buff)(enum ipa_hdr_proc_type type,
			void *const base, u32 offset, u32 hdr_len,
			bool is_hdr_proc_ctx, dma_addr_t phys_base,
			u32 hdr_base_addr,
			struct ipa_hdr_offset_entry *offset_entry);

	int (*ipahal_get_proc_ctx_needed_len)(enum ipa_hdr_proc_type type);
};

static struct ipahal_hdr_funcs hdr_funcs;

static void ipahal_hdr_init(enum ipa_hw_type ipa_hw_type)
{

	IPAHAL_DBG("Entry - HW_TYPE=%d\n", ipa_hw_type);

	/*
	 * once there are changes in HW and need to use different case, insert
	 * new case for the new h/w. put the default always for the latest HW
	 * and make sure all previous supported versions have their cases.
	 */
	switch (ipa_hw_type) {
	case IPA_HW_v3_0:
	default:
		hdr_funcs.ipahal_cp_hdr_to_hw_buff =
				ipahal_cp_hdr_to_hw_buff_v3;
		hdr_funcs.ipahal_cp_proc_ctx_to_hw_buff =
				ipahal_cp_proc_ctx_to_hw_buff_v3;
		hdr_funcs.ipahal_get_proc_ctx_needed_len =
				ipahal_get_proc_ctx_needed_len_v3;
	}
	IPAHAL_DBG("Exit\n");
}

/*
 * ipahal_cp_hdr_to_hw_buff() - copy header to hardware buffer according to
 * base address and offset given.
 * @base: dma base address
 * @offset: offset from base address where the data will be copied
 * @hdr: the header to be copied
 * @hdr_len: the length of the header
 */
void ipahal_cp_hdr_to_hw_buff(void *base, u32 offset, u8 *const hdr,
		u32 hdr_len)
{
	IPAHAL_DBG_LOW("Entry\n");
	IPAHAL_DBG("base %p, offset %d, hdr %p, hdr_len %d\n", base,
			offset, hdr, hdr_len);
	BUG_ON(!base || !hdr_len || !hdr);

	hdr_funcs.ipahal_cp_hdr_to_hw_buff(base, offset, hdr, hdr_len);

	IPAHAL_DBG_LOW("Exit\n");
}

/*
 * ipahal_cp_proc_ctx_to_hw_buff() - copy processing context to
 * base address and offset given.
 * @type: type of header processing context
 * @base: dma base address
 * @offset: offset from base address where the data will be copied
 * @hdr_len: the length of the header
 * @is_hdr_proc_ctx: header is located in phys_base (true) or hdr_base_addr
 * @phys_base: memory location in DDR
 * @hdr_base_addr: base address in table
 * @offset_entry: offset from hdr_base_addr in table
 */
void ipahal_cp_proc_ctx_to_hw_buff(enum ipa_hdr_proc_type type,
		void *const base, u32 offset, u32 hdr_len,
		bool is_hdr_proc_ctx, dma_addr_t phys_base,
		u32 hdr_base_addr, struct ipa_hdr_offset_entry *offset_entry)
{
	IPAHAL_DBG_LOW("entry\n");
	IPAHAL_DBG(
		"type %d, base %p, offset %d, hdr_len %d, is_hdr_proc_ctx %d, hdr_base_addr %d, offset_entry %p\n"
			, type, base, offset, hdr_len, is_hdr_proc_ctx,
			hdr_base_addr, offset_entry);

	if (!base ||
		!hdr_len ||
		(!phys_base && !hdr_base_addr) ||
		!hdr_base_addr ||
		((is_hdr_proc_ctx == false) && !offset_entry)) {
		IPAHAL_ERR(
			"invalid input: hdr_len:%u phys_base:%pad hdr_base_addr:%u is_hdr_proc_ctx:%d offset_entry:%pK\n"
			, hdr_len, &phys_base, hdr_base_addr
			, is_hdr_proc_ctx, offset_entry);
		BUG();
	}

	hdr_funcs.ipahal_cp_proc_ctx_to_hw_buff(type, base, offset,
			hdr_len, is_hdr_proc_ctx, phys_base,
			hdr_base_addr, offset_entry);

	IPAHAL_DBG_LOW("Exit\n");
}

/*
 * ipahal_get_proc_ctx_needed_len() - calculates the needed length for
 * addition of header processing context according to the type of processing
 * context
 * @type: header processing context type (no processing context,
 *	IPA_HDR_PROC_ETHII_TO_ETHII etc.)
 */
int ipahal_get_proc_ctx_needed_len(enum ipa_hdr_proc_type type)
{
	int res;

	IPAHAL_DBG("entry\n");

	res = hdr_funcs.ipahal_get_proc_ctx_needed_len(type);

	IPAHAL_DBG("Exit\n");

	return res;
}

int ipahal_init(enum ipa_hw_type ipa_hw_type, void __iomem *base)
{
	int result;

	IPAHAL_DBG("Entry - IPA HW TYPE=%d base=%p\n",
		ipa_hw_type, base);

	ipahal_ctx = kzalloc(sizeof(*ipahal_ctx), GFP_KERNEL);
	if (!ipahal_ctx) {
		IPAHAL_ERR("kzalloc err for ipahal_ctx\n");
		result = -ENOMEM;
		goto bail_err_exit;
	}

	if (ipa_hw_type < IPA_HW_v3_0) {
		IPAHAL_ERR("ipahal supported on IPAv3 and later only\n");
		result = -EINVAL;
		goto bail_free_ctx;
	}

	if (ipa_hw_type >= IPA_HW_MAX) {
		IPAHAL_ERR("invalid IPA HW type (%d)\n", ipa_hw_type);
		result = -EINVAL;
		goto bail_free_ctx;
	}

	if (!base) {
		IPAHAL_ERR("invalid memory io mapping addr\n");
		result = -EINVAL;
		goto bail_free_ctx;
	}

	ipahal_ctx->hw_type = ipa_hw_type;
	ipahal_ctx->base = base;

	if (ipahal_reg_init(ipa_hw_type)) {
		IPAHAL_ERR("failed to init ipahal reg\n");
		result = -EFAULT;
		goto bail_free_ctx;
	}

	if (ipahal_imm_cmd_init(ipa_hw_type)) {
		IPAHAL_ERR("failed to init ipahal imm cmd\n");
		result = -EFAULT;
		goto bail_free_ctx;
	}

	if (ipahal_pkt_status_init(ipa_hw_type)) {
		IPAHAL_ERR("failed to init ipahal pkt status\n");
		result = -EFAULT;
		goto bail_free_ctx;
	}

	ipahal_hdr_init(ipa_hw_type);

	ipahal_debugfs_init();

	return 0;

bail_free_ctx:
	kfree(ipahal_ctx);
	ipahal_ctx = NULL;
bail_err_exit:
	return result;
}

void ipahal_destroy(void)
{
	IPAHAL_DBG("Entry\n");
	ipahal_debugfs_remove();
	kfree(ipahal_ctx);
	ipahal_ctx = NULL;
}
