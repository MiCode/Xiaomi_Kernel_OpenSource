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
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>

#include "mdw_cmn.h"
#include "thread_pool.h"
#include "cmd_parser.h"

struct thread_pool_inst {
	/* work thread used */
	struct task_struct *task;
	struct mutex mtx;

	/* list with thread pool mgr */
	struct list_head list; // link to thread_pool_mgr's thread_list

	void *sc;
	int idx;
	int status;

	bool stop;
};

struct job_inst {
	void *sc; // should be struct apusys_subcmd
	void *dev_info;
	struct list_head list; // link to thread_pool_mgr's job_list
};

struct thread_pool_mgr {
	/* thread info */
	struct list_head thread_list;
	struct mutex mtx;

	uint32_t total;
	struct completion comp;

	routine_func func_ptr;

	/* for job queue */
	struct list_head job_list;
	struct mutex job_mtx;
};

static struct thread_pool_mgr g_pool_mgr;

void thread_pool_set_group(void)
{
	struct file *fd;
	char buf[8];
	mm_segment_t oldfs;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct thread_pool_inst *inst = NULL;

	oldfs = get_fs();
	set_fs(get_ds());

	fd = filp_open(APUSYS_THD_TASK_FILE_PATH, O_WRONLY, 0);
	if (IS_ERR(fd)) {
		mdw_drv_debug("don't support low latency group\n");
		goto out;
	}

	mutex_lock(&g_pool_mgr.mtx);
	/* query all thread inst to mark stop */
	list_for_each_safe(list_ptr, tmp, &g_pool_mgr.thread_list) {
		inst = list_entry(list_ptr, struct thread_pool_inst, list);
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf)-1, "%d", inst->task->pid);
		vfs_write(fd, (__force const char __user *)buf,
			sizeof(buf), &fd->f_pos);
		mdw_drv_debug("setup worker(%d/%s) to group\n",
			inst->task->pid, buf);
	}
	mutex_unlock(&g_pool_mgr.mtx);

	filp_close(fd, NULL);
out:
	set_fs(oldfs);
}

static int tp_service_routine(void *arg)
{
	struct thread_pool_inst *inst = (struct thread_pool_inst *)arg;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct job_inst *job_arg = NULL;
	int ret = 0;

	if (inst == NULL) {
		mdw_drv_err("invalid argument, thread end\n");
		return -EINVAL;
	}

	while (!kthread_should_stop() && inst->stop != true) {
		ret = wait_for_completion_interruptible(&g_pool_mgr.comp);
		if (ret) {
			switch (ret) {
			case -ERESTARTSYS:
				mdw_drv_err("restart...\n");
				/* TODO: error handle, retry? */
				msleep(50);
				break;
			default:
				mdw_drv_err("thread interruptible(%d)\n", ret);
				/* TODO: error handle */
				break;
			}
			continue;
		}

		/* 1. get cmd from job_list */
		mutex_lock(&g_pool_mgr.job_mtx);

		/* query list to find mem in apusys user */
		job_arg = NULL;
		list_for_each_safe(list_ptr, tmp, &g_pool_mgr.job_list) {
			job_arg = list_entry(list_ptr, struct job_inst, list);
			list_del(&job_arg->list);
			break;
		}
		mutex_unlock(&g_pool_mgr.job_mtx);
		if (job_arg == NULL)
			continue;

		mdw_flw_debug("thread(%d) execute sc(%p)\n",
			inst->idx, job_arg->sc);

		/* 2. execute cmd */
		mutex_lock(&inst->mtx);
		inst->status = APUSYS_THREAD_STATUS_BUSY;
		/* execute cmd */
		inst->sc = job_arg->sc;
		ret = g_pool_mgr.func_ptr(job_arg->sc, job_arg->dev_info);
		if (ret) {
			mdw_drv_err("process arg(%p/%d) fail\n",
				job_arg->sc, ret);
		}

		kfree(job_arg);
		inst->sc = NULL;
		inst->status = APUSYS_THREAD_STATUS_IDLE;
		mutex_unlock(&inst->mtx);
	}

	/* delete itself inst */
	mutex_lock(&g_pool_mgr.mtx);
	mdw_drv_info("thread(%d) end\n", inst->idx);
	list_del(&inst->list);
	kfree(inst);
	g_pool_mgr.total--;
	mutex_unlock(&g_pool_mgr.mtx);

	return 0;
}

void thread_pool_dump(void)
{
	struct thread_pool_inst *inst = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_subcmd *sc = NULL;
	struct apusys_cmd *cmd = NULL;
	int i = 0;

	mutex_lock(&g_pool_mgr.mtx);

	mdw_drv_debug("=====================================\n");
	mdw_drv_debug("| apusys thread pool status: total(%d)\n",
		g_pool_mgr.total);
	list_for_each_safe(list_ptr, tmp, &g_pool_mgr.thread_list) {
		inst = list_entry(list_ptr, struct thread_pool_inst, list);
		mdw_drv_debug("-------------------------------------\n");
		sc = (struct apusys_subcmd *)inst->sc;
		if (sc == NULL)
			continue;
		cmd = sc->par_cmd;

		mdw_drv_debug(" thread idx = %d\n", i);
		mdw_drv_debug(" status     = %d\n", inst->status);
		if (sc == NULL || inst->status == APUSYS_THREAD_STATUS_IDLE)
			continue;
		mdw_drv_debug(" cmd        = 0x%llx\n", cmd->cmd_id);
		mdw_drv_debug(" subcmd     = %d/%p\n", sc->idx, sc);
		mdw_drv_debug(" stop       = %d\n", inst->stop);
	}
	mdw_drv_debug("=====================================\n");

	mutex_unlock(&g_pool_mgr.mtx);

}

int thread_pool_trigger(void *sc, void *dev_info)
{
	struct job_inst *job_arg = NULL;

	/* check argument */
	if (sc == NULL)
		return -EINVAL;

	/* 1. add cmd to job queue */
	job_arg = kzalloc(sizeof(struct job_inst), GFP_KERNEL);
	if (job_arg == NULL)
		return -ENOMEM;

	job_arg->sc = sc;
	job_arg->dev_info = dev_info;
	mdw_flw_debug("add to thread pool's job queue(%p)\n", sc);
	mutex_lock(&g_pool_mgr.job_mtx);
	list_add_tail(&job_arg->list, &g_pool_mgr.job_list);
	mutex_unlock(&g_pool_mgr.job_mtx);

	/* 2. hint thread pool dispatch one thread to service */
	complete(&g_pool_mgr.comp);

	return 0;
}

int thread_pool_add_once(void)
{
	struct thread_pool_inst *inst = NULL;
	char name[32];

	inst = kzalloc(sizeof(struct thread_pool_inst), GFP_KERNEL);
	if (inst == NULL)
		return -ENOMEM;

	mutex_init(&inst->mtx);
	INIT_LIST_HEAD(&inst->list);

	memset(name, 0, sizeof(name));
	/* critical seesion */
	mutex_lock(&g_pool_mgr.mtx);
	snprintf(name, sizeof(name)-1, "apusys_worker%d", g_pool_mgr.total);
	inst->status = APUSYS_THREAD_STATUS_IDLE;
	inst->idx = g_pool_mgr.total;

	inst->task = kthread_run(tp_service_routine, inst, name);
	if (inst->task == NULL) {
		mdw_drv_err("create kthread(%d) fail\n", g_pool_mgr.total);
		kfree(inst);
		mutex_unlock(&g_pool_mgr.mtx);
		return -ENOMEM;
	}

	g_pool_mgr.total++;

	/* add to global mgr to store */
	list_add_tail(&inst->list, &g_pool_mgr.thread_list);
	mutex_unlock(&g_pool_mgr.mtx);
	return 0;
}

int thread_pool_delete(int num)
{
	int i = 0;
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct thread_pool_inst *inst = NULL;

	mutex_lock(&g_pool_mgr.mtx);

	list_for_each_safe(list_ptr, tmp, &g_pool_mgr.thread_list) {
		inst = list_entry(list_ptr, struct thread_pool_inst, list);
		inst->stop = true;

		/* stop thread mechanism */
		if (i >= num)
			break;
	}

	complete_all(&g_pool_mgr.comp);
	mutex_unlock(&g_pool_mgr.mtx);

	mdw_drv_debug("delete %d thread from pool\n", i);

	return 0;
}

int thread_pool_init(routine_func func_ptr)
{
	if (func_ptr == NULL)
		return -EINVAL;

	/* clean mgr */
	memset(&g_pool_mgr, 0, sizeof(struct thread_pool_mgr));

	/* init all list and mtx */
	mutex_init(&g_pool_mgr.mtx);
	INIT_LIST_HEAD(&g_pool_mgr.thread_list);
	mutex_init(&g_pool_mgr.job_mtx);
	INIT_LIST_HEAD(&g_pool_mgr.job_list);
	init_completion(&g_pool_mgr.comp);

	/* assign callback func*/
	g_pool_mgr.func_ptr = func_ptr;

	return 0;
}

int thread_pool_destroy(void)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct thread_pool_inst *inst = NULL;

	mutex_lock(&g_pool_mgr.mtx);
	/* query all thread inst to mark stop */
	list_for_each_safe(list_ptr, tmp, &g_pool_mgr.thread_list) {
		inst = list_entry(list_ptr, struct thread_pool_inst, list);
		inst->stop = true;
	}

	complete_all(&g_pool_mgr.comp);
	mutex_unlock(&g_pool_mgr.mtx);

	return 0;
}

