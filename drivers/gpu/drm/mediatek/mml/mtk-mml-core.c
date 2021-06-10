// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>

#include "mtk-mml-core.h"

int mtk_mml_msg;
EXPORT_SYMBOL(mtk_mml_msg);
module_param(mtk_mml_msg, int, 0644);

struct topology_ip_node {
	struct list_head entry;
	const struct mml_topology_ops *op;
	const char *ip;
};

/* list of topology ip nodes for different platform,
 * which contains operation for specific platform.
 */
static LIST_HEAD(tp_ips);
static DEFINE_MUTEX(tp_mutex);

void mml_topology_register_ip(const char *ip,
	const struct mml_topology_ops *op)
{
	struct topology_ip_node *ip_node = kzalloc(sizeof(*ip_node),
						   GFP_KERNEL);
	if (!ip) {
		mml_err("fail to register ip %s", ip);
		return;
	}

	INIT_LIST_HEAD(&ip_node->entry);
	ip_node->ip = ip;
	ip_node->op = op;

	mutex_lock(&tp_mutex);
	list_add_tail(&ip_node->entry, &tp_ips);
	mutex_unlock(&tp_mutex);
}

void mml_topology_unregister_ip(const char *ip)
{
	struct topology_ip_node *tp_node, *tmp;

	mutex_lock(&tp_mutex);
	list_for_each_entry_safe(tp_node, tmp, &tp_ips, entry) {
		if (strcmp(tp_node->ip, ip) == 0) {
			list_del(&tp_node->entry);
			kfree(tp_node);
			break;
		}
	}
	mutex_unlock(&tp_mutex);
}

struct mml_topology_cache *mml_topology_create(struct mml_dev *mml,
					       struct platform_device *pdev,
					       struct cmdq_client **clts,
					       u8 clt_cnt)
{
	struct mml_topology_cache *tp;
	struct topology_ip_node *tp_node;
	const char *tp_plat;
	int err;

	err = of_property_read_string(pdev->dev.of_node, "topology", &tp_plat);
	if (err < 0) {
		mml_err("fail to parse topology from dts %d");
		tp_plat = "mt6893";
	}

	tp = devm_kzalloc(&pdev->dev, sizeof(*tp), GFP_KERNEL);
	if (!tp)
		return ERR_PTR(-ENOMEM);

	mutex_lock(&tp_mutex);
	list_for_each_entry(tp_node, &tp_ips, entry) {
		if (strcmp(tp_node->ip, tp_plat) == 0) {
			tp->op = tp_node->op;
			break;
		}
	}
	mutex_unlock(&tp_mutex);

	if (tp->op->init_cache)
		tp->op->init_cache(mml, tp, clts, clt_cnt);

	return tp;
}

static s32 topology_select_path(struct mml_frame_config *cfg)
{
	struct mml_topology_cache *tp = mml_topology_get_cache(cfg->mml);
	s32 ret;

	if (!tp) {
		mml_err("%s path not exists", __func__);
		return -ENXIO;
	}

	if (cfg->path[0]) {
		mml_err("%s select path twice", __func__);
		return -EBUSY;
	}

	if (!tp->op->select)
		return -EPIPE;

	ret = tp->op->select(tp, cfg);
	if (ret < 0)
		return ret;

	return 0;
}

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
	s32 err;

	task = container_of(work, struct mml_task, work_config[0]);

	/* topology */
	if (task->state == MML_TASK_INITIAL) {
		/* topology will fill in path instan */
		err = topology_select_path(task->config);
		if (err < 0) {
			mml_err("%s select path fail %d", __func__, err);
			return;
		}
	}

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

