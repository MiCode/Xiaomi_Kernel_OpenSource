// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/soc/mediatek/mtk-cmdq.h>

#include "cmdq-util.h"
#include "cmdq-sec.h"
#include "cmdq-sec-iwc-common.h"

#define ADDR_METADATA_MAX_COUNT_ORIGIN	(8)

// s32 cmdq_rec_realloc_addr_metadata_buffer
static s32 cmdq_sec_realloc_addr_list(struct cmdq_pkt *pkt, const u32 count)
{
	struct cmdqSecDataStruct *sec_data =
		(struct cmdqSecDataStruct *)pkt->sec_data;
	void *prev = (void *)(unsigned long)sec_data->addrMetadatas, *curr;

	if (count <= sec_data->addrMetadataMaxCount)
		return 0;

	curr = kcalloc(count, sizeof(*sec_data), GFP_KERNEL);
	if (!curr)
		return -ENOMEM;
	if (count && sec_data->addrMetadatas)
		memcpy(curr, prev,
			sizeof(*sec_data) * sec_data->addrMetadataMaxCount);
	kfree(prev);

	sec_data->addrMetadatas = (u64)curr;
	sec_data->addrMetadataMaxCount = count;
	return 0;
}

// static void cmdq_task_reset_thread(struct cmdqRecStruct *handle)
// s32 cmdq_task_duplicate

// s32 cmdq_append_addr_metadata
static s32 cmdq_sec_append_metadata(
	struct cmdq_pkt *pkt, const enum CMDQ_SEC_ADDR_METADATA_TYPE type,
	const u64 base, const u32 offset, const u32 size, const u32 port)
{
	struct cmdqSecDataStruct *sec_data;
	struct cmdqSecAddrMetadataStruct *meta;
	s32 idx, max, ret = 0;

	cmdq_log("pkt:%p type:%u base:%#llx offset:%#x size:%#x port:%#x",
		pkt, type, base, offset, size, port);

	if (!pkt->sec_data) {
		sec_data = kzalloc(sizeof(*sec_data), GFP_KERNEL);
		if (!sec_data)
			return -ENOMEM;
		pkt->sec_data = (void *)sec_data;
	}
	sec_data = (struct cmdqSecDataStruct *)pkt->sec_data;
	idx = sec_data->addrMetadataCount;

	if (idx >= CMDQ_IWC_MAX_ADDR_LIST_LENGTH) {
		cmdq_err("idx:%u reach over:%u",
			idx, CMDQ_IWC_MAX_ADDR_LIST_LENGTH);
		return -EFAULT;
	}

	if (!sec_data->addrMetadataMaxCount)
		max = ADDR_METADATA_MAX_COUNT_ORIGIN;
	else if (idx >= sec_data->addrMetadataMaxCount)
		max = sec_data->addrMetadataMaxCount * 2;
	ret = cmdq_sec_realloc_addr_list(pkt, max);
	if (ret)
		return ret;

	if (!sec_data->addrMetadatas) {
		cmdq_log("addrMetadatas is missing");
		meta = kzalloc(sizeof(*meta), GFP_KERNEL);
		if (!meta)
			return -ENOMEM;
		sec_data->addrMetadatas = (u64)(void *)meta;
	}
	meta = (struct cmdqSecAddrMetadataStruct *)
		(unsigned long)sec_data->addrMetadatas;

	meta[idx].instrIndex = pkt->cmd_buf_size / CMDQ_INST_SIZE - 1;
	meta[idx].type = type;
	meta[idx].baseHandle = base;
	meta[idx].offset = offset;
	meta[idx].size = size;
	meta[idx].port = port;
	return 0;
}

s32 cmdq_sec_pkt_set_data(struct cmdq_pkt *pkt, const u64 dapc_engine,
	const u64 port_sec_engine, const enum CMDQ_SCENARIO_ENUM scenario)
{
	struct cmdqSecDataStruct *sec_data;

	if (!pkt) {
		cmdq_err("invalid pkt:%p", pkt);
		return -EINVAL;
	}
	if (!pkt->sec_data) {
		sec_data = kzalloc(sizeof(*sec_data), GFP_KERNEL);
		if (!sec_data)
			return -ENOMEM;
		pkt->sec_data = (void *)sec_data;
	}
	cmdq_msg("pkt:%p sec_data:%p dapc:%llu port_sec:%llu scen:%u",
		pkt, pkt->sec_data, dapc_engine, port_sec_engine, scenario);

	sec_data = (struct cmdqSecDataStruct *)pkt->sec_data;
	sec_data->enginesNeedDAPC |= dapc_engine;
	sec_data->enginesNeedPortSecurity |= port_sec_engine;
	sec_data->scenario = scenario;
	return 0;
}
EXPORT_SYMBOL(cmdq_sec_pkt_set_data);

// s32 cmdq_op_write_reg_secure
s32 cmdq_sec_pkt_write_reg(struct cmdq_pkt *pkt, u32 addr, u64 base,
	const enum CMDQ_SEC_ADDR_METADATA_TYPE type,
	const u32 offset, const u32 size, const u32 port)
{
	s32 ret;

	ret = cmdq_pkt_write_value_addr(pkt, addr, base, UINT_MAX);
	if (ret)
		return ret;
	return cmdq_sec_append_metadata(pkt, type, base, offset, size, port);
}
EXPORT_SYMBOL(cmdq_sec_pkt_write_reg);

void cmdq_sec_dump_secure_data(struct cmdq_pkt *pkt)
{
	struct cmdqSecDataStruct *data;
	struct cmdqSecAddrMetadataStruct *meta;
	s32 i;

	if (!pkt || !pkt->sec_data) {
		cmdq_msg("pkt without sec_data");
		return;
	}

	data = (struct cmdqSecDataStruct *)pkt->sec_data;
	cmdq_util_msg(
		"meta cnt:%u addr:%#llx max:%u scen:%d dapc:%#llx port:%#llx wait:%d reset:%d",
		data->addrMetadataCount, data->addrMetadatas,
		data->addrMetadataMaxCount, data->scenario,
		data->enginesNeedDAPC, data->enginesNeedPortSecurity,
		data->waitCookie, data->resetExecCnt);

	meta = (struct cmdqSecAddrMetadataStruct *)(unsigned long)
		data->addrMetadatas;
	for (i = 0; i < data->addrMetadataCount; i++)
		cmdq_util_msg(
			"meta:%d instr:%u type:%u base:%#llx block:%u ofst:%u size:%u port:%u",
			i, meta[i].instrIndex, meta[i].type,
			meta[i].baseHandle, meta[i].blockOffset, meta[i].offset,
			meta[i].size, meta[i].port);
}
EXPORT_SYMBOL(cmdq_sec_dump_secure_data);
