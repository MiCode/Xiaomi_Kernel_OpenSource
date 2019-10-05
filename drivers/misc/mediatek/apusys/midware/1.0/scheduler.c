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

#include "apusys_cmn.h"
#include "apusys_device.h"
#include "cmd_parser.h"
#include "resource_mgt.h"
#include "scheduler.h"
#include "cmd_parser_mdla.h"
#include "thread_pool.h"
#include "midware_trace.h"
#include "cmd_format.h"

#include "mnoc_api.h"

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
static struct task_struct *sche_task;

//----------------------------------------------
struct mem_ctx_mgr {
	struct mutex mtx;
	unsigned long ctx[BITS_TO_LONGS(32)];
};

static struct mem_ctx_mgr g_ctx_mgr;

static int mem_alloc_ctx(void)
{
	int ctx = 0;

	mutex_lock(&g_ctx_mgr.mtx);
	ctx = find_first_zero_bit(g_ctx_mgr.ctx, 32);
	if (ctx >= 32)
		ctx = -1;
	else
		bitmap_set(g_ctx_mgr.ctx, ctx, 1);

	mutex_unlock(&g_ctx_mgr.mtx);

	return ctx;
}

static int mem_free_ctx(int ctx)
{
	if (ctx == VALUE_SUBGAPH_CTX_ID_NONE)
		return 0;

	mutex_lock(&g_ctx_mgr.mtx);
	if (!test_bit(ctx, g_ctx_mgr.ctx))
		LOG_ERR("ctx id confuse, idx(%d) is not set\n", ctx);
	bitmap_clear(g_ctx_mgr.ctx, ctx, 1);
	mutex_unlock(&g_ctx_mgr.mtx);

	return 0;
}

//----------------------------------------------
static int alloc_ctx(struct apusys_subcmd *sc, struct apusys_cmd *cmd)
{
	int ctx_id = -1;

	if (sc->ctx_group == VALUE_SUBGAPH_CTX_ID_NONE) {
		ctx_id = mem_alloc_ctx();
		if (ctx_id < 0)
			return -EINVAL;
		sc->ctx_id = ctx_id;
		LOG_DEBUG("0x%llx-#%d sc ctx_group(0x%x) ctx(%d)\n",
			cmd->cmd_id, sc->idx,
			sc->ctx_group, cmd->ctx_list[sc->ctx_group]);
	} else {
		if (cmd->ctx_list[sc->ctx_group] == VALUE_SUBGAPH_CTX_ID_NONE) {
			ctx_id = mem_alloc_ctx();
			if (ctx_id < 0)
				return -EINVAL;

			cmd->ctx_list[sc->ctx_group] = ctx_id;
			LOG_DEBUG("0x%llx-#%d sc ctx_group(0x%x) ctx(%d)\n",
				cmd->cmd_id, sc->idx,
				sc->ctx_group, cmd->ctx_list[sc->ctx_group]);
		}
		sc->ctx_id = cmd->ctx_list[sc->ctx_group];
	}

	LOG_DEBUG("0x%llx-#%d sc ctx_group(0x%x) id(%d)\n",
		cmd->cmd_id, sc->idx,
		sc->ctx_group, sc->ctx_id);

	return 0;
}

static int free_ctx(struct apusys_subcmd *sc, struct apusys_cmd *cmd)
{
	int ret = 0;

	if (sc->ctx_group == VALUE_SUBGAPH_CTX_ID_NONE) {
		LOG_DEBUG("0x%llx-#%d sc ctx_group(0x%x) free id(%d)\n",
			cmd->cmd_id, sc->idx,
			sc->ctx_group, sc->ctx_id);
		mem_free_ctx(sc->ctx_id);
		sc->ctx_id = VALUE_SUBGAPH_CTX_ID_NONE;
	} else {
		cmd->ctx_ref[sc->ctx_group]--;
		if (cmd->ctx_ref[sc->ctx_group] == 0) {
			LOG_DEBUG("0x%llx-#%d sc ctx_group(0x%x) free id(%d)\n",
				cmd->cmd_id, sc->idx,
				sc->ctx_group, sc->ctx_id);
			mem_free_ctx(cmd->ctx_list[sc->ctx_group]);
		}
		sc->ctx_id = VALUE_SUBGAPH_CTX_ID_NONE;
	}

	return ret;
}

static int insert_pack_cmd(struct apusys_subcmd *sc, struct pack_cmd **ipc)
{
	struct apusys_cmd *cmd = NULL;
	struct pack_cmd *pc = NULL;
	struct apusys_subcmd *tmp_sc = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	int next_idx = 0, ret = 0, bit0 = 0, bit1 = 0;

	if (sc == NULL)
		return -EINVAL;

	cmd = (struct apusys_cmd *)sc->parent_cmd;

	LOG_DEBUG("pack sc(0x%llx/%d/%d)", cmd->cmd_id, sc->idx, sc->pack_idx);

	list_add_tail(&sc->pc_list, &cmd->pc_col.sc_list);
	DEBUG_TAG;

	/* packed cmd collect done */

	LOG_DEBUG("before : pack bit check(%d/%d)(%d/%d)\n",
		sc->idx, sc->pack_idx,
		test_bit(sc->idx, cmd->pc_col.pack_status),
		test_bit(sc->pack_idx, cmd->pc_col.pack_status));

	bit0 = test_and_change_bit(sc->idx, cmd->pc_col.pack_status);
	bit1 = test_and_change_bit(sc->pack_idx, cmd->pc_col.pack_status);
	if (bit0 && bit1) {
		LOG_INFO("pack cmd satified(0x%llx)(%d/%d)\n",
			cmd->cmd_id, sc->idx, sc->pack_idx);
		pc = kzalloc(sizeof(struct pack_cmd), GFP_KERNEL);
		if (pc == NULL) {
			LOG_ERR("alloc packcmd(0x%llx/%d) for execute fail\n",
				cmd->cmd_id, sc->idx);
			/* TODO, error handling for cmd list added already */
			return -ENOMEM;
		}

		INIT_LIST_HEAD(&pc->acq.dev_info_list);
		INIT_LIST_HEAD(&pc->sc_list);
		pc->dev_type = sc->type;

		/* find all packed subcmd id */
		/* TODO, guarntee can find all packed cmd*/
		/* now just query one round, user must set in order */
		next_idx = sc->pack_idx;
		list_for_each_safe(list_ptr, tmp, &cmd->pc_col.sc_list) {
			tmp_sc = list_entry(list_ptr,
				struct apusys_subcmd, pc_list);
			LOG_DEBUG("find pack idx(%d/%d)\n",
				next_idx, tmp_sc->idx);
			if (tmp_sc->idx == next_idx) {
				next_idx = tmp_sc->pack_idx;
				list_del(&tmp_sc->pc_list);
				list_add_tail(&tmp_sc->pc_list, &pc->sc_list);
				pc->sc_num++;
			}
		}

		if (tmp_sc->idx != sc->pack_idx) {
			LOG_WARN("pack idx error, (%d->%d)\n",
				tmp_sc->idx, sc->pack_idx);
		}

		/* return pack cmd after all packed sc ready */
		*ipc = pc;
		ret = pc->sc_num;
	}

	LOG_DEBUG("after : pack bit check(%d/%d)(%d/%d)\n",
		sc->idx, sc->pack_idx,
		test_bit(sc->idx, cmd->pc_col.pack_status),
		test_bit(sc->pack_idx, cmd->pc_col.pack_status));

	return ret;
}

static int exec_pack_cmd(void *iacq)
{
	struct apusys_dev_aquire *acq = (struct apusys_dev_aquire *)iacq;
	struct pack_cmd *pc = (struct pack_cmd *)acq->user;
	struct apusys_cmd *cmd = NULL;
	struct apusys_subcmd *sc = NULL;
	struct apusys_dev_info *info = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	int count = 0;

	if (acq == NULL || pc == NULL)
		return -EINVAL;

	list_for_each_safe(list_ptr, tmp, &acq->dev_info_list) {
		info = list_entry(list_ptr, struct apusys_dev_info, acq_list);
		list_del(&info->acq_list);
		if (list_empty(&pc->sc_list)) {
			LOG_ERR("pack cmd and device(%d) is not same number!\n",
				info->dev->dev_type);
			if (put_apusys_device(info->dev)) {
				LOG_ERR("put device(%d/%d) fail\n",
					info->dev->dev_type, info->dev->idx);
			}
		} else {
			sc = (struct apusys_subcmd *)list_first_entry
				(&pc->sc_list, struct apusys_subcmd, pc_list);
			list_del(&sc->pc_list);
			cmd = (struct apusys_cmd *)sc->parent_cmd;
			count++;
			DEBUG_TAG;
			if (thread_pool_trigger(sc, info->dev)) {
				LOG_ERR("tp cmd(0x%llx/%d)dev(%d/%d) fail\n",
					cmd->cmd_id, sc->idx,
					info->dev->dev_type, info->dev->idx);
			}
		}
	}

	if (count != pc->sc_num) {
		LOG_ERR("execute pack cmd(%d/%d) number issue\n",
		count, pc->sc_num);
	}

	/* destroy pack_cmd and dev_acquire */
	kfree(pc);

	return 0;
}

static void subcmd_done(void *isc)
{
	struct apusys_subcmd *sc = (struct apusys_subcmd *)isc;
	struct apusys_cmd *cmd = (struct apusys_cmd *)sc->parent_cmd;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_subcmd *sc_node = NULL;
	int ret = 0, done_idx = 0, state = 0;
	struct apusys_res_mgr *res_mgr = resource_get_mgr();

	if (sc == NULL) {
		LOG_ERR("invalid argument(%p)\n", sc);
		return;
	}

	DEBUG_TAG;

	mutex_lock(&cmd->mtx);

	if (free_ctx(sc, cmd))
		LOG_ERR("free memory ctx id fail\n");
	DEBUG_TAG;

	/* check sc state and delete */
	mutex_lock(&sc->mtx);
	done_idx = sc->idx;
	sc->state = CMD_STATE_DONE;
	kfree(sc->dp_status);
	mutex_unlock(&sc->mtx);
	if (sc->u_time)
		sc->u_time = get_time_from_system() - sc->u_time;
	/* write time back to cmdbuf */
	set_dtime_to_subcmd(sc->entry, sc->d_time);
	set_utime_to_subcmd(sc->entry, sc->u_time);
	LOG_DEBUG("0x%llx-#%d sc: time(%llu/%llu) bw(%llu)\n",
		cmd->cmd_id, sc->idx, sc->d_time, sc->u_time, sc->bw);

	kfree(sc);
	DEBUG_TAG;

	/* clear subcmd bit in cmd entry's status */
	bitmap_clear(cmd->sc_status, done_idx, 1);
	if (bitmap_empty(cmd->sc_status, cmd->sc_num)) {
		cmd->state = CMD_STATE_DONE;
		state = cmd->state;
	}
	mutex_lock(&cmd->sc_mtx);

	/* should insert subcmd which dependency satisfied */
	list_for_each_safe(list_ptr, tmp, &cmd->sc_list) {
		sc_node = list_entry(list_ptr, struct apusys_subcmd, ce_list);
		LOG_DEBUG("check dependency satified(0x%x/%d)\n",
			*sc_node->dp_status, done_idx);
		mutex_lock(&res_mgr->mtx);
		mutex_lock(&sc_node->mtx);
		bitmap_clear(sc_node->dp_status, done_idx, 1);

		if (bitmap_empty(sc_node->dp_status, cmd->sc_num)) {
			LOG_DEBUG("sc(%d/%p) dp satified, insert to queue(%d)n",
				sc_node->idx, sc_node, cmd->priority);
			list_del(&sc_node->ce_list);
			ret = insert_subcmd(sc_node, cmd->priority);
			if (ret) {
				LOG_ERR("insert sc(%p/%p) to q(%d/%d) fail\n",
					sc_node->entry, sc_node,
					sc_node->type, cmd->priority);
			}
		}
		mutex_unlock(&sc_node->mtx);
		mutex_unlock(&res_mgr->mtx);
	}
	mutex_unlock(&cmd->sc_mtx);
	mutex_unlock(&cmd->mtx);
	DEBUG_TAG;

	/* if whole apusys cmd done, wakeup user context thread */
	if (state == CMD_STATE_DONE) {
		LOG_DEBUG("apusys cmd(%p/0x%llx) done\n", cmd, cmd->cmd_id);
		complete(&cmd->comp);
	}
}

static int exec_cmd_func(void *isc, void *idev)
{
	int dev_type = APUSYS_DEVICE_NONE;
	struct apusys_device *dev = (struct apusys_device *)idev;
	struct apusys_cmd_hnd cmd_hnd;
	struct apusys_subcmd *sc = (struct apusys_subcmd *)isc;
	struct apusys_cmd *cmd = NULL;
	int i = 0, ret = 0;

	if (isc == NULL || idev == NULL) {
		ret = -EINVAL;
		goto out;
	}

	LOG_DEBUG("execute routine sc(%p/%p)\n", sc, sc->entry);
	cmd = (struct apusys_cmd *)sc->parent_cmd;
	memset(&cmd_hnd, 0, sizeof(cmd_hnd));

	/* get subcmd information */
	cmd_hnd.kva = get_addr_from_subcmd(sc->entry);
	cmd_hnd.size = get_size_from_subcmd(sc->entry);
	cmd_hnd.boost_val = 100;
	cmd_hnd.cmd_id = cmd->cmd_id;
	cmd_hnd.cmd_entry = (uint64_t)cmd->kva;
	if (cmd_hnd.kva == 0 || cmd_hnd.size == 0) {
		LOG_ERR("invalid sc(%d)(0x%llx/%d)\n",
			i, cmd_hnd.kva, cmd_hnd.size);
		ret = -EINVAL;
		goto out;
	}

	/* fill specific subcmd info */
	if (sc->type == APUSYS_DEVICE_MDLA) {
		if (parse_mdla_sg((struct apusys_cmd *)sc->parent_cmd,
			(void *)sc, &cmd_hnd))
			LOG_ERR("fill mdla specific info fail\n");
	}

	/* call execute */
	midware_trace_begin(sc->idx);

	LOG_DEBUG("execute cmd hnd (%d/0x%llx/%d) boost(%d)\n",
		sc->type, cmd_hnd.kva,
		cmd_hnd.size, cmd_hnd.boost_val);

	/* 1. allocate memory ctx id*/
	mutex_lock(&cmd->mtx);
	if (alloc_ctx(sc, cmd))
		LOG_ERR("allocate memory ctx id(%d) fail\n", sc->ctx_id);
	mutex_unlock(&cmd->mtx);

	mutex_lock(&sc->mtx);
	sc->state = CMD_STATE_RUN;

	/* 2. get driver time start */
	LOG_INFO("exec sc(%d/%p/%d) cmd(0x%llx) on dev(%d/%p)\n",
		sc->idx, sc, sc->type,
		cmd->cmd_id, dev->dev_type, dev);

	if (sc->d_time)
		sc->d_time = get_time_from_system();

	/* 3. start count cmd qos */
	LOG_DEBUG("mnoc: cmd qos start(0x%llx/%d/%d/%d)\n",
		cmd->cmd_id, sc->idx,
		sc->type, dev->idx);
	if (apu_cmd_qos_start(cmd->cmd_id, sc->idx, sc->type, dev->idx)) {
		LOG_ERR("start qos for cmd(0x%llx/%d) fail\n",
			cmd->cmd_id, sc->idx);
	}

	/* 4. execute subcmd */
	ret = dev->send_cmd(APUSYS_CMD_EXECUTE, (void *)&cmd_hnd, dev);

	/* count qos end */
	sc->bw = apu_cmd_qos_end(cmd->cmd_id, sc->idx);
	LOG_DEBUG("mnoc: cmd qos end(0x%llx/%d/%d/%d), bw(%d)\n",
		cmd->cmd_id, dev->idx, sc->type,
		dev->idx, sc->bw);

	if (sc->d_time)
		sc->d_time = get_time_from_system() - sc->d_time;

	if (ret) {
		LOG_ERR("execute cmd(0x%llx/%d) on devfail[%d/%d/%p]\n",
			cmd->cmd_id,
			sc->idx,
			dev_type,
			dev->idx,
			dev);
		cmd->cmd_ret = ret;
		mutex_unlock(&sc->mtx);
		if (put_device_lock(dev)) {
			LOG_ERR("return device(%d/%d/%p) fail\n",
				dev->dev_type,
				dev->idx,
				dev);
			ret = -EINVAL;
			goto out;
		}
		ret = -EINVAL;
		goto out;
	} else {
		mutex_unlock(&sc->mtx);
	}

	// put back device
	if (put_device_lock(dev)) {
		LOG_ERR("return device(%d/%d/%p) fail\n",
			dev->dev_type,
			dev->idx,
			dev);
		ret = -EINVAL;
		goto out;
	}

out:
	midware_trace_end(ret);
	LOG_DEBUG("wakeup user context thread\n");
	subcmd_done(sc);

	return ret;
}

int sche_routine(void *arg)
{
	int ret = 0, type = 0, dev_num = 0;
	unsigned long available[BITS_TO_LONGS(APUSYS_DEV_TABLE_MAX)];
	struct apusys_subcmd *sc = NULL;
	struct apusys_res_mgr *res_mgr = NULL;
	struct apusys_dev_aquire acq, *acq_async = NULL;
	struct apusys_device *dev = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_dev_info *info = NULL;
	struct apusys_cmd *cmd = NULL;
	struct pack_cmd *pc = NULL;

	res_mgr = (struct apusys_res_mgr *)arg;

	while (!kthread_should_stop() && !res_mgr->sched_stop) {
		ret = wait_for_completion_interruptible(&res_mgr->sched_comp);
		if (ret)
			LOG_WARN("sched thread(%d)\n", ret);

		DEBUG_TAG;

		bitmap_and(available, res_mgr->cmd_exist,
			res_mgr->dev_exist, APUSYS_DEV_TABLE_MAX);
		/* if dev/cmd available or  */
		while (!bitmap_empty(available, APUSYS_DEV_TABLE_MAX)
			|| acquire_device_check(&acq_async) == 0) {

			DEBUG_TAG;
			mutex_lock(&res_mgr->mtx);

			/* check any device acq ready for packcmd */
			if (acq_async != NULL) {
				LOG_DEBUG("get pack cmd(%d) ready",
					acq_async->dev_type);
				/* exec packcmd */
				if (exec_pack_cmd(acq_async))
					LOG_ERR("execute pack cmd fail\n");

				goto sched_retrigger;
			}

			/* cmd/dev not same bit available, continue */
			if (bitmap_empty(available, APUSYS_DEV_TABLE_MAX))
				goto sched_retrigger;

			/* get type from available */
			type = find_first_bit(available, APUSYS_DEV_TABLE_MAX);
			if (type >= APUSYS_DEV_TABLE_MAX) {
				LOG_WARN("find first bit for type(%d) fail\n",
					type);
				goto sched_retrigger;
			}

			/* pop cmd from priority queue */
			ret = pop_subcmd(type, (void **)&sc);
			if (ret) {
				LOG_ERR("pop subcmd for dev(%d) fail\n", type);
				goto sched_retrigger;
			}

			cmd = (struct apusys_cmd *)sc->parent_cmd;

			/* if sc is packed, collect all packed cmd and send */
			if (sc->pack_idx != VALUE_SUBGAPH_PACK_ID_NONE) {
				if (insert_pack_cmd(sc, &pc) <= 0)
					goto sched_retrigger;

				LOG_DEBUG("packcmd done, insert acq(%d/%d)\n",
					pc->dev_type,
					pc->sc_num);
				pc->acq.target_num = pc->sc_num;
				pc->acq.owner = APUSYS_DEV_OWNER_SCHEDULER;
				pc->acq.dev_type = pc->dev_type;
				pc->acq.user = (void *)pc;
				pc->acq.is_done = 0;
				ret = acquire_device_async(&pc->acq);
				if (ret < 0) {
					LOG_ERR("acq dev(%d/%d) fail\n",
						pc->acq.dev_type,
						pc->acq.target_num);
				} else {
					if (pc->acq.acq_num ==
						pc->acq.target_num) {
						LOG_INFO("execute pack cmd\n");
						exec_pack_cmd(&pc->acq);
					}
				}
				goto sched_retrigger;
			}

			DEBUG_TAG;
			/* get device */
			dev_num = 1;
			memset(&acq, 0, sizeof(acq));
			acq.target_num = dev_num;
			acq.dev_type = type;
			acq.owner = APUSYS_DEV_OWNER_SCHEDULER;
			INIT_LIST_HEAD(&acq.dev_info_list);
			INIT_LIST_HEAD(&acq.tab_list);
			ret = acquire_device_try(&acq);
			if (ret < 0 || acq.acq_num <= 0) {
				LOG_ERR("no dev(%d) available\n", type);
				mutex_lock(&sc->mtx);
				/* can't get device, insert sc back */
				if (insert_subcmd(sc, cmd->priority)) {
					LOG_ERR("re sc(%p) devq(%d/%d) fail\n",
						sc,
						type,
						cmd->priority);
				}
				mutex_unlock(&sc->mtx);
				goto sched_retrigger;
			}

			DEBUG_TAG;

			list_for_each_safe(list_ptr, tmp, &acq.dev_info_list) {
				info = list_entry(list_ptr,
					struct apusys_dev_info, acq_list);
				dev = info->dev;
				ret = thread_pool_trigger(sc, dev);
				if (ret)
					LOG_ERR("trigger thread pool fail\n");
			}
sched_retrigger:
			bitmap_and(available, res_mgr->cmd_exist,
				res_mgr->dev_exist, APUSYS_DEV_TABLE_MAX);
			acq_async = NULL;
			mutex_unlock(&res_mgr->mtx);
		}
	}

	LOG_WARN("scheduling thread stop\n");

	return 0;
}

int apusys_sched_wait_cmd(struct apusys_cmd *cmd)
{
	int ret = 0, state = -1, i = 0;
	struct apusys_subcmd *sc = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_res_mgr *res_mgr = resource_get_mgr();

	if (cmd == NULL)
		return -EINVAL;

	/* wait all subcmd completed */
	mutex_lock(&cmd->sc_mtx);
	state = cmd->state;
	mutex_unlock(&cmd->sc_mtx);

	if (state == CMD_STATE_DONE)
		return ret;

	ret = wait_for_completion_interruptible(&cmd->comp);
	if (ret) {
		LOG_ERR("user ctx int(%d), error handle...\n",
			ret);
		if (cmd->state == CMD_STATE_DONE)
			return 0;

		/* delete all subcmd in cmd */
		mutex_lock(&cmd->mtx);
		mutex_lock(&cmd->sc_mtx);
		list_for_each_safe(list_ptr, tmp, &cmd->sc_list) {
			sc = list_entry(list_ptr,
				struct apusys_subcmd, ce_list);

			mutex_lock(&res_mgr->mtx);
			mutex_lock(&sc->mtx);
			if (sc->state < CMD_STATE_RUN) {
				/* delete from cmd */
				list_del(&sc->ce_list);
				bitmap_clear(cmd->sc_status, sc->idx, 1);

				/* delete subcmd from priority q */
				if (delete_subcmd((void *)sc)) {
					LOG_ERR("delete sc(0x%llx/%d) fail\n",
						cmd->cmd_id, sc->idx);
				}
				if (free_ctx(sc, cmd))
					LOG_ERR("free memory ctx id fail\n");

				if (sc->dp_status) {
					DEBUG_TAG;
					kfree(sc->dp_status);
				}

				mutex_unlock(&sc->mtx);
				mutex_unlock(&res_mgr->mtx);

				kfree(sc);
			} else {
				mutex_unlock(&sc->mtx);
				mutex_unlock(&res_mgr->mtx);
			}
		}
		mutex_unlock(&cmd->sc_mtx);
		mutex_unlock(&cmd->mtx);

		for (i = 0; i < 30; i++) {
			if (bitmap_empty(cmd->sc_status, cmd->sc_num)) {
				LOG_INFO("delete cmd safely\n");
				break;
			}
			LOG_INFO("sleep 200ms to wait sc done\n");
			msleep(200);
		}
	}
	DEBUG_TAG;

	return ret;
}

int apusys_sched_add_list(struct apusys_cmd *cmd)
{
	int ret = 0, i = 0, type = 0, j = 0;
	void *sc_entry = NULL;
	struct apusys_subcmd *sc = NULL;
	uint32_t *dp_ptr = NULL;
	uint32_t dp_num = 0;

	if (cmd == NULL) {
		ret = -EINVAL;
		goto out;
	}

	dp_ptr = (uint32_t *)cmd->dp_entry;

	/* 1. add all independent subcmd to corresponding queue */
	while (i < cmd->sc_num) {
		dp_num = *dp_ptr;

		LOG_DEBUG("0x%llx-#%d sc (%p/%d)\n",
			cmd->cmd_id, i, dp_ptr, dp_num);
		/* get subcmd */
		sc_entry = (void *)get_subcmd_by_idx(cmd, i);
		if (sc_entry == 0) {
			LOG_ERR("get #%d subcmd from cmd(0x%llx) fail\n",
				i, cmd->cmd_id);
			ret = -EINVAL;
			break;
		}

		/* get type */
		type = (int)get_type_from_subcmd(sc_entry);

		/* allocate subcmd struct */
		if (apusys_subcmd_create(sc_entry, cmd, &sc)) {
			LOG_ERR("create sc for cmd(%d/%p) fail\n", i, cmd);
			ret = -EINVAL;
			break;
		}
		sc->idx = i;
		LOG_DEBUG("0x%llx-#%d sc(%p/%d)dp(%p/%d)set(%d/%d/%d)\n",
			cmd->cmd_id,
			sc->idx,
			sc, sc->idx,
			dp_ptr,
			dp_num,
			sc->boost_val,
			sc->exec_ms,
			sc->state);

		/* setup subcmd's dependency */
		for (j = 0; j < dp_num; j++) {
			dp_ptr += 1;
			LOG_DEBUG("mark sc(%d) dp(%d) entry(%p)\n",
				i, *dp_ptr, sc_entry);
			bitmap_set(sc->dp_status, *dp_ptr, 1);
		}

		mutex_lock(&cmd->sc_mtx);

		if (bitmap_and(sc->dp_status, sc->dp_status,
			cmd->sc_status, cmd->sc_num)) {
			LOG_ERR("AND dp sc status fail(%p/%p)(%d)(0x%x/0x%x)\n",
				sc,
				cmd,
				cmd->sc_num,
				*sc->dp_status,
				*cmd->sc_status);
		}

		/* add sc to cmd's sc_list*/
		list_add_tail(&sc->ce_list, &cmd->sc_list);

		if (bitmap_empty(sc->dp_status, cmd->sc_num)) {
			/* insert to type priority queue */
			ret = insert_subcmd_lock(sc, cmd->priority);
			if (ret) {
				LOG_ERR("insert sc(%p/%p) to q(%d/%d) fail\n",
					sc->entry, sc, type, cmd->priority);
				mutex_unlock(&cmd->sc_mtx);
				goto out;
			}
		}

		mutex_unlock(&cmd->sc_mtx);

		dp_ptr += 1;
		i++;
	}

	DEBUG_TAG;

out:
	return ret;
}


//----------------------------------------------
/* init function */
int apusys_sched_init(void)
{
	int ret = 0;

	LOG_INFO("%s +\n", __func__);

	memset(&g_ctx_mgr, 0, sizeof(struct mem_ctx_mgr));
	mutex_init(&g_ctx_mgr.mtx);

	memset(&g_pack_mgr, 0, sizeof(struct pack_cmd_mgr));
	INIT_LIST_HEAD(&g_pack_mgr.pc_list);

	ret = thread_pool_init(exec_cmd_func);
	if (ret) {
		LOG_ERR("init thread pool fail(%d)\n", ret);
		return ret;
	}

	sche_task = kthread_run(sche_routine,
		(void *)resource_get_mgr(), "apusys_sched");
	if (sche_task == NULL) {
		LOG_ERR("create kthread(sched) fail\n");
		return -ENOMEM;
	}

	LOG_INFO("%s done -\n", __func__);
	return 0;
}

int apusys_sched_destroy(void)
{
	int ret = 0;

	/* destroy thread pool */
	ret = thread_pool_destroy();
	if (ret)
		LOG_ERR("destroy thread pool fail(%d)\n", ret);

	LOG_INFO("%s +\n", __func__);
	LOG_INFO("%s done -\n", __func__);
	return 0;
}
