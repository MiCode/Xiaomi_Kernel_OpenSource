/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chris-YC Chen <chris-yc.chen@mediatek.com>
 */

#include <linux/dma-fence.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>

#include "mtk-mml-core.h"
#include "mtk-mml-buf.h"
#include "mtk-mml-tile.h"

int mtk_mml_msg;
EXPORT_SYMBOL(mtk_mml_msg);
module_param(mtk_mml_msg, int, 0644);

int mml_pkt_dump;
EXPORT_SYMBOL(mml_pkt_dump);
module_param(mml_pkt_dump, int, 0644);

int mml_trace;
EXPORT_SYMBOL(mml_trace);
module_param(mml_trace, int, 0644);

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

int mml_topology_register_ip(const char *ip, const struct mml_topology_ops *op)
{
	struct topology_ip_node *ip_node = kzalloc(sizeof(*ip_node),
						   GFP_KERNEL);
	if (!ip) {
		mml_err("fail to register ip %s", ip);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&ip_node->entry);
	ip_node->ip = ip;
	ip_node->op = op;

	mutex_lock(&tp_mutex);
	list_add_tail(&ip_node->entry, &tp_ips);
	mutex_unlock(&tp_mutex);
	return 0;
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

#define has_cfg_op(_comp, op) \
	(_comp->config_ops && _comp->config_ops->op)
#define call_cfg_op(_comp, op, ...) \
	(has_cfg_op(_comp, op) ? \
		_comp->config_ops->op(_comp, ##__VA_ARGS__) : 0)

#define call_hw_op(_comp, op) \
	(_comp->hw_ops->op ? _comp->hw_ops->op(_comp) : 0)

static void core_prepare(struct mml_task *task, u8 pipe)
{
	const struct mml_topology_path *path = task->config->path[pipe];
	struct mml_pipe_cache *cache = &task->config->cache[pipe];
	u8 i;

	mml_trace_ex_begin("%s_%u", __func__, pipe);

	mml_msg("%s task %p pipe %hhu", __func__, task, pipe);

	for (i = 0; i < path->node_cnt; i++) {
		/* collect infos for later easy use */
		cache->cfg[i].pipe = pipe;
		cache->cfg[i].node = &path->nodes[i];
		cache->cfg[i].tile_eng_idx = path->nodes[i].tile_eng_idx;
	}

	for (i = 0; i < path->node_cnt; i++) {
		struct mml_comp *comp = path->nodes[i].comp;

		call_cfg_op(comp, prepare, task, &cache->cfg[i]);
		call_cfg_op(comp, buf_prepare, task, &cache->cfg[i]);
	}

	mml_trace_ex_end();
}

static void core_reuse(struct mml_task *task, u8 pipe)
{
	const struct mml_topology_path *path = task->config->path[pipe];
	struct mml_pipe_cache *cache = &task->config->cache[pipe];
	u8 i;

	for (i = 0; i < path->node_cnt; i++) {
		struct mml_comp *comp = path->nodes[i].comp;

		call_cfg_op(comp, buf_prepare, task, &cache->cfg[i]);
	}
}

static s32 command_make(struct mml_task *task, u8 pipe)
{
	const struct mml_topology_path *path = task->config->path[pipe];
	struct cmdq_pkt *pkt = cmdq_pkt_create(path->clt);

	struct mml_pipe_cache *cache = &task->config->cache[pipe];
	struct mml_comp_config *cfg = cache->cfg;

	struct mml_comp *comp;
	u8 i, tile;
	u32 label_cnt = 0;
	s32 ret;

	if (IS_ERR(pkt)) {
		mml_err("%s fail err %d", __func__, PTR_ERR(pkt));
		return PTR_ERR(pkt);
	}
	task->pkts[pipe] = pkt;

	/* get total label count to create label array */
	for (i = 0; i < path->node_cnt; i++) {
		comp = task_comp(task, pipe, i);
		label_cnt += call_cfg_op(comp, get_label_count, task);
	}

	cache->labels = kcalloc(label_cnt, sizeof(*cache->labels), GFP_KERNEL);
	if (!cache->labels) {
		mml_err("%s not able to alloc label table", __func__);
		ret = -ENOMEM;
		goto err;
	}
	cache->label_cnt = label_cnt;

	/* call all component init and frame op, include mmlsys and mutex */
	for (i = 0; i < path->node_cnt; i++) {
		comp = task_comp(task, pipe, i);
		call_cfg_op(comp, init, task, &cfg[i]);
	}
	for (i = 0; i < path->node_cnt; i++) {
		comp = task_comp(task, pipe, i);
		call_cfg_op(comp, frame, task, &cfg[i]);
	}

	if (!task->config->tile_output[pipe]) {
		mml_err("%s no tile for input pipe %hhu", __func__, pipe);
		ret = -EINVAL;
		goto err;
	}

	for (tile = 0; tile < task->config->tile_output[pipe]->tile_cnt;
		tile++) {
		for (i = 0; i < path->node_cnt; i++) {
			comp = task_comp(task, pipe, i);
			call_cfg_op(comp, tile, task, &cfg[i], tile);
		}

		path->mutex->config_ops->mutex(path->mutex, task,
					       &cfg[path->mutex_idx]);

		for (i = 0; i < path->node_cnt; i++) {
			comp = task_comp(task, pipe, i);
			call_cfg_op(comp, wait, task, &cfg[i]);
		}
	}

	for (i = 0; i < path->node_cnt; i++) {
		comp = task_comp(task, pipe, i);
		call_cfg_op(comp, post, task, &cfg[i]);
	}

	return 0;

err:
	cmdq_pkt_destroy(task->pkts[pipe]);
	task->pkts[pipe] = NULL;
	return ret;
}

static s32 command_reuse(struct mml_task *task, u8 pipe)
{
	const struct mml_topology_path *path = task->config->path[pipe];
	struct mml_pipe_cache *cache = &task->config->cache[pipe];
	struct mml_comp_config *cfg = cache->cfg;
	struct mml_comp *comp;
	u8 i;

	for (i = 0; i < path->node_cnt; i++) {
		comp = task_comp(task, pipe, i);
		call_cfg_op(comp, reframe, task, &cfg[i]);
	}

	for (i = 0; i < path->node_cnt; i++) {
		comp = task_comp(task, pipe, i);
		call_cfg_op(comp, repost, task, &cfg[i]);
	}

	mml_msg("%s task %p pkt %p label cnt %u/%u",
		__func__, task, task->pkts[pipe],
		cache->label_idx, cache->label_cnt);
	cmdq_pkt_reuse_buf_va(task->pkts[pipe], cache->labels, cache->label_idx);

	return 0;
}

static s32 core_enable(struct mml_task *task, u8 pipe)
{
	const struct mml_topology_path *path = task->config->path[pipe];
	struct mml_comp *comp;
	u8 i;

	mml_msg("%s task %p pipe %hhu", __func__, task, pipe);

	for (i = 0; i < path->node_cnt; i++) {
		comp = task_comp(task, pipe, i);
		call_hw_op(comp, pw_enable);
	}

	if (path->mmlsys)
		call_hw_op(path->mmlsys, clk_enable);
	if (path->mutex)
		call_hw_op(path->mutex, clk_enable);

	for (i = 0; i < path->node_cnt; i++) {
		if (i == path->mmlsys_idx || i == path->mutex_idx)
			continue;
		comp = task_comp(task, pipe, i);
		call_hw_op(comp, clk_enable);
	}

	return 0;
}

static s32 core_disable(struct mml_task *task, u8 pipe)
{
	const struct mml_topology_path *path = task->config->path[pipe];
	struct mml_comp *comp;
	u8 i;

	for (i = 0; i < path->node_cnt; i++) {
		if (i == path->mmlsys_idx || i == path->mutex_idx)
			continue;
		comp = task_comp(task, pipe, i);
		call_hw_op(comp, clk_disable);
	}

	if (path->mutex)
		call_hw_op(path->mutex, clk_disable);
	if (path->mmlsys)
		call_hw_op(path->mmlsys, clk_disable);

	for (i = 0; i < path->node_cnt; i++) {
		comp = task_comp(task, pipe, i);
		call_hw_op(comp, pw_disable);
	}

	mml_msg("%s task %p pipe %hhu", __func__, task, pipe);

	return 0;
}

static void core_taskdone(struct work_struct *work)
{
	struct mml_task *task;
	int cnt;
	u8 i;

	mml_trace_begin("%s", __func__);

	task = container_of(work, struct mml_task, work_wait);
	cnt = atomic_inc_return(&task->pipe_done);

	/* cnt should be 1 or 2, if dual on and count 2 means pipes done */
	if (task->config->dual && cnt == 1)
		goto done;

	for (i = 0; i < task->buf.dest_cnt; i++) {
		if (task->buf.dest[i].invalid)
			mml_buf_invalid(&task->buf.dest[i]);
	}

	/* before clean up, make sure buffer fence signaled */
	if (task->fence) {
		dma_fence_signal(task->fence);
		dma_fence_put(task->fence);
	}

	core_disable(task, 0);
	if (task->config->dual)
		core_disable(task, 1);

	task->config->task_ops->frame_done(task);

done:
	mml_trace_end();
}

static void core_taskdone_cb(struct cmdq_cb_data data)
{
	struct mml_task *task = (struct mml_task *)data.data;

	queue_work(task->config->wq_wait, &task->work_wait);
}

static s32 core_config(struct mml_task *task, u8 pipe)
{
	if (task->state == MML_TASK_INITIAL) {
		/* prepare data in each component for later tile use */
		core_prepare(task, pipe);

		/* call to tile to calculate */
		mml_trace_ex_begin("%s_tile_%hhu", __func__, pipe);
		calc_tile(task, pipe);
		mml_trace_ex_end();

		/* dump tile output for debug */
		dump_tile_output(task, pipe);

		/* make commands into pkt for later flash */
		mml_trace_ex_begin("%s_cmd_%hhu", __func__, pipe);
		command_make(task, pipe);
		mml_trace_ex_end();
	} else {
		mml_trace_ex_begin("%s_reuse_cmd_%hhu", __func__, pipe);

		if (task->state == MML_TASK_DUPLICATE) {
			/* task need duplcicate before reuse */
			int ret = task->config->task_ops->dup_task(task, pipe);
			if (ret < 0) {
				mml_err("dup task fail %d", ret);
				return ret;
			}
		}

		/* pkt exists, reuse it directly */
		core_reuse(task, pipe);
		command_reuse(task, pipe);
		mml_trace_ex_end();
	}

	core_enable(task, pipe);

	return 0;
}

static void wait_dma_fence(const char *name, struct dma_fence *fence)
{
	long ret;

	if (!fence)
		return;

	ret = dma_fence_wait_timeout(fence, true, 3000);
	if (ret < 0)
		mml_err("wait %s fence fail %p ret %ld",
			name, fence, ret);
}

#define call_dbg_op(_comp, op) \
	((_comp->debug_ops && _comp->debug_ops->op) ? _comp->debug_ops->op(_comp) : 0)

static void core_taskdump_cb(struct mml_task *task, u8 pipe)
{
	const struct mml_topology_path *path = task->config->path[pipe];
	struct mml_comp *comp;
	u8 i;

	for (i = 0; i < path->node_cnt; i++) {
		comp = task_comp(task, pipe, i);
		call_dbg_op(comp, dump);
	}
}

static void core_taskdump_pipe0_cb(struct cmdq_cb_data data)
{
	struct mml_task *task = (struct mml_task *)data.data;

	core_taskdump_cb(task, 0);
}

static void core_taskdump_pipe1_cb(struct cmdq_cb_data data)
{
	struct mml_task *task = (struct mml_task *)data.data;

	core_taskdump_cb(task, 1);
}

static const cmdq_async_flush_cb dump_cbs[2] = {
	[0] = core_taskdump_pipe0_cb,
	[1] = core_taskdump_pipe1_cb,
};

static s32 core_flush(struct mml_task *task, u8 pipe)
{
	int i;
	s32 err;
	struct cmdq_pkt *pkt = task->pkts[pipe];

	mml_msg("%s task %p pipe %hhu pkt %p", __func__, task, pipe, pkt);

	if (mml_pkt_dump)
		cmdq_pkt_dump_buf(pkt, 0);

	/* before flush, make sure buffer fence signaled */
	wait_dma_fence("src", task->buf.src.fence);
	for (i = 0; i < task->buf.dest_cnt; i++)
		wait_dma_fence("dest", task->buf.dest[i].fence);

	/* also make sure buffer content flushed by other module */
	if (task->buf.src.flush) {
		mml_msg("%s flush source", __func__);
		mml_buf_flush(&task->buf.src);
	}
	for (i = 0; i < task->buf.dest_cnt; i++) {
		if (task->buf.dest[i].flush) {
			mml_msg("%s flush dest %d plane %hhu",
				__func__, i, task->buf.dest[i].cnt);
			mml_buf_flush(&task->buf.dest[i]);
		}
	}

	/* assign error handler */
	pkt->err_cb.cb = dump_cbs[pipe];
	pkt->err_cb.data = (void *)task;

	err = cmdq_pkt_flush_async(pkt, core_taskdone_cb, (void *)task);

	return err;
}

static void core_init_pipe(struct mml_task *task, u8 pipe)
{
	s32 err;

	mml_trace_ex_begin("%s_%hhu", __func__, pipe);

	err = core_config(task, pipe);
	if (err < 0) {
		/* error handling */
	}

	err = core_flush(task, pipe);
	if (err < 0) {
		/* error handling */
	}

	mml_trace_ex_end();
}

static void core_init_pipe1(struct work_struct *work)
{
	struct mml_task *task;

	task = container_of(work, struct mml_task, work_config[1]);
	core_init_pipe(task, 1);
}

static void core_config_thread(struct work_struct *work)
{
	struct mml_task *task;
	s32 err;
	u8 i;

	mml_trace_begin("%s", __func__);

	task = container_of(work, struct mml_task, work_config[0]);

	/* topology */
	if (task->state == MML_TASK_INITIAL) {
		mml_msg("in:%u %u f:%#010x stride %u %u fence:%s",
			task->config->info.src.width,
			task->config->info.src.height,
			task->config->info.src.format,
			task->config->info.src.y_stride,
			task->config->info.src.uv_stride,
			task->buf.src.fence ? "true" : "false");
		for (i = 0; i < task->config->info.dest_cnt; i++)
			mml_msg("out %hhu:%u %u f:%#010x stride %u %u rot:%hu fence:%s",
				i,
				task->config->info.dest[i].data.width,
				task->config->info.dest[i].data.height,
				task->config->info.dest[i].data.format,
				task->config->info.dest[i].data.y_stride,
				task->config->info.dest[i].data.uv_stride,
				task->config->info.dest[i].rotate,
				task->buf.dest[i].fence ? "true" : "false");

		/* topology will fill in path instance */
		err = topology_select_path(task->config);
		if (err < 0) {
			mml_err("%s select path fail %d", __func__, err);
			goto done;
		}
	}

	/* dualpipe create work_thread[1] */
	if (task->config->dual) {
		if (!task->config->wq_config[1])
			task->config->wq_config[1] =
				alloc_workqueue("mml_work1", 0, 0);

		queue_work(task->config->wq_config[1], &task->work_config[1]);
	}

	/* ref count to 2 thus destroy can be one of
	 * submit done and frame done
	 */
	kref_get(&task->ref);

	core_init_pipe(task, 0);

	/* check pipe 1 done and callback */
	if (!task->config->dual || flush_work(&task->work_config[1]))
		task->config->task_ops->submit_done(task);

done:
	mml_trace_end();
}

struct mml_task *mml_core_create_task(void)
{
	struct mml_task *task = kzalloc(sizeof(*task), GFP_KERNEL);

	if (IS_ERR(task))
		return task;
	INIT_LIST_HEAD(&task->entry);
	INIT_WORK(&task->work_config[0], core_config_thread);
	INIT_WORK(&task->work_config[1], core_init_pipe1);
	INIT_WORK(&task->work_wait, core_taskdone);
	kref_init(&task->ref);
	return task;
}

void mml_core_destroy_task(struct mml_task *task)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(task->pkts); i++)
		if (task->pkts[i])
			cmdq_pkt_destroy(task->pkts[i]);
	kfree(task);
}

#define core_destroy_wq(wq) do {\
	if (wq) {\
		destroy_workqueue(wq); \
		wq = NULL; \
	} \
} while (0);

void mml_core_deinit_config(struct mml_frame_config *config)
{
	u8 pipe, i;

	/* make command, engine allocated private data */
	for (pipe = 0; pipe < MML_PIPE_CNT; pipe++) {
		for (i = 0; i < config->path[pipe]->node_cnt; i++)
			kfree(config->cache[pipe].cfg[i].data);
		kfree(config->cache[pipe].labels);
		destroy_tile_output(config->tile_output[pipe]);
	}
	for (i = 0; i < ARRAY_SIZE(config->wq_config); i++)
		core_destroy_wq(config->wq_config[i]);
	core_destroy_wq(config->wq_wait);
}

s32 mml_core_submit_task(struct mml_frame_config *frame_config,
			 struct mml_task *task)
{
	/* mml create work_thread 0, wait thread */
	if (!task->config->wq_config[0])
		task->config->wq_config[0] = alloc_workqueue("mml_work0", 0, 0);
	if (!task->config->wq_wait)
		task->config->wq_wait= alloc_workqueue("mml_wait", 0, 0);

	/* reset to 0 in case reuse task */
	atomic_set(&task->pipe_done, 0);

	queue_work(task->config->wq_config[0], &task->work_config[0]);

	return 0;
}

s32 mml_write(struct cmdq_pkt *pkt, dma_addr_t addr, u32 value, u32 mask,
	struct mml_pipe_cache *cache, u16 *label_idx)
{
	if (cache->label_idx >= cache->label_cnt) {
		mml_err("out of label cnt idx %u count %u",
			cache->label_idx, cache->label_cnt);
		return -ENOMEM;
	}

	cmdq_pkt_write_value_addr_reuse(pkt, addr, value, mask,
		&cache->labels[cache->label_idx].va);

	*label_idx = cache->label_idx;
	cache->labels[cache->label_idx].val = value;
	cache->label_idx++;

	return 0;
}

void mml_update(struct mml_pipe_cache *cache, u16 label_idx, u32 value)
{
	cache->labels[label_idx].val = value;
}

unsigned long mml_get_tracing_mark(void)
{
	static unsigned long __read_mostly tracing_mark_write_addr;

#ifdef IF_ZERO
	if (unlikely(tracing_mark_write_addr == 0))
		tracing_mark_write_addr =
			kallsyms_lookup_name("tracing_mark_write");
#endif

	return tracing_mark_write_addr;
}
