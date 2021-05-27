// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/module.h>

#include "mtk-mml-core.h"

int mtk_mml_msg;
EXPORT_SYMBOL(mtk_mml_msg);
module_param(mtk_mml_msg, int, 0644);

extern s32 cmdq_pkt_flush_async(struct cmdq_pkt *pkt,
    cmdq_async_flush_cb cb, void *data);
static void core_taskdone(struct work_struct *work)
{
	struct mml_task *task;

	task = container_of(work, struct mml_task, work_wait);

	task->config->task_ops->frame_done(task);
}

static void core_taskdone_cb(struct cmdq_cb_data data)
{
	struct mml_task *task = (struct mml_task *)data.data;

	queue_work(task->config->wq_wait, &task->work_wait);
}

static s32 core_config(struct mml_task *task, u32 pipe_id)
{
	return 0;
}

static s32 core_flush(struct mml_task *task, u32 pipe_id)
{
	s32 err;

	err = cmdq_pkt_flush_async(&task->pkts[pipe_id],
				   core_taskdone_cb,
				   (void *)task);

	return err;
}

static void core_init_pipe0(struct mml_task *task, u32 pipe_id)
{
	s32 err;

	err = core_config(task, 0);
	if (err < 0) {
		/* error handling */
	}

	err = core_flush(task, 0);
	if (err < 0) {
		/* error handling */
	}
}

static void core_init_pipe1(struct work_struct *work)
{
	s32 err;
	struct mml_task *task;

	task = container_of(work, struct mml_task, work_config[1]);

	err = core_config(task, 1);
	if (err < 0) {
		/* error handling */
	}

	err = core_flush(task, 1);
	if (err < 0) {
		/* error handling */
	}
}

static void core_config_thread(struct work_struct *work)
{
	struct mml_task *task;

	task = container_of(work, struct mml_task, work_config[0]);
	/* topology */

	/* dualpipe create work_thread[1] */
	if (task->config->dual) {
		if (!task->config->wq_config[1])
			task->config->wq_config[1] =
				alloc_workqueue("mml_work1", 0, 0);

		queue_work(task->config->wq_config[1], &task->work_config[1]);
	}

	core_init_pipe0(task, 0);

	/* check pipe 1 done and callback */
	if (task->config->dual &&
	    flush_work(&task->work_config[1]))
		task->config->task_ops->submit_done(task);
}

struct mml_task *mml_core_create_task(void)
{
	struct mml_task *task = kzalloc(sizeof(struct mml_task), GFP_KERNEL);

	INIT_LIST_HEAD(&task->entry);
	INIT_WORK(&task->work_config[0], core_config_thread);
	INIT_WORK(&task->work_config[1], core_init_pipe1);
	INIT_WORK(&task->work_wait, core_taskdone);

	return task;
}

void mml_core_destroy_task(struct mml_task *task)
{
	return kfree(task);
}

s32 mml_core_submit_task(struct mml_frame_config *frame_config,
			 struct mml_task *task)
{
	/* mml create work_thread 0, wait thread */
	if (!task->config->wq_config[0])
		task->config->wq_config[0] = alloc_workqueue("mml_work0", 0, 0);
	if (!task->config->wq_wait)
		task->config->wq_wait= alloc_workqueue("mml_wait", 0, 0);

	queue_work(task->config->wq_config[0], &task->work_config[0]);

	return 0;
}

