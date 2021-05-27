// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/atomic.h>
#include <linux/file.h>

#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"

#define MML_REF_NAME "mml"

struct mml_drm_ctx {
	struct list_head configs;
	u32 config_cnt;
	struct mutex config_mutex;
	struct mml_dev *mml;
	const struct mml_task_ops *task_ops;
	atomic_t job_serial;
};

static bool check_hw_cap(struct mml_frame_data *src,
			 struct mml_frame_dest *dest,
			 u8 cnt)
{
	/* TODO: porting dpframework code */
	return true;
}

static bool check_read_format(u32 format)
{
	/* TODO: porting dpframework code */
	return true;
}

static bool check_write_format(u32 format)
{
	/* TODO: porting dpframework code */
	return true;
}

static enum mml_mode query_mode(struct mml_frame_info *info)
{
	/* TODO: find mode by table */
	return MML_MODE_MML_DECOUPLE;
}

struct mml_cap mml_drm_query_cap(struct mml_frame_info *info)
{
	struct mml_cap cap = {0};
	u8 i;

	if (!check_hw_cap(&info->src, info->dest, info->dest_cnt))
		goto not_support;

	if (!check_read_format(info->src.format))
		goto not_support;

	for (i = 0; i < info->dest_cnt; i++)
		if (!check_write_format(info->dest[i].data.format))
			goto not_support;

	cap.target = query_mode(info);
	return cap;

not_support:
	cap.target = MML_MODE_NOT_SUPPORT;
	return cap;
}

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

	list_for_each_entry(cfg, &ctx->configs, entry) {
		if (submit->update && cfg->last_jobid == submit->job->jobid)
			return cfg;

		if (check_frame_change(&submit->info, cfg))
			return cfg;
	}

	return NULL;
}

static struct mml_task *task_get_idle(struct mml_frame_config *cfg)
{
	struct mml_task *task = list_first_entry_or_null(
		&cfg->done_tasks, struct mml_task, entry);

	if (task)
		list_del_init(&task->entry);
	return task;
}

static struct mml_frame_config *frame_config_create(
	struct mml_drm_ctx *ctx,
	struct mml_frame_info *info)
{
	struct mml_frame_config *cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);
	if (!cfg)
		return ERR_PTR(-ENOMEM);

	cfg->mml = ctx->mml;
	cfg->task_ops = ctx->task_ops;
	cfg->info = *info;
	mutex_init(&cfg->task_mutex);
	INIT_LIST_HEAD(&cfg->entry);
	list_add_tail(&cfg->entry, &ctx->configs);

	return cfg;
}

static void frame_config_destroy(struct mml_frame_config *cfg)
{
	struct mml_task *task, *tmp;

	if (unlikely(!list_empty(&cfg->tasks))) {
		mml_log("%s task still running");
		return;
	}

	list_del_init(&cfg->entry);

	if (!list_empty(&cfg->tasks)) {
		mml_err("still busy tasks during destroy config");
		list_for_each_entry_safe(task, tmp, &cfg->tasks, entry) {
			/* unable to handling error,
			 * print error but not destroy
			 */
			mml_err("busy task:%p", task);
			list_del_init(&task->entry);
			task->config = NULL;
		}
	}

	list_for_each_entry_safe(task, tmp, &cfg->done_tasks, entry) {
		list_del_init(&task->entry);
		mml_core_destroy_task(task);
	}

	kfree(cfg);
}

static void frame_buf_to_task_buf(struct mml_file_buf *fbuf,
				  struct mml_buffer *fdbuf)
{
	u8 i;

	for (i = 0; i < fdbuf->cnt; i++) {
		fbuf->f[i] = fget(fdbuf->fd[i]);
		fbuf->size[i] = fdbuf->size[i];
	}
	fbuf->cnt = fdbuf->cnt;
	fbuf->usage = fdbuf->usage;

	/* TODO: fence replace with timeline file later */
	/* fbuf->fence = fdbuf->fence; */
}

static void task_submit_done(struct mml_task *task)
{
	struct mml_drm_ctx *ctx = (struct mml_drm_ctx *)task->ctx;

	mutex_lock(&ctx->config_mutex);
	task->state = MML_TASK_RUNNING;
	list_add_tail(&task->config->tasks, &task->entry);
	mutex_unlock(&ctx->config_mutex);
}

static void task_frame_done(struct mml_task *task)
{
	struct mml_drm_ctx *ctx = (struct mml_drm_ctx *)task->ctx;
	u8 idx_d, idx_p;

	/* clean up */
	for (idx_d = 0; idx_d < task->buf.dest_cnt; idx_d++)
		for (idx_p = 0; idx_p < task->buf.dest[idx_d].cnt; idx_p++)
			fput(task->buf.dest[idx_d].f[idx_p]);
	for (idx_p = 0; idx_p < task->buf.src.cnt; idx_p++)
		fput(task->buf.src.f[idx_p]);

	mutex_lock(&ctx->config_mutex);
	list_del_init(&task->entry);
	task->state = MML_TASK_IDLE;
	list_add_tail(&task->config->done_tasks, &task->entry);
	mutex_unlock(&ctx->config_mutex);
}

s32 mml_drm_submit(struct mml_drm_ctx *ctx, struct mml_submit *submit)
{
	struct mml_frame_config *cfg;
	struct mml_task *task;
	s32 result;
	u8 i;

	mutex_lock(&ctx->config_mutex);

	cfg = frame_config_find_reuse(ctx, submit);
	if (cfg) {
		task = task_get_idle(cfg);
		if (task) {
			/* reuse case change state IDLE to REUSE */
			task->state = MML_TASK_REUSE;
		} else {
			task = mml_core_create_task();
			if (IS_ERR(task))
				return PTR_ERR(task);
			task->config = cfg;
		}
	} else {
		cfg = frame_config_create(ctx, &submit->info);
		if (IS_ERR(cfg))
			return PTR_ERR(cfg);
		task = mml_core_create_task();
		if (IS_ERR(task)) {
			frame_config_destroy(cfg);
			return PTR_ERR(task);
		}
		task->config = cfg;
	}

	/* make sure id unique and cached last */
	task->job.jobid = atomic_inc_return(&ctx->job_serial);
	cfg->last_jobid = task->job.jobid;

	mutex_unlock(&ctx->config_mutex);

	/* copy per-frame info */
	task->ctx = ctx;
	task->end_time.tv_sec = submit->end.sec;
	task->end_time.tv_nsec = submit->end.nsec;
	frame_buf_to_task_buf(&task->buf.src, &submit->buffer.src);
	for (i = 0; i < submit->buffer.dest_cnt; i++)
		frame_buf_to_task_buf(&task->buf.dest[i],
				      &submit->buffer.dest[i]);

	/* TODO: create fence to task->job.fence */

	/* submit to core */
	result = mml_core_submit_task(cfg, task);

	return result;
}

const static struct mml_task_ops drm_task_ops = {
	.submit_done = task_submit_done,
	.frame_done = task_frame_done,
};

static struct mml_drm_ctx *drm_ctx_create(struct mml_dev *mml)
{
	struct mml_drm_ctx *ctx;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return ERR_PTR(-ENOMEM);

	mutex_init(&ctx->config_mutex);
	ctx->task_ops = &drm_task_ops;
	return ctx;
}

struct mml_drm_ctx *mml_drm_get_context(struct platform_device *pdev)
{
	struct mml_dev *mml = platform_get_drvdata(pdev);

	mml_log("%s", __func__);
	if (!mml) {
		mml_err("%s not init mml", __func__);
		return ERR_PTR(-EPERM);
	}
	return mml_dev_get_drm_ctx(mml, drm_ctx_create);
}
EXPORT_SYMBOL_GPL(mml_drm_get_context);

static void drm_ctx_release(struct mml_drm_ctx *ctx)
{
	struct mml_frame_config *cfg, *tmp;

	mutex_lock(&ctx->config_mutex);
	list_for_each_entry_safe(cfg, tmp, &ctx->configs, entry) {
		/* check and remove configs/tasks in this context */
		frame_config_destroy(cfg);
	}

	mutex_unlock(&ctx->config_mutex);
	kfree(ctx);
}

void mml_drm_put_context(struct mml_drm_ctx *ctx)
{
	mml_log("%s", __func__);
	mml_dev_put_drm_ctx(ctx->mml, drm_ctx_release);
}
EXPORT_SYMBOL_GPL(mml_drm_put_context);
