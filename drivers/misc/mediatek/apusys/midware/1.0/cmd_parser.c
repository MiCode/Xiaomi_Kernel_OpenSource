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

static uint16_t _get_targettime(void *ce)
{
	return *(TYPE_TARGET_TIME *)(ce + OFFSET_TARGET_TIME);
}

uint32_t _get_deadline(void *ce)
{
	return *(TYPE_DEADLINE *)(ce + OFFSET_DEADLINE);
}

static uint32_t _get_numofsc(void *ce)
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
	LOG_DEBUG(" cmd magic            = 0x%llx\n", _get_magic(ce));
	LOG_DEBUG(" cmd id               = 0x%llx\n", _get_cmdid(ce));
	LOG_DEBUG(" version              = %d\n", _get_cmdversion(ce));
	LOG_DEBUG(" priority             = %d\n", _get_priority(ce));
	LOG_DEBUG(" target time          = %d\n", _get_targettime(ce));
	LOG_DEBUG(" deadline             = %d\n", _get_deadline(ce));
	LOG_DEBUG(" #subcmd              = %d\n", _get_numofsc(ce));
	LOG_DEBUG(" dependency list entry= 0x%llx\n", _get_dp_entry(ce));
	LOG_DEBUG(" subcmd list entry    = 0x%llx\n", _get_sc_list_entry(ce));
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

	return (uint64_t)(cmd->kva) +
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

uint64_t get_utime_from_subcmd(void *sc_entry)
{
	if (sc_entry == NULL) {
		LOG_ERR("invalid argument\n");
		return 0;
	}
	return *(TYPE_SUBGRAPH_UP_TURNAROUND *)
		(sc_entry + OFFSET_SUBGRAPH_UP_TURNAROUND);
}

int set_utime_to_subcmd(void *sc_entry, uint64_t us)
{
	if (sc_entry == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	*(TYPE_SUBGRAPH_UP_TURNAROUND *)
		(sc_entry + OFFSET_SUBGRAPH_UP_TURNAROUND) = us;

	return 0;
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

uint32_t get_tcmusage_from_subcmd(void *sc_entry)
{
	if (sc_entry == NULL) {
		LOG_ERR("invalid argument\n");
		return 0;
	}
	return *(TYPE_SUBGRAPH_TCM_USAGE *)
		(sc_entry + OFFSET_SUBGRAPH_TCM_USAGE);
}

uint32_t get_tcmforce_from_subcmd(void *sc_entry)
{
	if (sc_entry == NULL) {
		LOG_ERR("invalid argument\n");
		return 0;
	}
	return *(TYPE_SUBGRAPH_TCM_FORCE *)
		(sc_entry + OFFSET_SUBGRAPH_TCM_FORCE);
}

uint32_t get_ctxid_from_subcmd(void *sc_entry)
{
	if (sc_entry == NULL) {
		LOG_ERR("invalid argument\n");
		return 0;
	}
	return *(TYPE_SUBGRAPH_CTX_ID *)
		(sc_entry + OFFSET_SUBGRAPH_CTX_ID);
}

uint32_t get_size_from_subcmd(void *sc_entry)
{
	if (sc_entry == NULL) {
		LOG_ERR("invalid argument\n");
		return 0;
	}
	return *(TYPE_SUBGRAPH_SIZE *)
		(sc_entry + OFFSET_SUBGRAPH_SIZE);
}

uint64_t get_addr_from_subcmd(void *sc_entry)
{
	if (sc_entry == NULL) {
		LOG_ERR("invalid argument\n");
		return 0;
	}
	return *(TYPE_SUBGRAPH_ADDR *)
		(sc_entry + OFFSET_SUBGRAPH_ADDR);
}

uint32_t get_packid_from_subcmd(void *sc_entry, int type)
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
inline uint8_t get_cmdformat_version(void)
{
	return APUSYS_CMD_VERSION;
}

inline uint64_t get_cmdformat_magic(void)
{
	return APUSYS_MAGIC_NUMBER;
}

int apusys_subcmd_create(void *sc_entry,
	struct apusys_cmd *cmd, struct apusys_subcmd **isc)
{
	int type = 0;
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
	sc->pack_idx = get_packid_from_subcmd(sc_entry, sc->type);
	sc->ctx_group = get_ctxid_from_subcmd(sc_entry);
	sc->ctx_id = VALUE_SUBGAPH_CTX_ID_NONE;
	sc->state = CMD_STATE_IDLE;
	sc->d_time = get_dtime_from_subcmd(sc_entry);
	sc->u_time = get_utime_from_subcmd(sc_entry);
	if (sc->u_time)
		sc->u_time = get_time_from_system();
	INIT_LIST_HEAD(&sc->q_list);
	INIT_LIST_HEAD(&sc->ce_list);
	mutex_init(&sc->mtx);
	sc->dp_status = kcalloc(BITS_TO_LONGS(cmd->sc_num),
		sizeof(unsigned long), GFP_KERNEL);
	LOG_DEBUG("create subcmd(%p/%p) type(%d) ctx_group(%d)\n",
		sc, sc->entry, sc->type, sc->ctx_group);

	*isc = sc;

	return 0;
}

int apusys_subcmd_delete(struct apusys_subcmd *sc)
{
	if (sc == NULL)
		return -EINVAL;

	DEBUG_TAG;

	list_del(&sc->ce_list);
	sc->parent_cmd = NULL;
	resource_delete_subcmd((void *)sc);
	if (sc->dp_status != NULL) {
		DEBUG_TAG;
		kfree(sc->dp_status);
	}

	kfree(sc);

	return 0;
}

int apusys_cmd_create(struct apusys_ioctl_cmd *ioctl_cmd,
	struct apusys_cmd **icmd)
{
	struct apusys_cmd *cmd;
	uint8_t cmd_version = 0;
	int ret = 0, i = 0, ctx_idx = 0;
	uint32_t sc_num = 0;

	/* check argument */
	if (ioctl_cmd == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	/* check cmd valid */
	if (ioctl_cmd->iova == 0 || ioctl_cmd->uva == 0 ||
		ioctl_cmd->kva == 0 || ioctl_cmd->size == 0) {
		LOG_ERR("invalid cmd(%d/%d/%d/%d)\n",
			ioctl_cmd->iova, ioctl_cmd->uva,
			ioctl_cmd->kva, ioctl_cmd->size);
		return -EINVAL;
	}

	/* print header for debug */
	_print_header((void *)ioctl_cmd->kva);

	/* check version */
	cmd_version = _get_cmdversion((void *)ioctl_cmd->kva);
	if (cmd_version != APUSYS_CMD_VERSION) {
		LOG_ERR("cmd version mismatch(%d/%d)\n",
			cmd_version, APUSYS_CMD_VERSION);
		return -EINVAL;
	}

	/* check subcmd num */
	sc_num = _get_numofsc((void *)ioctl_cmd->kva);
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
	cmd->kva = (void *)ioctl_cmd->kva;
	cmd->size = ioctl_cmd->size;
	cmd->cmd_id = _get_cmdid(cmd->kva);
	cmd->sc_num = sc_num;
	cmd->sc_list_entry = (void *)_get_sc_list_entry(cmd->kva);
	cmd->dp_entry = (void *)_get_dp_entry(cmd->kva);
	cmd->priority = _get_priority(cmd->kva);
	cmd->state = CMD_STATE_READY;
	cmd->target_time = _get_deadline(cmd->kva);
	cmd->estimate_time = _get_targettime(cmd->kva);

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

	LOG_DEBUG("create cmd (0x%llx/%d)(%d/%d/%d)\n",
		cmd->cmd_id, cmd->sc_num, cmd->priority,
		cmd->target_time, cmd->estimate_time);

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
	mutex_unlock(&cmd->mtx);
	kfree(cmd->pc_col.pack_status);
	kfree(cmd->ctx_list);
	kfree(cmd->ctx_ref);
	kfree(cmd);

	return 0;
}
