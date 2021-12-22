// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Dennis YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */

#include <linux/atomic.h>
#include <linux/file.h>
#include <linux/module.h>
#include <linux/sync_file.h>
#include <linux/time64.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <mtk_sync.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_gem_framebuffer_helper.h>

#include "mtk-mml-drm-adaptor.h"
#include "mtk-mml-buf.h"
#include "mtk-mml-color.h"
#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "mtk-mml-sys.h"
#include "mtk-mml-mmp.h"

#define MML_DEFAULT_END_NS	15000000

/* set to 0 to disable reuse config */
int mml_reuse = 1;
module_param(mml_reuse, int, 0644);

int mml_max_cache_task = 4;
module_param(mml_max_cache_task, int, 0644);

int mml_max_cache_cfg = 2;
module_param(mml_max_cache_cfg, int, 0644);

struct mml_drm_ctx {
	struct list_head configs;
	u32 config_cnt;
	atomic_t racing_cnt;	/* ref count for racing tasks */
	struct mutex config_mutex;
	struct mml_dev *mml;
	const struct mml_task_ops *task_ops;
	atomic_t job_serial;
	struct workqueue_struct *wq_config[MML_PIPE_CNT];
	struct workqueue_struct *wq_destroy;
	struct kthread_worker kt_done;
	struct task_struct *kt_done_task;
	struct sync_timeline *timeline;
	bool kt_priority;
	bool disp_dual;
	bool disp_vdo;
	void (*submit_cb)(void *cb_param);
	struct mml_tile_cache tile_cache[MML_PIPE_CNT];
};

enum mml_mode mml_drm_query_cap(struct mml_drm_ctx *ctx,
				struct mml_frame_info *info)
{
	u8 i;
	struct mml_topology_cache *tp = mml_topology_get_cache(ctx->mml);
	const u32 srcw = info->src.width;
	const u32 srch = info->src.height;
	enum mml_mode mode;

	if (mml_pq_disable) {
		for (i = 0; i < MML_MAX_OUTPUTS; i++) {
			memset(&info->dest[i].pq_config, 0,
				sizeof(info->dest[i].pq_config));
		}
	}

	if (!info->src.format) {
		mml_err("[drm]invalid src mml color format %#010x", info->src.format);
		goto not_support;
	}

	if (MML_FMT_BLOCK(info->src.format)) {
		if ((info->src.width & 0x0f) || (info->src.height & 0x1f)) {
			mml_err(
				"[drm]invalid blk width %u height %u must alignment width 16x height 32x",
				info->src.width, info->src.height);
			goto not_support;
		}
	}

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

		/* check crop and pq combination */
		if (dest->pq_config.en && dest->crop.r.width < 48) {
			mml_err("[drm]exceed HW limitation crop width %u < 48 with pq",
				dest->crop.r.width);
			goto not_support;
		}
	}

	if (!tp || !tp->op->query_mode)
		goto not_support;

	mode = tp->op->query_mode(ctx->mml, info);
	if (atomic_read(&ctx->racing_cnt) && mode == MML_MODE_MML_DECOUPLE) {
		/* if mml hw running racing mode and query info need dc,
		 * go back to MDP decouple to avoid hw conflict.
		 *
		 * Note: no mutex lock here cause disp call query/submit on
		 * same kernel thread, thus racing_cnt can only decrease and
		 * not increase after read. And it's safe to do one more mdp
		 * decouple w/o mml racing/dc conflict.
		 */
		mml_log("%s mode %u to mdp dc", __func__, mode);
		mode = MML_MODE_MDP_DECOUPLE;
	}

	return mode;

not_support:
	return MML_MODE_NOT_SUPPORT;
}
EXPORT_SYMBOL_GPL(mml_drm_query_cap);

static void mml_adjust_src(struct mml_frame_data *src)
{
	const u32 srcw = src->width;
	const u32 srch = src->height;

	if (MML_FMT_H_SUBSAMPLE(src->format) && (srcw & 0x1))
		src->width &= ~1;

	if (MML_FMT_V_SUBSAMPLE(src->format) && (srch & 0x1))
		src->height &= ~1;
}

static void mml_adjust_dest(struct mml_frame_data *src, struct mml_frame_dest *dest)
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

void mml_drm_try_frame(struct mml_drm_ctx *ctx, struct mml_frame_info *info)
{
	u32 i;

	mml_adjust_src(&info->src);

	for (i = 0; i < info->dest_cnt; i++) {
		/* adjust info data directly for user */
		mml_adjust_dest(&info->src, &info->dest[i]);
	}

	if ((MML_FMT_PLANE(info->src.format) > 1) && info->src.uv_stride <= 0)
		info->src.uv_stride = mml_color_get_min_uv_stride(
			info->src.format, info->src.width);

}
EXPORT_SYMBOL_GPL(mml_drm_try_frame);

static u32 afbc_drm_to_mml(u32 drm_format)
{
	switch (drm_format) {
	case MML_FMT_RGBA8888:
		return MML_FMT_RGBA8888_AFBC;
	case MML_FMT_BGRA8888:
		return MML_FMT_BGRA8888_AFBC;
	case MML_FMT_RGBA1010102:
		return MML_FMT_RGBA1010102_AFBC;
	case MML_FMT_BGRA1010102:
		return MML_FMT_BGRA1010102_AFBC;
	case MML_FMT_NV12:
		return MML_FMT_YUV420_AFBC;
	case MML_FMT_NV21:
		return MML_FMT_YVU420_AFBC;
	case MML_FMT_NV12_10L:
		return MML_FMT_YUV420_10P_AFBC;
	case MML_FMT_NV21_10L:
		return MML_FMT_YVU420_10P_AFBC;
	default:
		mml_err("[drm]%s unknown drm format %#x", __func__, drm_format);
		return drm_format;
	}
}

#define MML_AFBC	DRM_FORMAT_MOD_ARM_AFBC( \
	AFBC_FORMAT_MOD_BLOCK_SIZE_32x8 | AFBC_FORMAT_MOD_SPLIT)

static u32 format_drm_to_mml(u32 drm_format, u64 modifier)
{
	/* check afbc modifier with rdma/wrot supported
	 * 32x8 block and split mode
	 */
	if (modifier == MML_AFBC)
		return afbc_drm_to_mml(drm_format);

	return drm_format;
}

static bool check_frame_change(struct mml_frame_info *info,
			       struct mml_frame_config *cfg)
{
	return !memcmp(&cfg->info, info, sizeof(*info));
}

static struct mml_frame_config *frame_config_find_reuse(
	struct mml_drm_ctx *ctx,
	struct mml_submit *submit)
{
	struct mml_frame_config *cfg;
	u32 idx = 0, mode = MML_MODE_UNKNOWN;

	if (!mml_reuse)
		return NULL;

	mml_trace_ex_begin("%s", __func__);

	list_for_each_entry(cfg, &ctx->configs, entry) {
		if (!idx)
			mode = cfg->info.mode;

		if (submit->update && cfg->last_jobid == submit->job->jobid)
			goto done;

		if (check_frame_change(&submit->info, cfg))
			goto done;

		idx++;
	}

	/* not found, give return value to NULL */
	cfg = NULL;

done:
	if (cfg && idx) {
		if (mode != cfg->info.mode)
			mml_log("[drm]mode change to %hhu", cfg->info.mode);
		list_rotate_to_front(&cfg->entry, &ctx->configs);
	}

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

	if (WARN_ON(!list_empty(&cfg->await_tasks))) {
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

	if (WARN_ON(!list_empty(&cfg->tasks))) {
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

static void frame_config_queue_destroy(struct kref *kref)
{
	struct mml_frame_config *cfg = container_of(kref, struct mml_frame_config, ref);
	struct mml_drm_ctx *ctx = (struct mml_drm_ctx *)cfg->ctx;

	queue_work(ctx->wq_destroy, &cfg->work_destroy);
}

static struct mml_frame_config *frame_config_create(
	struct mml_drm_ctx *ctx,
	struct mml_frame_info *info)
{
	struct mml_frame_config *cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);

	if (!cfg)
		return ERR_PTR(-ENOMEM);
	mml_core_init_config(cfg);

	list_add(&cfg->entry, &ctx->configs);
	ctx->config_cnt++;
	cfg->info = *info;
	cfg->disp_dual = ctx->disp_dual;
	cfg->disp_vdo = ctx->disp_vdo;
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

	if (user_buf->fence >= 0) {
		fbuf->fence = sync_file_get_fence(user_buf->fence);
		mml_msg("[drm]get dma fence %p by %d", fbuf->fence, user_buf->fence);
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

	if (list_empty(&task->entry)) {
		mml_err("[drm]%s task %p already leave config",
			__func__, task);
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
	struct mml_frame_config *cfg = task->config;
	struct mml_drm_ctx *ctx = (struct mml_drm_ctx *)task->ctx;

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

	/* maintain racing ref count decrease after done */
	if (cfg->info.mode == MML_MODE_RACING)
		atomic_dec(&ctx->racing_cnt);

	mml_msg("[drm]%s task cnt (%u %u %hhu) racing %d",
		__func__,
		task->config->await_task_cnt,
		task->config->run_task_cnt,
		task->config->done_task_cnt,
		atomic_read(&ctx->racing_cnt));
}

static void task_move_to_destroy(struct kref *kref)
{
	struct mml_task *task = container_of(kref,
		struct mml_task, ref);

	if (task->config)
		kref_put(&task->config->ref, frame_config_queue_destroy);

	mml_core_destroy_task(task);
}

static void task_submit_done(struct mml_task *task)
{
	struct mml_drm_ctx *ctx = (struct mml_drm_ctx *)task->ctx;

	mml_msg("[drm]%s task %p state %u", __func__, task, task->state);

	mml_mmp(submit_cb, MMPROFILE_FLAG_PULSE, task->job.jobid, 0);

	if (ctx->submit_cb)
		ctx->submit_cb(task->cb_param);

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
		mml_msg("[drm]release dest %hhu iova %#011llx",
			i, task->buf.dest[i].dma[0].iova);
		mml_buf_put(&task->buf.dest[i]);
	}
	mml_msg("[drm]release src iova %#011llx",
		task->buf.src.dma[0].iova);
	mml_buf_put(&task->buf.src);
	mml_trace_ex_end();
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

static void task_frame_done(struct mml_task *task)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_config *tmp;
	struct mml_drm_ctx *ctx = (struct mml_drm_ctx *)task->ctx;
	struct mml_dev *mml = cfg->mml;

	mml_trace_ex_begin("%s", __func__);

	mml_msg("[drm]frame done task %p state %u job %u",
		task, task->state, task->job.jobid);

	/* clean up */
	task_buf_put(task);

	mutex_lock(&ctx->config_mutex);

	if (unlikely(!task->pkts[0] || (cfg->dual && !task->pkts[1]))) {
		task_state_dec(cfg, task, __func__);
		mml_err("[drm]%s task cnt (%u %u %hhu) error from state %d",
			__func__,
			cfg->await_task_cnt,
			cfg->run_task_cnt,
			cfg->done_task_cnt,
			task->state);
		kref_put(&task->ref, task_move_to_destroy);
	} else {
		/* works fine, safe to move */
		task_move_to_idle(task);
		mml_record_track(mml, task);
	}

	if (cfg->done_task_cnt > mml_max_cache_task) {
		task = list_first_entry(&cfg->done_tasks, typeof(*task), entry);
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
	list_for_each_entry_safe_reverse(cfg, tmp, &ctx->configs, entry) {
		/* only remove config not running */
		if (!list_empty(&cfg->tasks) || !list_empty(&cfg->await_tasks))
			continue;
		list_del_init(&cfg->entry);
		task_put_idles(cfg);
		kref_put(&cfg->ref, frame_config_queue_destroy);
		ctx->config_cnt--;
		mml_msg("[drm]config %p send destroy remain %u",
			cfg, ctx->config_cnt);

		/* check cache num again */
		if (ctx->config_cnt <= mml_max_cache_cfg)
			break;
	}

done:
	mutex_unlock(&ctx->config_mutex);

	mml_lock_wake_lock(mml, false);

	mml_trace_ex_end();
}

static void dump_pq_en(u32 idx, struct mml_pq_param *pq_param,
	struct mml_pq_config *pq_config)
{
	u32 pqen = 0;

	memcpy(&pqen, pq_config, min(sizeof(*pq_config), sizeof(pqen)));

	if (pq_param)
		mml_msg("[drm]PQ %u config %#x param en %u %s",
			idx, pqen, pq_param->enable,
			mml_pq_disable ? "FORCE DISABLE" : "");
	else
		mml_msg("[drm]PQ %u config %#x param NULL %s",
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

static void frame_check_end_time(struct timespec64 *endtime)
{
	if (!endtime->tv_sec && !endtime->tv_nsec) {
		ktime_get_real_ts64(endtime);
		timespec64_add_ns(endtime, MML_DEFAULT_END_NS);
	}
}

s32 mml_drm_submit(struct mml_drm_ctx *ctx, struct mml_submit *submit,
	void *cb_param)
{
	struct mml_frame_config *cfg;
	struct mml_task *task;
	s32 result;
	u32 i;
	struct fence_data fence = {0};

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
	if (likely(submit->info.mode != MML_MODE_SRAM_READ)) {
		frame_calc_plane_offset(&submit->info.src, &submit->buffer.src);
		for (i = 0; i < submit->info.dest_cnt; i++)
			frame_calc_plane_offset(&submit->info.dest[i].data,
				&submit->buffer.dest[i]);
	}

	/* always fixup format/modifier for afbc case
	 * the format in info should change to fourcc format in future design
	 * and store mml format in another structure
	 */
	submit->info.src.format = format_drm_to_mml(
		submit->info.src.format, submit->info.src.modifier);

	if (MML_FMT_YUV_COMPRESS(submit->info.src.format)) {
		submit->info.src.y_stride =
			mml_color_get_min_y_stride(submit->info.src.format, submit->info.src.width);
		submit->info.src.uv_stride = 0;
		submit->info.src.plane_cnt = 1;
		submit->buffer.src.cnt = 1;
		submit->buffer.src.fd[1] = -1;
		submit->buffer.src.fd[2] = -1;
	}

	for (i = 0; i < submit->info.dest_cnt; i++)
		submit->info.dest[i].data.format = format_drm_to_mml(
			submit->info.dest[i].data.format,
			submit->info.dest[i].data.modifier);

	/* TODO: remove after disp support calc time 6.75 * 1000 * height
	 * give default total time for mml frame in racing case
	 */
	if (submit->info.mode == MML_MODE_RACING && !submit->info.act_time)
		submit->info.act_time = 3375 * submit->info.dest[0].data.height;

	/* always do frame info adjust for now
	 * but this flow should call from hwc/disp in future version
	 */
	mml_drm_try_frame(ctx, &submit->info);

	mml_mmp(submit, MMPROFILE_FLAG_PULSE, atomic_read(&ctx->job_serial), 0);

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
				mml_err("%s create task for reuse frame fail", __func__);
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

	/* maintain racing ref count for easy query mode */
	if (cfg->info.mode == MML_MODE_RACING)
		atomic_inc(&ctx->racing_cnt);

	/* add more count for new task create */
	kref_get(&cfg->ref);

	/* make sure id unique and cached last */
	task->job.jobid = atomic_fetch_inc(&ctx->job_serial);
	task->cb_param = cb_param;
	cfg->last_jobid = task->job.jobid;
	list_add_tail(&task->entry, &cfg->await_tasks);
	cfg->await_task_cnt++;
	mml_msg("[drm]%s task cnt (%u %u %hhu) racing %d",
		__func__,
		task->config->await_task_cnt,
		task->config->run_task_cnt,
		task->config->done_task_cnt,
		atomic_read(&ctx->racing_cnt));

	mutex_unlock(&ctx->config_mutex);

	/* copy per-frame info */
	task->ctx = ctx;
	task->end_time.tv_sec = submit->end.sec;
	task->end_time.tv_nsec = submit->end.nsec;
	/* give default time if empty */
	frame_check_end_time(&task->end_time);
	frame_buf_to_task_buf(&task->buf.src, &submit->buffer.src, "mml_rdma");
	task->buf.dest_cnt = submit->buffer.dest_cnt;
	for (i = 0; i < submit->buffer.dest_cnt; i++)
		frame_buf_to_task_buf(&task->buf.dest[i],
				      &submit->buffer.dest[i],
				      "mml_wrot");

	/* create fence for this task */
	fence.value = task->job.jobid;
	if (submit->job && ctx->timeline && submit->info.mode != MML_MODE_RACING &&
		mtk_sync_fence_create(ctx->timeline, &fence) >= 0) {
		task->job.fence = fence.fence;
		task->fence = sync_file_get_fence(task->job.fence);
	} else {
		task->job.fence = -1;
	}
	mml_msg("[drm]mml job %u fence fd %d task %p fence %p config %p mode %hhu%s act_t %u",
		task->job.jobid, task->job.fence, task, task->fence, cfg,
		cfg->info.mode,
		(cfg->info.mode == MML_MODE_RACING && cfg->disp_dual) ? " disp dual" : "",
		submit->info.act_time);

	/* copy job content back, must do before call submit */
	if (submit->job)
		memcpy(submit->job, &task->job, sizeof(*submit->job));

	/* copy pq parameters */
	for (i = 0; i < submit->buffer.dest_cnt && submit->pq_param[i]; i++)
		memcpy(&task->pq_param[i], submit->pq_param[i], sizeof(task->pq_param[i]));

	/* wake lock */
	mml_lock_wake_lock(task->config->mml, true);

	/* submit to core */
	mml_core_submit_task(cfg, task);

	mml_trace_end();
	return 0;

err_unlock_exit:
	mutex_unlock(&ctx->config_mutex);
	mml_trace_end();
	mml_log("%s fail result %d", __func__, result);
	return result;
}
EXPORT_SYMBOL_GPL(mml_drm_submit);

s32 mml_drm_stop(struct mml_drm_ctx *ctx, struct mml_submit *submit, bool force)
{
	struct mml_frame_config *cfg;

	mml_trace_begin("%s", __func__);
	mutex_lock(&ctx->config_mutex);

	cfg = frame_config_find_reuse(ctx, submit);
	if (!cfg) {
		mml_err("[drm]The submit info not found for stop");
		goto done;
	}

	mml_log("[drm]stop config %p", cfg);
	mml_core_stop_racing(cfg, force);

done:
	mutex_unlock(&ctx->config_mutex);
	mml_trace_end();
	return 0;
}
EXPORT_SYMBOL_GPL(mml_drm_stop);

void mml_drm_config_rdone(struct mml_drm_ctx *ctx, struct mml_submit *submit,
	struct cmdq_pkt *pkt)
{
	struct mml_frame_config *cfg;

	mml_trace_begin("%s", __func__);
	mutex_lock(&ctx->config_mutex);

	cfg = frame_config_find_reuse(ctx, submit);
	if (!cfg) {
		mml_err("[drm]The submit info not found for stop");
		goto done;
	}

	cmdq_pkt_write_value_addr(pkt,
		cfg->path[0]->mmlsys->base_pa +
		mml_sys_get_reg_ready_sel(cfg->path[0]->mmlsys),
		0x24, U32_MAX);

done:
	mutex_unlock(&ctx->config_mutex);
	mml_trace_end();
}
EXPORT_SYMBOL_GPL(mml_drm_config_rdone);

void mml_drm_dump(struct mml_drm_ctx *ctx, struct mml_submit *submit)
{
	mml_log("[drm]dump threads for mml, submit job %u",
		submit->job ? submit->job->jobid : 0);
	mml_dump_thread(ctx->mml);
}
EXPORT_SYMBOL_GPL(mml_drm_dump);

static s32 dup_task(struct mml_task *task, u32 pipe)
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

	/* check running tasks, have to check it is valid task */
	list_for_each_entry(src, &cfg->tasks, entry) {
		if (!src->pkts[0] || (src->config->dual && !src->pkts[1])) {
			mml_err("[drm]%s error running task %p not able to copy",
				__func__, src);
			continue;
		}
		goto dup_command;
	}

	list_for_each_entry_reverse(src, &cfg->await_tasks, entry) {
		/* the first one should be current task, skip it */
		if (src == task) {
			mml_msg("[drm]%s await task %p pkt %p",
				__func__, src, src->pkts[pipe]);
			continue;
		}
		if (!src->pkts[0] || (src->config->dual && !src->pkts[1])) {
			mml_err("[drm]%s error await task %p not able to copy",
				__func__, src);
			continue;
		}
		goto dup_command;
	}

	mutex_unlock(&ctx->config_mutex);
	return -EBUSY;

dup_command:
	task->pkts[pipe] = cmdq_pkt_create(cfg->path[pipe]->clt);
	cmdq_pkt_copy(task->pkts[pipe], src->pkts[pipe]);
	task->pkts[pipe]->user_data = (void *)task;

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

static void task_queue(struct mml_task *task, u32 pipe)
{
	struct mml_drm_ctx *ctx = (struct mml_drm_ctx *)task->ctx;

	queue_work(ctx->wq_config[pipe], &task->work_config[pipe]);
}

static struct mml_tile_cache *task_get_tile_cache(struct mml_task *task, u32 pipe)
{
	return &((struct mml_drm_ctx *)task->ctx)->tile_cache[pipe];
}

static void kt_setsched(void *adaptor_ctx)
{
	struct mml_drm_ctx *ctx = adaptor_ctx;
	struct sched_param kt_param = { .sched_priority = MAX_RT_PRIO - 1 };
	int ret;

	if (ctx->kt_priority)
		return;

	ret = sched_setscheduler(ctx->kt_done_task, SCHED_FIFO, &kt_param);
	mml_log("[drm]%s set kt done priority %d ret %d",
		__func__, kt_param.sched_priority, ret);
	ctx->kt_priority = true;
}

const static struct mml_task_ops drm_task_ops = {
	.queue = task_queue,
	.submit_done = task_submit_done,
	.frame_done = task_frame_done,
	.dup_task = dup_task,
	.get_tile_cache = task_get_tile_cache,
	.kt_setsched = kt_setsched,
};

static struct mml_drm_ctx *drm_ctx_create(struct mml_dev *mml,
					  struct mml_drm_param *disp)
{
	struct mml_drm_ctx *ctx;
	struct task_struct *taskdone_task;

	mml_msg("[drm]%s on dev %p", __func__, mml);

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return ERR_PTR(-ENOMEM);

	/* create taskdone kthread first cause it is more easy for fail case */
	kthread_init_worker(&ctx->kt_done);
	taskdone_task = kthread_run(kthread_worker_fn, &ctx->kt_done, "mml_drm_done");
	if (IS_ERR(taskdone_task)) {
		mml_err("[drm]fail to create kt taskdone %d", (s32)PTR_ERR(taskdone_task));
		kfree(ctx);
		return ERR_PTR(-EIO);
	}
	ctx->kt_done_task = taskdone_task;

	INIT_LIST_HEAD(&ctx->configs);
	mutex_init(&ctx->config_mutex);
	ctx->mml = mml;
	ctx->task_ops = &drm_task_ops;
	ctx->wq_destroy = alloc_ordered_workqueue("mml_destroy", 0, 0);
	ctx->disp_dual = disp->dual;
	ctx->disp_vdo = disp->vdo_mode;
	ctx->submit_cb = disp->submit_cb;
	ctx->wq_config[0] = alloc_ordered_workqueue("mml_work0", WORK_CPU_UNBOUND | WQ_HIGHPRI, 0);
	ctx->wq_config[1] = alloc_ordered_workqueue("mml_work1", WORK_CPU_UNBOUND | WQ_HIGHPRI, 0);

	ctx->timeline = mtk_sync_timeline_create("mml_timeline");
	if (!ctx->timeline)
		mml_err("[drm]fail to create timeline");
	else
		mml_msg("[drm]timeline for mml %p", ctx->timeline);

	/* return info to display */
	disp->racing_height = mml_sram_get_racing_height(mml);

	return ctx;
}

struct mml_drm_ctx *mml_drm_get_context(struct platform_device *pdev,
					struct mml_drm_param *disp)
{
	struct mml_dev *mml = platform_get_drvdata(pdev);

	mml_msg("[drm]%s", __func__);
	if (!mml) {
		mml_err("[drm]%s not init mml", __func__);
		return ERR_PTR(-EPERM);
	}
	return mml_dev_get_drm_ctx(mml, disp, drm_ctx_create);
}
EXPORT_SYMBOL_GPL(mml_drm_get_context);

bool mml_drm_ctx_idle(struct mml_drm_ctx *ctx)
{
	bool idle = false;

	struct mml_frame_config *cfg;

	mutex_lock(&ctx->config_mutex);
	list_for_each_entry(cfg, &ctx->configs, entry) {
		if (!list_empty(&cfg->await_tasks))
			goto done;

		if (!list_empty(&cfg->tasks))
			goto done;
	}

	idle = true;
done:
	mutex_unlock(&ctx->config_mutex);
	return idle;
}

static void drm_ctx_release(struct mml_drm_ctx *ctx)
{
	struct mml_frame_config *cfg, *tmp;
	u32 i, j;

	mml_msg("[drm]%s on ctx %p", __func__, ctx);

	mutex_lock(&ctx->config_mutex);
	list_for_each_entry_safe_reverse(cfg, tmp, &ctx->configs, entry) {
		/* check and remove configs/tasks in this context */
		list_del_init(&cfg->entry);
		frame_config_destroy(cfg);
	}

	mutex_unlock(&ctx->config_mutex);
	destroy_workqueue(ctx->wq_destroy);
	destroy_workqueue(ctx->wq_config[0]);
	destroy_workqueue(ctx->wq_config[1]);
	kthread_flush_worker(&ctx->kt_done);
	kthread_stop(ctx->kt_done_task);
	kthread_destroy_worker(&ctx->kt_done);
	mtk_sync_timeline_destroy(ctx->timeline);
	for (i = 0; i < ARRAY_SIZE(ctx->tile_cache); i++) {
		for (j = 0; j < ARRAY_SIZE(ctx->tile_cache[i].func_list); j++)
			kfree(ctx->tile_cache[i].func_list[j]);
		if (ctx->tile_cache[i].tiles)
			vfree(ctx->tile_cache[i].tiles);
	}
	kfree(ctx);
}

void mml_drm_put_context(struct mml_drm_ctx *ctx)
{
	if (IS_ERR_OR_NULL(ctx))
		return;
	mml_log("[drm]%s", __func__);
	mml_dev_put_drm_ctx(ctx->mml, drm_ctx_release);
}
EXPORT_SYMBOL_GPL(mml_drm_put_context);

s32 mml_drm_racing_config_sync(struct mml_drm_ctx *ctx, struct cmdq_pkt *pkt)
{
	struct cmdq_operand lhs, rhs;

	mml_msg("[drm]%s for disp", __func__);

	/* debug current task idx */
	cmdq_pkt_assign_command(pkt, CMDQ_THR_SPR_IDX3,
		atomic_read(&ctx->job_serial));

	/* set NEXT bit on, to let mml know should jump next */
	lhs.reg = true;
	lhs.idx = MML_CMDQ_NEXT_SPR;
	rhs.reg = false;
	rhs.value = MML_NEXTSPR_NEXT;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_OR, MML_CMDQ_NEXT_SPR, &lhs, &rhs);

	cmdq_pkt_set_event(pkt, mml_ir_get_disp_ready_event(ctx->mml));
	cmdq_pkt_wfe(pkt, mml_ir_get_mml_ready_event(ctx->mml));

	/* clear next bit since disp with new mml now */
	rhs.value = ~(u16)MML_NEXTSPR_NEXT;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_AND, MML_CMDQ_NEXT_SPR, &lhs, &rhs);

	return 0;
}
EXPORT_SYMBOL_GPL(mml_drm_racing_config_sync);

s32 mml_drm_racing_stop_sync(struct mml_drm_ctx *ctx, struct cmdq_pkt *pkt)
{
	struct cmdq_operand lhs, rhs;

	/* debug current task idx */
	cmdq_pkt_assign_command(pkt, CMDQ_THR_SPR_IDX3,
		atomic_read(&ctx->job_serial));

	/* set NEXT bit on, to let mml know should jump next */
	lhs.reg = true;
	lhs.idx = MML_CMDQ_NEXT_SPR;
	rhs.reg = false;
	rhs.value = MML_NEXTSPR_NEXT;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_OR, MML_CMDQ_NEXT_SPR, &lhs, &rhs);

	cmdq_pkt_wait_no_clear(pkt, mml_ir_get_mml_stop_event(ctx->mml));

	/* clear next bit since disp with new mml now */
	rhs.value = ~(u16)MML_NEXTSPR_NEXT;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_AND, MML_CMDQ_NEXT_SPR, &lhs, &rhs);

	return 0;
}
EXPORT_SYMBOL_GPL(mml_drm_racing_stop_sync);

void mml_drm_split_info(struct mml_submit *submit, struct mml_submit *submit_pq)
{
	struct mml_frame_info *info = &submit->info;
	struct mml_frame_info *info_pq = &submit_pq->info;
	u32 i;

	submit_pq->info = submit->info;
	submit_pq->buffer = submit->buffer;
	if (submit_pq->job && submit->job)
		*submit_pq->job = *submit->job;
	for (i = 0; i < MML_MAX_OUTPUTS; i++)
		if (submit_pq->pq_param[i] && submit->pq_param[i])
			*submit_pq->pq_param[i] = *submit->pq_param[i];

	if (info->dest[0].rotate == MML_ROT_0 ||
	    info->dest[0].rotate == MML_ROT_180) {
		info->dest[0].compose.left = 0;
		info->dest[0].compose.top = 0;
		info->dest[0].compose.width = info->dest[0].crop.r.width;
		info->dest[0].compose.height = info->dest[0].crop.r.height;
	} else {
		info->dest[0].compose.left = 0;
		info->dest[0].compose.top = 0;
		info->dest[0].compose.width = info->dest[0].crop.r.height;
		info->dest[0].compose.height = info->dest[0].crop.r.width;
	}

	info->dest[0].data.width = info->dest[0].compose.width;
	info->dest[0].data.height = info->dest[0].compose.height;
	info->dest[0].data.y_stride = mml_color_get_min_y_stride(
		info->dest[0].data.format, info->dest[0].compose.width);
	info->dest[0].data.uv_stride = mml_color_get_min_uv_stride(
		info->dest[0].data.format, info->dest[0].compose.width);
	memset(&info->dest[0].pq_config, 0, sizeof(info->dest[0].pq_config));

	info_pq->src = info->dest[0].data;
	info_pq->dest[0].crop.r.left = 0;
	info_pq->dest[0].crop.r.top = 0;
	info_pq->dest[0].crop.r.width = info_pq->src.width;
	info_pq->dest[0].crop.r.height = info_pq->src.height;
	info_pq->dest[0].rotate = 0;
	info_pq->dest[0].flip = 0;
	info_pq->mode = MML_MODE_DDP_ADDON;
	submit_pq->buffer.src = submit->buffer.dest[0];

	if (MML_FMT_PLANE(info->dest[0].data.format) > 1)
		mml_err("%s dest plane should be 1 but format %#010x",
			__func__, info->dest[0].data.format);
}
EXPORT_SYMBOL_GPL(mml_drm_split_info);

