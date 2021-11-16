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

#include "mdw_cmn.h"
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

	mdw_cmd_debug("=====================================\n");
	mdw_cmd_debug(" apusys header(0x%llx)\n", (uint64_t)hdr);
	mdw_cmd_debug("-------------------------------------\n");
	mdw_cmd_debug(" cmd magic             = 0x%llx\n", hdr->magic);
	mdw_cmd_debug(" cmd uid               = 0x%llx\n", hdr->uid);
	mdw_cmd_debug(" version               = %d\n", hdr->version);
	mdw_cmd_debug(" priority              = %d\n", hdr->priority);
	mdw_cmd_debug(" hardlimit             = %hu\n", hdr->hard_limit);
	mdw_cmd_debug(" softlimit             = %hu\n", hdr->soft_limit);
	mdw_cmd_debug(" flag                  = 0x%llx\n", hdr->flag_bitmap);
	mdw_cmd_debug(" #subcmd               = %d\n", hdr->num_sc);
	mdw_cmd_debug(" dependency list offset= 0x%x\n", hdr->ofs_scr_list);
	mdw_cmd_debug(" depend cnt list offset= 0x%x\n", hdr->ofs_pdr_cnt_list);
	mdw_cmd_debug(" subcmd list offset    = 0x%x\n", hdr->scofs_list_entry);
	mdw_cmd_debug("-------------------------------------\n");
	mdw_cmd_debug(" cmd id                = 0x%llx\n", cmd->cmd_id);
	mdw_cmd_debug("=====================================\n");
}

static void _print_sc_info(const struct apusys_subcmd *sc)
{
	struct apusys_sc_hdr_cmn *hdr = &sc->c_hdr->cmn;

	mdw_cmd_debug("=====================================\n");
	mdw_cmd_debug(" apusys sc info(0x%llx)\n", (uint64_t)hdr);
	mdw_cmd_debug("-------------------------------------\n");
	mdw_cmd_debug(" type                 = %u\n", hdr->dev_type);
	mdw_cmd_debug(" driver time          = %u\n", hdr->driver_time);
	mdw_cmd_debug(" ip time              = %u\n", hdr->ip_time);
	mdw_cmd_debug(" suggest time         = %u\n", hdr->suggest_time);
	mdw_cmd_debug(" bandwidth            = %u\n", hdr->bandwidth);
	mdw_cmd_debug(" tcm usage            = %d\n", hdr->tcm_usage);
	mdw_cmd_debug(" tcm force            = %d\n", hdr->tcm_force);
	mdw_cmd_debug(" boost value          = %d\n", hdr->boost_val);
	mdw_cmd_debug(" reserved             = %d\n", hdr->reserved);
	mdw_cmd_debug(" memory context       = %u\n", hdr->mem_ctx);
	mdw_cmd_debug(" codebuf info size    = %d\n", hdr->cb_info_size);
	mdw_cmd_debug(" codebuf info offset  = 0x%x\n", hdr->ofs_cb_info);
	mdw_cmd_debug("-------------------------------------\n");
	mdw_cmd_debug(" parent cmd           = 0x%llx\n",
		(uint64_t)sc->par_cmd);
	mdw_cmd_debug(" successor num        = %d\n", sc->scr_num);
	mdw_cmd_debug(" idx                  = %d\n", sc->idx);
	mdw_cmd_debug(" codebuf info         = 0x%llx\n",
		(uint64_t)sc->codebuf);
	mdw_cmd_debug(" codebuf fd           = %d\n", sc->codebuf_fd);
	mdw_cmd_debug(" codebuf mem hnd      = 0x%llx\n", sc->codebuf_mem_hnd);
	mdw_cmd_debug(" boost val            = %u/%u\n",
		sc->boost_val,
		hdr->boost_val);
	mdw_cmd_debug("=====================================\n");
}

static unsigned long long _get_dp_entry(const struct apusys_cmd *cmd)
{
	mdw_flw_debug("offset successor list %u\n", cmd->hdr->ofs_scr_list);
	return (uint64_t)((void *)cmd->u_hdr + cmd->hdr->ofs_scr_list);
}

static unsigned long long _get_dp_cnt_entry(const struct apusys_cmd *cmd)
{
	return (uint64_t)((void *)cmd->u_hdr + cmd->hdr->ofs_pdr_cnt_list);
}

static int _set_data_to_cmdbuf(struct apusys_subcmd *sc)
{
	if (sc == NULL) {
		mdw_drv_err("invalid argument\n");
		return -EINVAL;
	}

	/* execution time */
	sc->u_hdr->driver_time = sc->driver_time;
	/* ip time */
	sc->u_hdr->ip_time = sc->ip_time;
	/* bandwidth */
	sc->u_hdr->bandwidth = sc->bw;
	/* boost val */
	sc->u_hdr->boost_val = sc->boost_val;

	return 0;
}

static int _get_multicore_sched(struct apusys_cmd *cmd)
{
	int multicore_sched = CMD_SCHED_NORMAL;
	int dbg_multi = 0;
	unsigned long long multi0 = 0, multi1 = 0;

	if (cmd == NULL) {
		mdw_drv_warn("invalid arg\n");
		return CMD_SCHED_NORMAL;
	}

	dbg_multi = dbg_get_prop(DBG_PROP_MULTICORE);
	switch (dbg_multi) {
	case 1:
		mdw_drv_debug("multicore policy: force single\n");
		multicore_sched = CMD_SCHED_FORCE_SINGLE;
		break;
	case 2:
		mdw_drv_debug("multicore policy: force multi\n");
		multicore_sched = CMD_SCHED_FORCE_MULTI;
		break;
	case 0:
	default:
		mdw_drv_debug("multicore policy: sched decide\n");
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

static int _check_apusys_sc_hdr(struct apusys_sc_hdr *hdr)
{
	if (hdr == NULL) {
		mdw_drv_err("miss hdr\n");
		return -EINVAL;
	}

	if (hdr->cmn.dev_type > APUSYS_DEVICE_MAX) {
		mdw_drv_err("sc device type(%d) error\n", hdr->cmn.dev_type);
		return -EINVAL;
	}

	if (hdr->cmn.tcm_force > 1) {
		mdw_drv_err("sc tcm force(%d) error\n", hdr->cmn.tcm_force);
		return -EINVAL;
	}

	if (hdr->cmn.ofs_cb_info == 0) {
		mdw_drv_err("invalid cb ofs(%u)\n", hdr->cmn.ofs_cb_info);
		return -EINVAL;
	}

	if (hdr->cmn.cb_info_size == 0) {
		mdw_drv_err("invalid cb size(%u)\n", hdr->cmn.cb_info_size);
		return -EINVAL;
	}

	return 0;
}

static int _copy_apusys_sc_hdr(struct apusys_sc_hdr *hdr,
	struct apusys_sc_hdr_cmn *u_hdr)
{
	void *d_hdr = NULL;

	if (hdr == NULL || u_hdr == NULL)
		return -EINVAL;

	memcpy(hdr, u_hdr, sizeof(struct apusys_sc_hdr_cmn));
	if (_check_apusys_sc_hdr(hdr)) {
		mdw_drv_err("check sc hdr fail\n");
		return -EINVAL;
	}

	d_hdr = (void *)u_hdr + sizeof(struct apusys_sc_hdr_cmn);

	switch (hdr->cmn.dev_type) {
	case APUSYS_DEVICE_SAMPLE:
		memcpy(&hdr->sample, d_hdr,
			sizeof(struct apusys_sc_hdr_sample));
		break;
	case APUSYS_DEVICE_MDLA:
		memcpy(&hdr->mdla, d_hdr, sizeof(struct apusys_sc_hdr_mdla));
		break;
	case APUSYS_DEVICE_VPU:
		memcpy(&hdr->vpu, d_hdr, sizeof(struct apusys_sc_hdr_vpu));
		break;
	case APUSYS_DEVICE_EDMA:
		break;
	default:
		mdw_drv_err("wrong dev type(%d)\n", hdr->cmn.dev_type);
		return -EINVAL;
	}

	return 0;
}

static int _check_apusys_hdr(struct apusys_cmd_hdr *hdr)
{
	if (hdr == NULL) {
		mdw_drv_err("miss hdr\n");
		return -EINVAL;
	}

	/* check parameters */
	if (hdr->magic != APUSYS_MAGIC_NUMBER) {
		mdw_drv_err("cmd magic mismatch(0x%llx)\n",
			hdr->magic);
		return -EINVAL;
	}

	/* check version */
	if (hdr->version != APUSYS_CMD_VERSION) {
		mdw_drv_err("cmd version mismatch(%d/%d)\n",
			hdr->version, APUSYS_CMD_VERSION);
		return -EINVAL;
	}

	/* check subcmd num */
	if (hdr->num_sc == 0) {
		mdw_drv_err("sc num(%d), nothing to do\n", hdr->num_sc);
		return -EINVAL;
	}

	/* check priority */
	if (hdr->priority > APUSYS_PRIORITY_MAX) {
		mdw_drv_err("cmd priority(%d/%d) mismatch\n",
			hdr->priority,
			APUSYS_PRIORITY_MAX);
		return -EINVAL;
	}

	return 0;
}

static unsigned int get_pack_idx(const struct apusys_subcmd *sc)
{
	unsigned int pack_idx = VALUE_SUBGRAPH_PACK_ID_NONE;

	if (sc == NULL) {
		mdw_drv_err("invalid subcmd(%p)\n", sc);
		return VALUE_SUBGRAPH_PACK_ID_NONE;
	}

	if (sc->c_hdr == NULL) {
		mdw_drv_err("invalid header(%p)\n", sc->c_hdr);
		return VALUE_SUBGRAPH_PACK_ID_NONE;
	}

	switch (sc->type) {
	case APUSYS_DEVICE_VPU:
	case APUSYS_DEVICE_VPU + APUSYS_DEVICE_RT:
		pack_idx = sc->c_hdr->vpu.pack_idx;
		break;
	case APUSYS_DEVICE_SAMPLE:
		pack_idx = sc->c_hdr->sample.pack_idx;
		break;
	default:
		mdw_flw_debug("dev type(%d) not support pack id\n", sc->type);
		return VALUE_SUBGRAPH_PACK_ID_NONE;
	}
	mdw_flw_debug("packid(%d)\n", pack_idx);

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
	int ret = -1;

	/* check argument valid */
	if (cmd == NULL) {
		mdw_drv_err("invalid argument\n");
		return -1;
	}

	if (idx >= cmd->hdr->num_sc) {
		mdw_drv_err("idx too big(%d/%d)\n", idx, cmd->hdr->num_sc);
		return -1;
	}

	if (cmd->sc_list[idx] == NULL)
		return -1;

	if (cmd->pdr_cnt_list[idx] == 0 &&
		cmd->sc_list[idx]->state == CMD_STATE_IDLE) {
		ret = 0;
	}

	mdw_flw_debug("0x%llx-#%d pdr cnt(%d)\n",
		cmd->cmd_id,
		idx,
		cmd->pdr_cnt_list[idx]);

	return ret;
}

int check_cmd_done(struct apusys_cmd *cmd)
{
	if (cmd == NULL)
		return -EINVAL;

	if (bitmap_empty(cmd->sc_status, cmd->hdr->num_sc)) {
		mdw_flw_debug("apusys cmd(0%llx) done\n", cmd->cmd_id);
		cmd->state = CMD_STATE_DONE;
		return 0;
	}

	return -EBUSY;
}

int get_sc_tcm_usage(struct apusys_subcmd *sc)
{
	if (sc == NULL)
		return -EINVAL;

	return sc->c_hdr->cmn.tcm_usage;
}

void decrease_pdr_cnt(struct apusys_cmd *cmd, int idx)
{
	/* check argument valid */
	if (cmd == NULL) {
		mdw_drv_err("invalid argument\n");
		return;
	}

	if (idx >= cmd->hdr->num_sc) {
		mdw_drv_err("idx too big(%d/%d)\n",
			idx,
			cmd->hdr->num_sc);
		return;
	}

	if (cmd->pdr_cnt_list[idx] <= 0) {
		mdw_drv_err("0x%llx-#%d pdr(%d) warning\n",
			cmd->cmd_id,
			idx,
			cmd->pdr_cnt_list[idx]);
	}

	cmd->pdr_cnt_list[idx]--;
	mdw_flw_debug("0x%llx-#%d pdr cnt(%d)\n",
		cmd->cmd_id,
		idx,
		cmd->pdr_cnt_list[idx]);
}

uint64_t get_subcmd_by_idx(const struct apusys_cmd *cmd, int idx)
{
	uint32_t sc_hdr_ofs = 0;

	/* check argument valid */
	if (cmd == NULL) {
		mdw_drv_err("invalid argument\n");
		return 0;
	}

	if (idx >= cmd->hdr->num_sc) {
		mdw_drv_err("idx too big(%d/%d)\n", idx, cmd->hdr->num_sc);
		return 0;
	}

	sc_hdr_ofs = *(uint32_t *)((void *)&cmd->u_hdr->scofs_list_entry +
		SIZE_SUBGRAPH_SCOFS_ELEMENT * idx);

	mdw_flw_debug("0x%llx/0x%x\n", (uint64_t)cmd->u_hdr, sc_hdr_ofs);

	return (uint64_t)cmd->u_hdr + sc_hdr_ofs;
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
		mdw_drv_err("invalid arguments(%p)\n", isc);
		return -EINVAL;
	}

	/* check idx valid */
	if (idx >= cmd->hdr->num_sc) {
		mdw_drv_err("idx(%d) larget than total sc num(%d)\n",
			idx, cmd->hdr->num_sc);
		return -EINVAL;
	}

	/* get subcmd info from cmd entry */
	sc_entry = (void *)get_subcmd_by_idx(cmd, idx);
	if (sc_entry == 0) {
		mdw_drv_err("get 0x%llx-#%d sc) fail\n",
			cmd->cmd_id,
			i);
		return -EINVAL;
	}
	sc_hdr = (struct apusys_sc_hdr_cmn *)sc_entry;

	/* check device type */
	if (res_get_device_num(sc_hdr->dev_type) <= 0) {
		mdw_drv_err("invalid device type(%d)\n",
			sc_hdr->dev_type);
		return -EINVAL;
	}

	/* allocate subcmd struct */
	sc = kzalloc(sizeof(struct apusys_subcmd), GFP_KERNEL);
	if (sc == NULL) {
		ret = -ENOMEM;
		goto alloc_sc_fail;
	}

	sc->c_hdr = kzalloc(sizeof(struct apusys_sc_hdr), GFP_KERNEL);
	if (sc->c_hdr == NULL) {
		ret = -ENOMEM;
		goto alloc_sc_hdr_fail;
	}
	if (_copy_apusys_sc_hdr(sc->c_hdr, sc_hdr)) {
		mdw_drv_err("copy to apusys sc hdr fail\n");
		ret = -EINVAL;
		goto map_kva_fail;
	}

	sc->u_hdr = sc_hdr;
	//sc->d_hdr = (void *)sc_hdr + sizeof(struct apusys_sc_hdr_cmn);
	sc->type = sc->c_hdr->cmn.dev_type;
	sc->par_cmd = cmd;
	sc->idx = idx;
	sc->ctx_id = VALUE_SUBGRAPH_CTX_ID_NONE;
	sc->state = CMD_STATE_IDLE;
	sc->period = cmd->hdr->soft_limit * 1000;
	sc->deadline = jiffies + usecs_to_jiffies(sc->period);
	sc->runtime = sc->c_hdr->cmn.driver_time;

	if (sc->period && preemption_support) {
		if (res_get_device_num(sc->type + APUSYS_DEVICE_RT) != 0)
			sc->type += APUSYS_DEVICE_RT;
		else
			sc->period = 0; /* No RT device avail, disable period */
	} else
		sc->period = 0; /* No RT device avail, disable period */

	/* check codebuf type, fd or offset */
	mdw_flw_debug("cb offset = 0x%x\n", sc->c_hdr->cmn.ofs_cb_info);
	if (_check_fd_from_codebuf_offset(sc->c_hdr->cmn.ofs_cb_info)) {
		/* from lib, fd need to map */
		mdw_flw_debug("codebuf is fd, need to map\n");
		sc->codebuf_fd = sc->c_hdr->cmn.ofs_cb_info &
			~(1UL << SUBGRAPH_CODEBUF_INFO_BIT_FD);
		memset(&mem, 0, sizeof(struct apusys_kmem));
		mem.size = sc->c_hdr->cmn.cb_info_size;
		mem.fd = sc->codebuf_fd;

		if (apusys_mem_map_kva(&mem)) {
			mdw_drv_err("map sc fail\n");
			ret = -ENOMEM;
			goto map_kva_fail;
		}
		if (apusys_mem_map_iova(&mem)) {
			if (apusys_mem_unmap_kva(&mem))
				mdw_drv_err("map sc io fail\n");
			ret = -ENOMEM;
			goto map_iova_fail;
		}

		sc->codebuf_iosize = (int)mem.size;
		sc->codebuf = (void *)mem.kva;
		sc->codebuf_mem_hnd = mem.khandle;
		mdw_flw_debug("map sc codebuf from fd(%d/%p/%u)\n",
			sc->codebuf_fd,
			sc->codebuf,
			sc->c_hdr->cmn.cb_info_size);

		/* check offset and size */
		if (sc->c_hdr->cmn.cb_info_size > sc->codebuf_iosize) {
			mdw_drv_err("check sc cb size(%u/%u) fail",
				sc->c_hdr->cmn.cb_info_size,
				sc->codebuf_iosize);
			ret = -EINVAL;
			goto check_size_fail;
		}
	} else {
		/* from neuron, kva need offset */
		sc->codebuf_fd = -1;
		sc->codebuf = (void *)((void *)cmd->u_hdr +
			sc->c_hdr->cmn.ofs_cb_info);
		mdw_flw_debug("calc sc codebuf from offset(%d/%p/%u)\n",
			sc->codebuf_fd,
			sc->codebuf,
			sc->c_hdr->cmn.cb_info_size);
	}

	/* check boost value */
	if (sc->c_hdr->cmn.boost_val > 100) {
		mdw_flw_debug("boost_val over 100, set\n");
		sc->boost_val = 100;
	} else {
		sc->boost_val = sc->c_hdr->cmn.boost_val;
	};

	/* check mem context */
	if (sc->c_hdr->cmn.mem_ctx != VALUE_SUBGRAPH_CTX_ID_NONE &&
		sc->c_hdr->cmn.mem_ctx > cmd->hdr->num_sc) {
		mdw_drv_err("invalid ctx group(%d/%d)\n",
			sc->c_hdr->cmn.mem_ctx,
			cmd->hdr->num_sc);
		ret = -EINVAL;
		goto check_size_fail;
	}

	mdw_flw_debug("successor offset = %u\n", scr_ofs);
	/* check successor */
	scr_num = *(TYPE_SUBGRAPH_SCOFS_ELEMENT *)(cmd->dp_entry + scr_ofs);
	scr_ofs += SIZE_CMD_SUCCESSOR_ELEMENT;
	if (scr_num >= cmd->hdr->num_sc) {
		mdw_drv_err("scr num too much(%u/%u)\n",
			scr_num, cmd->hdr->num_sc);
		ret = -EINVAL;
		goto check_size_fail;
	}
	sc->scr_list = kcalloc(scr_num, SIZE_CMD_SUCCESSOR_ELEMENT, GFP_KERNEL);
	if (sc->scr_list == NULL) {
		ret = -EINVAL;
		goto check_size_fail;
	}
	sc->scr_num = scr_num;
	for (i = 0; i < scr_num; i++) {
		sc->scr_list[i] = *(TYPE_SUBGRAPH_SCOFS_ELEMENT *)
			(cmd->dp_entry + scr_ofs);
		scr_ofs += SIZE_CMD_SUCCESSOR_ELEMENT;
		if (sc->scr_list[i] > cmd->hdr->num_sc) {
			mdw_drv_err("scr list(%d)(%u/%u) check fail", i,
				sc->scr_list[i], cmd->hdr->num_sc);
			ret = -EINVAL;
			goto check_scr_fail;
		}
	}

	INIT_LIST_HEAD(&sc->q_list);
	mutex_init(&sc->mtx);

	if (res_task_inc(sc)) {
		mdw_drv_warn("inc 0x%llx-#%d sc softlimit(%llu) fail",
			cmd->cmd_id,
			sc->idx,
			sc->period);
	}
	sc->pack_id = get_pack_idx(sc);

	_print_sc_info(sc);
	*isc = sc;

	/* Calc system load and boost */
	deadline_task_start(sc);

	mutex_lock(&cmd->mtx);
	cmd->sc_list[sc->idx] = sc;
	mutex_unlock(&cmd->mtx);

	return 0;

check_scr_fail:
	kfree(sc->scr_list);
check_size_fail:
	if (!sc->codebuf_fd) {
		if (apusys_mem_unmap_iova(&mem))
			mdw_drv_err("map sc iova fail\n");
	}
map_iova_fail:
	if (!sc->codebuf_fd) {
		if (apusys_mem_unmap_kva(&mem))
			mdw_drv_err("unmap sc kva fail\n");
	}
map_kva_fail:
	kfree(sc->c_hdr);
alloc_sc_hdr_fail:
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
	_set_data_to_cmdbuf(sc);

	if (sc->codebuf_fd >= 0) {
		memset(&mem, 0, sizeof(struct apusys_kmem));
		mem.kva = (unsigned long long)sc->codebuf;
		mem.size = sc->c_hdr->cmn.cb_info_size;
		mem.fd = sc->codebuf_fd;
		mem.khandle = sc->codebuf_mem_hnd;

		if (apusys_mem_unmap_iova(&mem)) {
			mdw_drv_err("unmap codebuf iova fd(%d) fail\n",
				sc->codebuf_fd);
		}
		if (apusys_mem_unmap_kva(&mem)) {
			mdw_drv_err("unmap codebuf kva fd(%d) fail\n",
				sc->codebuf_fd);
		}
	}

	mdw_lne_debug();
	if (sc->state < CMD_STATE_RUN) {
		mdw_drv_warn("0x%llx-#%d sc not done, st(%d)\n",
			sc->par_cmd->cmd_id,
			sc->idx,
			sc->state);
		delete_subcmd_lock(sc);
	}
	mdw_lne_debug();

	if (res_task_dec(sc)) {
		mdw_drv_warn("dec 0x%llx-#%d sc softlimit(%llu) fail",
			sc->par_cmd->cmd_id,
			sc->idx,
			sc->period);
	}

	sc->par_cmd->sc_list[sc->idx] = NULL;
	if (!test_bit(sc->idx, sc->par_cmd->sc_status))
		mdw_drv_warn("sc status already clear(0x%lx/%d)",
		*sc->par_cmd->sc_status, sc->idx);

	bitmap_clear(sc->par_cmd->sc_status, sc->idx, 1);
	kfree(sc->scr_list);
	kfree(sc->c_hdr);
	kfree(sc);

	return 0;
}

int apusys_cmd_create(int mem_fd, uint32_t offset,
	struct apusys_cmd **icmd, struct apusys_user *u)
{
	struct apusys_cmd *cmd = NULL;
	struct apusys_cmd_hdr *u_cmd_hdr = NULL;
	struct apusys_sc_hdr_cmn *subcmd_hdr = NULL;
	struct apusys_kmem mem;
	int ret = 0, i = 0;

	/* map kva */
	memset(&mem, 0, sizeof(struct apusys_kmem));
	mem.fd = mem_fd;
	if (apusys_mem_map_kva(&mem)) {
		mdw_drv_err("map cmdbuf kva from fd(%d)fail\n",
			mem_fd);
		return -ENOMEM;
	}

	if (apusys_mem_map_iova(&mem)) {
		mdw_drv_err("map cmdbuf iova from fd(%d)fail\n",
			mem_fd);
		if (apusys_mem_unmap_kva(&mem)) {
			mdw_drv_err("unmap cmdbuf kva fd(%d) fail\n",
				mem_fd);
		}
		return -ENOMEM;
	}

	/* verify offset and size */
	if ((offset + sizeof(struct apusys_cmd_hdr) > mem.iova_size)) {
		mdw_drv_err("check cmdbuf offset(%d/%u) fail",
			offset, mem.iova_size);
		ret = -EINVAL;
		goto alloc_ce_fail;
	}

	u_cmd_hdr = (struct apusys_cmd_hdr *)(mem.kva + offset);
	if (_check_apusys_hdr(u_cmd_hdr)) {
		mdw_drv_err("check apusys hdr fail\n");
		ret = -EINVAL;
		goto alloc_ce_fail;
	}

	/* allocate apusys cmd */
	cmd = kzalloc(sizeof(struct apusys_cmd), GFP_KERNEL);
	if (cmd == NULL) {
		ret = -ENOMEM;
		goto alloc_ce_fail;
	}

	/* allocate cmd hdr, copy from ioctl cmd */
	cmd->hdr = kzalloc(sizeof(struct apusys_cmd_hdr), GFP_KERNEL);
	if (cmd->hdr == NULL) {
		ret = -ENOMEM;
		goto alloc_hdr_fail;
	}
	memcpy(cmd->hdr, u_cmd_hdr, sizeof(struct apusys_cmd_hdr));
	if (_check_apusys_hdr(cmd->hdr)) {
		mdw_drv_err("check apusys hdr fail\n");
		ret = -EINVAL;
		goto alloc_sc_status_fail;
	}
	/* get cmdbuf(kmem) for */
	cmd->cmdbuf = apusys_user_get_mem(u, mem.fd);
	if (cmd->cmdbuf == NULL)
		mdw_drv_warn("no cmdbuf\n");

	/* assign value */
	cmd->pid = u->open_pid;
	cmd->tgid = u->open_tgid;
	cmd->u_hdr = u_cmd_hdr;
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

		ret = -ENOMEM;
		goto alloc_sc_status_fail;
	}

	cmd->pdr_cnt_list = kcalloc(cmd->hdr->num_sc,
		sizeof(int), GFP_KERNEL);
	if (cmd->pdr_cnt_list == NULL) {

		ret = -ENOMEM;
		goto alloc_pdr_fail;
	}

	/* allocate ctx ref and list */
	cmd->ctx_ref = kcalloc(cmd->hdr->num_sc, sizeof(uint32_t), GFP_KERNEL);
	if (cmd->ctx_ref == NULL) {
		ret = -ENOMEM;
		goto alloc_sc_ctx_fail;
	}
	cmd->ctx_list = kcalloc(cmd->hdr->num_sc, sizeof(uint32_t), GFP_KERNEL);
	if (cmd->ctx_list == NULL) {
		ret = -ENOMEM;
		goto alloc_sc_ctx_list_fail;
	}
	cmd->pc_col.pack_status = kzalloc(sizeof(unsigned long) *
		cmd->hdr->num_sc, GFP_KERNEL);
	if (cmd->pc_col.pack_status == NULL) {

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
			mdw_drv_err("cmd(0x%llx) #%d subcmd's ctx_id(%d) invalid\n",
				cmd->cmd_id, i, subcmd_hdr->mem_ctx);
			ret = -EINVAL;
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

		ret = -ENOMEM;
		goto alloc_sc_list_fail;
	}

	mutex_init(&cmd->mtx);
	INIT_LIST_HEAD(&cmd->pc_col.sc_list);
	INIT_LIST_HEAD(&cmd->u_list);
	init_completion(&cmd->comp);

	*icmd = cmd;

#define APUSYS_CMDINFO_PRINT "create cmd(0x%llx/0x%llx)"\
	" sc_num(%u) priority(%u) deadline(%u/%u/%u)"\
	" mp(%d) flag(0x%llx)\n"
	mdw_drv_debug(APUSYS_CMDINFO_PRINT,
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
	kfree(cmd->hdr);
alloc_hdr_fail:
	kfree(cmd);
alloc_ce_fail:
	if (apusys_mem_unmap_iova(&mem)) {
		mdw_drv_err("unmap cmdbuf iova fd(%d) fail\n",
			mem_fd);
	}
	if (apusys_mem_unmap_kva(&mem)) {
		mdw_drv_err("unmap cmdbuf kva fd(%d) fail\n",
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
		mdw_drv_err("cmd is busy\n");
		return -EBUSY;
	}

	mutex_lock(&cmd->mtx);

	/* check sc_status */
	if (check_cmd_done(cmd))
		mdw_drv_warn("cmd(0x%llx) non-cleared(0x%lx)\n",
			cmd->cmd_id, *cmd->sc_status);

	/* unmap iova/kva */
	memset(&mem, 0, sizeof(struct apusys_kmem));
	mem.fd = cmd->mem_fd;
	mem.khandle = cmd->mem_hnd;
	if (apusys_mem_unmap_iova(&mem)) {
		mdw_drv_err("unmap cmdbuf iova fd(%d) fail\n",
			cmd->mem_fd);
	}
	if (apusys_mem_unmap_kva(&mem)) {
		mdw_drv_err("unmap cmdbuf kva from fd(%d)fail\n",
			cmd->mem_fd);
	}

	mutex_unlock(&cmd->mtx);
	kfree(cmd->sc_list);
	kfree(cmd->pc_col.pack_status);
	kfree(cmd->ctx_list);
	kfree(cmd->ctx_ref);
	kfree(cmd->pdr_cnt_list);
	kfree(cmd->sc_status);
	kfree(cmd->hdr);
	kfree(cmd);

	return 0;
}
