/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Dennis YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */

#include <linux/atomic.h>
#include <linux/file.h>
#include <linux/module.h>
#include <linux/sync_file.h>

#include "mediatek_v2/mtk_drm_ddp_comp.h"
#include "mediatek_v2/mtk_sync.h"
#include "mediatek_v2/mtk_drm_drv.h"

#include "mtk-mml-drm-adaptor.h"
#include "mtk-mml-buf.h"
#include "mtk-mml-color.h"
#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"


#define MML_QUERY_ADJUST	1

#define MML_REF_NAME "mml"

/* set to 0 to disable reuse config */
int mml_reuse = 1;
EXPORT_SYMBOL(mml_reuse);
module_param(mml_reuse, int, 0644);

int mml_max_cache_task = 4;
EXPORT_SYMBOL(mml_max_cache_task);
module_param(mml_max_cache_task, int, 0644);

int mml_max_cache_cfg = 2;
EXPORT_SYMBOL(mml_max_cache_cfg);
module_param(mml_max_cache_cfg, int, 0644);

int mml_pq_disable = 1;
EXPORT_SYMBOL(mml_pq_disable);
module_param(mml_pq_disable, int, 0644);

struct mml_drm_ctx {
	struct list_head configs;
	u32 config_cnt;
	struct mutex config_mutex;
	struct mml_dev *mml;
	const struct mml_task_ops *task_ops;
	atomic_t job_serial;
	struct workqueue_struct *wq_destroy;
	struct sync_timeline *timeline;
};

#if MML_QUERY_ADJUST == 1
static void mml_adjust_src(struct mml_frame_data *src)
{
	const u32 srcw = src->width;
	const u32 srch = src->height;

	if (MML_FMT_H_SUBSAMPLE(src->format) && (srcw & 0x1))
		src->width &= ~1;

	if (MML_FMT_V_SUBSAMPLE(src->format) && (srch & 0x1))
		src->height &= ~1;
}

static void mml_adjust_dest(struct mml_frame_data *src,
	struct mml_frame_dest *dest)
{
	if (dest->rotate == MML_ROT_90 || dest->rotate == MML_ROT_270) {
		if (MML_FMT_H_SUBSAMPLE(dest->data.format)) {
			dest->data.width &= ~1; /* WROT HW constraint */
			dest->data.height &= ~1;
		} else if (MML_FMT_V_SUBSAMPLE(dest->data.format)) {
			dest->data.width &= ~1;
		}
	} else {
		if (MML_FMT_H_SUBSAMPLE(dest->data.format))
			dest->data.width &= ~1;

	        if (MML_FMT_V_SUBSAMPLE(dest->data.format))
			dest->data.height &= ~1;
	}

	/* help user fill in crop if not give */
	if (!dest->crop.r.width && !dest->crop.r.height) {
		dest->crop.r.width = src->width;
		dest->crop.r.height = src->height;
	}

	if (!dest->compose.width && !dest->compose.height) {
		dest->compose.width = dest->data.width;
		dest->compose.height = dest->data.height;
	}
}
#endif

enum mml_mode mml_drm_query_cap(struct mml_frame_info *info)
{
	u8 i;
	const u32 srcw = info->src.width;
	const u32 srch = info->src.height;

#if MML_QUERY_ADJUST == 0
	/* following part should adjust later */
	if (MML_FMT_H_SUBSAMPLE(info->src.format) && (srcw & 0x1)) {
		mml_err("[drm]invalid src width %u alignment format %#010x",
			srcw, info->src.format);
		goto not_support;
	}

	if (MML_FMT_V_SUBSAMPLE(info->src.format) && (srch & 0x1)) {
		mml_err("[drm]invalid src height %u alignment format %#010x",
			srch, info->src.format);
		goto not_support;
	}
#else
	mml_adjust_src(&info->src);
#endif

	/* for alpha rotate */
	if (srcw < 9) {
		mml_err("[drm]exceed HW limitation src width %u < 9", srcw);
		goto not_support;
	}

	for (i = 0; i < info->dest_cnt; i++) {
		const struct mml_frame_dest *dest = &info->dest[i];
		u32 destw = dest->data.width;
		u32 desth = dest->data.height;

		if (dest->rotate == MML_ROT_90 || dest->rotate == MML_ROT_270)
			swap(destw, desth);

		if (srcw / destw > 20 || srch / desth > 255 ||
			destw / srcw > 32 || desth / srch > 32) {
			mml_err("[drm]exceed HW limitation src %ux%u dest %ux%u",
				srcw, srch, destw, desth);
			goto not_support;
		}

#if MML_QUERY_ADJUST == 0
		/* following part should adjust later */
		if (MML_FMT_H_SUBSAMPLE(dest->data.format)) {
			if (destw & 0x1) {
				mml_err("[drm]invalid dest width %u alignment format %#010x",
					destw, dest->data.format);
				goto not_support;
			}

			if ((desth & 0x1) && (dest->rotate == MML_ROT_90 ||
				dest->rotate == MML_ROT_270)) {
				mml_err("[drm]invalid dest %ux%u alignment format %#010x",
					destw, desth,
					dest->data.format);
				goto not_support;
			}
		}

		if (MML_FMT_V_SUBSAMPLE(dest->data.format) && (desth & 0x1)) {
			mml_err("[drm]invalid dest height %u alignment format %#010x",
				desth, dest->data.format);
			goto not_support;
		}
#else
		/* adjust info data directly for user */
	        mml_adjust_dest(&info->src, &info->dest[i]);
#endif

		/* check crop and pq combination */
		if (dest->pq_config.en && dest->crop.r.width < 48) {
			mml_err("[drm]exceed HW limitation crop width %u < 48 with pq",
				dest->crop.r.width);
			goto not_support;
		}
	}

	return mml_topology_query_mode(info);

not_support:
	return MML_MODE_NOT_SUPPORT;
}
EXPORT_SYMBOL_GPL(mml_drm_query_cap);

static bool check_frame_change(struct mml_frame_info *info,
			       struct mml_frame_config *cfg)
{
	if (memcmp(&cfg->info, info, sizeof(*info)))
		return false;

	return true;
}

static struct mml_frame_config *frame_config_find_reuse(
	struct mml_drm_ctx *ctx,
	struct mml_submit *submit)
{
	struct mml_frame_config *cfg;

	if (!mml_reuse)
		return NULL;

	mml_trace_ex_begin("%s", __func__);

	list_for_each_entry(cfg, &ctx->configs, entry) {
		if (submit->update && cfg->last_jobid == submit->job->jobid)
			goto done;

		if (check_frame_change(&submit->info, cfg))
			goto done;
	}

	/* not found, give return value to NULL */
	cfg = NULL;

done:
	mml_trace_ex_end();
	return cfg;
}

static struct mml_task *task_get_idle(struct mml_frame_config *cfg)
{
	struct mml_task *task = list_first_entry_or_null(
		&cfg->done_tasks, struct mml_task, entry);

	if (task) {
		list_del_init(&task->entry);
		cfg->done_task_cnt--;
		memset(&task->buf, 0, sizeof(task->buf));
	}
	return task;
}

static void frame_config_destroy(struct mml_frame_config *cfg)
{
	struct mml_task *task, *tmp;

	mml_msg("[drm]%s frame config %p", __func__, cfg);

	if (!list_empty(&cfg->await_tasks)) {
		mml_err("[drm]still waiting tasks in wq during destroy config");
		list_for_each_entry_safe(task, tmp, &cfg->await_tasks, entry) {
			/* unable to handling error,
			 * print error but not destroy
			 */
			mml_err("[drm]busy task:%p", task);
			list_del_init(&task->entry);
			task->config = NULL;
		}
	}

	if (!list_empty(&cfg->tasks)) {
		mml_err("[drm]still busy tasks during destroy config");
		list_for_each_entry_safe(task, tmp, &cfg->tasks, entry) {
			/* unable to handling error,
			 * print error but not destroy
			 */
			mml_err("[drm]busy task:%p", task);
			list_del_init(&task->entry);
			task->config = NULL;
		}
	}

	while (!list_empty(&cfg->done_tasks)) {
		task = list_first_entry(&cfg->done_tasks, typeof(*task),
			entry);
		list_del_init(&task->entry);
		mml_core_destroy_task(task);
	}

	mml_core_deinit_config(cfg);
	kfree(cfg);

	mml_msg("[drm]%s frame config %p destroy done", __func__, cfg);
}

static void frame_config_destroy_work(struct work_struct *work)
{
	struct mml_frame_config *cfg = container_of(work,
		struct mml_frame_config, work_destroy);

	frame_config_destroy(cfg);
}

static struct mml_frame_config *frame_config_create(
	struct mml_drm_ctx *ctx,
	struct mml_frame_info *info)
{
	struct mml_frame_config *cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);
	if (!cfg)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&cfg->entry);
	list_add_tail(&cfg->entry, &ctx->configs);
	ctx->config_cnt++;
	cfg->info = *info;
	mutex_init(&cfg->task_mutex);
	mutex_init(&cfg->pipe_mutex);
	INIT_LIST_HEAD(&cfg->tasks);
	INIT_LIST_HEAD(&cfg->await_tasks);
	INIT_LIST_HEAD(&cfg->done_tasks);
	cfg->mml = ctx->mml;
	cfg->task_ops = ctx->task_ops;
	INIT_WORK(&cfg->work_destroy, frame_config_destroy_work);

	mml_core_init_config(cfg);

	return cfg;
}

static void frame_buf_to_task_buf(struct mml_file_buf *fbuf,
				  struct mml_buffer *fdbuf)
{
	u8 i;

	mml_buf_get(fbuf, fdbuf->fd, fdbuf->cnt);

	/* also copy size for later use */
	for (i = 0; i < fdbuf->cnt; i++)
		fbuf->size[i] = fdbuf->size[i];
	fbuf->cnt = fdbuf->cnt;
	fbuf->flush = fdbuf->flush;
	fbuf->invalid = fdbuf->invalid;

	if (fdbuf->fence >= 0) {
		fbuf->fence = sync_file_get_fence(fdbuf->fence);
		mml_msg("[drm]get dma fence %p by %d", fbuf->fence, fdbuf->fence);
	}
}

static void task_move_to_running(struct mml_task *task)
{
	/* Must lock ctx->config_mutex before call
	 * For INITIAL / DUPLICATE state move to running,
	 * otherwise do nothing.
	 */
	if (task->state != MML_TASK_INITIAL &&
		task->state != MML_TASK_DUPLICATE &&
		task->state != MML_TASK_REUSE) {
		mml_msg("[drm]%s task %p state conflict %u",
			__func__, task, task->state);
		return;
	}

	list_del_init(&task->entry);
	task->config->await_task_cnt--;
	task->state = MML_TASK_RUNNING;
	list_add_tail(&task->entry, &task->config->tasks);
	task->config->run_task_cnt++;

	mml_msg("[drm]%s task cnt (%u %u %hhu)",
		__func__,
		task->config->await_task_cnt,
		task->config->run_task_cnt,
		task->config->done_task_cnt);
}

static void task_move_to_idle(struct mml_task *task)
{
	/* Must lock ctx->config_mutex before call */
	if (task->state == MML_TASK_INITIAL ||
		task->state == MML_TASK_DUPLICATE ||
		task->state == MML_TASK_REUSE) {
		/* move out from awat list */
		task->config->await_task_cnt--;
	} else if (task->state == MML_TASK_RUNNING) {
		/* move out from tasks list */
		task->config->run_task_cnt--;
	} else {
		/* unknown state transition */
		mml_err("[drm]%s state conflict %d", __func__, task->state);
	}

	list_del_init(&task->entry);
	task->state = MML_TASK_IDLE;
	list_add_tail(&task->entry, &task->config->done_tasks);
	task->config->done_task_cnt++;

	mml_msg("[drm]%s task cnt (%u %u %hhu)",
		__func__,
		task->config->await_task_cnt,
		task->config->run_task_cnt,
		task->config->done_task_cnt);
}

static void task_move_to_destroy(struct kref *kref)
{
	struct mml_task *task = container_of(kref,
		struct mml_task, ref);

	mml_core_destroy_task(task);
}

static void task_submit_done(struct mml_task *task)
{
	struct mml_drm_ctx *ctx = (struct mml_drm_ctx *)task->ctx;

	mml_msg("[drm]%s task %p state %u", __func__, task, task->state);

	mutex_lock(&ctx->config_mutex);
	task_move_to_running(task);
	mutex_unlock(&ctx->config_mutex);

	kref_put(&task->ref, task_move_to_destroy);
}

static void task_frame_done(struct mml_task *task)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_config *cfg_tmp;
	struct mml_drm_ctx *ctx = (struct mml_drm_ctx *)task->ctx;
	u8 i;

	mml_log("[drm]frame done task %p state %u job %u",
		task, task->state, task->job.jobid);

	/* clean up */
	for (i = 0; i < task->buf.dest_cnt; i++) {
		mml_msg("[drm]release dest %hhu iova %#011llx",
			i, task->buf.dest[i].dma[0].iova);
		mml_buf_put(&task->buf.dest[i]);
	}
	mml_msg("[drm]release src iova %#011llx",
		task->buf.src.dma[0].iova);
	mml_buf_put(&task->buf.src);

	/* TODO: Confirm buf file and fence release correctly,
	 * after implement dmabuf and fence mechanism.
	 */

	mutex_lock(&ctx->config_mutex);
	task_move_to_idle(task);

	if (cfg->done_task_cnt > mml_max_cache_task) {
		task = list_first_entry(&cfg->done_tasks, typeof(*task),
			entry);
		list_del_init(&task->entry);
		cfg->done_task_cnt--;
		mml_msg("[drm]%s task cnt (%u %u %hhu)",
			__func__,
			task->config->await_task_cnt,
			task->config->run_task_cnt,
			task->config->done_task_cnt);
		kref_put(&task->ref, task_move_to_destroy);
	}

	/* still have room to cache, done */
	if (ctx->config_cnt <= mml_max_cache_cfg)
		goto done;

	/* must pick cfg from list which is not running */
	list_for_each_entry_safe(cfg, cfg_tmp, &ctx->configs, entry) {
		/* only remove config not running */
		if (!list_empty(&cfg->tasks) || !list_empty(&cfg->await_tasks))
			continue;
		list_del_init(&cfg->entry);
		queue_work(ctx->wq_destroy, &cfg->work_destroy);
		ctx->config_cnt--;
		mml_msg("[drm]config %p send destroy remain %u",
			cfg, ctx->config_cnt);

		/* check cache num again */
		if (ctx->config_cnt <= mml_max_cache_cfg)
			break;
	}

done:
	mutex_unlock(&ctx->config_mutex);
}

static void dump_pq_en(u32 idx, struct mml_pq_param *pq_param,
	struct mml_pq_config *pq_config)
{
	u32 pqen = 0;

	memcpy(&pqen, pq_config, min(sizeof(*pq_config), sizeof(pqen)));

	if (pq_param)
		mml_log("[drm]PQ %u config %#x param en %u %s",
			idx, pqen, pq_param->enable,
			mml_pq_disable ? "FORCE DISABLE" : "");
	else
		mml_log("[drm]PQ %u config %#x param NULL %s",
			idx, pqen,
			mml_pq_disable ? "FORCE DISABLE" : "");
}

s32 mml_drm_submit(struct mml_drm_ctx *ctx, struct mml_submit *submit)
{
	struct mml_frame_config *cfg;
	struct mml_task *task;
	s32 result;
	u32 i;
	struct fence_data fence = {0};

	mml_trace_begin("%s", __func__);

	if (mtk_mml_msg || mml_pq_disable) {
		for (i = 0; i < submit->info.dest_cnt; i++) {
			dump_pq_en(i, submit->pq_param[i],
				&submit->info.dest[i].pq_config);

			if (mml_pq_disable) {
				submit->pq_param[i] = NULL;
				memset(&submit->info.dest[i].pq_config, 0,
					sizeof(submit->info.dest[i].pq_config));
			}
		}
	}

	mutex_lock(&ctx->config_mutex);

	cfg = frame_config_find_reuse(ctx, submit);
	if (cfg) {
		mml_msg("[drm]%s reuse config %p", __func__, cfg);
		task = task_get_idle(cfg);
		if (task) {
			/* reuse case change state IDLE to REUSE */
			task->state = MML_TASK_REUSE;
			init_completion(&task->pkts[0]->cmplt);
			if (task->pkts[1])
				init_completion(&task->pkts[1]->cmplt);
			mml_msg("[drm]reuse task %p pkt %p %p",
				task, task->pkts[0], task->pkts[1]);
		} else {
			task = mml_core_create_task();
			if (IS_ERR(task)) {
				result = PTR_ERR(task);
				goto err_unlock_exit;
			}
			task->config = cfg;
			task->state = MML_TASK_DUPLICATE;
		}
	} else {
		cfg = frame_config_create(ctx, &submit->info);
		mml_msg("[drm]%s create config %p", __func__, cfg);
		if (IS_ERR(cfg)) {
			result = PTR_ERR(cfg);
			goto err_unlock_exit;
		}
		task = mml_core_create_task();
		if (IS_ERR(task)) {
			list_del_init(&cfg->entry);
			frame_config_destroy(cfg);
			result = PTR_ERR(task);
			goto err_unlock_exit;
		}
		task->config = cfg;
	}

	/* make sure id unique and cached last */
	task->job.jobid = atomic_inc_return(&ctx->job_serial);
	cfg->last_jobid = task->job.jobid;
	list_add_tail(&task->entry, &cfg->await_tasks);
	cfg->await_task_cnt++;
	mml_msg("[drm]%s task cnt (%u %u %hhu)",
		__func__,
		task->config->await_task_cnt,
		task->config->run_task_cnt,
		task->config->done_task_cnt);

	mutex_unlock(&ctx->config_mutex);

	/* copy per-frame info */
	task->ctx = ctx;
	task->end_time.tv_sec = submit->end.sec;
	task->end_time.tv_nsec = submit->end.nsec;
	frame_buf_to_task_buf(&task->buf.src, &submit->buffer.src);
	task->buf.dest_cnt = submit->buffer.dest_cnt;
	for (i = 0; i < submit->buffer.dest_cnt; i++)
		frame_buf_to_task_buf(&task->buf.dest[i],
				      &submit->buffer.dest[i]);

	/* create fence for this task */
	fence.value = task->job.jobid;
	if (submit->job && ctx->timeline &&
		mtk_sync_fence_create(ctx->timeline, &fence) >= 0) {
		task->job.fence = fence.fence;
		task->fence = sync_file_get_fence(task->job.fence);
	} else {
		task->job.fence = -1;
	}
	mml_log("[drm]mml job %u fence fd %d task %p fence %p config %p",
		task->job.jobid, task->job.fence, task, task->fence, cfg);

	/* copy pq parameters */
	for (i = 0; i < submit->buffer.dest_cnt && submit->pq_param[i]; i++)
		memcpy(&task->pq_param[i], submit->pq_param[i], sizeof(struct mml_pq_param));

	/* submit to core */
	mml_core_submit_task(cfg, task);

	/* copy job content back */
	if (submit->job)
		memcpy(submit->job, &task->job, sizeof(*submit->job));

	mml_trace_end();
	return 0;

err_unlock_exit:
	mutex_unlock(&ctx->config_mutex);
	mml_trace_end();
	return result;
}
EXPORT_SYMBOL_GPL(mml_drm_submit);

s32 dup_task(struct mml_task *task, u32 pipe)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_drm_ctx *ctx = (struct mml_drm_ctx *)task->ctx;
	struct mml_task *src;

	if (task->pkts[pipe]) {
		mml_err("[drm]%s task %p pipe %hhu already has pkt before dup",
			__func__, task, pipe);
		return -EINVAL;
	}

	mutex_lock(&ctx->config_mutex);

	mml_msg("[drm]%s task cnt (%u %u %hhu) task %p pipe %u config %p",
		__func__,
		task->config->await_task_cnt,
		task->config->run_task_cnt,
		task->config->done_task_cnt,
		task, pipe, task->config);

	/* check if available done task first */
	src = list_first_entry_or_null(&cfg->done_tasks, struct mml_task,
				       entry);
	if (src && src->pkts[pipe])
		goto dup_command;

	src = list_first_entry_or_null(&cfg->tasks, struct mml_task, entry);
	if (src)
		goto dup_command;


	list_for_each_entry_reverse(src, &cfg->await_tasks, entry) {
		/* the first one should be current task, skip it */
		if (src == task || !src->pkts[pipe]) {
			mml_msg("[drm]%s await task %p pkt %p",
				__func__, src, src->pkts[pipe]);
			continue;
		}
		goto dup_command;
	}

	mutex_unlock(&ctx->config_mutex);
	return -EBUSY;

dup_command:
	task->pkts[pipe] = cmdq_pkt_create(cfg->path[pipe]->clt);
	cmdq_pkt_copy(task->pkts[pipe], src->pkts[pipe]);

	task->reuse[pipe].labels = kcalloc(cfg->cache[pipe].label_cnt,
		sizeof(*task->reuse[pipe].labels), GFP_KERNEL);
	if (task->reuse[pipe].labels) {
		memcpy(task->reuse[pipe].labels, src->reuse[pipe].labels,
			sizeof(*task->reuse[pipe].labels) * cfg->cache[pipe].label_cnt);
		task->reuse[pipe].label_idx = src->reuse[pipe].label_idx;
		cmdq_reuse_refresh(task->pkts[pipe], task->reuse[pipe].labels,
			task->reuse[pipe].label_idx);
	} else {
		mml_err("[drm]copy reuse labels fail");
	}

	mutex_unlock(&ctx->config_mutex);
	return 0;
}

const static struct mml_task_ops drm_task_ops = {
	.submit_done = task_submit_done,
	.frame_done = task_frame_done,
	.dup_task = dup_task,
};

static struct mml_drm_ctx *drm_ctx_create(struct mml_dev *mml)
{
	struct mml_drm_ctx *ctx;

	mml_msg("[drm]%s on dev %p", __func__, mml);

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&ctx->configs);
	mutex_init(&ctx->config_mutex);
	ctx->mml = mml;
	ctx->task_ops = &drm_task_ops;
	ctx->wq_destroy = alloc_ordered_workqueue("mml_destroy", 0, 0);

	ctx->timeline = mtk_sync_timeline_create("mml_timeline");
	if (!ctx->timeline)
		mml_err("[drm]fail to create timeline");
	else
		mml_msg("[drm]timeline for mml %p", ctx->timeline);

	return ctx;
}

struct mml_drm_ctx *mml_drm_get_context(struct platform_device *pdev)
{
	struct mml_dev *mml = platform_get_drvdata(pdev);

	mml_msg("[drm]%s", __func__);
	if (!mml) {
		mml_err("[drm]%s not init mml", __func__);
		return ERR_PTR(-EPERM);
	}
	return mml_dev_get_drm_ctx(mml, drm_ctx_create);
}
EXPORT_SYMBOL_GPL(mml_drm_get_context);

static void drm_ctx_release(struct mml_drm_ctx *ctx)
{
	struct mml_frame_config *cfg, *tmp;

	mml_msg("[drm]%s on ctx %p", __func__, ctx);

	mutex_lock(&ctx->config_mutex);
	list_for_each_entry_safe(cfg, tmp, &ctx->configs, entry) {
		/* check and remove configs/tasks in this context */
		list_del_init(&cfg->entry);
		frame_config_destroy(cfg);
	}

	mutex_unlock(&ctx->config_mutex);
	destroy_workqueue(ctx->wq_destroy);
	kfree(ctx);

	mtk_sync_timeline_destroy(ctx->timeline);
}

void mml_drm_put_context(struct mml_drm_ctx *ctx)
{
	mml_log("[drm]%s", __func__);
	mml_dev_put_drm_ctx(ctx->mml, drm_ctx_release);
}
EXPORT_SYMBOL_GPL(mml_drm_put_context);

int mml_ddp_comp_register(struct drm_device *drm, struct mtk_ddp_comp *comp)
{
	struct mtk_drm_private *private = drm->dev_private;

	if (private->ddp_comp[comp->id])
		return -EBUSY;

	if (comp->id < 0)
		return -EINVAL;

	private->ddp_comp[comp->id] = comp;
	return 0;
}

void mml_ddp_comp_unregister(struct drm_device *drm, struct mtk_ddp_comp *comp)
{
	struct mtk_drm_private *private = drm->dev_private;
	if (comp && comp->id >= 0)
		private->ddp_comp[comp->id] = NULL;
}

