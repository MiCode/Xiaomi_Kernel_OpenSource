/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/time.h>

#include "apusys_cmn.h"
#include "apusys_drv.h"
#include "scheduler.h"
#include "cmd_parser.h"
#include "cmd_format.h"
#include "memory_mgt.h"
#include "resource_mgt.h"

//----------------------------------------------
// parser function
static uint64_t _get_magic(void *ce)
{
	return *(TYPE_APUSYS_MAGIC *)(ce + OFFSET_APUSYS_MAGIC);
}

static uint64_t _get_cmdid(void *ce)
{
	return *(TYPE_APUSYS_CMD_ID *)(ce + OFFSET_APUSYS_CMD_ID);
}

static uint8_t _get_cmdversion(void *ce)
{
	return *(TYPE_APUSYS_CMD_VERSION *)(ce + OFFSET_APUSYS_CMD_VERSION);
}

static uint8_t _get_priority(void *ce)
{
	return *(TYPE_PRIORITY *)(ce + OFFSET_PRIORITY);
}

static uint16_t _get_hardlimit(void *ce)
{
	return *(TYPE_HARDLIMIT *)(ce + OFFSET_HARDLIMIT);
}

static uint16_t _get_softlimit(void *ce)
{
	return *(TYPE_SOFTLIMIT *)(ce + OFFSET_SOFTLIMIT);
}

static uint64_t _get_flag_bitmap(void *ce)
{
	return *(TYPE_FLAG *)(ce + OFFSET_FLAG);
}

static TYPE_NUM_OF_SUBGRAPH _get_numofsc(void *ce)
{
	return *(TYPE_NUM_OF_SUBGRAPH *)(ce + OFFSET_NUM_OF_SUBGRAPH);
}

static uint64_t _get_dp_entry(void *ce)
{
	uint32_t offset = *(TYPE_OFFSET_TO_DEPENDENCY_INFO_LIST *)
		(ce + OFFSET_OFFSET_TO_DEPENDENCY_INFO_LIST);
	return (uint64_t)(ce + (uint64_t)offset);
}

static uint64_t _get_sc_list_entry(void *ce)
{
	return (uint64_t)(ce + OFFSET_SUBGRAPH_INFO_OFFSET_LIST);
}

static void _print_header(void *ce)
{
	LOG_DEBUG("=====================================\n");
	LOG_DEBUG(" apusys header(%p)\n", ce);
	LOG_DEBUG("-------------------------------------\n");
	LOG_DEBUG(" entry                = 0x%llx\n", (uint64_t)ce);
	LOG_DEBUG(" cmd magic            = 0x%llx\n", _get_magic(ce));
	LOG_DEBUG(" cmd id               = 0x%llx\n", _get_cmdid(ce));
	LOG_DEBUG(" version              = %d\n", _get_cmdversion(ce));
	LOG_DEBUG(" priority             = %d\n", _get_priority(ce));
	LOG_DEBUG(" hardlimit            = %hu\n", _get_hardlimit(ce));
	LOG_DEBUG(" softlimit            = %hu\n", _get_softlimit(ce));
	LOG_DEBUG(" flag                 = 0x%llx\n", _get_flag_bitmap(ce));
	LOG_DEBUG(" #subcmd              = %d\n", _get_numofsc(ce));
	LOG_DEBUG(" dependency list entry= 0x%llx\n", _get_dp_entry(ce));
	LOG_DEBUG(" subcmd list entry    = 0x%llx\n", _get_sc_list_entry(ce));
	LOG_DEBUG("=====================================\n");
}

void _print_sc_info(struct apusys_subcmd *sc)
{
	LOG_DEBUG("=====================================\n");
	LOG_DEBUG(" apusys sc info(%p)\n", sc);
	LOG_DEBUG("-------------------------------------\n");
	LOG_DEBUG(" type                 = %d\n", sc->type);
	LOG_DEBUG(" sc entry             = 0x%llx\n", (uint64_t)sc->entry);
	LOG_DEBUG(" parent cmd           = %p\n", sc->parent_cmd);
	LOG_DEBUG(" idx                  = %d\n", sc->idx);
	LOG_DEBUG(" estimate time        = %llu\n", sc->d_time);
	LOG_DEBUG(" codebuf info         = %p\n", sc->codebuf);
	LOG_DEBUG(" codebuf size         = %u\n", sc->codebuf_size);
	LOG_DEBUG(" codebuf fd           = %d\n", sc->codebuf_fd);
	LOG_DEBUG(" boost val            = %u\n", sc->boost_val);
	LOG_DEBUG(" bandwidth            = %u\n", sc->bw);
	LOG_DEBUG(" suggest time         = %u\n", sc->suggest_time);
	LOG_DEBUG(" tcm force            = %u\n", sc->tcm_force);
	LOG_DEBUG(" pack id              = 0x%x\n", sc->pack_idx);
	LOG_DEBUG(" ctx group            = %u\n", sc->ctx_group);
	LOG_DEBUG("=====================================\n");
}

uint64_t get_time_from_system(void)
{
	struct timeval now;

	do_gettimeofday(&now);

	return (uint64_t)now.tv_usec;
}

uint64_t get_subcmd_by_idx(struct apusys_cmd *cmd, int idx)
{
	/* check argument valid */
	if (cmd == NULL) {
		LOG_ERR("invalid argument\n");
		return 0;
	}

	if (idx > cmd->sc_num) {
		LOG_ERR("idx too big(%d/%d)\n", idx, cmd->sc_num);
		return 0;
	}

	return (uint64_t)(cmd->entry) +
		(uint64_t)(*(TYPE_SUBGRAPH_INFO_POINTER *)
		(cmd->sc_list_entry + SIZE_SUBGRAPH_INFO_POINTER * idx));
}

uint32_t get_type_from_subcmd(void *sc_entry)
{
	if (sc_entry == NULL) {
		LOG_ERR("invalid argument\n");
		return 0;
	}

	return *(TYPE_SUBGRAPH_TYPE *)
	(sc_entry + OFFSET_SUBGRAPH_TYPE);
}

uint64_t get_dtime_from_subcmd(void *sc_entry)
{
	if (sc_entry == NULL) {
		LOG_ERR("invalid argument\n");
		return 0;
	}
	return *(TYPE_SUBGRAPH_DRIVER_TURNAROUND *)
		(sc_entry + OFFSET_SUBGRAPH_DRIVER_TURNAROUND);
}

int set_dtime_to_subcmd(void *sc_entry, uint64_t us)
{
	if (sc_entry == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	*(TYPE_SUBGRAPH_DRIVER_TURNAROUND *)
		(sc_entry + OFFSET_SUBGRAPH_DRIVER_TURNAROUND) = us;
	return 0;
}

static uint32_t get_suggesttime_from_subcmd(void *sc_entry)
{
	if (sc_entry == NULL) {
		LOG_ERR("invalid argument\n");
		return 0;
	}
	return *(TYPE_SUBGRAPH_SUGGEST_TIME *)
		(sc_entry + OFFSET_SUBGRAPH_SUGGEST_TIME);
}

static uint32_t get_bandwidth_from_subcmd(void *sc_entry)
{
	if (sc_entry == NULL) {
		LOG_ERR("invalid argument\n");
		return 0;
	}
	return *(TYPE_SUBGRAPH_BANDWIDTH *)
		(sc_entry + OFFSET_SUBGRAPH_BANDWIDTH);
}

int set_bandwidth_to_subcmd(void *sc_entry, uint32_t bandwidth)
{
	if (sc_entry == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}
	*(TYPE_SUBGRAPH_BANDWIDTH *)
		(sc_entry + OFFSET_SUBGRAPH_BANDWIDTH) = bandwidth;
	return 0;
}

int set_tcmusage_from_subcmd(void *sc_entry, uint32_t tcm_usage)
{
	if (sc_entry == NULL) {
		LOG_ERR("invalid argument\n");
		return 0;
	}
	*(TYPE_SUBGRAPH_TCM_USAGE *)
		(sc_entry + OFFSET_SUBGRAPH_TCM_USAGE) = tcm_usage;
	return 0;
}

static uint8_t get_tcmforce_from_subcmd(void *sc_entry)
{
	if (sc_entry == NULL) {
		LOG_ERR("invalid argument\n");
		return 0;
	}
	return *(TYPE_SUBGRAPH_TCM_FORCE *)
		(sc_entry + OFFSET_SUBGRAPH_TCM_FORCE);
}

static uint8_t get_boostval_from_subcmd(void *sc_entry)
{
	if (sc_entry == NULL) {
		LOG_ERR("invalid argument\n");
		return 0;
	}
	return *(TYPE_SUBGRAPH_BANDWIDTH *)
		(sc_entry + OFFSET_SUBGRAPH_BOOST_VAL);
}

static uint32_t get_ctxid_from_subcmd(void *sc_entry)
{
	if (sc_entry == NULL) {
		LOG_ERR("invalid argument\n");
		return 0;
	}
	return *(TYPE_SUBGRAPH_CTX_ID *)
		(sc_entry + OFFSET_SUBGRAPH_CTX_ID);
}

uint32_t get_codebuf_size_from_subcmd(void *sc_entry)
{
	if (sc_entry == NULL) {
		LOG_ERR("invalid argument\n");
		return 0;
	}
	return *(TYPE_SUBGRAPH_CODEBUF_INFO_SIZE *)
		(sc_entry + OFFSET_SUBGRAPH_CODEBUF_INFO_SIZE);
}

uint32_t get_codebuf_offset_from_subcmd(void *sc_entry)
{
	if (sc_entry == NULL) {
		LOG_ERR("invalid argument\n");
		return 0;
	}
	return *(TYPE_SUBGRAPH_CODEBUF_INFO_OFFSET *)
		(sc_entry + OFFSET_SUBGRAPH_CODEBUF_INFO_OFFSET);
}

static int check_fd_from_codebuf_offset(unsigned int codebuf_offset)
{
	if (codebuf_offset & (1UL << SUBGRAPH_CODEBUF_INFO_BIT_FD))
		return 1;

	return 0;
}

static uint32_t get_packid_from_subcmd(void *sc_entry, int type)
{
	if (sc_entry == NULL || (type != APUSYS_DEVICE_SAMPLE &&
		type != APUSYS_DEVICE_VPU)) {
		//LOG_ERR("invalid argument\n");
		return VALUE_SUBGAPH_PACK_ID_NONE;
	}
	return *(TYPE_SUBGRAPH_PACK *)
		(sc_entry + OFFSET_SUBGRAPH_PACK);
}

//----------------------------------------------
uint8_t get_cmdformat_version(void)
{
	return APUSYS_CMD_VERSION;
}

uint64_t get_cmdformat_magic(void)
{
	return APUSYS_MAGIC_NUMBER;
}

int apusys_subcmd_create(void *sc_entry,
	struct apusys_cmd *cmd, struct apusys_subcmd **isc)
{
	int type = 0;
	unsigned int codebuf_offset = 0;
	struct apusys_mem mem;
	struct apusys_subcmd *sc = NULL;

	if (sc_entry == NULL || isc == NULL || cmd == NULL) {
		LOG_ERR("invalid arguments(%p/%p)\n", sc_entry, isc);
		return -EINVAL;
	}

	/* get type */
	type = (int)get_type_from_subcmd(sc_entry);
	if (type <= APUSYS_DEVICE_NONE || type >= APUSYS_DEVICE_MAX) {
		LOG_ERR("invalid device type(%d/%d)\n",
			type, APUSYS_DEVICE_MAX);
		return -EINVAL;
	}

	/* allocate subcmd struct */
	sc = kzalloc(sizeof(struct apusys_subcmd), GFP_KERNEL);
	sc->entry = sc_entry;
	sc->type = type;
	sc->parent_cmd = cmd;
	sc->codebuf_size = get_codebuf_size_from_subcmd(sc_entry);
	if (sc->codebuf_size == 0) {
		LOG_ERR("codebuf size = 0, error\n");
		kfree(sc);
		return -EINVAL;
	}
	codebuf_offset = get_codebuf_offset_from_subcmd(sc_entry);
	LOG_DEBUG("codebuf_offset = 0x%x\n", codebuf_offset);
	if (check_fd_from_codebuf_offset(codebuf_offset)) {
		/* from lib, fd need to map */
		LOG_DEBUG("codebuf is fd, need to map\n");
		sc->codebuf_fd = codebuf_offset &
			~(1UL << SUBGRAPH_CODEBUF_INFO_BIT_FD);
		memset(&mem, 0, sizeof(struct apusys_mem));
		mem.size = sc->codebuf_size;
		mem.ion_data.ion_share_fd = sc->codebuf_fd;
		if (apusys_mem_map_kva(&mem)) {
			LOG_ERR("map sc fail\n");
			kfree(sc);
			return -EINVAL;
		}
		sc->codebuf = (void *)mem.kva;
		sc->codebuf_mem_hnd = mem.ion_data.ion_khandle;
		LOG_DEBUG("map sc codebuf from fd(%d/%p/%u)\n",
			sc->codebuf_fd,
			sc->codebuf,
			sc->codebuf_size);
	} else {
		/* from neuron, kva need offset */
		sc->codebuf_fd = -1;
		sc->codebuf = (void *)((uint64_t)cmd->entry + codebuf_offset);
		LOG_DEBUG("calc sc codebuf from offset(%d/%p/%p/%u)\n",
			sc->codebuf_fd,
			cmd->entry,
			sc->codebuf,
			sc->codebuf_size);
	}

	sc->d_time = get_dtime_from_subcmd(sc_entry);
	sc->boost_val = get_boostval_from_subcmd(sc_entry);
	if (sc->boost_val > 100) {
		LOG_DEBUG("boost_val over 100, set\n");
		sc->boost_val = 100;
	}
	sc->suggest_time = get_suggesttime_from_subcmd(sc_entry);
	sc->bw = get_bandwidth_from_subcmd(sc_entry);
	sc->tcm_force = get_tcmforce_from_subcmd(sc_entry);
	sc->tcm_usage = 0;
	sc->pack_idx = get_packid_from_subcmd(sc_entry, sc->type);
	sc->ctx_group = get_ctxid_from_subcmd(sc_entry);
	if (sc->ctx_group != VALUE_SUBGAPH_CTX_ID_NONE &&
		sc->ctx_group > cmd->sc_num) {
		LOG_ERR("invalid ctx group(%d/%d)\n",
			sc->ctx_group, cmd->sc_num);
		kfree(sc);
		return -EINVAL;
	}
	sc->ctx_id = VALUE_SUBGAPH_CTX_ID_NONE;
	sc->state = CMD_STATE_IDLE;
	INIT_LIST_HEAD(&sc->q_list);
	INIT_LIST_HEAD(&sc->ce_list);
	mutex_init(&sc->mtx);
	sc->dp_status = kcalloc(BITS_TO_LONGS(cmd->sc_num),
		sizeof(unsigned long), GFP_KERNEL);

	*isc = sc;

	_print_sc_info(sc);

	return 0;
}

int apusys_subcmd_delete(struct apusys_subcmd *sc)
{
	struct apusys_mem mem;
	struct apusys_cmd *cmd = NULL;

	if (sc == NULL)
		return -EINVAL;

	cmd = (struct apusys_cmd *)sc->parent_cmd;
	if (cmd == NULL)
		return -EINVAL;

	/* write time back to cmdbuf */
	set_dtime_to_subcmd(sc->entry, sc->d_time);
	set_bandwidth_to_subcmd(sc->entry, sc->bw);
	LOG_DEBUG("0x%llx-#%d sc: time(%llu) bw(%u)\n",
		cmd->cmd_id, sc->idx, sc->d_time, sc->bw);

	DEBUG_TAG;
	if (sc->codebuf_fd >= 0) {
		memset(&mem, 0, sizeof(struct apusys_mem));
		mem.kva = (unsigned long long)sc->codebuf;
		mem.size = sc->codebuf_size;
		mem.ion_data.ion_share_fd = sc->codebuf_fd;
		mem.ion_data.ion_khandle = sc->codebuf_mem_hnd;
		if (apusys_mem_unmap_kva(&mem)) {
			LOG_ERR("unmap codebuf fd(%d) fail\n",
				sc->codebuf_fd);
		}
	}

	if (sc->state <= CMD_STATE_READY) {
		list_del(&sc->ce_list);
		sc->parent_cmd = NULL;
		delete_subcmd_lock((void *)sc);
		if (sc->dp_status != NULL) {
			DEBUG_TAG;
			kfree(sc->dp_status);
		}
	}

	kfree(sc);

	return 0;
}

int apusys_cmd_create(int mem_fd, uint32_t offset,
	struct apusys_cmd **icmd)
{
	struct apusys_cmd *cmd;
	struct apusys_mem mem;
	void *cmd_entry = NULL;
	uint8_t cmd_version = 0;
	uint64_t cmd_magic = 0;
	int ret = 0, i = 0, ctx_idx = 0;
	uint32_t sc_num = 0;

	/* map kva */
	memset(&mem, 0, sizeof(struct apusys_mem));
	mem.ion_data.ion_share_fd = mem_fd;
	if (apusys_mem_map_kva(&mem)) {
		LOG_ERR("map cmd buffer kva from fd(%d)fail\n",
			mem_fd);
		return -ENOMEM;
	}

	cmd_entry = (void *)(mem.kva + offset);
	LOG_DEBUG("cmd entry = 0x%llx/%u/%p\n",
		mem.kva, offset, cmd_entry);

	_print_header(cmd_entry);

	/* check magic */
	cmd_magic = _get_magic(cmd_entry);
	if (cmd_magic != APUSYS_MAGIC_NUMBER) {
		LOG_ERR("cmd magic mismatch(0x%llx)\n",
			cmd_magic);
		return -EINVAL;
	}

	/* check version */
	cmd_version = _get_cmdversion(cmd_entry);
	if (cmd_version != APUSYS_CMD_VERSION) {
		LOG_ERR("cmd version mismatch(%d/%d)\n",
			cmd_version, APUSYS_CMD_VERSION);
		return -EINVAL;
	}

	/* check subcmd num */
	sc_num = _get_numofsc(cmd_entry);
	if (sc_num == 0) {
		LOG_ERR("subcmd num(%d), nothing to do\n", sc_num);
		return -EINVAL;
	}

	/* allocate apusys cmd */
	cmd = kzalloc(sizeof(struct apusys_cmd), GFP_KERNEL);
	if (cmd == NULL) {
		LOG_ERR("alloc apusys cmd fail\n");
		ret = -ENOMEM;
		goto ce_fail;
	}

	/* assign value */
	cmd->mem_fd = mem_fd;
	cmd->mem_hnd = mem.ion_data.ion_khandle;

	cmd->entry = (void *)cmd_entry;
	cmd->cmd_uid = _get_cmdid(cmd->entry);
	cmd->cmd_id = (uint64_t)(cmd);
	cmd->sc_num = sc_num;
	cmd->sc_list_entry = (void *)_get_sc_list_entry(cmd->entry);
	cmd->dp_entry = (void *)_get_dp_entry(cmd->entry);
	cmd->priority = _get_priority(cmd->entry);
	cmd->hard_limit = _get_hardlimit(cmd->entry);
	cmd->soft_limit = _get_softlimit(cmd->entry);
	cmd->power_save = (_get_flag_bitmap(cmd->entry) &
		1UL << CMD_FLAG_BITMAP_POWERSAVE) ? 1 : 0;

	cmd->state = CMD_STATE_READY;

	/* allocate subcmd bitmap for tracing status */
	cmd->sc_status = kcalloc(BITS_TO_LONGS(cmd->sc_num),
	sizeof(unsigned long), GFP_KERNEL);
	if (cmd->sc_status == NULL) {
		LOG_ERR("alloc bitmap for subcmd status fail\n");
		ret = -ENOMEM;
		goto sc_status_fail;
	}

	/* allocate ctx ref and list */
	cmd->ctx_ref = kcalloc(cmd->sc_num, sizeof(uint32_t), GFP_KERNEL);
	if (cmd->ctx_ref == NULL) {
		LOG_ERR("alloc ctx ref count for ctx fail\n");
		ret = -ENOMEM;
		goto sc_ctx_fail;
	}
	cmd->ctx_list = kcalloc(cmd->sc_num, sizeof(uint32_t), GFP_KERNEL);
	if (cmd->ctx_list == NULL) {
		LOG_ERR("alloc ctx list count for ctx fail\n");
		ret = -ENOMEM;
		goto sc_ctx_list_fail;
	}
	cmd->pc_col.pack_status = kzalloc(sizeof(unsigned long) *
		cmd->sc_num, GFP_KERNEL);
	if (cmd->pc_col.pack_status == NULL) {
		LOG_ERR("alloc pack status fail\n");
		ret = -ENOMEM;
		goto pc_status_fail;
	}

	/* set subcmd status */
	bitmap_set(cmd->sc_status, 0, cmd->sc_num);

	/* set memory ctx ref count */
	for (i = 0; i < cmd->sc_num; i++) {
		ctx_idx = get_ctxid_from_subcmd
			((void *)get_subcmd_by_idx(cmd, i));
		if (ctx_idx == VALUE_SUBGAPH_CTX_ID_NONE)
			continue;
		if (ctx_idx >= cmd->sc_num) {
			LOG_ERR("cmd(0x%llx) #%d subcmd's ctx_id(%d) invalid\n",
				cmd->cmd_id, i, ctx_idx);
			goto count_ctx_fail;
		}
		cmd->ctx_ref[ctx_idx]++;
	}
	memset(cmd->ctx_list, VALUE_SUBGAPH_CTX_ID_NONE,
		sizeof(uint32_t) * cmd->sc_num);

	mutex_init(&cmd->sc_mtx);
	mutex_init(&cmd->mtx);
	INIT_LIST_HEAD(&cmd->pc_col.sc_list);
	INIT_LIST_HEAD(&cmd->sc_list);
	INIT_LIST_HEAD(&cmd->u_list);
	init_completion(&cmd->comp);

	*icmd = cmd;

	LOG_INFO("create cmd (0x%llx/0x%llx/%u)(%u/%u/%u)(%u)\n",
		cmd->cmd_uid, cmd->cmd_id,
		cmd->sc_num, cmd->priority,
		cmd->hard_limit, cmd->soft_limit,
		cmd->power_save);

	return ret;

count_ctx_fail:
	kfree(cmd->pc_col.pack_status);
pc_status_fail:
	kfree(cmd->ctx_list);
sc_ctx_list_fail:
	kfree(cmd->ctx_ref);
sc_ctx_fail:
	kfree(cmd->sc_status);
sc_status_fail:
	kfree(cmd);
ce_fail:
	return ret;
}

int apusys_cmd_delete(struct apusys_cmd *cmd)
{
	struct apusys_subcmd *sc = NULL;
	struct apusys_mem mem;
	struct list_head *tmp = NULL, *list_ptr = NULL;

	if (cmd == NULL)
		return -EINVAL;

	mutex_lock(&cmd->mtx);
	mutex_lock(&cmd->sc_mtx);

	DEBUG_TAG;
	list_for_each_safe(list_ptr, tmp, &cmd->sc_list) {
		DEBUG_TAG;
		sc = list_entry(list_ptr, struct apusys_subcmd, ce_list);
		DEBUG_TAG;
		if (sc != NULL) {
			if (apusys_subcmd_delete(sc))
				LOG_ERR("delete subcmd fail(%p)\n", sc);
		}
	}
	DEBUG_TAG;

	mutex_unlock(&cmd->sc_mtx);

	/* map kva */
	memset(&mem, 0, sizeof(struct apusys_mem));
	mem.ion_data.ion_share_fd = cmd->mem_fd;
	mem.ion_data.ion_khandle = cmd->mem_hnd;
	if (apusys_mem_unmap_kva(&mem)) {
		LOG_ERR("map cmd buffer kva from fd(%d)fail\n",
			cmd->mem_fd);
	}
	mutex_unlock(&cmd->mtx);
	kfree(cmd->pc_col.pack_status);
	kfree(cmd->ctx_list);
	kfree(cmd->ctx_ref);
	kfree(cmd);

	return 0;
}
