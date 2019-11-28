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
#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/sched/clock.h>

#include "apusys_cmn.h"
#include "apusys_drv.h"
#include "scheduler.h"
#include "cmd_parser.h"
#include "memory_mgt.h"
#include "resource_mgt.h"
#include "apusys_dbg.h"

//----------------------------------------------
static void _print_cmd_info(const struct apusys_cmd *cmd)
{
	struct apusys_cmd_hdr *hdr = cmd->hdr;

	CLOG_DEBUG("=====================================\n");
	CLOG_DEBUG(" apusys header(0x%llx)\n", (uint64_t)hdr);
	CLOG_DEBUG("-------------------------------------\n");
	CLOG_DEBUG(" cmd magic             = 0x%llx\n", hdr->magic);
	CLOG_DEBUG(" cmd uid               = 0x%llx\n", hdr->uid);
	CLOG_DEBUG(" version               = %d\n", hdr->version);
	CLOG_DEBUG(" priority              = %d\n", hdr->priority);
	CLOG_DEBUG(" hardlimit             = %hu\n", hdr->hard_limit);
	CLOG_DEBUG(" softlimit             = %hu\n", hdr->soft_limit);
	CLOG_DEBUG(" flag                  = 0x%llx\n", hdr->flag_bitmap);
	CLOG_DEBUG(" #subcmd               = %d\n", hdr->num_sc);
	CLOG_DEBUG(" dependency list offset= 0x%x\n", hdr->ofs_scr_list);
	CLOG_DEBUG(" depend cnt list offset= 0x%x\n", hdr->ofs_pdr_cnt_list);
	CLOG_DEBUG(" subcmd list offset    = 0x%x\n", hdr->scofs_list_entry);
	CLOG_DEBUG("-------------------------------------\n");
	CLOG_DEBUG(" cmd id                = 0x%llx\n", cmd->cmd_id);
	CLOG_DEBUG("=====================================\n");
}

void _print_sc_info(const struct apusys_subcmd *sc)
{
	struct apusys_sc_hdr_cmn *hdr = sc->c_hdr;

	CLOG_DEBUG("=====================================\n");
	CLOG_DEBUG(" apusys sc info(0x%llx)\n", (uint64_t)hdr);
	CLOG_DEBUG("-------------------------------------\n");
	CLOG_DEBUG(" type                 = %u\n", hdr->dev_type);
	CLOG_DEBUG(" driver time          = %u\n", hdr->driver_time);
	CLOG_DEBUG(" suggest time         = %u\n", hdr->suggest_time);
	CLOG_DEBUG(" bandwidth            = %u\n", hdr->bandwidth);
	CLOG_DEBUG(" tcm usage            = %d\n", hdr->tcm_usage);
	CLOG_DEBUG(" tcm force            = %d\n", hdr->tcm_force);
	CLOG_DEBUG(" boost value          = %d\n", hdr->boost_val);
	CLOG_DEBUG(" reserved             = %d\n", hdr->reserved);
	CLOG_DEBUG(" memory context       = %u\n", hdr->mem_ctx);
	CLOG_DEBUG(" codebuf info size    = %d\n", hdr->cb_info_size);
	CLOG_DEBUG(" codebuf info offset  = 0x%x\n", hdr->ofs_cb_info);
	CLOG_DEBUG("-------------------------------------\n");
	CLOG_DEBUG(" parent cmd           = 0x%llx\n", (uint64_t)sc->par_cmd);
	CLOG_DEBUG(" successor num        = %d\n", sc->scr_num);
	CLOG_DEBUG(" idx                  = %d\n", sc->idx);
	CLOG_DEBUG(" codebuf info         = 0x%llx\n", (uint64_t)sc->codebuf);
	CLOG_DEBUG(" codebuf fd           = %d\n", sc->codebuf_fd);
	CLOG_DEBUG(" codebuf mem hnd      = 0x%llx\n", sc->codebuf_mem_hnd);
	CLOG_DEBUG(" boost val            = %u/%u\n",
		sc->boost_val,
		hdr->boost_val);
	CLOG_DEBUG("=====================================\n");
}

static unsigned long long _get_dp_entry(const struct apusys_cmd *cmd)
{
	LOG_DEBUG("offset successor list %u\n", cmd->hdr->ofs_scr_list);
	return (uint64_t)((void *)cmd->hdr + cmd->hdr->ofs_scr_list);
}

static unsigned long long _get_dp_cnt_entry(const struct apusys_cmd *cmd)
{
	return (uint64_t)((void *)cmd->hdr + cmd->hdr->ofs_pdr_cnt_list);
}

static int _set_data_to_cmdbuf(struct apusys_subcmd *sc)
{
	if (sc == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	/* execution time */
	sc->c_hdr->driver_time = sc->driver_time;
	/* ip time */
	sc->c_hdr->ip_time = sc->ip_time;
	/* bandwidth */
	sc->c_hdr->bandwidth = sc->bw;
	/* boost val */
	sc->c_hdr->boost_val = sc->boost_val;

	return 0;
}

static int _get_multicore_sched(struct apusys_cmd *cmd)
{
	int multicore_sched = CMD_SCHED_NORMAL;
	int dbg_multi = 0;
	unsigned long long multi0 = 0, multi1 = 0;

	if (cmd == NULL) {
		LOG_WARN("invalid arg\n");
		return CMD_SCHED_NORMAL;
	}

	dbg_multi = dbg_get_multitest();
	switch (dbg_multi) {
	case 1:
		LOG_DEBUG("multicore policy: force single\n");
		multicore_sched = CMD_SCHED_FORCE_SINGLE;
		break;
	case 2:
		LOG_DEBUG("multicore policy: force multi\n");
		multicore_sched = CMD_SCHED_FORCE_MULTI;
		break;
	case 0:
	default:
		LOG_DEBUG("multicore policy: sched decide\n");
		multi0 = cmd->hdr->flag_bitmap &
			(1ULL << CMD_FLAG_BITMAP_MULTI0);
		multi1 = cmd->hdr->flag_bitmap &
			(1ULL << CMD_FLAG_BITMAP_MULTI1);

		if (!multi0 && multi1) /* bit62 = 0 bit63 = 1, multi */
			multicore_sched = CMD_SCHED_FORCE_MULTI;
		else if (multi0 && !multi1) /* bit62 = 1 bit63 = 0, single */
			multicore_sched = CMD_SCHED_FORCE_SINGLE;
		else /* ohter, scheduler decide */
			multicore_sched = CMD_SCHED_NORMAL;
		break;
	}

	return multicore_sched;
}

static int _check_fd_from_codebuf_offset(unsigned int codebuf_offset)
{
	if (codebuf_offset & (1UL << SUBGRAPH_CODEBUF_INFO_BIT_FD))
		return 1;

	return 0;
}

unsigned int get_pack_idx(const struct apusys_subcmd *sc)
{
	struct apusys_sc_hdr_vpu *vpu_hdr = NULL;
	struct apusys_sc_hdr_sample *sample_hdr = NULL;
	unsigned int pack_idx = VALUE_SUBGRAPH_PACK_ID_NONE;

	if (sc == NULL) {
		LOG_ERR("invalid subcmd(%p)\n", sc);
		return VALUE_SUBGRAPH_PACK_ID_NONE;
	}

	if (sc->c_hdr == NULL || sc->d_hdr == NULL) {
		LOG_ERR("invalid header(%p/%p)\n", sc->c_hdr, sc->d_hdr);
		return VALUE_SUBGRAPH_PACK_ID_NONE;
	}

	switch (sc->type) {
	case APUSYS_DEVICE_VPU:
		vpu_hdr = (struct apusys_sc_hdr_vpu *)sc->d_hdr;
		pack_idx = vpu_hdr->pack_idx;
		break;
	case APUSYS_DEVICE_SAMPLE:
		sample_hdr = (struct apusys_sc_hdr_sample *)sc->d_hdr;
		pack_idx = sample_hdr->pack_idx;
		break;
	default:
		LOG_DEBUG("dev type(%d) not support pack id\n", sc->type);
		return VALUE_SUBGRAPH_PACK_ID_NONE;
	}

	return pack_idx;
}

uint32_t get_time_diff_from_system(struct timeval *duration)
{
	struct timeval now;
	uint32_t diff = 0;

	do_gettimeofday(&now);
	diff = (now.tv_sec - duration->tv_sec)*1000000 +
		now.tv_usec - duration->tv_usec;
	duration->tv_sec = now.tv_sec;
	duration->tv_usec = now.tv_usec;

	return diff;
}

int check_sc_ready(const struct apusys_cmd *cmd, int idx)
{
	/* check argument valid */
	if (cmd == NULL) {
		LOG_ERR("invalid argument\n");
		return -1;
	}

	if (idx >= cmd->hdr->num_sc) {
		LOG_ERR("idx too big(%d/%d)\n", idx, cmd->hdr->num_sc);
		return -1;
	}

	if (cmd->sc_list[idx] == NULL)
		return -1;

	if (cmd->pdr_cnt_list[idx] == 0 &&
		cmd->sc_list[idx]->state == CMD_STATE_IDLE) {
		LOG_DEBUG("0x%llx-#%d pdr cnt(%d)\n",
			cmd->cmd_id,
			idx,
			cmd->pdr_cnt_list[idx]);
		return 0;
	}

	LOG_DEBUG("0x%llx-#%d pdr cnt(%d)\n",
		cmd->cmd_id,
		idx,
		cmd->pdr_cnt_list[idx]);
	return -1;
}

int check_cmd_done(struct apusys_cmd *cmd)
{
	if (cmd == NULL)
		return -EINVAL;

	if (bitmap_empty(cmd->sc_status, cmd->hdr->num_sc)) {
		LOG_DEBUG("apusys cmd(0%llx) done\n", cmd->cmd_id);
		cmd->state = CMD_STATE_DONE;
		return 0;
	}

	return -EBUSY;
}

int get_sc_tcm_usage(struct apusys_subcmd *sc)
{
	if (sc == NULL)
		return -EINVAL;

	return sc->c_hdr->tcm_usage;
}

void decrease_pdr_cnt(struct apusys_cmd *cmd, int idx)
{
	/* check argument valid */
	if (cmd == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	if (idx >= cmd->hdr->num_sc) {
		LOG_ERR("idx too big(%d/%d)\n",
			idx,
			cmd->hdr->num_sc);
		return;
	}

	if (cmd->pdr_cnt_list[idx] <= 0) {
		LOG_ERR("0x%llx-#%d pdr(%d) warning\n",
			cmd->cmd_id,
			idx,
			cmd->pdr_cnt_list[idx]);
	}

	cmd->pdr_cnt_list[idx]--;
	LOG_DEBUG("0x%llx-#%d pdr cnt(%d)\n",
		cmd->cmd_id,
		idx,
		cmd->pdr_cnt_list[idx]);
}

uint64_t get_subcmd_by_idx(const struct apusys_cmd *cmd, int idx)
{
	uint32_t sc_hdr_ofs = 0;

	/* check argument valid */
	if (cmd == NULL) {
		LOG_ERR("invalid argument\n");
		return 0;
	}

	if (idx >= cmd->hdr->num_sc) {
		LOG_ERR("idx too big(%d/%d)\n", idx, cmd->hdr->num_sc);
		return 0;
	}

	sc_hdr_ofs = *(uint32_t *)((void *)&cmd->hdr->scofs_list_entry +
		SIZE_SUBGRAPH_SCOFS_ELEMENT * idx);

	return (uint64_t)cmd->hdr + sc_hdr_ofs;
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

int apusys_subcmd_create(int idx, struct apusys_cmd *cmd,
	struct apusys_subcmd **isc, unsigned int scr_ofs)
{
	struct apusys_kmem mem;
	struct apusys_sc_hdr_cmn *sc_hdr;
	struct apusys_subcmd *sc = NULL;
	unsigned int scr_num = 0, i = 0;
	void *sc_entry = NULL;
	int ret = 0;

	if (isc == NULL || cmd == NULL) {
		LOG_ERR("invalid arguments(%p)\n", isc);
		return -EINVAL;
	}

	/* get subcmd info from cmd entry */
	sc_entry = (void *)get_subcmd_by_idx(cmd, idx);
	if (sc_entry == 0) {
		LOG_ERR("get 0x%llx-#%d sc) fail\n",
			cmd->cmd_id,
			i);
		return -EINVAL;
	}
	sc_hdr = (struct apusys_sc_hdr_cmn *)sc_entry;

	/* check device type */
	if (res_get_device_num(sc_hdr->dev_type) <= 0) {
		LOG_ERR("invalid device type(%d)\n",
			sc_hdr->dev_type);
		return -EINVAL;
	}

	/* allocate subcmd struct */
	sc = kzalloc(sizeof(struct apusys_subcmd), GFP_KERNEL);
	if (sc == NULL) {
		LOG_ERR("alloc sc fail\n");
		ret = -ENOMEM;
		goto alloc_sc_fail;
	}
	sc->c_hdr = sc_hdr;
	sc->d_hdr = (void *)sc_hdr + sizeof(struct apusys_sc_hdr_cmn);
	sc->type = sc->c_hdr->dev_type;
	sc->par_cmd = cmd;
	sc->idx = idx;
	sc->ctx_id = VALUE_SUBGRAPH_CTX_ID_NONE;
	sc->state = CMD_STATE_IDLE;
	sc->period = cmd->hdr->soft_limit;
	sc->deadline = jiffies + usecs_to_jiffies(sc->period);
	sc->runtime = sc->c_hdr->driver_time;

	/* check codebuf info size */
	if (sc->c_hdr->cb_info_size == 0) {
		LOG_ERR("codebuf size = 0, error\n");
		kfree(sc);
		return -EINVAL;
	}

	/* check codebuf type, fd or offset */
	LOG_DEBUG("cb offset = 0x%x\n", sc->c_hdr->ofs_cb_info);
	if (_check_fd_from_codebuf_offset(sc->c_hdr->ofs_cb_info)) {
		/* from lib, fd need to map */
		LOG_DEBUG("codebuf is fd, need to map\n");
		sc->codebuf_fd = sc->c_hdr->ofs_cb_info &
			~(1UL << SUBGRAPH_CODEBUF_INFO_BIT_FD);
		memset(&mem, 0, sizeof(struct apusys_kmem));
		mem.size = sc->c_hdr->cb_info_size;
		mem.fd = sc->codebuf_fd;

		if (apusys_mem_map_kva(&mem)) {
			LOG_ERR("map sc fail\n");
			ret = -ENOMEM;
			goto map_kva_fail;
		}
		if (apusys_mem_map_iova(&mem)) {
			if (apusys_mem_unmap_kva(&mem))
				LOG_ERR("map sc io fail\n");
			ret = -ENOMEM;
			goto map_iova_fail;
		}

		sc->codebuf_iosize = (int)mem.size;
		sc->codebuf = (void *)mem.kva;
		sc->codebuf_mem_hnd = mem.khandle;
		LOG_DEBUG("map sc codebuf from fd(%d/%p/%u)\n",
			sc->codebuf_fd,
			sc->codebuf,
			sc->c_hdr->cb_info_size);

		/* check offset and size */
		if (sc->c_hdr->cb_info_size > sc->codebuf_iosize) {
			LOG_ERR("check sc cb size(%u/%u) fail",
				sc->c_hdr->cb_info_size,
				sc->codebuf_iosize);
			ret = -EINVAL;
			goto check_size_fail;
		}
	} else {
		/* from neuron, kva need offset */
		sc->codebuf_fd = -1;
		sc->codebuf = (void *)((void *)cmd->hdr +
			sc->c_hdr->ofs_cb_info);
		LOG_DEBUG("calc sc codebuf from offset(%d/%p/%u)\n",
			sc->codebuf_fd,
			sc->codebuf,
			sc->c_hdr->cb_info_size);
	}

	/* check boost value */
	if (sc->c_hdr->boost_val > 100) {
		LOG_DEBUG("boost_val over 100, set\n");
		sc->boost_val = 100;
	} else {
		sc->boost_val = sc->c_hdr->boost_val;
	};

	/* check mem context */
	if (sc->c_hdr->mem_ctx != VALUE_SUBGRAPH_CTX_ID_NONE &&
		sc->c_hdr->mem_ctx > cmd->hdr->num_sc) {
		LOG_ERR("invalid ctx group(%d/%d)\n",
			sc->c_hdr->mem_ctx,
			cmd->hdr->num_sc);
		ret = -EINVAL;
		goto check_size_fail;
	}

	LOG_DEBUG("successor offset = %u\n", scr_ofs);
	/* check successor */
	scr_num = *(TYPE_SUBGRAPH_SCOFS_ELEMENT *)(cmd->dp_entry + scr_ofs);
	scr_ofs += SIZE_CMD_SUCCESSOR_ELEMENT;
	if (scr_num >= cmd->hdr->num_sc) {
		LOG_ERR("scr num too much(%u/%u)\n", scr_num, cmd->hdr->num_sc);
		ret = -EINVAL;
		goto check_size_fail;
	}
	sc->scr_list = kcalloc(scr_num, SIZE_CMD_SUCCESSOR_ELEMENT, GFP_KERNEL);
	if (sc->scr_list == NULL) {
		LOG_ERR("alloc scr list fail\n");
		ret = -EINVAL;
		goto check_size_fail;
	}
	sc->scr_num = scr_num;
	for (i = 0; i < scr_num; i++) {
		sc->scr_list[i] = *(TYPE_SUBGRAPH_SCOFS_ELEMENT *)
			(cmd->dp_entry + scr_ofs);
		scr_ofs += SIZE_CMD_SUCCESSOR_ELEMENT;
		LOG_DEBUG("0x%llx-#%d sc: scr(%d)\n",
			cmd->cmd_id,
			sc->idx,
			sc->scr_list[i]);
	}

	INIT_LIST_HEAD(&sc->q_list);
	mutex_init(&sc->mtx);

	if (res_task_inc(sc)) {
		LOG_WARN("inc 0x%llx-#%d sc softlimit(%u) fail",
			cmd->cmd_id,
			sc->idx,
			cmd->hdr->soft_limit
			);
	}

	_print_sc_info(sc);
	*isc = sc;

	/* Calc system load and boost */
	deadline_task_start(sc);

	mutex_lock(&cmd->sc_mtx);
	cmd->sc_list[sc->idx] = sc;
	mutex_unlock(&cmd->sc_mtx);

	return 0;

check_size_fail:
	if (!sc->codebuf_fd) {
		if (apusys_mem_unmap_iova(&mem))
			LOG_ERR("map sc iova fail\n");
	}
map_iova_fail:
	if (!sc->codebuf_fd) {
		if (apusys_mem_unmap_kva(&mem))
			LOG_ERR("unmap sc kva fail\n");
	}
map_kva_fail:
	kfree(sc);
alloc_sc_fail:
	return ret;
}

int apusys_subcmd_delete(struct apusys_subcmd *sc)
{
	struct apusys_kmem mem;

	if (sc == NULL)
		return -EINVAL;

	/* Calc system load and boost */
	deadline_task_end(sc);

	/* write time back to cmdbuf */
	LOG_DEBUG("0x%llx-#%d sc: time(%u) bw(%u) st(%d)\n",
		sc->par_cmd->cmd_id,
		sc->idx,
		sc->c_hdr->driver_time,
		sc->c_hdr->bandwidth,
		sc->state);
	_set_data_to_cmdbuf(sc);

	if (sc->codebuf_fd >= 0) {
		memset(&mem, 0, sizeof(struct apusys_kmem));
		mem.kva = (unsigned long long)sc->codebuf;
		mem.size = sc->c_hdr->cb_info_size;
		mem.fd = sc->codebuf_fd;
		mem.khandle = sc->codebuf_mem_hnd;

		if (apusys_mem_unmap_iova(&mem)) {
			LOG_ERR("unmap codebuf iova fd(%d) fail\n",
				sc->codebuf_fd);
		}
		if (apusys_mem_unmap_kva(&mem)) {
			LOG_ERR("unmap codebuf kva fd(%d) fail\n",
				sc->codebuf_fd);
		}
	}

	DEBUG_TAG;
	if (sc->state < CMD_STATE_RUN) {
		LOG_WARN("0x%llx-#%d sc not done, st(%d)\n",
			sc->par_cmd->cmd_id,
			sc->idx,
			sc->state);
		delete_subcmd_lock(sc);
	}
	DEBUG_TAG;

	if (res_task_dec(sc)) {
		LOG_WARN("dec 0x%llx-#%d sc softlimit(%u) fail",
			sc->par_cmd->cmd_id,
			sc->idx,
			sc->par_cmd->hdr->soft_limit
			);
	}

	sc->par_cmd->sc_list[sc->idx] = NULL;
	bitmap_clear(sc->par_cmd->sc_status, sc->idx, 1);
	sc->par_cmd = NULL;
	kfree(sc->scr_list);
	kfree(sc);
	DEBUG_TAG;

	return 0;
}

int apusys_cmd_create(int mem_fd, uint32_t offset,
	struct apusys_cmd **icmd, struct apusys_user *u)
{
	struct apusys_cmd *cmd = NULL;
	struct apusys_cmd_hdr *cmd_hdr = NULL;
	struct apusys_sc_hdr_cmn *subcmd_hdr = NULL;
	struct apusys_kmem mem;
	int ret = 0, i = 0;

	/* map kva */
	memset(&mem, 0, sizeof(struct apusys_kmem));
	mem.fd = mem_fd;
	if (apusys_mem_map_kva(&mem)) {
		LOG_ERR("map cmdbuf kva from fd(%d)fail\n",
			mem_fd);
		return -ENOMEM;
	}

	if (apusys_mem_map_iova(&mem)) {
		LOG_ERR("map cmdbuf iova from fd(%d)fail\n",
			mem_fd);
		if (apusys_mem_unmap_kva(&mem)) {
			LOG_ERR("unmap cmdbuf kva fd(%d) fail\n",
				mem_fd);
		}
		return -ENOMEM;
	}

	/* verify offset and size */
	if ((offset + sizeof(struct apusys_cmd_hdr) > mem.iova_size)) {
		LOG_ERR("check cmdbuf offset(%d/%u) fail",
			offset, mem.iova_size);
		ret = -EINVAL;
		goto alloc_ce_fail;
	}
	cmd_hdr = (struct apusys_cmd_hdr *)(mem.kva + offset);

	/* check parameters */
	if (cmd_hdr->magic != APUSYS_MAGIC_NUMBER) {
		LOG_ERR("cmd magic mismatch(0x%llx)\n",
			cmd_hdr->magic);
		return -EINVAL;
	}

	/* check version */
	if (cmd_hdr->version != APUSYS_CMD_VERSION) {
		LOG_ERR("cmd version mismatch(%d/%d)\n",
			cmd_hdr->version, APUSYS_CMD_VERSION);
		return -EINVAL;
	}

	/* check subcmd num */
	if (cmd_hdr->num_sc == 0) {
		LOG_ERR("subcmd num(%d), nothing to do\n", cmd_hdr->num_sc);
		return -EINVAL;
	}

	/* allocate apusys cmd */
	cmd = kzalloc(sizeof(struct apusys_cmd), GFP_KERNEL);
	if (cmd == NULL) {
		LOG_ERR("alloc apusys cmd fail\n");
		ret = -ENOMEM;
		goto alloc_ce_fail;
	}

	/* assign value */
	cmd->pid = u->open_pid;
	cmd->tgid = u->open_tgid;
	cmd->hdr = cmd_hdr;
	cmd->mem_fd = mem_fd;
	cmd->mem_hnd = mem.khandle;
	cmd->cmd_id = (uint64_t)(cmd);
	cmd->power_save = (cmd->hdr->flag_bitmap &
		1UL << CMD_FLAG_BITMAP_POWERSAVE) ? 1 : 0;
	cmd->multicore_sched = _get_multicore_sched(cmd);
	cmd->dp_entry = (void *)_get_dp_entry(cmd);
	cmd->dp_cnt_entry = (void *)_get_dp_cnt_entry(cmd);
	cmd->state = CMD_STATE_READY;

	_print_cmd_info(cmd);

	/* allocate subcmd bitmap for tracing status */
	cmd->sc_status = kcalloc(BITS_TO_LONGS(cmd->hdr->num_sc),
	sizeof(unsigned long), GFP_KERNEL);
	if (cmd->sc_status == NULL) {
		LOG_ERR("alloc bitmap for subcmd status fail\n");
		ret = -ENOMEM;
		goto alloc_sc_status_fail;
	}

	cmd->pdr_cnt_list = kcalloc(cmd->hdr->num_sc,
		sizeof(int), GFP_KERNEL);
	if (cmd->pdr_cnt_list == NULL) {
		LOG_ERR("alloc pdr cnt list fail\n");
		ret = -ENOMEM;
		goto alloc_pdr_fail;
	}

	/* allocate ctx ref and list */
	cmd->ctx_ref = kcalloc(cmd->hdr->num_sc, sizeof(uint32_t), GFP_KERNEL);
	if (cmd->ctx_ref == NULL) {
		LOG_ERR("alloc ctx ref count for ctx fail\n");
		ret = -ENOMEM;
		goto alloc_sc_ctx_fail;
	}
	cmd->ctx_list = kcalloc(cmd->hdr->num_sc, sizeof(uint32_t), GFP_KERNEL);
	if (cmd->ctx_list == NULL) {
		LOG_ERR("alloc ctx list count for ctx fail\n");
		ret = -ENOMEM;
		goto alloc_sc_ctx_list_fail;
	}
	cmd->pc_col.pack_status = kzalloc(sizeof(unsigned long) *
		cmd->hdr->num_sc, GFP_KERNEL);
	if (cmd->pc_col.pack_status == NULL) {
		LOG_ERR("alloc pack status fail\n");
		ret = -ENOMEM;
		goto alloc_pc_status_fail;
	}

	/* set subcmd status */
	bitmap_set(cmd->sc_status, 0, cmd->hdr->num_sc);

	/* set memory ctx ref count & pdr cnt*/
	for (i = 0; i < cmd->hdr->num_sc; i++) {
		/* TODO: check scr list valid */
		subcmd_hdr = (struct apusys_sc_hdr_cmn *)
			get_subcmd_by_idx(cmd, i);
		if (subcmd_hdr->mem_ctx == VALUE_SUBGRAPH_CTX_ID_NONE)
			continue;
		if (subcmd_hdr->mem_ctx >= cmd->hdr->num_sc) {
			LOG_ERR("cmd(0x%llx) #%d subcmd's ctx_id(%d) invalid\n",
				cmd->cmd_id, i, subcmd_hdr->mem_ctx);
			goto count_ctx_fail;
		}
		cmd->ctx_ref[subcmd_hdr->mem_ctx]++;
		/* set pdr cnt */
		cmd->pdr_cnt_list[i] = *(TYPE_CMD_PREDECCESSOR_CMNT_ELEMENT *)
			(cmd->dp_cnt_entry +
			(i * SIZE_CMD_PREDECCESSOR_CMNT_ELEMENT));
	}

	memset(cmd->ctx_list, VALUE_SUBGRAPH_CTX_ID_NONE,
		sizeof(uint32_t) * cmd->hdr->num_sc);

	cmd->sc_list = kcalloc(cmd->hdr->num_sc,
		sizeof(struct apusys_subcmd *), GFP_KERNEL);
	if (cmd->sc_list == NULL) {
		LOG_ERR("alloc sc list fail\n");
		ret = -ENOMEM;
		goto alloc_sc_list_fail;
	}

	mutex_init(&cmd->sc_mtx);
	mutex_init(&cmd->mtx);
	INIT_LIST_HEAD(&cmd->pc_col.sc_list);
	INIT_LIST_HEAD(&cmd->u_list);
	init_completion(&cmd->comp);

	*icmd = cmd;

#define APUSYS_CMDINFO_PRINT "create cmd(0x%llx/0x%llx)"\
	" sc_num(%u) prority(%u) deadline(%u/%u/%u)"\
	" mp(%d) flag(0x%llx)\n"
	LOG_INFO(APUSYS_CMDINFO_PRINT,
		cmd->hdr->uid, cmd->cmd_id,
		cmd->hdr->num_sc, cmd->hdr->priority,
		cmd->hdr->hard_limit, cmd->hdr->soft_limit,
		cmd->power_save,
		cmd->multicore_sched, cmd->hdr->flag_bitmap);
#undef APUSYS_CMDINFO_PRINT
	return ret;

alloc_sc_list_fail:
count_ctx_fail:
	kfree(cmd->pc_col.pack_status);
alloc_pc_status_fail:
	kfree(cmd->ctx_list);
alloc_sc_ctx_list_fail:
	kfree(cmd->ctx_ref);
alloc_sc_ctx_fail:
	kfree(cmd->pdr_cnt_list);
alloc_pdr_fail:
	kfree(cmd->sc_status);
alloc_sc_status_fail:
	kfree(cmd);
alloc_ce_fail:
	if (apusys_mem_unmap_iova(&mem)) {
		LOG_ERR("unmap cmdbuf iova fd(%d) fail\n",
			mem_fd);
	}
	if (apusys_mem_unmap_kva(&mem)) {
		LOG_ERR("unmap cmdbuf kva fd(%d) fail\n",
			mem_fd);
	}

	return ret;
}

int apusys_cmd_delete(struct apusys_cmd *cmd)
{
	struct apusys_kmem mem;

	if (cmd == NULL)
		return -EINVAL;

	if (apusys_sched_del_cmd(cmd)) {
		LOG_ERR("cmd is busy\n");
		return -EBUSY;
	}

	mutex_lock(&cmd->mtx);

	/* unmap iova/kva */
	memset(&mem, 0, sizeof(struct apusys_kmem));
	mem.fd = cmd->mem_fd;
	mem.khandle = cmd->mem_hnd;
	if (apusys_mem_unmap_iova(&mem)) {
		LOG_ERR("unmap cmdbuf iova fd(%d) fail\n",
			cmd->mem_fd);
	}
	if (apusys_mem_unmap_kva(&mem)) {
		LOG_ERR("unmap cmdbuf kva from fd(%d)fail\n",
			cmd->mem_fd);
	}

	mutex_unlock(&cmd->mtx);
	kfree(cmd->sc_list);
	kfree(cmd->pc_col.pack_status);
	kfree(cmd->ctx_list);
	kfree(cmd->ctx_ref);
	kfree(cmd->pdr_cnt_list);
	kfree(cmd->sc_status);
	kfree(cmd);

	return 0;
}
