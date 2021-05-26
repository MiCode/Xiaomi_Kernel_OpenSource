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

#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#ifdef CONFIG_PM_SLEEP
#include <linux/pm_wakeup.h>
#endif

#include "apusys_options.h"
#include "apusys_device.h"
#include "apusys_dbg.h"
#include "cmd_parser.h"
#include "resource_mgt.h"
#include "scheduler.h"
#include "cmd_parser_mdla.h"
#include "thread_pool.h"
#include "midware_trace.h"
#include "sched_deadline.h"
#include "mnoc_api.h"
#include "reviser_export.h"
#include "apusys_user.h"
#include "memory_dump.h"
#include "mdw_cmn.h"
#include "mdw_tag.h"
#define CREATE_TRACE_POINTS
#include "mdw_events.h"

/* init link list head, which link all dev table */
struct apusys_cmd_table {
	struct list_head list;
	struct mutex list_mtx;
};

struct apusys_prio_q_table {
	struct list_head list;
	struct mutex list_mtx;
};

struct pack_cmd {
	int dev_type;
	int sc_num;

	struct list_head sc_list;
	struct apusys_dev_aquire acq;
};

struct pack_cmd_mgr {
	int ready_num;
	struct list_head pc_list;
};

//----------------------------------------------
static struct pack_cmd_mgr g_pack_mgr;
static struct task_struct *sched_task;
static atomic_t sthd_group = ATOMIC_INIT(0);

//----------------------------------------------
#ifdef CONFIG_PM_SLEEP
static struct wakeup_source *apusys_sched_ws;
static uint32_t ws_count;
static struct mutex ws_mutex;
#endif

static void sched_ws_init(void)
{
#ifdef CONFIG_PM_SLEEP
	ws_count = 0;
	mutex_init(&ws_mutex);
	apusys_sched_ws = wakeup_source_register(NULL, "apusys_sched");
	if (!apusys_sched_ws)
		mdw_drv_err("apusys sched wakelock register fail!\n");
#else
	mdw_flw_debug("not support pm wakelock\n");
#endif
}

static void sched_ws_lock(void)
{
#ifdef CONFIG_PM_SLEEP
	mutex_lock(&ws_mutex);
	if (apusys_sched_ws && !ws_count) {
		mdw_flw_debug("lock wakelock\n");
		__pm_stay_awake(apusys_sched_ws);
	}
	ws_count++;
	mutex_unlock(&ws_mutex);
#else
	mdw_flw_debug("not support pm wakelock\n");
#endif
}

static void sched_ws_unlock(void)
{
#ifdef CONFIG_PM_SLEEP
	mutex_lock(&ws_mutex);
	ws_count--;
	if (apusys_sched_ws && !ws_count) {
		mdw_flw_debug("unlock wakelock\n");
		__pm_relax(apusys_sched_ws);
	}
	mutex_unlock(&ws_mutex);
#else
	mdw_flw_debug("not support pm wakelock\n");
#endif
}
struct mem_ctx_mgr {
	struct mutex mtx;
	unsigned long ctx[BITS_TO_LONGS(32)];
};

static struct mem_ctx_mgr g_ctx_mgr;

static int mem_alloc_ctx(uint8_t tcm_force, uint32_t req_size,
	unsigned long *ctx, uint32_t *allocated_size)
{
#if 0

	mutex_lock(&g_ctx_mgr.mtx);
	ctx = find_first_zero_bit(g_ctx_mgr.ctx, 32);
	if (ctx >= 32)
		ctx = -1;
	else
		bitmap_set(g_ctx_mgr.ctx, ctx, 1);

	mutex_unlock(&g_ctx_mgr.mtx);
	return ctx;
#else
	if (req_size == 0) {
		req_size = dbg_get_prop(DBG_PROP_TCM_DEFAULT);
		mdw_flw_debug("tcm default request size(0x%x)\n", req_size);
	}
	return reviser_get_vlm(req_size, tcm_force, ctx, allocated_size);
#endif
}

static int mem_free_ctx(int ctx)
{
	int ret = 0;

	if (ctx == VALUE_SUBGRAPH_CTX_ID_NONE)
		return 0;
#if 0
	mutex_lock(&g_ctx_mgr.mtx);
	if (!test_bit(ctx, g_ctx_mgr.ctx))
		mdw_drv_err("ctx id confuse, idx(%d) is not set\n", ctx);
	bitmap_clear(g_ctx_mgr.ctx, ctx, 1);
	mutex_unlock(&g_ctx_mgr.mtx);
#else
	ret = reviser_free_vlm((unsigned long)ctx);
	if (ret)
		mdw_drv_err("free ctx(%d) fail(%d)\n", ctx, ret);
#endif
	return ret;
}

//----------------------------------------------
static int alloc_ctx(struct apusys_subcmd *sc, struct apusys_cmd *cmd)
{
	unsigned long ctx_id = -1;

	if (sc->c_hdr->cmn.mem_ctx == VALUE_SUBGRAPH_CTX_ID_NONE) {
		if (mem_alloc_ctx(sc->c_hdr->cmn.tcm_force,
			sc->c_hdr->cmn.tcm_usage,
			&ctx_id, &sc->tcm_real_usage))
			return -EINVAL;
		sc->ctx_id = ctx_id;
		mdw_flw_debug("0x%llx-#%d sc ctx_group(0x%x) ctx(%d)\n",
			cmd->cmd_id, sc->idx,
			sc->c_hdr->cmn.mem_ctx, sc->ctx_id);
	} else {
		if (cmd->ctx_list[sc->c_hdr->cmn.mem_ctx] ==
			VALUE_SUBGRAPH_CTX_ID_NONE) {
			if (mem_alloc_ctx(sc->c_hdr->cmn.tcm_force,
				sc->c_hdr->cmn.tcm_usage, &ctx_id,
				&sc->tcm_real_usage))
				return -EINVAL;

			cmd->ctx_list[sc->c_hdr->cmn.mem_ctx] = ctx_id;
			mdw_flw_debug("0x%llx-#%d sc ctx_group(0x%x) ctx(%d)\n",
				cmd->cmd_id, sc->idx,
				sc->c_hdr->cmn.mem_ctx,
				cmd->ctx_list[sc->c_hdr->cmn.mem_ctx]);
		}
		sc->ctx_id = cmd->ctx_list[sc->c_hdr->cmn.mem_ctx];
	}

	mdw_flw_debug("0x%llx-#%d sc ctx_group(0x%x) id(%d)\n",
		cmd->cmd_id, sc->idx,
		sc->c_hdr->cmn.mem_ctx, sc->ctx_id);

	return 0;
}

static int free_ctx(struct apusys_subcmd *sc, struct apusys_cmd *cmd)
{
	int ret = 0;

	if (sc->c_hdr->cmn.mem_ctx == VALUE_SUBGRAPH_CTX_ID_NONE) {
		mdw_flw_debug("0x%llx-#%d sc ctx_group(0x%x) free id(%d)\n",
			cmd->cmd_id, sc->idx,
			sc->c_hdr->cmn.mem_ctx, sc->ctx_id);
		mem_free_ctx(sc->ctx_id);
		sc->ctx_id = VALUE_SUBGRAPH_CTX_ID_NONE;
	} else {
		cmd->ctx_ref[sc->c_hdr->cmn.mem_ctx]--;
		if (cmd->ctx_ref[sc->c_hdr->cmn.mem_ctx] == 0) {
			mdw_flw_debug("0x%llx-#%d sc ctx_group(0x%x) free id(%d)\n",
				cmd->cmd_id, sc->idx,
				sc->c_hdr->cmn.mem_ctx, sc->ctx_id);
			mem_free_ctx(cmd->ctx_list[sc->c_hdr->cmn.mem_ctx]);
		}
		sc->ctx_id = VALUE_SUBGRAPH_CTX_ID_NONE;
	}

	return ret;
}

static int insert_pack_cmd(struct apusys_subcmd *sc, struct pack_cmd **ipc)
{
	struct pack_cmd *pc = NULL;
	struct apusys_subcmd *tmp_sc = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	int next_idx = 0, ret = 0, bit0 = 0, bit1 = 0;
	unsigned int pack_idx = VALUE_SUBGRAPH_PACK_ID_NONE;

	if (sc == NULL)
		return -EINVAL;

	pack_idx = sc->pack_id;

	mdw_flw_debug("0x%llx-#%d sc: before pack(%u)(%d/%d)\n",
		sc->par_cmd->cmd_id,
		sc->idx,
		pack_idx,
		test_bit(sc->idx, sc->par_cmd->pc_col.pack_status),
		test_bit(pack_idx, sc->par_cmd->pc_col.pack_status));

	list_add_tail(&sc->pc_list, &sc->par_cmd->pc_col.sc_list);

	/* packed cmd collect done */
	bit0 = test_and_change_bit(sc->idx, sc->par_cmd->pc_col.pack_status);
	bit1 = test_and_change_bit(pack_idx, sc->par_cmd->pc_col.pack_status);
	if (bit0 && bit1) {
		mdw_flw_debug("pack cmd satified(0x%llx/0x%llx)(%d/%d)\n",
			sc->par_cmd->hdr->uid,
			sc->par_cmd->cmd_id,
			sc->idx,
			pack_idx);
		pc = kzalloc(sizeof(struct pack_cmd), GFP_KERNEL);
		if (pc == NULL) {
			/* TODO, error handling for cmd list added already */
			return -ENOMEM;
		}

		INIT_LIST_HEAD(&pc->acq.dev_info_list);
		INIT_LIST_HEAD(&pc->sc_list);
		pc->dev_type = sc->type;

		/* find all packed subcmd id */
		/* TODO, guarntee can find all packed cmd*/
		/* now just query one round, user must set in order */
		next_idx = pack_idx;
		list_for_each_safe(list_ptr, tmp,
			&sc->par_cmd->pc_col.sc_list) {
			tmp_sc = list_entry(list_ptr,
				struct apusys_subcmd, pc_list);
			mdw_flw_debug("find pack idx(%d/%d)\n",
				next_idx, tmp_sc->idx);
			if (tmp_sc->idx == next_idx) {
				next_idx = tmp_sc->pack_id;
				list_del(&tmp_sc->pc_list);
				list_add_tail(&tmp_sc->pc_list, &pc->sc_list);
				tmp_sc->state = CMD_STATE_RUN;
				pc->sc_num++;
			}
		}

		/* TODO: don't show fake error msg */
		if (tmp_sc->idx != pack_idx) {
			mdw_flw_debug("pack idx, (%d->%d)\n",
				tmp_sc->idx, pack_idx);
		}

		/* return pack cmd after all packed sc ready */
		*ipc = pc;
		ret = pc->sc_num;
	}

	mdw_flw_debug("0x%llx-#%d sc: after pack(%u)(%d/%d)\n",
		sc->par_cmd->cmd_id,
		sc->idx,
		pack_idx,
		test_bit(sc->idx, sc->par_cmd->pc_col.pack_status),
		test_bit(pack_idx, sc->par_cmd->pc_col.pack_status));

	return ret;
}

static int exec_pack_cmd(void *iacq)
{
	struct apusys_dev_aquire *acq = NULL;
	struct pack_cmd *pc = NULL;
	struct apusys_subcmd *sc = NULL;
	struct apusys_dev_info *info = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	int count = 0;

	acq = (struct apusys_dev_aquire *)iacq;
	if (acq == NULL)
		return -EINVAL;

	pc = (struct pack_cmd *)acq->user;
	if (pc == NULL)
		return -EINVAL;

	list_for_each_safe(list_ptr, tmp, &acq->dev_info_list) {
		info = list_entry(list_ptr, struct apusys_dev_info, acq_list);
		list_del(&info->acq_list);
		if (list_empty(&pc->sc_list)) {
			mdw_drv_err("pack cmd and device(%d) is not same number!\n",
				info->dev->dev_type);
			if (put_apusys_device(info)) {
				mdw_drv_err("put device(%d/%d) fail\n",
					info->dev->dev_type, info->dev->idx);
			}
		} else {
			sc = (struct apusys_subcmd *)list_first_entry
				(&pc->sc_list, struct apusys_subcmd, pc_list);
			list_del(&sc->pc_list);
			count++;
			info->cmd_id = sc->par_cmd->cmd_id;
			info->sc_idx = sc->idx;
			sc->state = CMD_STATE_RUN;
			sc->exec_core_num = 1;
			sc->exec_core_bitmap = (1UL << info->dev->idx);
			/* mark device execute deadline task */
			if (sc->period)
				info->is_deadline = true;
			/* trigger device by thread pool */
			if (thread_pool_trigger(sc, info)) {
				mdw_drv_err("tp cmd(0x%llx/%d)dev(%d/%d) fail\n",
					sc->par_cmd->cmd_id, sc->idx,
					info->dev->dev_type, info->dev->idx);
			}
		}
	}

	if (count != pc->sc_num) {
		mdw_drv_err("execute pack cmd(%d/%d) number issue\n",
		count, pc->sc_num);
	}

	/* destroy pack_cmd and dev_acquire */
	kfree(pc);

	return 0;
}

static int clear_pack_cmd(struct apusys_cmd *cmd)
{
	//struct list_head *tmp = NULL, *list_ptr = NULL;
	//struct pack_cmd *pc = NULL;

	if (cmd == NULL)
		return -EINVAL;

	mdw_flw_debug("0x%llx cmd clear pack list\n",
		cmd->cmd_id);

	//list_del(&tmp_sc->pc_list);
	//list_add_tail(&tmp_sc->pc_list, &pc->sc_list);

	bitmap_clear(cmd->pc_col.pack_status, 0, cmd->hdr->num_sc);
	INIT_LIST_HEAD(&cmd->pc_col.sc_list);

	return 0;
}

void subcmd_done(struct apusys_subcmd *sc, int dev_idx)
{
	struct apusys_subcmd *scr = NULL;
	struct apusys_cmd *cmd = NULL;
	int ret = 0, done_idx = 0, i = 0;
	struct apusys_res_mgr *res_mgr = res_get_mgr();

	if (sc == NULL) {
		mdw_drv_err("invalid sc(%p)\n", sc);
		return;
	}

	cmd = sc->par_cmd;
	if (cmd == NULL) {
		mdw_drv_err("invalid cmd(%p)\n", cmd);
		return;
	}

	/* check sc state and delete */
	mutex_lock(&cmd->mtx);
	if (!(sc->exec_core_bitmap & (1ULL << dev_idx))) {
		mdw_drv_err("miss exec cores(0x%llx/%u)",
			sc->exec_core_bitmap,
			dev_idx);
	}
	sc->exec_core_bitmap &= (~(1ULL << dev_idx));
	if (sc->exec_core_bitmap == 0) {
		mdw_flw_debug("0x%llx-#%d sc done\n",
			cmd->cmd_id, sc->idx);
		sc->state = CMD_STATE_DONE;
		if (free_ctx(sc, cmd))
			mdw_drv_err("free memory ctx id fail\n");
		done_idx = sc->idx;
	} else {
		mutex_unlock(&cmd->mtx);
		return;
	}

	/* should insert subcmd which dependency satisfied */
	for (i = 0; i < sc->scr_num; i++) {
		/* decreate pdr count of sc */
		decrease_pdr_cnt(cmd, sc->scr_list[i]);

		if (check_sc_ready(cmd, sc->scr_list[i]) != 0)
			continue;

		scr = cmd->sc_list[sc->scr_list[i]];
		if (scr != NULL) {
			mdw_flw_debug("0x%llx-#%d sc: dp satified, ins q(%d-#%d)\n",
				cmd->cmd_id,
				scr->idx,
				scr->type,
				cmd->hdr->priority);

			mutex_lock(&res_mgr->mtx);
			mutex_lock(&scr->mtx);
			ret = insert_subcmd(scr);
			if (ret) {
				mdw_drv_err("ins 0x%llx-#%d sc to q(%d-#%d) fail\n",
					cmd->cmd_id,
					scr->idx,
					scr->type,
					cmd->hdr->priority);
			}
			mutex_unlock(&scr->mtx);
			mutex_unlock(&res_mgr->mtx);
		}
	}

	if (apusys_subcmd_delete(sc)) {
		mdw_drv_err("delete sc(0x%llx/%d) fail\n",
			cmd->cmd_id,
			sc->idx);
	}

	/* clear subcmd bit in cmd entry's status */
	/* if whole apusys cmd done, wakeup user context thread */
	if (check_cmd_done(cmd) == 0) {
		mdw_flw_debug("apusys cmd(0x%llx) done\n",
			cmd->cmd_id);
		mdw_flw_debug("wakeup user context thread\n");
		complete(&cmd->comp);
	}

	mutex_unlock(&cmd->mtx);
}

static int multicore_get_cmd_idx(struct apusys_subcmd *sc, int dev_idx)
{
	int multi_idx = 0, i = 0;
	int size = sizeof(sc->exec_core_bitmap)*8;

	if (sc->exec_core_num < 2)
		return -ENODATA;

	/* get cmd idx of multicore from bitmap */
	for (i = 0; i < sc->exec_core_num; i++) {
		if (i == 0) {
			multi_idx = find_next_bit((unsigned long *)
				sc->exec_core_bitmap, 0, size);
		} else {
			multi_idx = find_next_bit((unsigned long *)
				sc->exec_core_bitmap, multi_idx+1, size);
		}
		if (multi_idx == dev_idx)
			break;
		if (multi_idx >= size)
			return -EINVAL;
	}

	if (i >= sc->exec_core_num)
		return -EINVAL;

	return i;
}

static int multicore_get_devnum(struct apusys_subcmd *sc)
{
	int dev_num = -1, queue_empty = -1, codebuf_valid = -1;
	int total_core = 0, multicore_num = 2;

	/* check supported device */
	if (sc->type != APUSYS_DEVICE_MDLA)
		return 1;

	/* get type device support num */
	total_core = res_get_device_num(sc->type);
	if (total_core <= 0)
		return 0;

	/* check codebuf contain multicore info */
	codebuf_valid = check_multimdla_support(sc);

	switch (sc->par_cmd->multicore_sched) {
	case CMD_SCHED_NORMAL:
		/* CMD_SCHED_NORMAL: scheduler decide */
		queue_empty = normal_task_empty(APUSYS_DEVICE_MDLA);
		/* check normal queue empty and codebuf valid */
		if (queue_empty && codebuf_valid)
			dev_num = total_core < multicore_num ?
				total_core : multicore_num;
		else
			dev_num = 1;
		break;
	case CMD_SCHED_FORCE_MULTI:
		/* CMD_SCHED_FORCE_MULTI: force multicore if codebuf is valid */
		if (codebuf_valid) {
			if (dev_num > total_core)
				mdw_drv_warn("force multi: dev(%d) core(%d/%d)\n",
				sc->type, multicore_num, total_core);

			dev_num = total_core < multicore_num ?
				total_core : multicore_num;
		} else {
			dev_num = 1;
		}
		break;
	case CMD_SCHED_FORCE_SINGLE:
	default:
		/* CMD_SCHED_FORCE_SINGLE: always single core */
		dev_num = 1;
		break;
	}

	mdw_flw_debug("multicore(%d/%d/%d) require dev(%d) %d cores\n",
		sc->par_cmd->multicore_sched, queue_empty,
		codebuf_valid, sc->type, dev_num);

	return dev_num;
}

static int multicore_poweron(struct apusys_subcmd *sc,
	struct apusys_dev_aquire *acq)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_power_hnd pwr_hnd;
	struct apusys_dev_info *info = NULL;
	int ret = 0;

	/* poweron if multicore */
	if (acq->acq_num >= 2) {
		list_for_each_safe(list_ptr, tmp, &acq->dev_info_list) {
			info = list_entry(list_ptr,
				struct apusys_dev_info,
				acq_list);
			pwr_hnd.boost_val = sc->boost_val;
			pwr_hnd.timeout = APUSYS_SETPOWER_TIMEOUT;
			mdw_flw_debug("poweron dev(%d-#%d)\n",
				info->dev->dev_type, info->dev->idx);
			ret = info->dev->send_cmd(APUSYS_CMD_POWERON,
				&pwr_hnd, info->dev);
			if (ret) {
				mdw_drv_err("poweron dev(%d-#%d) fail(%d)\n",
					info->dev->dev_type,
					info->dev->idx,
					ret);
				return ret;
			}
		}
	}

	return ret;
}

static int setup_cmn_hnd(struct apusys_subcmd *sc,
	struct apusys_dev_info *dev_info, struct apusys_cmd_hnd *hnd)
{
	int multicore_cmd_idx = 0;

	/* setup cmn info */
	hnd->kva = (uint64_t)sc->codebuf;
	hnd->size = sc->c_hdr->cmn.cb_info_size;
	hnd->boost_val = deadline_task_boost(sc);
	hnd->cmd_id = sc->par_cmd->cmd_id;
	hnd->subcmd_idx = sc->idx;
	hnd->priority = sc->par_cmd->hdr->priority;
	hnd->cmd_entry = (uint64_t)sc->par_cmd->u_hdr;
	hnd->cmdbuf = sc->par_cmd->cmdbuf;
	hnd->cluster_size = sc->cluster_size;
	if (hnd->kva == 0 || hnd->size == 0) {
		mdw_drv_err("invalid sc(%d)(0x%llx/%d)\n",
			sc->idx, hnd->kva, hnd->size);
		return -EINVAL;
	}

	/* setup device specific info */
	if (sc->type == APUSYS_DEVICE_MDLA) {
		/* setup mdla cmn codebuf info */
		if (parse_mdla_codebuf_info(sc, hnd)) {
			mdw_drv_err("fill mdla specific info fail\n");
			return -EINVAL;
		}
		/* setup mdla multicore codebuf info, may overwrite */
		multicore_cmd_idx = multicore_get_cmd_idx(sc,
			dev_info->dev->idx);
		mdw_flw_debug("dev(%d-#%d) cmd idx(%d)\n",
			sc->type, dev_info->dev->idx,
			multicore_cmd_idx);
		if (multicore_cmd_idx >= 0) {
			hnd->multicore_total = sc->exec_core_num;
			hnd->multicore_idx = multicore_cmd_idx;
			if (set_multimdla_codebuf(sc, hnd, multicore_cmd_idx))
				mdw_drv_err("set multicore fail(%d)\n",
				multicore_cmd_idx);
		}
	}

	mdw_flw_debug("0x%llx-#%d sc: exec hnd(%d/0x%llx/%d)\n",
		sc->par_cmd->cmd_id,
		sc->idx,
		sc->type,
		hnd->kva,
		hnd->size);

	return 0;
}

static int exec_cmd_func(void *isc, void *idev_info)
{
	struct apusys_dev_info *dev_info = (struct apusys_dev_info *)idev_info;
	struct apusys_cmd_hnd cmd_hnd;
	struct apusys_subcmd *sc = (struct apusys_subcmd *)isc;
	int ret = 0;
	uint32_t t_diff = 0;
	struct timeval driver_time;

	if (isc == NULL || idev_info == NULL) {
		ret = -EINVAL;
		goto out;
	}

	memset(&cmd_hnd, 0, sizeof(cmd_hnd));
	/* get subcmd information */
	ret = setup_cmn_hnd(sc, dev_info, &cmd_hnd);
	if (ret) {
		sc->par_cmd->cmd_ret = ret;
		mdw_drv_warn("setup sc hnd(0x%llx/0x%llx) fail\n",
			sc->par_cmd->hdr->uid,
			sc->par_cmd->cmd_id);
		goto out;
	}

	/* call execute */
	midware_trace_begin("apusys_scheduler|dev: %d_%d, cmd_id: 0x%08llx",
			dev_info->dev->dev_type,
			dev_info->dev->idx,
			sc->par_cmd->cmd_id);

	/* 1. allocate memory ctx id*/
	mutex_lock(&sc->par_cmd->mtx);
	ret = alloc_ctx(sc, sc->par_cmd);
	mutex_unlock(&sc->par_cmd->mtx);

	if (ret) {
		mdw_drv_err("allocate memory ctx id(%d) fail\n", sc->ctx_id);
		sc->par_cmd->cmd_ret = ret;
		goto out;
	}

	/* Execute reviser to switch VLM:
	 * Skip set context on preemptive command, context should be set by
	 * engine driver itself. Give engine a callback to set context id.
	 */
	if (dev_info->dev->dev_type == APUSYS_DEVICE_MDLA ||
		dev_info->dev->dev_type == APUSYS_DEVICE_MDLA_RT) {
		cmd_hnd.ctx_id = sc->ctx_id;
		cmd_hnd.context_callback = reviser_set_context;
	} else {
		reviser_set_context(dev_info->dev->dev_type,
				dev_info->dev->idx, sc->ctx_id);
	}

#ifdef APUSYS_OPTIONS_INF_MNOC
	/* 2. start count cmd qos */
	mdw_flw_debug("mnoc: cmd qos start 0x%llx-#%d dev(%s-#%d)\n",
		sc->par_cmd->cmd_id, sc->idx,
		dev_info->name, dev_info->dev->idx);

	/* count qos start */
	if (apu_cmd_qos_start(sc->par_cmd->cmd_id, sc->idx,
		sc->type, dev_info->dev->idx, sc->boost_val)) {
		mdw_flw_debug("start qos for 0x%llx-#%d sc fail\n",
			sc->par_cmd->cmd_id, sc->idx);
	}
#endif

	/* 3. get driver time start */
#define MDW_EXEC_PRINT " pid(%d/%d) cmd(0x%llx/0x%llx-#%d/%u)"\
	" dev(%d/%s-#%d) mp(0x%x/%u/%u/0x%llx) sched(%d/%u/%u/%u/%u/%d)"\
	" mem(%u/%d/0x%x/0x%x) boost(%u) time(%u/%u)"
	mdw_drv_debug("start:"MDW_EXEC_PRINT"\n",
		sc->par_cmd->pid,
		sc->par_cmd->tgid,
		sc->par_cmd->hdr->uid,
		sc->par_cmd->cmd_id,
		sc->idx,
		sc->par_cmd->hdr->num_sc,
		sc->type,
		dev_info->name,
		dev_info->dev->idx,
		sc->pack_id,
		cmd_hnd.multicore_idx,
		sc->exec_core_num,
		sc->exec_core_bitmap,
		sc->par_cmd->hdr->priority,
		sc->par_cmd->hdr->soft_limit,
		sc->par_cmd->hdr->hard_limit,
		sc->c_hdr->cmn.ip_time,
		sc->c_hdr->cmn.suggest_time,
		sc->par_cmd->power_save,
		sc->ctx_id,
		sc->c_hdr->cmn.tcm_force,
		sc->c_hdr->cmn.tcm_usage,
		sc->tcm_real_usage,
		cmd_hnd.boost_val,
		cmd_hnd.ip_time,
		0);

	/* trace cmd start */
	trace_mdw_cmd(0,
		sc->par_cmd->pid,
		sc->par_cmd->tgid,
		sc->par_cmd->hdr->uid,
		sc->par_cmd->cmd_id,
		sc->idx,
		sc->par_cmd->hdr->num_sc,
		sc->type,
		dev_info->name,
		dev_info->dev->idx,
		sc->pack_id,
		cmd_hnd.multicore_idx,
		sc->exec_core_num,
		sc->exec_core_bitmap,
		sc->par_cmd->hdr->priority,
		sc->par_cmd->hdr->soft_limit,
		sc->par_cmd->hdr->hard_limit,
		sc->c_hdr->cmn.ip_time,
		sc->c_hdr->cmn.suggest_time,
		sc->par_cmd->power_save,
		sc->ctx_id,
		sc->c_hdr->cmn.tcm_force,
		sc->c_hdr->cmn.tcm_usage,
		sc->tcm_real_usage,
		cmd_hnd.boost_val,
		cmd_hnd.ip_time,
		ret);

	memset(&driver_time, 0, sizeof(driver_time));
	t_diff = get_time_diff_from_system(&driver_time);

	/* 4. execute subcmd */
	ret = dev_info->dev->send_cmd(APUSYS_CMD_EXECUTE,
		(void *)&cmd_hnd, dev_info->dev);

	/* 5. get driver time and ip time */
	t_diff = get_time_diff_from_system(&driver_time);

	/* 6. check execution result */
	if (ret) {
		mdw_drv_err("fail :"MDW_EXEC_PRINT" ret(%d)\n",
			sc->par_cmd->pid,
			sc->par_cmd->tgid,
			sc->par_cmd->hdr->uid,
			sc->par_cmd->cmd_id,
			sc->idx,
			sc->par_cmd->hdr->num_sc,
			sc->type,
			dev_info->name,
			dev_info->dev->idx,
			sc->pack_id,
			cmd_hnd.multicore_idx,
			sc->exec_core_num,
			sc->exec_core_bitmap,
			sc->par_cmd->hdr->priority,
			sc->par_cmd->hdr->soft_limit,
			sc->par_cmd->hdr->hard_limit,
			sc->c_hdr->cmn.ip_time,
			sc->c_hdr->cmn.suggest_time,
			sc->par_cmd->power_save,
			sc->ctx_id,
			sc->c_hdr->cmn.tcm_force,
			sc->c_hdr->cmn.tcm_usage,
			sc->tcm_real_usage,
			cmd_hnd.boost_val,
			cmd_hnd.ip_time,
			t_diff,
			ret);
		sc->par_cmd->cmd_ret = ret;
	} else {
		mdw_drv_debug("done :"MDW_EXEC_PRINT"\n",
			sc->par_cmd->pid,
			sc->par_cmd->tgid,
			sc->par_cmd->hdr->uid,
			sc->par_cmd->cmd_id,
			sc->idx,
			sc->par_cmd->hdr->num_sc,
			sc->type,
			dev_info->name,
			dev_info->dev->idx,
			sc->pack_id,
			cmd_hnd.multicore_idx,
			sc->exec_core_num,
			sc->exec_core_bitmap,
			sc->par_cmd->hdr->priority,
			sc->par_cmd->hdr->soft_limit,
			sc->par_cmd->hdr->hard_limit,
			sc->c_hdr->cmn.ip_time,
			sc->c_hdr->cmn.suggest_time,
			sc->par_cmd->power_save,
			sc->ctx_id,
			sc->c_hdr->cmn.tcm_force,
			sc->c_hdr->cmn.tcm_usage,
			sc->tcm_real_usage,
			cmd_hnd.boost_val,
			cmd_hnd.ip_time,
			t_diff);
	}
#undef MDW_EXEC_PRINT

	/* trace cmd end */
	trace_mdw_cmd(1,
		sc->par_cmd->pid,
		sc->par_cmd->tgid,
		sc->par_cmd->hdr->uid,
		sc->par_cmd->cmd_id,
		sc->idx,
		sc->par_cmd->hdr->num_sc,
		sc->type,
		dev_info->name,
		dev_info->dev->idx,
		sc->pack_id,
		cmd_hnd.multicore_idx,
		sc->exec_core_num,
		sc->exec_core_bitmap,
		sc->par_cmd->hdr->priority,
		sc->par_cmd->hdr->soft_limit,
		sc->par_cmd->hdr->hard_limit,
		sc->c_hdr->cmn.ip_time,
		sc->c_hdr->cmn.suggest_time,
		sc->par_cmd->power_save,
		sc->ctx_id,
		sc->c_hdr->cmn.tcm_force,
		sc->c_hdr->cmn.tcm_usage,
		sc->tcm_real_usage,
		cmd_hnd.boost_val,
		cmd_hnd.ip_time,
		ret);

	/* 7. setup max ip/driver time into sc */
	mutex_lock(&sc->mtx);
	sc->driver_time = sc->driver_time > t_diff ?
		sc->driver_time:t_diff;
	sc->ip_time = sc->ip_time > cmd_hnd.ip_time ?
		sc->ip_time:cmd_hnd.ip_time;
	mutex_unlock(&sc->mtx);

#ifdef APUSYS_OPTIONS_INF_MNOC
	/* 8. count qos end */
	sc->bw = apu_cmd_qos_end(sc->par_cmd->cmd_id, sc->idx,
		sc->type, dev_info->dev->idx);
	mdw_flw_debug("mnoc: cmd qos end 0x%llx-#%d dev(%d/%d) bw(%d)\n",
		sc->par_cmd->cmd_id, dev_info->dev->idx, sc->type,
		dev_info->dev->idx, sc->bw);
#endif

out:
	/* 9. put device back */
	if (put_device_lock(dev_info)) {
		mdw_drv_err("return dev(%d-#%d) fail\n",
			dev_info->dev->dev_type,
			dev_info->dev->idx);
		ret = -EINVAL;
	}

	midware_trace_end(
	"apusys_scheduler|dev: %d_%d, cmd_id: 0x%08llx, ret:%d",
			dev_info->dev->dev_type,
			dev_info->dev->idx,
			sc->par_cmd->cmd_id, ret);

	subcmd_done(sc, dev_info->dev->idx);

	return ret;
}

int sched_routine(void *arg)
{
	int ret = 0, type = 0, dev_num = 0;
	unsigned long available[BITS_TO_LONGS(APUSYS_DEV_TABLE_MAX)];
	struct apusys_subcmd *sc = NULL;
	struct apusys_res_mgr *res_mgr = NULL;
	struct apusys_dev_aquire acq, *acq_async = NULL;
	struct apusys_device *dev = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_dev_info *info = NULL;
	struct pack_cmd *pc = NULL;

	res_mgr = (struct apusys_res_mgr *)arg;

	while (!kthread_should_stop() && !res_mgr->sched_stop) {
		ret = wait_for_completion_interruptible(&res_mgr->sched_comp);
		if (ret)
			mdw_drv_warn("sched thread(%d)\n", ret);

		if (res_mgr->sched_pause != 0) {
			mdw_drv_debug("sched pause(%d)\n",
				res_mgr->sched_pause);
			continue;
		}

		memset(&available, 0, sizeof(available));
		bitmap_and(available, res_mgr->cmd_exist,
			res_mgr->dev_exist, APUSYS_DEVICE_MAX);
		/* if dev/cmd available or */
		while (!bitmap_empty(available, APUSYS_DEVICE_MAX)
			|| acq_device_check(&acq_async) == 0) {

			mutex_lock(&res_mgr->mtx);

			/* check any device acq ready for packcmd */
			if (acq_async != NULL) {
				mdw_flw_debug("get pack cmd(%d) ready",
					acq_async->dev_type);
				/* exec packcmd */
				if (exec_pack_cmd(acq_async))
					mdw_drv_err("execute pack cmd fail\n");

				goto sched_retrigger;
			}

			/* cmd/dev not same bit available, continue */
			if (bitmap_empty(available, APUSYS_DEVICE_MAX))
				goto sched_retrigger;

			/* get type from available */
			type = find_first_bit(available, APUSYS_DEVICE_MAX);
			if (type >= APUSYS_DEVICE_MAX) {
				mdw_drv_warn("find first bit for type(%d) fail\n",
					type);
				goto sched_retrigger;
			}

			/* pop cmd from priority queue */
			ret = pop_subcmd(type, &sc);
			if (ret) {
				mdw_drv_err("pop subcmd for dev(%d) fail\n",
					type);
				goto sched_retrigger;
			}

			/* if sc is packed, collect all packed cmd and send */
			if (sc->pack_id != VALUE_SUBGRAPH_PACK_ID_NONE) {
				if (insert_pack_cmd(sc, &pc) <= 0)
					goto sched_retrigger;

				mdw_flw_debug("packcmd done, insert acq(%d/%d)\n",
					pc->dev_type,
					pc->sc_num);
				pc->acq.target_num = pc->sc_num;
				pc->acq.owner = APUSYS_DEV_OWNER_SCHEDULER;
				pc->acq.dev_type = pc->dev_type;
				pc->acq.user = (void *)pc;
				pc->acq.is_done = 0;
				ret = acq_device_async(&pc->acq);
				if (ret < 0) {
					mdw_drv_err("acq dev(%d/%d) fail\n",
						pc->acq.dev_type,
						pc->acq.target_num);
				} else {
					if (pc->acq.acq_num ==
						pc->acq.target_num) {
						mdw_flw_debug("execute pack cmd\n");
						exec_pack_cmd(&pc->acq);
					}
				}
				goto sched_retrigger;
			}

			/* how many cores require from multicore policy */
			dev_num = multicore_get_devnum(sc);

			/* acquire device */
			memset(&acq, 0, sizeof(acq));
			acq.target_num = dev_num;
			acq.dev_type = type;
			acq.owner = APUSYS_DEV_OWNER_SCHEDULER;
			INIT_LIST_HEAD(&acq.dev_info_list);
			INIT_LIST_HEAD(&acq.tab_list);
			ret = acq_device_try(&acq);
			if (ret < 0 || acq.acq_num <= 0) {
				mdw_flw_debug("no dev(%d) available\n", type);
				mutex_lock(&sc->mtx);
				/* can't get device, insert sc back */
				if (insert_subcmd(sc)) {
					mdw_drv_err("re 0x%llx-#%d sc q(%d-#%d)\n",
						sc->par_cmd->cmd_id,
						sc->idx,
						type,
						sc->par_cmd->hdr->priority);
				}
				mutex_unlock(&sc->mtx);
				goto sched_retrigger;
			}

			/* check how many device acquired from resource mgr */
			sc->exec_core_num = acq.acq_num;
			sc->exec_core_bitmap = acq.acq_bitmap;
			mdw_flw_debug("sc exec cores(%u/0x%llx)\n",
				sc->exec_core_num, sc->exec_core_bitmap);

			/* poweron if multicore */
			if (acq.acq_num >= 2) {
				if (multicore_poweron(sc, &acq))
					mdw_drv_err("poweron(%d/%u/0x%llx) fail\n",
					sc->type,
					sc->exec_core_num,
					sc->exec_core_bitmap);
			}

			/* trigger thread pool to execute subcmd */
			list_for_each_safe(list_ptr, tmp, &acq.dev_info_list) {
				info = list_entry(list_ptr,
					struct apusys_dev_info, acq_list);
				dev = info->dev;
				info->cmd_id = sc->par_cmd->cmd_id;
				info->sc_idx = sc->idx;
				mutex_lock(&sc->mtx);
				sc->state = CMD_STATE_RUN;
				mutex_unlock(&sc->mtx);
				/* mark device execute deadline task */
				if (sc->period)
					info->is_deadline = true;
				/* trigger device by thread pool */
				ret = thread_pool_trigger(sc, info);
				if (ret)
					mdw_drv_err("trigger thread pool fail\n");
			}
sched_retrigger:
			bitmap_and(available, res_mgr->cmd_exist,
				res_mgr->dev_exist, APUSYS_DEVICE_MAX);
			acq_async = NULL;
			mutex_unlock(&res_mgr->mtx);
		}
	}

	mdw_drv_warn("scheduling thread stop\n");

	return 0;
}

int apusys_sched_del_cmd(struct apusys_cmd *cmd)
{
	int i = 0, ret = 0, times = 30, wait_ms = 200;
	struct apusys_subcmd *sc = NULL;
	struct apusys_res_mgr *res_mgr = res_get_mgr();

	if (cmd->state == CMD_STATE_DONE) {
		mdw_flw_debug("cmd done already\n");
		return 0;
	}

	mdw_drv_warn("abort cmd(0x%llx)\n",
		cmd->cmd_id);

	/* delete all subcmd in cmd */
	mutex_lock(&cmd->mtx);
	if (clear_pack_cmd(cmd))
		mdw_drv_warn("clear pack cmd list fail\n");

	for (i = 0; i < cmd->hdr->num_sc; i++) {
		sc = cmd->sc_list[i];
		if (sc == NULL)
			continue;

		mdw_flw_debug("check 0x%llx-#%d sc status\n",
			cmd->cmd_id,
			sc->idx);

		mutex_lock(&res_mgr->mtx);
		mutex_lock(&sc->mtx);
		if (sc->state < CMD_STATE_RUN) {
			if (free_ctx(sc, cmd))
				mdw_drv_err("free memory ctx id fail\n");

			if (sc->state == CMD_STATE_READY) {
				/* delete subcmd from q */
				if (delete_subcmd(sc)) {
					mdw_drv_err(
					"delete 0x%llx-#%d from q fail\n",
					sc->par_cmd->cmd_id,
					sc->idx);
				} else {
					sc->state = CMD_STATE_DONE;
				}
			}

			mutex_unlock(&sc->mtx);
			mutex_unlock(&res_mgr->mtx);

			if (apusys_subcmd_delete(sc)) {
				mdw_drv_err("delete 0x%llx-#%d sc fail\n",
					cmd->cmd_id, sc->idx);
			}

			mdw_flw_debug("delete 0x%llx-#%d sc\n",
					cmd->cmd_id, i);

		} else {
			mdw_flw_debug("0x%llx-#%d sc already execute(%d)\n",
				cmd->cmd_id,
				i,
				sc->state);
			mutex_unlock(&sc->mtx);
			mutex_unlock(&res_mgr->mtx);
		}
	}
	mutex_unlock(&cmd->mtx);


	mutex_lock(&cmd->mtx);
	mdw_drv_warn("wait 0x%llx cmd done...\n",
		cmd->cmd_id);
	/* final polling */
	for (i = 0; i < times; i++) {
		if (check_cmd_done(cmd) == 0) {
			mdw_drv_warn("delete cmd safely\n");
			break;
		}
		mdw_drv_warn("sleep 200ms to wait sc done\n");
		mutex_unlock(&cmd->mtx);
		msleep(wait_ms);
		mutex_lock(&cmd->mtx);
	}
	mutex_unlock(&cmd->mtx);

	if (i >= times) {
		mdw_drv_err("cmd busy\n");
		ret = -EBUSY;
	}

	return ret;
}

int apusys_sched_wait_cmd(struct apusys_cmd *cmd)
{
	int ret = 0;
	int retry = 140, retry_time = 50;
	unsigned long timeout = usecs_to_jiffies(APUSYS_PARAM_WAIT_TIMEOUT);

	if (cmd == NULL)
		return -EINVAL;

	sched_ws_lock();

start:
	ret = wait_for_completion_interruptible_timeout(&cmd->comp, timeout);
	if (ret == -ERESTARTSYS) {
		if (retry) {
			if (!(retry % 20))
				mdw_drv_warn("user int(%d) retry(0x%llx)(%d)...\n",
					ret,
					cmd->cmd_id,
					retry);
			retry--;
			msleep(retry_time);
			goto start;
		}
	} else if (ret == 0) {
		mdw_drv_err("user ctx int(%d) cmd(0x%llx) timeout(%d)\n",
			ret, cmd->cmd_id,
			APUSYS_PARAM_WAIT_TIMEOUT/1000/1000);
		cmd->cmd_ret = -ETIME;
	} else if (ret < 0) {
		mdw_drv_err("user ctx int(%d) cmd(0x%llx)\n",
			ret, cmd->cmd_id);
	} else {
		mutex_lock(&cmd->mtx);
		if (cmd->state != CMD_STATE_DONE) {
			mdw_drv_warn("(%d/%d)cmd(0x%llx/0x%llx) not done\n",
				cmd->pid,
				cmd->tgid,
				cmd->hdr->uid,
				cmd->cmd_id);
		}
		mutex_unlock(&cmd->mtx);
		ret = 0;
	}

	sched_ws_unlock();

	return ret;
}

int apusys_sched_add_cmd(struct apusys_cmd *cmd)
{
	int ret = 0, i = 0;
	struct apusys_subcmd *sc = NULL;
	uint32_t dp_ofs = 0;
	uint32_t dp_num = 0;

	if (cmd == NULL) {
		ret = -EINVAL;
		goto out;
	}

	/* 1. add all independent subcmd to corresponding queue */
	while (i < cmd->hdr->num_sc) {
		dp_num = *(uint32_t *)(cmd->dp_entry + dp_ofs);

		/* allocate subcmd struct */
		if (apusys_subcmd_create(i, cmd, &sc, dp_ofs)) {
			mdw_drv_err("create sc for cmd(%d/%p) fail\n", i, cmd);
			ret = -EINVAL;
			break;
		}

		mutex_lock(&cmd->mtx);

		/* add sc to cmd's sc_list*/
		if (check_sc_ready(cmd, i) == 0) {
			ret = insert_subcmd_lock(sc);
			if (ret) {
				mdw_drv_err("ins 0x%llx-#%d sc(%p) q(%d-#%d)\n",
					cmd->cmd_id,
					sc->idx,
					sc,
					sc->type,
					cmd->hdr->priority);
				mutex_unlock(&cmd->mtx);
				goto out;
			}
		}

		mutex_unlock(&cmd->mtx);

		dp_ofs += (SIZE_CMD_PREDECCESSOR_CMNT_ELEMENT *
			(dp_num + 1));
		i++;
	}

out:
	return ret;
}

int apusys_sched_pause(void)
{
	struct apusys_res_mgr *res_mgr = res_get_mgr();

	if (res_mgr == NULL)
		return -EINVAL;

	/* pause scheduler */
	if (res_mgr->sched_pause == 0) {
		mdw_drv_debug("scheduler pause\n");
		res_mgr->sched_pause = 1;
	} else {
		mdw_drv_warn("scheduler already pause\n");
	}

	/* check all device free */
	if (res_suspend_dev())
		mdw_drv_warn("suspend device fail\n");
	else
		mdw_drv_warn("suspend device done\n");

	return 0;
}

int apusys_sched_restart(void)
{
	struct apusys_res_mgr *res_mgr = res_get_mgr();

	if (res_mgr == NULL)
		return -EINVAL;

	if (res_mgr->sched_pause == 1) {
		mdw_drv_debug("scheduler restart\n");
		res_mgr->sched_pause = 0;
	} else {
		mdw_drv_warn("scheduler already resume\n");
	}

	if (res_resume_dev())
		mdw_drv_warn("resume device fail\n");
	else
		mdw_drv_warn("resume device done\n");
	/* trigger sched thread */
	complete(&res_mgr->sched_comp);

	return 0;
}

void apusys_sched_set_group(void)
{
	struct file *fd;
	char buf[8];
	mm_segment_t oldfs;

	if (atomic_read(&sthd_group))
		return;

	/* setup worker thread group */
	thread_pool_set_group();

	oldfs = get_fs();
	set_fs(get_ds());

	fd = filp_open(APUSYS_THD_TASK_FILE_PATH, O_WRONLY, 0);
	if (IS_ERR(fd)) {
		mdw_drv_debug("don't support low latency group\n");
		goto out;
	}

	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf)-1, "%d", sched_task->pid);
	vfs_write(fd, (__force const char __user *)buf,
		sizeof(buf), &fd->f_pos);
	mdw_drv_debug("setup sched thd(%d/%s) to group\n",
		sched_task->pid, buf);

	filp_close(fd, NULL);

out:
	set_fs(oldfs);
	atomic_set(&sthd_group, 1);
}

//----------------------------------------------
/* init function */
int apusys_sched_init(void)
{
	int ret = 0;

	mdw_flw_debug("%s +\n", __func__);
	sched_ws_init();

	memset(&g_ctx_mgr, 0, sizeof(struct mem_ctx_mgr));
	mutex_init(&g_ctx_mgr.mtx);

	memset(&g_pack_mgr, 0, sizeof(struct pack_cmd_mgr));
	INIT_LIST_HEAD(&g_pack_mgr.pc_list);

	ret = thread_pool_init(exec_cmd_func);
	if (ret) {
		mdw_drv_err("init thread pool fail(%d)\n", ret);
		return ret;
	}

	sched_task = kthread_run(sched_routine,
		(void *)res_get_mgr(), "apusys_sched");
	if (sched_task == NULL) {
		mdw_drv_err("create kthread(sched) fail\n");
		return -ENOMEM;
	}

	mdw_flw_debug("%s done -\n", __func__);
	return 0;
}

int apusys_sched_destroy(void)
{
	int ret = 0;

	mdw_flw_debug("%s +\n", __func__);

	/* destroy thread pool */
	ret = thread_pool_destroy();
	if (ret)
		mdw_drv_err("destroy thread pool fail(%d)\n", ret);

	mdw_flw_debug("%s done -\n", __func__);
	return 0;
}
