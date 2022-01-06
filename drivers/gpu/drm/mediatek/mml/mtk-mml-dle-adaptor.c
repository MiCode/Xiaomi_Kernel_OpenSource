// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <mtk_drm_drv.h>

#include "mtk-mml-dle-adaptor.h"
#include "mtk-mml-buf.h"
#include "mtk-mml-color.h"
#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "mtk-mml-mmp.h"

/* set to 0 to disable reuse config */
int dle_reuse = 1;
module_param(dle_reuse, int, 0644);

int dle_max_cache_task = 1;
module_param(dle_max_cache_task, int, 0644);

int dle_max_cache_cfg = 2;
module_param(dle_max_cache_cfg, int, 0644);

struct mml_dle_ctx {
	struct list_head configs;
	u32 config_cnt;
	struct mutex config_mutex;
	struct mml_dev *mml;
	const struct mml_task_ops *task_ops;
	atomic_t job_serial;
	struct workqueue_struct *wq_config;
	struct workqueue_struct *wq_destroy;
	struct kthread_worker kt_done;
	struct task_struct *kt_done_task;
	bool kt_priority;
	bool dl_dual;
	void (*config_cb)(struct mml_task *task, void *cb_param);
	struct mml_tile_cache tile_cache[MML_PIPE_CNT];
};

/* dle extension of mml_frame_config */
struct mml_dle_frame_config {
	struct mml_frame_config c;
	/* tile struct in frame mode for each pipe */
	struct mml_tile_config tile[MML_PIPE_CNT];
};

static bool check_frame_change(struct mml_frame_info *info,
			       struct mml_frame_config *cfg)
{
	return !memcmp(&cfg->info, info, sizeof(*info));
}

static bool check_dle_frame_change(struct mml_dle_frame_info *info,
				   struct mml_frame_config *cfg)
{
	return !memcmp(cfg->dl_out, info->dl_out, sizeof(info->dl_out));
}

static struct mml_frame_config *frame_config_find_reuse(
	struct mml_dle_ctx *ctx,
	struct mml_submit *submit,
	struct mml_dle_frame_info *dle_info)
{
	struct mml_frame_config *cfg;
	u32 idx = 0;

	if (!dle_reuse)
		return NULL;

	mml_trace_ex_begin("%s", __func__);

	list_for_each_entry(cfg, &ctx->configs, entry) {
		if (submit->update && cfg->last_jobid == submit->job->jobid)
			goto done;

		if (check_frame_change(&submit->info, cfg) &&
		    check_dle_frame_change(dle_info, cfg))
			goto done;

		idx++;
	}

	/* not found, give return value to NULL */
	cfg = NULL;

done:
	if (cfg && idx)
		list_rotate_to_front(&cfg->entry, &ctx->configs);

	mml_trace_ex_end();
	return cfg;
}

static struct mml_frame_config *frame_config_find_current(
	struct mml_dle_ctx *ctx)
{
	return list_first_entry_or_null(
		&ctx->configs, struct mml_frame_config, entry);
}

static struct mml_task *task_find_running(struct mml_frame_config *cfg)
{
	return list_first_entry_or_null(
		&cfg->tasks, struct mml_task, entry);
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

	mml_msg("[dle]%s frame config %p", __func__, cfg);

	if (WARN_ON(!list_empty(&cfg->await_tasks))) {
		mml_err("[dle]still waiting tasks in wq during destroy config");
		list_for_each_entry_safe(task, tmp, &cfg->await_tasks, entry) {
			/* unable to handling error,
			 * print error but not destroy
			 */
			mml_err("[dle]busy task:%p", task);
			list_del_init(&task->entry);
			task->config = NULL;
		}
	}

	if (WARN_ON(!list_empty(&cfg->tasks))) {
		mml_err("[dle]still busy tasks during destroy config");
		list_for_each_entry_safe(task, tmp, &cfg->tasks, entry) {
			/* unable to handling error,
			 * print error but not destroy
			 */
			mml_err("[dle]busy task:%p", task);
			list_del_init(&task->entry);
			task->config = NULL;
		}
	}

	mml_core_deinit_config(cfg);
	kfree(container_of(cfg, struct mml_dle_frame_config, c));

	mml_msg("[dle]%s frame config %p destroy done", __func__, cfg);
}

static void frame_config_destroy_work(struct work_struct *work)
{
	struct mml_frame_config *cfg = container_of(work,
		struct mml_frame_config, work_destroy);

	frame_config_destroy(cfg);
}

static void frame_config_queue_destroy(struct kref *kref)
{
	struct mml_frame_config *cfg = container_of(kref, struct mml_frame_config, ref);
	struct mml_dle_ctx *ctx = cfg->ctx;

	queue_work(ctx->wq_destroy, &cfg->work_destroy);
}

static struct mml_frame_config *frame_config_create(
	struct mml_dle_ctx *ctx,
	struct mml_frame_info *info,
	struct mml_dle_frame_info *dle_info)
{
	struct mml_dle_frame_config *dle_cfg = kzalloc(sizeof(*dle_cfg), GFP_KERNEL);
	struct mml_frame_config *cfg;

	if (!dle_cfg)
		return ERR_PTR(-ENOMEM);
	cfg = &dle_cfg->c;
	mml_core_init_config(cfg);

	list_add(&cfg->entry, &ctx->configs);
	ctx->config_cnt++;
	cfg->info = *info;
	memcpy(cfg->dl_out, dle_info->dl_out, sizeof(cfg->dl_out));
	cfg->disp_dual = ctx->dl_dual;
	cfg->ctx = ctx;
	cfg->mml = ctx->mml;
	cfg->task_ops = ctx->task_ops;
	cfg->ctx_kt_done = &ctx->kt_done;
	INIT_WORK(&cfg->work_destroy, frame_config_destroy_work);
	kref_init(&cfg->ref);

	return cfg;
}

static void frame_buf_to_task_buf(struct mml_file_buf *fbuf,
				  struct mml_buffer *user_buf,
				  const char *name)
{
	u8 i;

	if (user_buf->use_dma)
		mml_buf_get(fbuf, user_buf->dmabuf, user_buf->cnt, name);
	else
		mml_buf_get_fd(fbuf, user_buf->fd, user_buf->cnt, name);

	/* also copy size for later use */
	for (i = 0; i < user_buf->cnt; i++)
		fbuf->size[i] = user_buf->size[i];
	fbuf->cnt = user_buf->cnt;
	fbuf->flush = user_buf->flush;
	fbuf->invalid = user_buf->invalid;
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
		mml_msg("[dle]%s task %p state conflict %u",
			__func__, task, task->state);
		return;
	}

	if (list_empty(&task->entry)) {
		mml_err("[dle]%s task %p already leave config",
			__func__, task);
		return;
	}

	list_del_init(&task->entry);
	task->config->await_task_cnt--;
	task->state = MML_TASK_RUNNING;
	list_add_tail(&task->entry, &task->config->tasks);
	task->config->run_task_cnt++;

	mml_msg("[dle]%s task cnt (%u %u %hhu)",
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
		mml_err("[dle]%s state conflict %d", __func__, task->state);
	}

	list_del_init(&task->entry);
	task->state = MML_TASK_IDLE;
	list_add_tail(&task->entry, &task->config->done_tasks);
	task->config->done_task_cnt++;

	mml_msg("[dle]%s task cnt (%u %u %hhu)",
		__func__,
		task->config->await_task_cnt,
		task->config->run_task_cnt,
		task->config->done_task_cnt);
}

static void task_move_to_destroy(struct kref *kref)
{
	struct mml_task *task = container_of(kref,
		struct mml_task, ref);

	if (task->config)
		kref_put(&task->config->ref, frame_config_queue_destroy);

	mml_core_destroy_task(task);
}

static void task_config_done(struct mml_task *task)
{
	struct mml_dle_ctx *ctx = task->ctx;

	mml_msg("[dle]%s task %p state %u", __func__, task, task->state);

	/* mml_mmp(config_cb, MMPROFILE_FLAG_PULSE, task->job.jobid, 0); */

	if (ctx->config_cb)
		ctx->config_cb(task, task->cb_param);

	mutex_lock(&ctx->config_mutex);
	task_move_to_running(task);
	kref_put(&task->ref, task_move_to_destroy);
	mutex_unlock(&ctx->config_mutex);
}

static void task_buf_put(struct mml_task *task)
{
	u8 i;

	mml_trace_ex_begin("%s_putbuf", __func__);
	for (i = 0; i < task->buf.dest_cnt; i++) {
		mml_msg("[dle]release dest %hhu iova %#011llx",
			i, task->buf.dest[i].dma[0].iova);
		mml_buf_put(&task->buf.dest[i]);
	}
	mml_msg("[dle]release src iova %#011llx",
		task->buf.src.dma[0].iova);
	mml_buf_put(&task->buf.src);
	mml_trace_ex_end();
}

static struct mml_task *task_get_idle_or_running(struct mml_frame_config *cfg)
{
	struct mml_task *task = task_get_idle(cfg);

	if (task)
		return task;
	task = task_find_running(cfg);
	if (task) {
		/* stop this running task */
		mml_msg("[dle]stop task %p state %u job %u",
			task, task->state, task->job.jobid);

		task_buf_put(task);

		list_del_init(&task->entry);
		cfg->run_task_cnt--;
		memset(&task->buf, 0, sizeof(task->buf));
	}
	return task;
}

static void task_put_idles(struct mml_frame_config *cfg)
{
	struct mml_task *task, *task_tmp;

	list_for_each_entry_safe(task, task_tmp, &cfg->done_tasks, entry) {
		list_del_init(&task->entry);
		kref_put(&task->ref, task_move_to_destroy);
	}
}

static void task_state_dec(struct mml_frame_config *cfg, struct mml_task *task,
	const char *api)
{
	if (list_empty(&task->entry))
		return;

	list_del_init(&task->entry);

	switch (task->state) {
	case MML_TASK_INITIAL:
	case MML_TASK_DUPLICATE:
	case MML_TASK_REUSE:
		cfg->await_task_cnt--;
		break;
	case MML_TASK_RUNNING:
		cfg->run_task_cnt--;
		break;
	case MML_TASK_IDLE:
		cfg->done_task_cnt--;
		break;
	default:
		mml_err("%s conflict state %u", api, task->state);
	}
}

static void task_frame_err(struct mml_task *task)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_dle_ctx *ctx = task->ctx;

	mml_trace_ex_begin("%s", __func__);

	mml_log("[dle]config err task %p state %u job %u",
		task, task->state, task->job.jobid);

	/* clean up */
	task_buf_put(task);

	mutex_lock(&ctx->config_mutex);

	task_state_dec(cfg, task, __func__);
	mml_err("[dle]%s task cnt (%u %u %hhu) error from state %d",
		__func__,
		cfg->await_task_cnt,
		cfg->run_task_cnt,
		cfg->done_task_cnt,
		task->state);
	kref_put(&task->ref, task_move_to_destroy);

	mutex_unlock(&ctx->config_mutex);

	/* mml_lock_wake_lock(mml, false); */

	mml_trace_ex_end();
}

void mml_dle_start(struct mml_dle_ctx *ctx)
{
	struct mml_frame_config *cfg;
	struct mml_frame_config *tmp;
	struct mml_task *task;

	mml_trace_ex_begin("%s", __func__);

	mutex_lock(&ctx->config_mutex);

	cfg = frame_config_find_current(ctx);
	if (!cfg) {
		mml_log("%s cannot find current cfg", __func__);
		goto done;
	}

	/* clean up */
	if (cfg->done_task_cnt > dle_max_cache_task) {
		task = list_first_entry(&cfg->done_tasks, typeof(*task), entry);
		list_del_init(&task->entry);
		cfg->done_task_cnt--;
		mml_msg("[dle]%s task cnt (%u %u %hhu)",
			__func__,
			task->config->await_task_cnt,
			task->config->run_task_cnt,
			task->config->done_task_cnt);
		kref_put(&task->ref, task_move_to_destroy);
	}

	/* still have room to cache, done */
	if (ctx->config_cnt <= dle_max_cache_cfg)
		goto done;

	/* must pick cfg from list which is not running */
	list_for_each_entry_safe_reverse(cfg, tmp, &ctx->configs, entry) {
		/* only remove config not running */
		if (!list_empty(&cfg->tasks) || !list_empty(&cfg->await_tasks))
			continue;
		list_del_init(&cfg->entry);
		task_put_idles(cfg);
		kref_put(&cfg->ref, frame_config_queue_destroy);
		ctx->config_cnt--;
		mml_msg("[dle]config %p send destroy remain %u",
			cfg, ctx->config_cnt);

		/* check cache num again */
		if (ctx->config_cnt <= dle_max_cache_cfg)
			break;
	}

done:
	mutex_unlock(&ctx->config_mutex);

	/* mml_lock_wake_lock(mml, false); */

	mml_trace_ex_end();
}

static void dump_pq_en(u32 idx, struct mml_pq_param *pq_param,
	struct mml_pq_config *pq_config)
{
	u32 pqen = 0;

	memcpy(&pqen, pq_config, min(sizeof(*pq_config), sizeof(pqen)));

	if (pq_param)
		mml_msg("[dle]PQ %u config %#x param en %u %s",
			idx, pqen, pq_param->enable,
			mml_pq_disable ? "FORCE DISABLE" : "");
	else
		mml_msg("[dle]PQ %u config %#x param NULL %s",
			idx, pqen,
			mml_pq_disable ? "FORCE DISABLE" : "");
}

static void frame_calc_plane_offset(struct mml_frame_data *data,
	struct mml_buffer *buf)
{
	u32 i;

	data->plane_offset[0] = 0;
	for (i = 1; i < MML_FMT_PLANE(data->format); i++) {
		if (buf->fd[i] != buf->fd[i-1] && buf->fd[i] >= 0) {
			/* different buffer for different plane, set to begin */
			data->plane_offset[i] = 0;
			continue;
		}
		data->plane_offset[i] = data->plane_offset[i-1] + buf->size[i-1];
	}
}

s32 mml_dle_config(struct mml_dle_ctx *ctx, struct mml_submit *submit,
		   struct mml_dle_frame_info *dle_info, void *cb_param)
{
	struct mml_frame_config *cfg;
	struct mml_task *task;
	s32 result;
	u32 i;

	mml_trace_begin("%s", __func__);

	if (mtk_mml_msg || mml_pq_disable) {
		for (i = 0; i < MML_MAX_OUTPUTS; i++) {
			dump_pq_en(i, submit->pq_param[i],
				&submit->info.dest[i].pq_config);

			if (mml_pq_disable) {
				submit->pq_param[i] = NULL;
				memset(&submit->info.dest[i].pq_config, 0,
					sizeof(submit->info.dest[i].pq_config));
			}
		}
	}

	/* always fixup plane offset */
	frame_calc_plane_offset(&submit->info.src, &submit->buffer.src);
	for (i = 0; i < submit->info.dest_cnt; i++)
		frame_calc_plane_offset(&submit->info.dest[i].data,
			&submit->buffer.dest[i]);

	/* mml_mmp(submit, MMPROFILE_FLAG_PULSE, atomic_read(&ctx->job_serial), 0); */

	mutex_lock(&ctx->config_mutex);

	cfg = frame_config_find_reuse(ctx, submit, dle_info);
	if (cfg) {
		mml_msg("[dle]%s reuse config %p", __func__, cfg);
		task = task_get_idle_or_running(cfg);
		if (task) {
			/* reuse case change state IDLE to REUSE */
			task->state = MML_TASK_REUSE;
			mml_msg("[dle]reuse task %p", task);
		} else {
			task = mml_core_create_task();
			if (IS_ERR(task)) {
				result = PTR_ERR(task);
				mml_err("%s create task for reuse frame fail", __func__);
				goto err_unlock_exit;
			}
			task->config = cfg;
			task->state = MML_TASK_DUPLICATE;
		}
	} else {
		cfg = frame_config_create(ctx, &submit->info, dle_info);
		mml_msg("[dle]%s create config %p", __func__, cfg);
		if (IS_ERR(cfg)) {
			result = PTR_ERR(cfg);
			mml_err("%s create frame config fail", __func__);
			goto err_unlock_exit;
		}
		task = mml_core_create_task();
		if (IS_ERR(task)) {
			list_del_init(&cfg->entry);
			frame_config_destroy(cfg);
			result = PTR_ERR(task);
			mml_err("%s create task fail", __func__);
			goto err_unlock_exit;
		}
		task->config = cfg;
	}

	/* add more count for new task create */
	kref_get(&cfg->ref);

	/* make sure id unique and cached last */
	task->job.jobid = atomic_inc_return(&ctx->job_serial);
	task->cb_param = cb_param;
	cfg->last_jobid = task->job.jobid;
	list_add_tail(&task->entry, &cfg->await_tasks);
	cfg->await_task_cnt++;
	mml_msg("[dle]%s task cnt (%u %u %hhu)",
		__func__,
		task->config->await_task_cnt,
		task->config->run_task_cnt,
		task->config->done_task_cnt);

	mutex_unlock(&ctx->config_mutex);

	/* copy per-frame info */
	task->ctx = ctx;
	frame_buf_to_task_buf(&task->buf.src, &submit->buffer.src, "mml_rdma");
	task->buf.dest_cnt = submit->buffer.dest_cnt;
	for (i = 0; i < submit->buffer.dest_cnt; i++)
		frame_buf_to_task_buf(&task->buf.dest[i],
				      &submit->buffer.dest[i],
				      "mml_wrot");

	/* no fence for dle task */
	task->job.fence = -1;
	mml_log("[dle]mml job %u task %p config %p mode %hhu",
		task->job.jobid, task, cfg, cfg->info.mode);

	/* copy job content back */
	if (submit->job)
		memcpy(submit->job, &task->job, sizeof(*submit->job));

	/* copy pq parameters */
	for (i = 0; i < submit->buffer.dest_cnt && submit->pq_param[i]; i++)
		memcpy(&task->pq_param[i], submit->pq_param[i], sizeof(task->pq_param[i]));

	/* wake lock */
	/* mml_lock_wake_lock(task->config->mml, true); */

	/* get config from core */
	mml_core_config_task(cfg, task);

	mml_trace_end();
	return 0;

err_unlock_exit:
	mutex_unlock(&ctx->config_mutex);
	mml_trace_end();
	mml_log("%s fail result %d", __func__, result);
	return result;
}

struct mml_task *mml_dle_stop(struct mml_dle_ctx *ctx)
{
	struct mml_frame_config *cfg;
	struct mml_task *task = NULL;
	s32 cnt;

	mml_trace_begin("%s", __func__);
	mutex_lock(&ctx->config_mutex);

	cfg = frame_config_find_current(ctx);
	if (!cfg)
		mml_err("[dle]The current cfg not found to stop");
	else
		task = task_find_running(cfg);
	if (!task) {
		mml_log("%s cannot find running task", __func__);
		goto done;
	}

	/* cnt can be 1 or 2, if dual on and count 2 means pipes done */
	cnt = atomic_inc_return(&task->pipe_done);
	if (task->config->dual && cnt == 1)
		goto done;

	mml_msg("[dle]stop task %p state %u job %u pipes %d",
		task, task->state, task->job.jobid, cnt);

	task_buf_put(task);

	task_move_to_idle(task);

done:
	mutex_unlock(&ctx->config_mutex);
	mml_trace_end();
	return task;
}

struct mml_task *mml_dle_disable(struct mml_dle_ctx *ctx)
{
	struct mml_frame_config *cfg;
	struct mml_task *task = NULL;

	mml_trace_begin("%s", __func__);
	mutex_lock(&ctx->config_mutex);

	cfg = frame_config_find_current(ctx);
	if (cfg) {
		task = list_first_entry_or_null(
			&cfg->done_tasks, struct mml_task, entry);
		if (!task)
			task = task_find_running(cfg);
	}

	mutex_unlock(&ctx->config_mutex);
	mml_trace_end();
	return task;
}

static s32 dup_task(struct mml_task *task, u32 pipe)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_dle_ctx *ctx = task->ctx;
	struct mml_task *src;

	mutex_lock(&ctx->config_mutex);

	mml_msg("[dle]%s task cnt (%u %u %hhu) task %p pipe %u config %p",
		__func__,
		task->config->await_task_cnt,
		task->config->run_task_cnt,
		task->config->done_task_cnt,
		task, pipe, task->config);

	/* check if available done task first */
	src = list_first_entry_or_null(&cfg->done_tasks, struct mml_task,
				       entry);
	if (src /* && mml_pq_dup_task(task->pq_task, src->pq_task) */)
		goto dupd_pq;

	/* check running tasks, have to check it is valid task */
	list_for_each_entry(src, &cfg->tasks, entry) {
		if (0 /* !mml_pq_dup_task(task->pq_task, src->pq_task) */) {
			mml_err("[dle]%s error running task %p not able to copy",
				__func__, src);
			continue;
		}
		goto dupd_pq;
	}

	list_for_each_entry_reverse(src, &cfg->await_tasks, entry) {
		/* the first one should be current task, skip it */
		if (src == task) {
			mml_msg("[dle]%s await task %p pkt %p",
				__func__, src, src->pkts[pipe]);
			continue;
		}
		if (0 /* !mml_pq_dup_task(task->pq_task, src->pq_task) */) {
			mml_err("[dle]%s error await task %p not able to copy",
				__func__, src);
			continue;
		}
		goto dupd_pq;
	}

	mutex_unlock(&ctx->config_mutex);
	return -EBUSY;

dupd_pq:
	mml_err("[dle] TODO: %s copy pq_task results", __func__);
	mutex_unlock(&ctx->config_mutex);
	return 0;
}

static void task_queue(struct mml_task *task, u32 pipe)
{
	struct mml_dle_ctx *ctx = task->ctx;

	if (pipe)
		queue_work(ctx->wq_config, &task->work_config[pipe]);
	else
		mml_err("[dle] should not queue pipe %d", pipe);
}

static struct mml_tile_cache *task_get_tile_cache(struct mml_task *task, u32 pipe)
{
	struct mml_dle_ctx *ctx = task->ctx;
	struct mml_dle_frame_config *dle_cfg = container_of(task->config,
		struct mml_dle_frame_config, c);

	ctx->tile_cache[pipe].tiles = &dle_cfg->tile[pipe];
	return &ctx->tile_cache[pipe];
}

static void kt_setsched(void *adaptor_ctx)
{
	struct mml_dle_ctx *ctx = adaptor_ctx;
	struct sched_param kt_param = { .sched_priority = MAX_RT_PRIO - 1 };
	int ret;

	if (ctx->kt_priority)
		return;

	ret = sched_setscheduler(ctx->kt_done_task, SCHED_FIFO, &kt_param);
	mml_log("[dle]%s set kt done priority %d ret %d",
		__func__, kt_param.sched_priority, ret);
	ctx->kt_priority = true;
}

const static struct mml_task_ops dle_task_ops = {
	.queue = task_queue,
	.submit_done = task_config_done,
	.frame_err = task_frame_err,
	.dup_task = dup_task,
	.get_tile_cache = task_get_tile_cache,
	.kt_setsched = kt_setsched,
};

static struct mml_dle_ctx *dle_ctx_create(struct mml_dev *mml,
					  struct mml_dle_param *dl)
{
	struct mml_dle_ctx *ctx;
	struct task_struct *taskdone_task;

	mml_msg("[dle]%s on dev %p", __func__, mml);

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return ERR_PTR(-ENOMEM);

	/* create taskdone kthread first cause it is more easy for fail case */
	kthread_init_worker(&ctx->kt_done);
	taskdone_task = kthread_run(kthread_worker_fn, &ctx->kt_done, "mml_dle_done");
	if (IS_ERR(taskdone_task)) {
		mml_err("[dle]fail to create kt taskdone %d", (s32)PTR_ERR(taskdone_task));
		kfree(ctx);
		return ERR_PTR(-EIO);
	}
	ctx->kt_done_task = taskdone_task;

	INIT_LIST_HEAD(&ctx->configs);
	mutex_init(&ctx->config_mutex);
	ctx->mml = mml;
	ctx->task_ops = &dle_task_ops;
	ctx->wq_destroy = alloc_ordered_workqueue("mml_destroy_dl", 0, 0);
	ctx->dl_dual = dl->dual;
	ctx->config_cb = dl->config_cb;
	ctx->wq_config = alloc_ordered_workqueue("mml_work_dl", WORK_CPU_UNBOUND | WQ_HIGHPRI, 0);

	return ctx;
}

struct mml_dle_ctx *mml_dle_get_context(struct device *dev,
					struct mml_dle_param *dl)
{
	struct mml_dev *mml = dev_get_drvdata(dev);

	mml_msg("[dle]%s", __func__);
	if (!mml) {
		mml_err("[dle]%s not init mml", __func__);
		return ERR_PTR(-EPERM);
	}
	return mml_dev_get_dle_ctx(mml, dl, dle_ctx_create);
}

static void dle_ctx_release(struct mml_dle_ctx *ctx)
{
	struct mml_frame_config *cfg, *tmp;
	u32 i, j;

	mml_msg("[dle]%s on ctx %p", __func__, ctx);

	mutex_lock(&ctx->config_mutex);
	list_for_each_entry_safe_reverse(cfg, tmp, &ctx->configs, entry) {
		/* check and remove configs/tasks in this context */
		list_del_init(&cfg->entry);
		frame_config_destroy(cfg);
	}

	mutex_unlock(&ctx->config_mutex);
	destroy_workqueue(ctx->wq_destroy);
	destroy_workqueue(ctx->wq_config);
	kthread_flush_worker(&ctx->kt_done);
	kthread_stop(ctx->kt_done_task);
	kthread_destroy_worker(&ctx->kt_done);
	for (i = 0; i < ARRAY_SIZE(ctx->tile_cache); i++) {
		for (j = 0; j < ARRAY_SIZE(ctx->tile_cache[i].func_list); j++)
			kfree(ctx->tile_cache[i].func_list[j]);
		/* no need for ctx->tile_cache[i].tiles, since dle adaptor
		 * use tile struct in mml_dle_frame_config
		 */
	}
	kfree(ctx);
}

void mml_dle_put_context(struct mml_dle_ctx *ctx)
{
	if (IS_ERR_OR_NULL(ctx))
		return;
	mml_log("[dle]%s", __func__);
	mml_dev_put_dle_ctx(ctx->mml, dle_ctx_release);
}

struct mml_ddp_comp_match {
	enum mtk_ddp_comp_id id;
	enum mtk_ddp_comp_type type;
	const char *name;
};

static const struct mml_ddp_comp_match mml_ddp_matches[] = {
	{ DDP_COMPONENT_MML_RSZ0, MTK_MML_RSZ, "rsz0" },
	{ DDP_COMPONENT_MML_RSZ1, MTK_MML_RSZ, "rsz1" },
	{ DDP_COMPONENT_MML_RSZ2, MTK_MML_RSZ, "rsz2" },
	{ DDP_COMPONENT_MML_RSZ3, MTK_MML_RSZ, "rsz3" },
	{ DDP_COMPONENT_MML_HDR0, MTK_MML_HDR, "hdr0" },
	{ DDP_COMPONENT_MML_HDR1, MTK_MML_HDR, "hdr1" },
	{ DDP_COMPONENT_MML_AAL0, MTK_MML_AAL, "aal0" },
	{ DDP_COMPONENT_MML_AAL1, MTK_MML_AAL, "aal1" },
	{ DDP_COMPONENT_MML_TDSHP0, MTK_MML_TDSHP, "tdshp0" },
	{ DDP_COMPONENT_MML_TDSHP1, MTK_MML_TDSHP, "tdshp1" },
	{ DDP_COMPONENT_MML_COLOR0, MTK_MML_COLOR, "color0" },
	{ DDP_COMPONENT_MML_COLOR1, MTK_MML_COLOR, "color1" },
	{ DDP_COMPONENT_MML_MML0, MTK_MML_MML, "mmlsys" },
	{ DDP_COMPONENT_MML_DLI0, MTK_MML_MML, "dli0" },
	{ DDP_COMPONENT_MML_DLI1, MTK_MML_MML, "dli1" },
	{ DDP_COMPONENT_MML_DLO0, MTK_MML_MML, "dlo0" },
	{ DDP_COMPONENT_MML_DLO1, MTK_MML_MML, "dlo1" },
	{ DDP_COMPONENT_MML_MUTEX0, MTK_MML_MUTEX, "mutex0" },
	{ DDP_COMPONENT_MML_WROT0, MTK_MML_WROT, "wrot0" },
	{ DDP_COMPONENT_MML_WROT1, MTK_MML_WROT, "wrot1" },
	{ DDP_COMPONENT_MML_WROT2, MTK_MML_WROT, "wrot2" },
	{ DDP_COMPONENT_MML_WROT3, MTK_MML_WROT, "wrot3" },
};

static u32 mml_ddp_comp_get_id(struct device_node *node, const char *name)
{
	u32 i;

	if (!name) {
		mml_err("no comp-names in component %s for ddp binding",
			node->full_name);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(mml_ddp_matches); i++) {
		if (!strcmp(name, mml_ddp_matches[i].name))
			return mml_ddp_matches[i].id;
	}
	mml_err("no ddp component matches: %s", name);
	return -ENODATA;
}

int mml_ddp_comp_init(struct device *dev,
		      struct mtk_ddp_comp *ddp_comp, struct mml_comp *mml_comp,
		      const struct mtk_ddp_comp_funcs *funcs)
{
	if (unlikely(!funcs))
		return -EINVAL;

	ddp_comp->id = mml_ddp_comp_get_id(dev->of_node, mml_comp->name);
	if (IS_ERR_VALUE(ddp_comp->id))
		return ddp_comp->id;

	ddp_comp->funcs = funcs;
	ddp_comp->dev = dev;

	/* ddp_comp->clk = mml_comp->clks[0]; */
	ddp_comp->regs_pa = mml_comp->base_pa;
	ddp_comp->regs = mml_comp->base;
	/* ddp_comp->cmdq_base = cmdq_register_device(dev); */
	ddp_comp->larb_dev = mml_comp->larb_dev;
	ddp_comp->larb_id = mml_comp->larb_port;
	ddp_comp->sub_idx = mml_comp->sub_idx;
	return 0;
}

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

