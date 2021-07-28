/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chris-YC Chen <chris-yc.chen@mediatek.com>
 */

#include <linux/dma-fence.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <soc/mediatek/smi.h>
#include <cmdq-util.h>


#include "mtk-mml-core.h"
#include "mtk-mml-buf.h"
#include "mtk-mml-tile.h"

int mtk_mml_msg;
EXPORT_SYMBOL(mtk_mml_msg);
module_param(mtk_mml_msg, int, 0644);

int mml_pkt_dump;
module_param(mml_pkt_dump, int, 0644);

int mml_trace;
EXPORT_SYMBOL(mml_trace);
module_param(mml_trace, int, 0644);

int mml_qos = 1;
module_param(mml_qos, int, 0644);

int mml_qos_log;
module_param(mml_qos_log, int, 0644);

#define mml_msg_qos(fmt, args...) \
do { \
	if (mml_qos_log) \
		pr_notice("[mml]" fmt "\n", ##args); \
} while (0)

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

/* error counter */
atomic_t mml_err_cnt;

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
EXPORT_SYMBOL_GPL(mml_topology_register_ip);

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
EXPORT_SYMBOL_GPL(mml_topology_unregister_ip);

struct mml_topology_cache *mml_topology_create(struct mml_dev *mml,
					       struct platform_device *pdev,
					       struct cmdq_client **clts,
					       u32 clt_cnt)
{
	struct mml_topology_cache *tp;
	struct topology_ip_node *tp_node;
	const char *tp_plat;
	u32 i;
	int err;

	err = of_property_read_string(pdev->dev.of_node, "topology", &tp_plat);
	if (err < 0) {
		mml_err("fail to parse topology from dts %d");
		tp_plat = "mt6893";
	}

	tp = devm_kzalloc(&pdev->dev, sizeof(*tp), GFP_KERNEL);
	if (!tp)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < ARRAY_SIZE(tp->path_clts); i++) {
		INIT_LIST_HEAD(&tp->path_clts[i].tasks);
		mutex_init(&tp->path_clts[i].clt_mutex);
	}

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

#define call_hw_op(_comp, op, ...) \
	(_comp->hw_ops->op ? _comp->hw_ops->op(_comp, ##__VA_ARGS__) : 0)

static void core_prepare(struct mml_task *task, u32 pipe)
{
	const struct mml_topology_path *path = task->config->path[pipe];
	struct mml_pipe_cache *cache = &task->config->cache[pipe];
	u32 i;

	mml_trace_ex_begin("%s_%u", __func__, pipe);

	mml_msg("%s task %p pipe %u", __func__, task, pipe);

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

static void core_reuse(struct mml_task *task, u32 pipe)
{
	const struct mml_topology_path *path = task->config->path[pipe];
	struct mml_pipe_cache *cache = &task->config->cache[pipe];
	u32 i;

	for (i = 0; i < path->node_cnt; i++) {
		struct mml_comp *comp = path->nodes[i].comp;

		call_cfg_op(comp, buf_prepare, task, &cache->cfg[i]);
	}
}

static s32 command_make(struct mml_task *task, u32 pipe)
{
	const struct mml_topology_path *path = task->config->path[pipe];
	struct cmdq_pkt *pkt = cmdq_pkt_create(path->clt);
	struct mml_task_reuse *reuse = &task->reuse[pipe];

	struct mml_pipe_cache *cache = &task->config->cache[pipe];
	struct mml_comp_config *ccfg = cache->cfg;

	struct mml_comp *comp;
	u32 i, tile;
	s32 ret;

	if (IS_ERR(pkt)) {
		mml_err("%s fail err %d", __func__, PTR_ERR(pkt));
		return PTR_ERR(pkt);
	}
	task->pkts[pipe] = pkt;
	pkt->user_data = (void *)task;

	/* get total label count to create label array */
	cache->label_cnt = 0;
	for (i = 0; i < path->node_cnt; i++) {
		comp = task_comp(task, pipe, i);
		cache->label_cnt += call_cfg_op(comp, get_label_count, task);
	}

	reuse->labels = kcalloc(cache->label_cnt, sizeof(*reuse->labels), GFP_KERNEL);
	if (!reuse->labels) {
		mml_err("%s not able to alloc label table", __func__);
		ret = -ENOMEM;
		goto err;
	}

	/* call all component init and frame op, include mmlsys and mutex */
	for (i = 0; i < path->node_cnt; i++) {
		comp = task_comp(task, pipe, i);
		call_cfg_op(comp, init, task, &ccfg[i]);
	}
	for (i = 0; i < path->node_cnt; i++) {
		comp = task_comp(task, pipe, i);
		call_cfg_op(comp, frame, task, &ccfg[i]);
	}

	if (!task->config->tile_output[pipe]) {
		mml_err("%s no tile for input pipe %u", __func__, pipe);
		ret = -EINVAL;
		goto err;
	}

	for (tile = 0; tile < task->config->tile_output[pipe]->tile_cnt;
		tile++) {
		for (i = 0; i < path->node_cnt; i++) {
			comp = task_comp(task, pipe, i);
			call_cfg_op(comp, tile, task, &ccfg[i], tile);
		}

		path->mutex->config_ops->mutex(path->mutex, task,
					       &ccfg[path->mutex_idx]);

		for (i = 0; i < path->node_cnt; i++) {
			comp = task_comp(task, pipe, i);
			call_cfg_op(comp, wait, task, &ccfg[i]);
		}
	}

	for (i = 0; i < path->node_cnt; i++) {
		comp = task_comp(task, pipe, i);
		call_cfg_op(comp, post, task, &ccfg[i]);
	}

	return 0;

err:
	cmdq_pkt_destroy(task->pkts[pipe]);
	task->pkts[pipe] = NULL;
	return ret;
}

static s32 command_reuse(struct mml_task *task, u32 pipe)
{
	const struct mml_topology_path *path = task->config->path[pipe];
	struct mml_pipe_cache *cache = &task->config->cache[pipe];
	struct mml_comp_config *ccfg = cache->cfg;
	struct mml_comp *comp;
	u32 i;

	for (i = 0; i < path->node_cnt; i++) {
		comp = task_comp(task, pipe, i);
		call_cfg_op(comp, reframe, task, &ccfg[i]);
	}

	for (i = 0; i < path->node_cnt; i++) {
		comp = task_comp(task, pipe, i);
		call_cfg_op(comp, repost, task, &ccfg[i]);
	}

	mml_msg("%s task %p pipe %u pkt %p label cnt %u/%u",
		__func__, task, pipe, task->pkts[pipe],
		task->reuse[pipe].label_idx, cache->label_cnt);
	cmdq_pkt_reuse_buf_va(task->pkts[pipe], task->reuse[pipe].labels,
		task->reuse[pipe].label_idx);

	/* make sure this pkt not jump to others */
	cmdq_pkt_refinalize(task->pkts[pipe]);

	return 0;
}

static s32 core_enable(struct mml_task *task, u32 pipe)
{
	const struct mml_topology_path *path = task->config->path[pipe];
	struct mml_comp *comp;
	u32 i;

	mml_msg("%s task %p pipe %u", __func__, task, pipe);

	mml_clock_lock(task->config->mml);

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

	cmdq_util_prebuilt_enable(0);
	cmdq_util_prebuilt_init(CMDQ_PREBUILT_MML);

	mml_clock_unlock(task->config->mml);

	return 0;
}

static s32 core_disable(struct mml_task *task, u32 pipe)
{
	const struct mml_topology_path *path = task->config->path[pipe];
	struct mml_comp *comp;
	u32 i;

	mml_clock_lock(task->config->mml);

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

	mml_clock_unlock(task->config->mml);

	mml_msg("%s task %p pipe %u", __func__, task, pipe);

	return 0;
}

static void mml_core_qos_set(struct mml_task *task, u32 pipe, u32 throughput)
{
	const struct mml_topology_path *path = task->config->path[pipe];
	struct mml_pipe_cache *cache = &task->config->cache[pipe];
	struct mml_comp *comp;
	u32 i;

	for (i = 0; i < path->node_cnt; i++) {
		comp = task_comp(task, pipe, i);
		call_hw_op(comp, qos_set, task, &cache->cfg[i], throughput);
	}
}

static void mml_core_qos_clear(struct mml_task *task, u32 pipe)
{
	const struct mml_topology_path *path = task->config->path[pipe];
	struct mml_comp *comp;
	u32 i;

	for (i = 0; i < path->node_cnt; i++) {
		comp = task_comp(task, pipe, i);
		call_hw_op(comp, qos_clear);
	}
}

static u64 time_dur_us(const struct timespec64 *lhs, const struct timespec64 *rhs)
{
	struct timespec64 delta = timespec64_sub(*lhs, *rhs);

	return div_u64((u64)delta.tv_sec * 1000000000 + delta.tv_nsec, 1000);
}

static void mml_core_dvfs_begin(struct mml_task *task, u32 pipe)
{
	struct mml_topology_cache *tp = mml_topology_get_cache(task->config->mml);
	struct mml_path_client *path_clt =
		&tp->path_clts[task->config->path[pipe]->clt_id];
	struct mml_task *task_tmp;
	struct timespec64 curr_time;
	u32 throughput;
	u32 max_pixel = task->config->cache[pipe].max_pixel;

	ktime_get_real_ts64(&curr_time);
	mml_msg_qos("task dvfs begin %p pipe %u cur %2u.%03llu end %2u.%03llu",
		task, pipe,
		(u32)curr_time.tv_sec, div_u64(curr_time.tv_nsec, 1000000),
		(u32)task->end_time.tv_sec, div_u64(task->end_time.tv_nsec, 1000000));

	/* do not append to list and no qos/dvfs for this task */
	if (!mml_qos)
		return;

	if (timespec64_compare(&curr_time, &task->end_time) > 0)
		task->end_time = curr_time;

	if (!list_empty(&path_clt->tasks))
		task_tmp = list_last_entry(&path_clt->tasks, typeof(*task_tmp),
			entry_clt[pipe]);
	else
		task_tmp = NULL;
	if (task_tmp && timespec64_compare(&task_tmp->end_time, &curr_time) >= 0)
		task->submit_time = task_tmp->end_time;
	else
		task->submit_time = curr_time;

	if (timespec64_compare(&task->submit_time, &task->end_time) < 0) {
		/* calculate remaining time to complete pixels */
		task->throughput = (u32)div_u64(max_pixel,
			time_dur_us(&task->end_time, &task->submit_time));

		throughput = task->throughput;
		list_for_each_entry(task_tmp, &path_clt->tasks, entry_clt[pipe]) {
			/* find the max throughput (frequency) between tasks on same client */
			throughput = max(throughput, task_tmp->throughput);
		}
	} else {
		/* there is no time for this task, use mas throughput */
		task->throughput = tp->freq_max;
		/* make sure end time >= submit time to ensure
		 * next task calculate correct duration
		 */
		task->end_time = task->submit_time;
		/* use max as throughput this round */
		throughput = task->throughput;
	}

	/* now append at tail, this order should same as cmdq exec order */
	list_add_tail(&task->entry_clt[pipe], &path_clt->tasks);

	path_clt->throughput = throughput;
	mml_qos_update_tput(task->config->mml);

	/* note the running task not always current begin task */
	task_tmp = list_first_entry(&path_clt->tasks, typeof(*task_tmp), entry_clt[pipe]);
	mml_core_qos_set(task_tmp, pipe, throughput);

	mml_msg_qos("task dvfs begin %p pipe %u throughput %u (%u) pixel %u",
		task, pipe, throughput, task->throughput, max_pixel);
}

static void mml_core_dvfs_end(struct mml_task *task, u32 pipe)
{
	struct mml_topology_cache *tp = mml_topology_get_cache(task->config->mml);
	struct mml_path_client *path_clt =
		&tp->path_clts[task->config->path[pipe]->clt_id];
	struct mml_task *task_cur, *task_tmp;
	struct timespec64 curr_time;
	u32 throughput = 0, max_pixel = 0;

	ktime_get_real_ts64(&curr_time);
	mml_msg_qos("task dvfs end %p pipe %u cur %2u.%03llu end %2u.%03llu",
		task, pipe,
		(u32)curr_time.tv_sec, div_u64(curr_time.tv_nsec, 1000000),
		(u32)task->end_time.tv_sec, div_u64(task->end_time.tv_nsec, 1000000));

	if (list_empty(&task->entry_clt[pipe])) {
		/* task may already removed from other config (thread),
		 * so safe to leave directly.
		 */
		return;
	}

	list_for_each_entry_safe(task_cur, task_tmp, &path_clt->tasks, entry_clt[pipe]) {
		/* remove task from list include tasks before current
		 * ending task, since cmdq already finish them, too.
		 */
		list_del_init(&task->entry_clt[pipe]);

		/* clear port qos */
		mml_core_qos_clear(task, pipe);

		if (task == task_cur) {
			/* found ending one, stops delete */
			break;
		}

		mml_msg_qos("task dvfs end %p pipe %u clear qos (pre-end)", task, pipe);
	}

	task_cur = list_first_entry_or_null(&path_clt->tasks, typeof(*task_cur),
		entry_clt[pipe]);
	if (task_cur) {
		if (timespec64_compare(&curr_time, &task_cur->end_time) >= 0) {
			/* this task must done right now, skip all compare */
			throughput = tp->freq_max;
			goto done;
		}

		/* calculate remaining time to complete pixels */
		max_pixel = task_cur->config->cache[pipe].max_pixel;
		task_cur->throughput = (u32)div_u64(max_pixel,
			time_dur_us(&curr_time, &task->end_time));

		throughput = 0;
		list_for_each_entry(task_tmp, &path_clt->tasks, entry_clt[pipe]) {
			/* find the max throughput (frequency) between tasks on same client */
			throughput = max(throughput, task_tmp->throughput);
		}
	} else {
		/* no task anymore, clear */
		throughput = 0;
	}

done:
	mml_msg_qos("task dvfs end update new task %p throughput %u pixel %u",
		task_cur, throughput, max_pixel);
	path_clt->throughput = throughput;
	mml_qos_update_tput(task->config->mml);

	if (throughput)
		mml_core_qos_set(task_cur, pipe, throughput);
}

static struct mml_path_client *core_get_path_clt(struct mml_task *task, u32 pipe)
{
	struct mml_topology_cache *tp = mml_topology_get_cache(task->config->mml);

	return &tp->path_clts[task->config->path[pipe]->clt_id];
}

static void core_taskdone(struct mml_task *task, u32 pipe)
{
	struct mml_path_client *path_clt;
	u32 cnt, i;

	mml_trace_begin("%s", __func__);

	/* do task ending for this pipe to make sure hw run on necessary freq
	 * and must lock path clt to ensure tasks in list not handle by others
	 *
	 * and note we always lock pipe 0
	 */
	path_clt = core_get_path_clt(task, 0);
	mutex_lock(&path_clt->clt_mutex);
	mml_core_dvfs_end(task, pipe);
	mutex_unlock(&path_clt->clt_mutex);

	cnt = atomic_inc_return(&task->pipe_done);

	mml_msg("%s task %p cnt %d", __func__, task, cnt);

	/* cnt can be 1 or 2, if dual on and count 2 means pipes done */
	if (task->config->dual && cnt == 1)
		goto done;

	for (i = 0; i < task->buf.dest_cnt; i++) {
		if (task->buf.dest[i].invalid)
			mml_buf_invalid(&task->buf.dest[i]);
	}

	/* before clean up, signal buffer fence */
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

static void core_taskdone0(struct work_struct *work)
{
	struct mml_task *task = container_of(work, struct mml_task, work_wait[0]);

	core_taskdone(task, 0);
}

static void core_taskdone1(struct work_struct *work)
{
	struct mml_task *task = container_of(work, struct mml_task, work_wait[1]);

	core_taskdone(task, 1);
}

static void core_taskdone_cb(struct cmdq_cb_data data)
{
	struct cmdq_pkt *pkt = (struct cmdq_pkt *)data.data;
	struct mml_task *task = (struct mml_task *)pkt->user_data;
	u32 pipe;

	if (pkt == task->pkts[0])
		pipe = 0;
	else if (pkt == task->pkts[1])
		pipe = 1;
	else {
		mml_err("%s task %p pkt %p not match both pipe (%p and %p)",
			__func__, task, pkt, task->pkts[0], task->pkts[1]);
		return;
	}

	queue_work(task->config->wq_wait, &task->work_wait[pipe]);
}

static s32 core_config(struct mml_task *task, u32 pipe)
{
	if (task->state == MML_TASK_INITIAL) {
		/* prepare data in each component for later tile use */
		core_prepare(task, pipe);

		/* call to tile to calculate */
		mml_trace_ex_begin("%s_%s_%u", __func__, "tile", pipe);
		calc_tile(task, pipe);
		mml_trace_ex_end();

		/* dump tile output for debug */
		if (mtk_mml_msg)
			dump_tile_output(task, pipe);

		/* make commands into pkt for later flash */
		mml_trace_ex_begin("%s_%s_%u", __func__, "cmd", pipe);
		command_make(task, pipe);
		mml_trace_ex_end();
	} else {
		if (task->state == MML_TASK_DUPLICATE) {
			int ret;

			/* task need duplcicate before reuse */
			mml_trace_ex_begin("%s_%s_%u", __func__, "dup", pipe);
			ret = task->config->task_ops->dup_task(task, pipe);
			mml_trace_ex_end();
			if (ret < 0) {
				mml_err("dup task fail %d", ret);
				return ret;
			}
		}

		/* pkt exists, reuse it directly */
		mml_trace_ex_begin("%s_%s_%u", __func__, "reuse", pipe);
		core_reuse(task, pipe);
		command_reuse(task, pipe);
		mml_trace_ex_end();
	}

	core_enable(task, pipe);
	mml_core_dvfs_begin(task, pipe);

	return 0;
}

static void wait_dma_fence(const char *name, struct dma_fence *fence)
{
	long ret;

	if (!fence)
		return;
	ret = dma_fence_wait_timeout(fence, true, 3000);
	if (ret < 0)
		mml_err("wait %s fence fail %p ret %ld", name, fence, ret);
}

#define call_dbg_op(_comp, op, ...) \
	((_comp->debug_ops && _comp->debug_ops->op) ? \
		_comp->debug_ops->op(_comp, ##__VA_ARGS__) : 0)

static void core_taskdump_cb(struct mml_task *task, u32 pipe)
{
	const struct mml_topology_path *path = task->config->path[pipe];
	struct mml_comp *comp;
	u32 i;
	int cnt = atomic_fetch_inc(&mml_err_cnt);

	mml_err("error dump %d task %p pipe %u config %p",
		cnt, task, pipe, task->config);

	for (i = 0; i < path->node_cnt; i++) {
		comp = task_comp(task, pipe, i);
		call_dbg_op(comp, dump);
	}

	if (cnt < 3) {
		mml_err("dump smi");
	}

	mml_err("error dump %d end", cnt);

	call_dbg_op(path->mmlsys, reset, task, pipe);

	mml_err("error %d engine reset end", cnt);
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

static const cmdq_async_flush_cb dump_cbs[MML_PIPE_CNT] = {
	[0] = core_taskdump_pipe0_cb,
	[1] = core_taskdump_pipe1_cb,
};

static s32 core_flush(struct mml_task *task, u32 pipe)
{
	int i;
	struct cmdq_pkt *pkt = task->pkts[pipe];

	mml_msg("%s task %p pipe %u pkt %p", __func__, task, pipe, pkt);

	if (mml_pkt_dump)
		cmdq_pkt_dump_buf(pkt, 0);

	/* before flush, wait buffer fence being signaled */
	wait_dma_fence("src", task->buf.src.fence);
	for (i = 0; i < task->buf.dest_cnt; i++)
		wait_dma_fence("dest", task->buf.dest[i].fence);

	/* flush only once for both pipe */
	mutex_lock(&task->config->pipe_mutex);

	if (!task->buf.flushed) {
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

		task->buf.flushed = true;
	}
	mutex_unlock(&task->config->pipe_mutex);

	/* assign error handler */
	pkt->err_cb.cb = dump_cbs[pipe];
	pkt->err_cb.data = (void *)task;

	return cmdq_pkt_flush_async(pkt, core_taskdone_cb, (void *)task->pkts[pipe]);
}

static void core_init_pipe(struct mml_task *task, u32 pipe)
{
	s32 err;

	mml_trace_ex_begin("%s_%u", __func__, pipe);

	err = core_config(task, pipe);
	if (err < 0) {
		/* error handling */
	}
	err = core_flush(task, pipe);
	if (err < 0) {
		/* error handling */
	}

	mml_msg("%s task %p pipe %u done", __func__, task, pipe);

	mml_trace_ex_end();
}

static void core_init_pipe1(struct work_struct *work)
{
	struct mml_task *task;

	task = container_of(work, struct mml_task, work_config[1]);
	core_init_pipe(task, 1);
}

static void core_buffer_map(struct mml_task *task)
{
	const struct mml_topology_path *path = task->config->path[0];
	u32 i;

	for (i = 0; i < path->node_cnt; i++) {
		struct mml_comp *comp = path->nodes[i].comp;

		call_cfg_op(comp, buf_map, task, &path->nodes[i]);
	}
}

static void core_config_thread(struct work_struct *work)
{
	struct mml_task *task;
	struct mml_path_client *path_clt;
	s32 err;
	u32 i;

	mml_trace_begin("%s", __func__);

	task = container_of(work, struct mml_task, work_config[0]);

	mml_msg("%s begin task %p config %p",
		__func__, task, task->config);

	/* topology */
	if (task->state == MML_TASK_INITIAL) {
		mml_log("in:%u %u f:%#010x stride %u %u fence:%s plane:%hhu%s",
			task->config->info.src.width,
			task->config->info.src.height,
			task->config->info.src.format,
			task->config->info.src.y_stride,
			task->config->info.src.uv_stride,
			task->buf.src.fence ? "true" : "false",
			task->buf.src.cnt,
			task->buf.src.flush ? " flush" : "");
		for (i = 0; i < task->config->info.dest_cnt; i++) {
			mml_log(
				"out %u:%u %u f:%#010x stride %u %u rot:%hu f:%hhu fence:%s plane:%hhu%s%s",
				i,
				task->config->info.dest[i].data.width,
				task->config->info.dest[i].data.height,
				task->config->info.dest[i].data.format,
				task->config->info.dest[i].data.y_stride,
				task->config->info.dest[i].data.uv_stride,
				task->config->info.dest[i].rotate,
				(u8)task->config->info.dest[i].flip,
				task->buf.dest[i].fence ? "true" : "false",
				task->buf.dest[i].cnt,
				task->buf.dest[i].flush ? " flush" : "",
				task->buf.dest[i].invalid ? " invalid" : "");
			mml_log("crop %u:%u %u %u %u compose %u %u %u %u",
				i,
				task->config->info.dest[i].crop.r.left,
				task->config->info.dest[i].crop.r.top,
				task->config->info.dest[i].crop.r.width,
				task->config->info.dest[i].crop.r.height,
				task->config->info.dest[i].compose.left,
				task->config->info.dest[i].compose.top,
				task->config->info.dest[i].compose.width,
				task->config->info.dest[i].compose.height);
		}

		/* topology will fill in path instance */
		err = topology_select_path(task->config);
		if (err < 0) {
			mml_err("%s select path fail %d", __func__, err);
			goto done;
		}
	}

	/* before pipe1 start, make sure iova map from device by pipe0 */
	core_buffer_map(task);

	/* create dual work_thread[1] */
	if (task->config->dual) {
		if (!task->config->wq_config[1])
			task->config->wq_config[1] =
				alloc_ordered_workqueue("mml_work1", 0, 0);
		queue_work(task->config->wq_config[1], &task->work_config[1]);
	}

	/* ref count to 2 thus destroy can be one of
	 * submit done and frame done
	 */
	kref_get(&task->ref);

	/* make sure no other config uses same client for current path */
	path_clt = core_get_path_clt(task, 0);
	mutex_lock(&path_clt->clt_mutex);

	core_init_pipe(task, 0);

	/* check single pipe or (dual) pipe 1 done then callback */
	if (!task->config->dual || flush_work(&task->work_config[1]))
		task->config->task_ops->submit_done(task);

	/* now both pipe submit done unlock client */
	mutex_unlock(&path_clt->clt_mutex);
done:
	mml_trace_end();
}

struct mml_task *mml_core_create_task(void)
{
	struct mml_task *task = kzalloc(sizeof(*task), GFP_KERNEL);

	if (IS_ERR(task))
		return task;
	INIT_LIST_HEAD(&task->entry);
	INIT_LIST_HEAD(&task->entry_clt[0]);
	INIT_LIST_HEAD(&task->entry_clt[1]);
	INIT_WORK(&task->work_config[0], core_config_thread);
	INIT_WORK(&task->work_config[1], core_init_pipe1);
	INIT_WORK(&task->work_wait[0], core_taskdone0);
	INIT_WORK(&task->work_wait[1], core_taskdone1);
	kref_init(&task->ref);
	return task;
}

void mml_core_destroy_task(struct mml_task *task)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(task->reuse); i++)
		kfree(task->reuse[i].labels);
	for (i = 0; i < ARRAY_SIZE(task->pkts); i++) {
		if (task->pkts[i])
			cmdq_pkt_destroy(task->pkts[i]);
	}
	kfree(task);
}

static void core_destroy_wq(struct workqueue_struct **wq)
{
	if (*wq) {
		destroy_workqueue(*wq);
		*wq = NULL;
	}
}

void mml_core_init_config(struct mml_frame_config *cfg)
{
	/* mml create work_thread 0, wait thread */
	cfg->wq_config[0] = alloc_ordered_workqueue("mml_work0", 0, 0);
	cfg->wq_wait = alloc_ordered_workqueue("mml_wait", 0, 0);
}

void mml_core_deinit_config(struct mml_frame_config *cfg)
{
	u32 pipe, i;

	/* make command, engine allocated private data */
	for (pipe = 0; pipe < MML_PIPE_CNT; pipe++) {
		for (i = 0; i < cfg->path[pipe]->node_cnt; i++)
			kfree(cfg->cache[pipe].cfg[i].data);
		destroy_tile_output(cfg->tile_output[pipe]);
	}
	for (i = 0; i < ARRAY_SIZE(cfg->wq_config); i++)
		core_destroy_wq(&cfg->wq_config[i]);
	core_destroy_wq(&cfg->wq_wait);
}

void mml_core_submit_task(struct mml_frame_config *cfg, struct mml_task *task)
{
	/* reset to 0 in case reuse task */
	atomic_set(&task->pipe_done, 0);

	queue_work(cfg->wq_config[0], &task->work_config[0]);
}

s32 mml_write(struct cmdq_pkt *pkt, struct mml_task_reuse *reuse,
	dma_addr_t addr, u32 value, u32 mask,
	struct mml_pipe_cache *cache, u16 *label_idx, bool write_sec,
	u16 reg_idx)
{
	if (reuse->label_idx >= cache->label_cnt) {
		mml_err("out of label cnt idx %u count %u",
			reuse->label_idx, cache->label_cnt);
		return -ENOMEM;
	}

	if (write_sec)
		cmdq_pkt_assign_command_reuse(pkt, reg_idx, value,
			&reuse->labels[reuse->label_idx].va,
			&reuse->labels[reuse->label_idx].offset);
	else
		cmdq_pkt_write_value_addr_reuse(pkt, addr, value, mask,
			&reuse->labels[reuse->label_idx].va,
			&reuse->labels[reuse->label_idx].offset);

	*label_idx = reuse->label_idx;
	reuse->labels[reuse->label_idx].val = value;
	reuse->label_idx++;

	return 0;
}

void mml_update(struct mml_task_reuse *reuse, u16 label_idx, u32 value)
{
	reuse->labels[label_idx].val = value;
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
