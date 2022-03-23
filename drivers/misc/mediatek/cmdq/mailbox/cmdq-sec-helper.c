// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/soc/mediatek/mtk-cmdq-ext.h>

#include "cmdq-util.h"
#include "cmdq-sec.h"
#include "cmdq-sec-mailbox.h"

#define ADDR_METADATA_MAX_COUNT_ORIGIN	(8)

#define CMDQ_IMMEDIATE_VALUE		(0)
#define CMDQ_REG_TYPE			(1)

static s32 cmdq_sec_realloc_addr_list(struct cmdq_pkt *pkt, const u32 count)
{
	struct cmdq_sec_data *sec_data =
		(struct cmdq_sec_data *)pkt->sec_data;
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

static s32 cmdq_sec_check_sec(struct cmdq_pkt *pkt)
{
	struct cmdq_sec_data *sec_data;

	if (pkt->sec_data)
		return 0;

	sec_data = kzalloc(sizeof(*sec_data), GFP_KERNEL);
	if (!sec_data)
		return -ENOMEM;
	pkt->sec_data = (void *)sec_data;

	return 0;
}

static s32 cmdq_sec_append_metadata(
	struct cmdq_pkt *pkt, const enum CMDQ_IWC_ADDR_METADATA_TYPE type,
	const u64 base, const u32 offset, const u32 size, const u32 port)
{
	struct cmdq_sec_data *sec_data;
	struct cmdq_sec_addr_meta *meta;
	s32 idx, max, ret;

	cmdq_log("pkt:%p type:%u base:%#llx offset:%#x size:%#x port:%#x",
		pkt, type, base, offset, size, port);

	ret = cmdq_sec_check_sec(pkt);
	if (ret < 0)
		return ret;
	sec_data = (struct cmdq_sec_data *)pkt->sec_data;
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
	else
		max = sec_data->addrMetadataMaxCount;
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
	meta = (struct cmdq_sec_addr_meta *)
		(unsigned long)sec_data->addrMetadatas;

	meta[idx].instrIndex = pkt->cmd_buf_size / CMDQ_INST_SIZE - 1;
	meta[idx].type = type;
	meta[idx].baseHandle = base;
	meta[idx].offset = offset;
	meta[idx].size = size;
	meta[idx].port = port;
	sec_data->addrMetadataCount += 1;
	return 0;
}

s32 cmdq_sec_pkt_set_data(struct cmdq_pkt *pkt, const u64 dapc_engine,
	const u64 port_sec_engine, const enum CMDQ_SEC_SCENARIO scenario,
	const enum cmdq_sec_meta_type meta_type)
{
	struct cmdq_sec_data *sec_data;
	s32 ret;

	if (!pkt) {
		cmdq_err("invalid pkt:%p", pkt);
		return -EINVAL;
	}

	ret = cmdq_sec_check_sec(pkt);
	if (ret < 0)
		return ret;
	cmdq_log(
		"pkt:%p sec_data:%p dapc:%llu port_sec:%llu scen:%u",
		pkt, pkt->sec_data, dapc_engine, port_sec_engine, scenario);

	sec_data = (struct cmdq_sec_data *)pkt->sec_data;
	sec_data->enginesNeedDAPC |= dapc_engine;
	sec_data->enginesNeedPortSecurity |= port_sec_engine;
	sec_data->scenario = scenario;
	sec_data->client_meta_type = meta_type;

	return 0;
}
EXPORT_SYMBOL(cmdq_sec_pkt_set_data);

void cmdq_sec_pkt_set_mtee(struct cmdq_pkt *pkt, const bool enable)
{
	struct cmdq_sec_data *sec_data =
		(struct cmdq_sec_data *)pkt->sec_data;
	sec_data->mtee = enable;
	cmdq_msg("%s pkt:%p mtee:%d\n",
		__func__, pkt, ((struct cmdq_sec_data *)pkt->sec_data)->mtee);
}
EXPORT_SYMBOL(cmdq_sec_pkt_set_mtee);

/* iommu_sec_id */
void cmdq_sec_pkt_set_secid(struct cmdq_pkt *pkt, int32_t sec_id)
{
	struct cmdq_sec_data *sec_data =
		(struct cmdq_sec_data *)pkt->sec_data;
	sec_data->sec_id = sec_id;
	cmdq_log("%s pkt:%p sec_id:%d\n",
		__func__, pkt, ((struct cmdq_sec_data *)pkt->sec_data)->sec_id);
}
EXPORT_SYMBOL(cmdq_sec_pkt_set_secid);

void cmdq_sec_pkt_free_data(struct cmdq_pkt *pkt)
{
	if (pkt->sec_data == NULL)
		return;
	kfree((void *)((struct cmdq_sec_data *)pkt->sec_data)->addrMetadatas);
	kfree(pkt->sec_data);
}
EXPORT_SYMBOL(cmdq_sec_pkt_free_data);

s32 cmdq_sec_pkt_set_payload(struct cmdq_pkt *pkt, u8 idx,
	const u32 meta_size, u32 *meta)
{
	struct cmdq_sec_data *sec_data;
	s32 ret;

	if (idx == 0) {
		cmdq_err("not allow set reserved payload 0");
		return -EINVAL;
	}

	if (!meta_size || !meta) {
		cmdq_err("not allow empty size or buffer");
		return -EINVAL;
	}

	ret = cmdq_sec_check_sec(pkt);
	if (ret < 0)
		return ret;

	if (idx == CMDQ_IWC_MSG1 &&
		meta_size >= sizeof(struct iwcCmdqMessageEx_t)) {
		cmdq_err("not enough size payload 1:%u msg size:%zu",
			meta_size, sizeof(struct iwcCmdqMessageEx_t));
		return -EINVAL;
	}

	if (idx == CMDQ_IWC_MSG2 &&
		meta_size >= sizeof(struct iwcCmdqMessageEx2_t)) {
		cmdq_err("not enough size payload 2:%u msg size:%zu",
			meta_size, sizeof(struct iwcCmdqMessageEx2_t));
		return -EINVAL;
	}

	sec_data = (struct cmdq_sec_data *)pkt->sec_data;
	sec_data->client_meta_size[idx] = meta_size;
	sec_data->client_meta[idx] = meta;

	return 0;
}
EXPORT_SYMBOL(cmdq_sec_pkt_set_payload);

s32 cmdq_sec_pkt_write_reg(struct cmdq_pkt *pkt, u32 addr, u64 base,
	const enum CMDQ_IWC_ADDR_METADATA_TYPE type,
	const u32 offset, const u32 size, const u32 port)
{
	s32 ret;

	ret = cmdq_pkt_assign_command(pkt, CMDQ_SPR_FOR_TEMP, addr);
	if (ret)
		return ret;

	ret = cmdq_pkt_append_command(pkt,
		base & 0xffff, base >> 16, CMDQ_SPR_FOR_TEMP, 0,
		CMDQ_IMMEDIATE_VALUE, CMDQ_IMMEDIATE_VALUE, CMDQ_REG_TYPE,
		CMDQ_CODE_WRITE_S);
	if (ret)
		return ret;

	return cmdq_sec_append_metadata(pkt, type, base, offset, size, port);
}
EXPORT_SYMBOL(cmdq_sec_pkt_write_reg);

s32 cmdq_sec_pkt_assign_metadata(struct cmdq_pkt *pkt,
	u32 count, void *meta_array)
{
	struct cmdq_sec_data *data;
	void *pkt_meta_array;
	size_t size;
	s32 ret;

	if (!count)
		return -EINVAL;

	ret = cmdq_sec_check_sec(pkt);
	if (ret < 0)
		return ret;
	data = (struct cmdq_sec_data *)pkt->sec_data;

	size = count * sizeof(struct cmdq_sec_addr_meta);
	pkt_meta_array = kzalloc(size, GFP_KERNEL);
	if (!pkt_meta_array)
		return -ENOMEM;

	memcpy(pkt_meta_array, meta_array, size);
	data->addrMetadatas = (unsigned long)pkt_meta_array;
	data->addrMetadataCount = count;

	return 0;
}
EXPORT_SYMBOL(cmdq_sec_pkt_assign_metadata);

void cmdq_sec_dump_secure_data(struct cmdq_pkt *pkt)
{
	struct cmdq_sec_data *data;
	struct cmdq_sec_addr_meta *meta;
	s32 i;
	u64 *inst;

	if (!pkt || !pkt->sec_data) {
		cmdq_msg("pkt without sec_data");
		return;
	}

	data = (struct cmdq_sec_data *)pkt->sec_data;
	cmdq_util_msg(
		"meta cnt:%u addr:%#llx max:%u scen:%d dapc:%#llx port:%#llx wait:%d reset:%d metatype:%u",
		data->addrMetadataCount, data->addrMetadatas,
		data->addrMetadataMaxCount, data->scenario,
		data->enginesNeedDAPC, data->enginesNeedPortSecurity,
		data->waitCookie, data->resetExecCnt,
		(u32)data->client_meta_type);

	meta = (struct cmdq_sec_addr_meta *)(unsigned long)
		data->addrMetadatas;
	for (i = 0; i < data->addrMetadataCount; i++) {
		inst = cmdq_pkt_get_va_by_offset(pkt,
			meta[i].instrIndex * CMDQ_INST_SIZE);
		cmdq_util_msg(
			"meta:%d instr:%u type:%u base:%#llx block:%u ofst:%u size:%u port:%u inst:%#018llx",
			i, meta[i].instrIndex, meta[i].type,
			meta[i].baseHandle, meta[i].blockOffset, meta[i].offset,
			meta[i].size, meta[i].port,
			inst ? *inst : 0);
	}
}
EXPORT_SYMBOL(cmdq_sec_dump_secure_data);

int cmdq_sec_pkt_wait_complete(struct cmdq_pkt *pkt)
{
	struct cmdq_client *client = pkt->cl;
	unsigned long ret;
	u8 cnt = 0;
	s32 thread_id = cmdq_sec_mbox_chan_id(client->chan);
	u32 timeout_ms = cmdq_mbox_get_thread_timeout((void *)client->chan);

#if IS_ENABLED(CONFIG_MMPROFILE)
	cmdq_sec_mmp_wait(client->chan, pkt);
#endif

	cmdq_sec_mbox_enable(client->chan);

	do {
		if (timeout_ms == CMDQ_NO_TIMEOUT) {
			cmdq_msg("%s: timeout:%u", __func__, timeout_ms);
			wait_for_completion(&pkt->cmplt);
			break;
		}

		ret = wait_for_completion_timeout(&pkt->cmplt,
			msecs_to_jiffies(CMDQ_PREDUMP_MS(timeout_ms)));
		if (ret)
			break;

		cmdq_util_dump_lock();

		cmdq_msg("===== SW timeout Pre-dump %hhu =====", cnt);
		cnt++;

		cmdq_dump_core(client->chan);
		cmdq_msg("thd:%d Hidden thread info since it's secure",
			thread_id);
		cmdq_sec_dump_operation(client->chan);
		cmdq_sec_dump_secure_thread_cookie(client->chan);
		cmdq_dump_pkt(pkt, 0, false);
		cmdq_sec_dump_notify_loop(client->chan);

		cmdq_util_dump_unlock();
	} while (1);

	cmdq_sec_mbox_disable(client->chan);

#if IS_ENABLED(CONFIG_MMPROFILE)
	cmdq_sec_mmp_wait_done(client->chan, pkt);
#endif

	return 0;
}
EXPORT_SYMBOL(cmdq_sec_pkt_wait_complete);

void cmdq_sec_err_dump(struct cmdq_pkt *pkt, struct cmdq_client *client,
	u64 **inst, const char **dispatch)
{
	cmdq_sec_dump_operation(client->chan);
	cmdq_sec_dump_secure_thread_cookie(client->chan);
	cmdq_sec_dump_notify_loop(client->chan);
	cmdq_sec_dump_secure_data(pkt);
	cmdq_sec_dump_response(client->chan, pkt, inst, dispatch);
}
EXPORT_SYMBOL(cmdq_sec_err_dump);

MODULE_LICENSE("GPL v2");
