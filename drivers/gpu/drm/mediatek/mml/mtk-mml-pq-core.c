/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/miscdevice.h>

#include "mtk-mml-pq.h"
#include "mtk-mml-pq-core.h"

#undef pr_fmt
#define pr_fmt(fmt) "[mml_pq_core]" fmt

int mml_pq_msg;
module_param(mml_pq_msg, int, 0644);

int mml_pq_dump;
module_param(mml_pq_dump, int, 0644);

int mml_pq_trace;
module_param(mml_pq_trace, int, 0644);

int mml_pq_rb_msg;
module_param(mml_pq_rb_msg, int, 0644);

struct mml_pq_chan {
	struct wait_queue_head msg_wq;
	atomic_t msg_cnt;
	struct list_head msg_list;
	struct mutex msg_lock;

	struct list_head job_list;
	struct mutex job_lock;
	u64 job_idx;
};

struct mml_pq_mbox {
	struct mml_pq_chan tile_init_chan;
	struct mml_pq_chan comp_config_chan;
	struct mml_pq_chan aal_readback_chan;
	struct mml_pq_chan hdr_readback_chan;
	struct mml_pq_chan rsz_callback_chan;
};

static struct mml_pq_mbox *pq_mbox;
static struct mutex rb_buf_list_mutex;
static struct list_head rb_buf_list;
static u32 buffer_num;
static u32 rb_buf_pool[TOTAL_RB_BUF_NUM];
static struct mutex rb_buf_pool_mutex;


static void init_pq_chan(struct mml_pq_chan *chan)
{
	init_waitqueue_head(&chan->msg_wq);
	INIT_LIST_HEAD(&chan->msg_list);
	mutex_init(&chan->msg_lock);

	INIT_LIST_HEAD(&chan->job_list);
	mutex_init(&chan->job_lock);
	chan->job_idx = 1;
}

static void queue_msg(struct mml_pq_chan *chan,
			struct mml_pq_sub_task *sub_task)
{
	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s sub_task[%p] sub_task->mbox_list[%d] chan[%p] chan->msg_list[%p]",
		__func__, sub_task, &sub_task->mbox_list, chan, &chan->msg_list);

	mutex_lock(&chan->msg_lock);
	list_add_tail(&sub_task->mbox_list, &chan->msg_list);
	atomic_inc(&chan->msg_cnt);
	sub_task->job_id = chan->job_idx++;
	mutex_unlock(&chan->msg_lock);

	mml_pq_msg("%s wake up channel message queue", __func__);
	wake_up_interruptible(&chan->msg_wq);

	mml_pq_trace_ex_end();
}

static s32 dequeue_msg(struct mml_pq_chan *chan,
			struct mml_pq_sub_task **out_sub_task)
{
	struct mml_pq_sub_task *temp = NULL;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s chan[%p] chan->msg_list[%p]", __func__, chan, &chan->msg_list);

	mutex_lock(&chan->msg_lock);
	temp = list_first_entry_or_null(&chan->msg_list,
		typeof(*temp), mbox_list);
	if (temp) {
		atomic_dec(&chan->msg_cnt);
		list_del(&temp->mbox_list);
	}
	mutex_unlock(&chan->msg_lock);

	if (!temp) {
		mml_pq_err("%s temp is null", __func__);
		return -ENOENT;
	}

	if (unlikely(!atomic_read(&temp->queued))) {
		mml_pq_log("%s sub_task not queued", __func__);
		return -EFAULT;
	}

	mml_pq_msg("%s temp[%p] temp->result[%p] sub_task->job_id[%d] chan[%p] chan_job_id[%d]",
		__func__, temp, temp->result, temp->job_id, chan, chan->job_idx);
	mml_pq_msg("%s chan_job_id[%d] temp->mbox_list[%p] chan->job_list[%p]",
		__func__, chan->job_idx, &temp->mbox_list, &chan->job_list);

	mutex_lock(&chan->job_lock);
	list_add_tail(&temp->mbox_list, &chan->job_list);
	mutex_unlock(&chan->job_lock);
	*out_sub_task = temp;

	mml_pq_trace_ex_end();
	return 0;
}

void mml_pq_comp_config_clear(struct mml_task *task)
{
	struct mml_pq_chan *chan = &pq_mbox->comp_config_chan;
	struct mml_pq_sub_task *sub_task = NULL, *tmp = NULL;
	u64 job_id = task->pq_task->comp_config.job_id;

	mml_pq_log("%s task_job_id[%d] job_id[%llx]",
		__func__, task->job.jobid, job_id);
	mutex_lock(&chan->msg_lock);
	if (atomic_read(&chan->msg_cnt)) {
		list_for_each_entry_safe(sub_task, tmp, &chan->msg_list, mbox_list) {
			mml_pq_log("%s msg sub_task[%p] msg_list[%08x] sub_job_id[%llx]",
				__func__, sub_task, &chan->msg_list, sub_task->job_id);
			if (sub_task->job_id == job_id) {
				list_del(&sub_task->mbox_list);
				atomic_dec_if_positive(&chan->msg_cnt);
			}
		}
		mutex_unlock(&chan->msg_lock);
	} else {
		mutex_unlock(&chan->msg_lock);
		mutex_lock(&chan->job_lock);
		list_for_each_entry_safe(sub_task, tmp, &chan->job_list, mbox_list) {
			mml_pq_log("%s job sub_task[%p] job_list[%08x] sub_job_id[%llx]",
				__func__, sub_task, &chan->job_list, sub_task->job_id);
			if (sub_task->job_id == job_id)
				list_del(&sub_task->mbox_list);
		}
		mutex_unlock(&chan->job_lock);
	}
}

static s32 remove_sub_task(struct mml_pq_chan *chan, u64 job_id)
{
	struct mml_pq_sub_task *sub_task = NULL, *tmp = NULL;
	s32 ret = 0;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s chan[%p] job_id[%d]", __func__, chan, job_id);

	mutex_lock(&chan->job_lock);
	list_for_each_entry_safe(sub_task, tmp, &chan->job_list, mbox_list) {
		mml_pq_msg("%s sub_task[%p] chan->job_list[%p] sub_job_id[%d]",
			__func__, sub_task, chan->job_list, sub_task->job_id);
		if (sub_task->job_id == job_id) {
			mml_pq_msg("%s find sub_task:%p id:%llx",
				__func__, sub_task, job_id);
			list_del(&sub_task->mbox_list);
			break;
		}
	}
	mutex_unlock(&chan->job_lock);

	mml_pq_trace_ex_end();
	return ret;
}

static s32 find_sub_task(struct mml_pq_chan *chan, u64 job_id,
			struct mml_pq_sub_task **out_sub_task)
{
	struct mml_pq_sub_task *sub_task = NULL, *tmp = NULL;
	s32 ret = 0;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s chan[%p] job_id[%d]", __func__, chan, job_id);

	mutex_lock(&chan->job_lock);
	list_for_each_entry_safe(sub_task, tmp, &chan->job_list, mbox_list) {
		mml_pq_msg("%s sub_task[%p] chan->job_list[%p] sub_job_id[%d]",
			__func__, sub_task, chan->job_list, sub_task->job_id);
		if (sub_task->job_id == job_id) {
			*out_sub_task = sub_task;
			mml_pq_msg("%s find sub_task:%p id:%llx",
				__func__, sub_task, job_id);
			list_del(&sub_task->mbox_list);
			break;
		}
	}
	if (!(*out_sub_task))
		ret = -ENOENT;
	mutex_unlock(&chan->job_lock);

	mml_pq_trace_ex_end();
	return ret;
}

static void init_sub_task(struct mml_pq_sub_task *sub_task)
{
	mutex_init(&sub_task->lock);
	init_waitqueue_head(&sub_task->wq);
	INIT_LIST_HEAD(&sub_task->mbox_list);
	sub_task->first_job = true;
}

s32 mml_pq_task_create(struct mml_task *task)
{
	struct mml_pq_task *pq_task = NULL;

	mml_pq_trace_ex_begin("%s pq_task kzalloc", __func__);
	pq_task = kzalloc(sizeof(*pq_task), GFP_KERNEL);
	mml_pq_trace_ex_end();
	if (unlikely(!pq_task)) {
		mml_pq_err("%s create pq_task failed", __func__);
		return -ENOMEM;
	}

	mml_pq_msg("%s jobid[%d]", __func__, task->job.jobid);

	mml_pq_trace_ex_begin("%s", __func__);

	pq_task->task = task;
	kref_init(&pq_task->ref);
	mutex_init(&pq_task->buffer_mutex);
	task->pq_task = pq_task;

	init_sub_task(&pq_task->tile_init);
	init_sub_task(&pq_task->comp_config);
	init_sub_task(&pq_task->aal_readback);
	init_sub_task(&pq_task->hdr_readback);
	init_sub_task(&pq_task->rsz_callback);

	pq_task->aal_readback.readback_data.pipe0_hist =
		kzalloc(sizeof(u32)*(AAL_HIST_NUM + AAL_DUAL_INFO_NUM),
		GFP_KERNEL);
	pq_task->aal_readback.readback_data.pipe1_hist =
		kzalloc(sizeof(u32)*(AAL_HIST_NUM + AAL_DUAL_INFO_NUM),
		GFP_KERNEL);
	pq_task->hdr_readback.readback_data.pipe0_hist =
		kzalloc(sizeof(u32)*HDR_HIST_NUM, GFP_KERNEL);
	pq_task->hdr_readback.readback_data.pipe1_hist =
		kzalloc(sizeof(u32)*HDR_HIST_NUM, GFP_KERNEL);


	mml_pq_trace_ex_end();
	return 0;
}

static void release_tile_init_result(void *data)
{
	struct mml_pq_tile_init_result *result =
		(struct mml_pq_tile_init_result *)data;

	mml_pq_msg("%s called", __func__);

	if (!result)
		return;
	kfree(result->rsz_regs[0]);
	if (result->rsz_param_cnt == MML_MAX_OUTPUTS)
		kfree(result->rsz_regs[1]);
	kfree(result);
}

static void release_comp_config_result(void *data)
{
	struct mml_pq_comp_config_result *result =
		(struct mml_pq_comp_config_result *)data;

	if (!result)
		return;
	kfree(result->hdr_regs);
	kfree(result->hdr_curve);
	kfree(result->aal_param);
	kfree(result->aal_regs);
	kfree(result->aal_curve);
	kfree(result->ds_regs);
	kfree(result->color_regs);
	kfree(result);
}

static void release_pq_task(struct kref *ref)
{
	struct mml_pq_task *pq_task = container_of(ref, struct mml_pq_task, ref);

	mml_pq_trace_ex_begin("%s", __func__);

	release_tile_init_result(pq_task->tile_init.result);
	release_comp_config_result(pq_task->comp_config.result);

	mml_pq_msg("%s aal_hist[%p] hdr_hist[%p]",
		__func__, pq_task->aal_hist[0], pq_task->hdr_hist[0]);

	kfree(pq_task->aal_readback.readback_data.pipe0_hist);
	kfree(pq_task->aal_readback.readback_data.pipe1_hist);
	kfree(pq_task->hdr_readback.readback_data.pipe0_hist);
	kfree(pq_task->hdr_readback.readback_data.pipe1_hist);

	kfree(pq_task);
	mml_pq_trace_ex_end();
}

void mml_pq_get_vcp_buf_offset(struct mml_task *task, u32 engine,
			       struct mml_pq_readback_buffer *hist)
{
	s32 engine_start = engine*MAX_ENG_RB_BUF;

	mml_pq_rb_msg("%s get offset job_id[%d] engine_start[%d]",
				__func__, task->job.jobid, engine_start);
	mutex_lock(&rb_buf_pool_mutex);
	while (engine_start < (engine+1)*MAX_ENG_RB_BUF) {
		if (rb_buf_pool[engine_start] != INVALID_OFFSET_ADDR) {
			hist->va_offset = rb_buf_pool[engine_start];
			rb_buf_pool[engine_start] = INVALID_OFFSET_ADDR;
			 mml_pq_rb_msg("%s get offset job_id[%d]engine[%d]offset[%d]eng_start[%d]",
				__func__, task->job.jobid, engine, hist->va_offset, engine_start);
			break;
		}
		engine_start++;
	}
	mutex_unlock(&rb_buf_pool_mutex);

	mml_pq_rb_msg("%s all end job_id[%d] engine[%d] hist_va[%p] hist_pa[%llx]",
		__func__, task->job.jobid, engine, hist->va, hist->pa);
}

void mml_pq_put_vcp_buf_offset(struct mml_task *task, u32 engine,
			       struct mml_pq_readback_buffer *hist)
{
	s32 engine_start = engine*MAX_ENG_RB_BUF;

	mml_pq_rb_msg("%s start job_id[%d] hist_va[%p] engine_start[%08x] offset[%d]",
		__func__, task->job.jobid, hist->va, engine_start, hist->va_offset);

	mutex_lock(&rb_buf_pool_mutex);
	while (engine_start < (engine+1)*MAX_ENG_RB_BUF) {
		if (rb_buf_pool[engine_start] == INVALID_OFFSET_ADDR) {
			rb_buf_pool[engine_start] = hist->va_offset;
			hist->va_offset = INVALID_OFFSET_ADDR;
			break;
		}
		engine_start++;
	}
	mutex_unlock(&rb_buf_pool_mutex);
}

void mml_pq_get_readback_buffer(struct mml_task *task, u8 pipe,
				 struct mml_pq_readback_buffer **hist)
{
	struct cmdq_client *clt = task->config->path[pipe]->clt;
	struct mml_pq_readback_buffer *temp_buffer = NULL;

	mml_pq_msg("%s job_id[%d]", __func__, task->job.jobid);

	mutex_lock(&rb_buf_list_mutex);
	temp_buffer = list_first_entry_or_null(&rb_buf_list,
		typeof(*temp_buffer), buffer_list);
	if (temp_buffer) {
		*hist = temp_buffer;
		list_del(&temp_buffer->buffer_list);
		mml_pq_rb_msg("%s aal get buffer from list jobid[%d] va[%p] pa[%08x]",
			__func__, task->job.jobid, temp_buffer->va, temp_buffer->pa);
	} else {
		temp_buffer = kzalloc(sizeof(struct mml_pq_readback_buffer),
			GFP_KERNEL);

		if (unlikely(!temp_buffer)) {
			mml_pq_err("%s create buffer failed", __func__);
			mutex_unlock(&rb_buf_list_mutex);
			return;
		}
		INIT_LIST_HEAD(&temp_buffer->buffer_list);
		buffer_num++;
		*hist = temp_buffer;
		mml_pq_rb_msg("%s aal reallocate jobid[%d] va[%p] pa[%08x]", __func__,
			task->job.jobid, temp_buffer->va, temp_buffer->pa);
	}
	mutex_unlock(&rb_buf_list_mutex);

	if (!temp_buffer->va && !temp_buffer->pa) {
		mutex_lock(&task->pq_task->buffer_mutex);
		temp_buffer->va = (u32 *)cmdq_mbox_buf_alloc(clt, &temp_buffer->pa);
		mutex_unlock(&task->pq_task->buffer_mutex);
	}
	mml_pq_rb_msg("%s job_id[%d] va[%p] pa[%llx] buffer_num[%d]", __func__,
			task->job.jobid, temp_buffer->va, temp_buffer->pa, buffer_num);
}

void mml_pq_put_readback_buffer(struct mml_task *task, u8 pipe,
				 struct mml_pq_readback_buffer *hist)
{
	mml_pq_rb_msg("%s all end job_id[%d] hist_va[%p] hist_pa[%llx]",
		__func__, task->job.jobid, hist->va, hist->pa);
	mutex_lock(&rb_buf_list_mutex);
	list_add_tail(&hist->buffer_list, &rb_buf_list);
	mutex_unlock(&rb_buf_list_mutex);
}

void mml_pq_task_release(struct mml_task *task)
{
	struct mml_pq_task *pq_task = task->pq_task;

	pq_task->task = NULL;
	task->pq_task = NULL;
	kref_put(&pq_task->ref, release_pq_task);
}

static struct mml_pq_task *from_tile_init(struct mml_pq_sub_task *sub_task)
{
	return container_of(sub_task, struct mml_pq_task, tile_init);
}

static struct mml_pq_task *from_comp_config(struct mml_pq_sub_task *sub_task)
{
	return container_of(sub_task, struct mml_pq_task, comp_config);
}

static struct mml_pq_task *from_aal_readback(struct mml_pq_sub_task *sub_task)
{
	return container_of(sub_task, struct mml_pq_task, aal_readback);
}

static struct mml_pq_task *from_hdr_readback(struct mml_pq_sub_task *sub_task)
{
	return container_of(sub_task,
		struct mml_pq_task, hdr_readback);
}

static struct mml_pq_task *from_rsz_callback(struct mml_pq_sub_task *sub_task)
{
	return container_of(sub_task,
		struct mml_pq_task, rsz_callback);
}

static void dump_pq_param(struct mml_pq_param *pq_param)
{
	if (!pq_param)
		return;

	mml_pq_dump(" enable=%u", pq_param->enable);
	mml_pq_dump(" time_stamp=%u", pq_param->time_stamp);
	mml_pq_dump(" scenario=%u", pq_param->scenario);
	mml_pq_dump(" layer_id=%u disp_id=%d",
		pq_param->layer_id, pq_param->disp_id);
	mml_pq_dump(" src_gamut=%u dst_gamut",
		pq_param->src_gamut, pq_param->dst_gamut);
	mml_pq_dump(" video_mode=%u", pq_param->src_hdr_video_mode);
	mml_pq_dump(" user=%u", pq_param->user_info);
}

static void mml_pq_check_dup_node(struct mml_pq_chan *chan, struct mml_pq_sub_task *sub_task)
{
	struct mml_pq_sub_task *tmp_sub_task = NULL, *tmp = NULL;
	u64 job_id = sub_task->job_id;

	mutex_lock(&chan->msg_lock);
	if (!list_empty(&chan->msg_list)) {
		list_for_each_entry_safe(tmp_sub_task, tmp, &chan->msg_list, mbox_list) {
			mml_pq_msg("%s sub_task[%p] chan->job_list[%p] sub_job_id[%d]",
				__func__, tmp_sub_task, chan->msg_list, tmp_sub_task->job_id);
			if (tmp_sub_task->job_id == job_id) {
				mml_pq_msg("%s find sub_task:%p id:%llx",
					__func__, tmp_sub_task, job_id);
				list_del(&tmp_sub_task->mbox_list);
				atomic_dec_if_positive(&chan->msg_cnt);
				break;
			}
		}
	}
	mutex_unlock(&chan->msg_lock);

	mutex_lock(&chan->job_lock);
	if (!list_empty(&chan->job_list)) {
		list_for_each_entry_safe(tmp_sub_task, tmp, &chan->job_list, mbox_list) {
			mml_pq_msg("%s sub_task[%p] chan->job_list[%p] sub_job_id[%d]",
				__func__, tmp_sub_task, chan->job_list, tmp_sub_task->job_id);
			if (tmp_sub_task->job_id == job_id) {
				mml_pq_msg("%s find sub_task:%p id:%llx",
					__func__, tmp_sub_task, job_id);
				list_del(&tmp_sub_task->mbox_list);
				break;
			}
		}
	}
	mutex_unlock(&chan->job_lock);

}

static int set_sub_task(struct mml_task *task,
			struct mml_pq_sub_task *sub_task,
			struct mml_pq_chan *chan,
			struct mml_pq_param *pq_param,
			bool is_dup_check)
{
	struct mml_pq_task *pq_task = task->pq_task;

	mml_pq_msg("%s called queued[%d] result_ref[%d] job_id[%d, %d] first_job[%d]",
		__func__, atomic_read(&sub_task->queued),
		atomic_read(&sub_task->result_ref),
		sub_task->job_id, task->job.jobid,
		sub_task->first_job);

	mutex_lock(&sub_task->lock);
	if (sub_task->mml_task_jobid != task->job.jobid || sub_task->first_job) {
		sub_task->mml_task_jobid = task->job.jobid;
		sub_task->first_job = false;
	} else {
		mutex_unlock(&sub_task->lock);
		mml_pq_msg("%s already queue queued[%d] job_id[%d, %d]",
			__func__, atomic_read(&sub_task->queued),
			sub_task->job_id, task->job.jobid);

		return 0;
	}
	mutex_unlock(&sub_task->lock);

	if (!atomic_fetch_add_unless(&sub_task->queued, 1, 1)) {
		if (is_dup_check)
			mml_pq_check_dup_node(chan, sub_task);
		//WARN_ON(atomic_read(&sub_task->result_ref));
		atomic_set(&sub_task->result_ref, 0);
		kref_get(&pq_task->ref);
		memcpy(&sub_task->frame_data.pq_param, task->pq_param,
			MML_MAX_OUTPUTS * sizeof(struct mml_pq_param));
		memcpy(&sub_task->frame_data.info, &task->config->info,
			sizeof(struct mml_frame_info));
		memcpy(&sub_task->frame_data.frame_out, &task->config->frame_out,
			MML_MAX_OUTPUTS * sizeof(struct mml_frame_size));
		sub_task->readback_data.is_dual = task->config->dual;
		queue_msg(chan, sub_task);
		dump_pq_param(pq_param);
	}
	mml_pq_msg("%s end queued[%d] result_ref[%d] job_id[%d, %d] first_job[%d]",
		__func__, atomic_read(&sub_task->queued),
		atomic_read(&sub_task->result_ref),
		sub_task->job_id, task->job.jobid,
		sub_task->first_job);
	return 0;
}

static int get_sub_task_result(struct mml_pq_task *pq_task,
			       struct mml_pq_sub_task *sub_task, u32 timeout_ms,
			       void (*dump_func)(void *data))
{
	s32 ret;

	if (unlikely(!atomic_read(&sub_task->queued)))
		mml_pq_msg("%s already handled or not queued", __func__);

	mml_pq_msg("begin wait for result");
	ret = wait_event_timeout(sub_task->wq,
				(!atomic_read(&sub_task->queued)) && (sub_task->result),
				msecs_to_jiffies(timeout_ms));
	mml_pq_msg("wait for result: %d", ret);
	mutex_lock(&sub_task->lock);	/* To wait unlock of handle thread */
	atomic_inc(&sub_task->result_ref);
	mutex_unlock(&sub_task->lock);
	if (ret && dump_func)
		dump_func(sub_task->result);

	if (ret)
		return 0;
	else
		return -EBUSY;
}

static void put_sub_task_result(struct mml_pq_sub_task *sub_task, struct mml_pq_chan *chan)
{
	mml_pq_msg("%s result_ref[%d] queued[%d] msg_cnt[%d]",
		__func__, atomic_read(&sub_task->result_ref),
		atomic_read(&sub_task->queued),
		atomic_read(&chan->msg_cnt));

	if (!atomic_dec_if_positive(&sub_task->result_ref))
		if (!atomic_dec_if_positive(&sub_task->queued))
			atomic_dec_if_positive(&chan->msg_cnt);

}

static void dump_tile_init(void *data)
{
	u32 i;
	struct mml_pq_tile_init_result *result = (struct mml_pq_tile_init_result *)data;
	if (!result)
		return;

	mml_pq_dump("[tile][param] count=%u", result->rsz_param_cnt);
	for (i = 0; i < result->rsz_param_cnt && i < MML_MAX_OUTPUTS; i++) {
		mml_pq_dump("[tile][%u] coeff_step_x=%u", i,
				result->rsz_param[i].coeff_step_x);
		mml_pq_dump("[tile][%u] coeff_step_y=%u", i,
				result->rsz_param[i].coeff_step_y);
		mml_pq_dump("[tile][%u] precision_x=%u", i,
				result->rsz_param[i].precision_x);
		mml_pq_dump("[tile][%u] precision_y=%u", i,
				result->rsz_param[i].precision_y);
		mml_pq_dump("[tile][%u] crop_offset_x=%u", i,
				result->rsz_param[i].crop_offset_x);
		mml_pq_dump("[tile][%u] crop_subpix_x=%u", i,
				result->rsz_param[i].crop_subpix_x);
		mml_pq_dump("[tile][%u] crop_offset_y=%u", i,
				result->rsz_param[i].crop_offset_y);
		mml_pq_dump("[tile][%u] crop_subpix_y=%u", i,
				result->rsz_param[i].crop_subpix_y);
		mml_pq_dump("[tile][%u] hor_dir_scale=%u", i,
				result->rsz_param[i].hor_dir_scale);
		mml_pq_dump("[tile][%u] hor_algorithm=%u", i,
				result->rsz_param[i].hor_algorithm);
		mml_pq_dump("[tile][%u] ver_dir_scale=%u", i,
				result->rsz_param[i].ver_dir_scale);
		mml_pq_dump("[tile][%u] ver_algorithm=%u", i,
				result->rsz_param[i].ver_algorithm);
		mml_pq_dump("[tile][%u] vertical_first=%u", i,
				result->rsz_param[i].vertical_first);
		mml_pq_dump("[tile][%u] ver_cubic_trunc=%u", i,
				result->rsz_param[i].ver_cubic_trunc);
	}
	mml_pq_dump("[tile] count=%u", result->rsz_reg_cnt);
}

int mml_pq_set_tile_init(struct mml_task *task)
{
	struct mml_pq_task *pq_task = task->pq_task;
	struct mml_pq_chan *chan = &pq_mbox->tile_init_chan;
	int ret;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s called, job_id[%d]", __func__, task->job.jobid);
	ret = set_sub_task(task, &pq_task->tile_init, chan,
			   &task->pq_param[0], false);
	mml_pq_trace_ex_end();
	return ret;
}

int mml_pq_get_tile_init_result(struct mml_task *task, u32 timeout_ms)
{
	struct mml_pq_task *pq_task = task->pq_task;
	s32 ret;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s called, %d job_id[%d]",
		__func__, timeout_ms, task->job.jobid);
	ret = get_sub_task_result(pq_task, &pq_task->tile_init, timeout_ms,
				  dump_tile_init);
	mml_pq_trace_ex_end();
	return ret;
}

void mml_pq_put_tile_init_result(struct mml_task *task)
{
	struct mml_pq_chan *chan = &pq_mbox->tile_init_chan;

	mml_pq_msg("%s called job_id[%d]\n", __func__,
		task->job.jobid);

	put_sub_task_result(&task->pq_task->tile_init, chan);
}

static void dump_comp_config(void *data)
{
	u32 i;
	struct mml_pq_comp_config_result *result =
		(struct mml_pq_comp_config_result *)data;

	if (!result)
		return;

	mml_pq_dump("[comp] count=%u", result->param_cnt);
	for (i = 0; i < result->param_cnt && i < MML_MAX_OUTPUTS; i++) {
		mml_pq_dump("[comp][%u] dre_blk_width=%u", i,
			result->aal_param[i].dre_blk_width);
		mml_pq_dump("[comp][%u] dre_blk_height=%u", i,
			result->aal_param[i].dre_blk_height);
	}
	mml_pq_dump("[comp] count=%u", result->aal_reg_cnt);
	for (i = 0; i < AAL_CURVE_NUM; i += 8)
		mml_pq_dump("[curve][%d - %d] = [%08x, %08x, %08x, %08x, %08x, %08x, %08x, %08x]",
			i, i+7,
			result->aal_curve[i], result->aal_curve[i+1],
			result->aal_curve[i+2], result->aal_curve[i+3],
			result->aal_curve[i+4], result->aal_curve[i+5],
			result->aal_curve[i+6], result->aal_curve[i+7]);
}

int mml_pq_set_comp_config(struct mml_task *task)
{
	struct mml_pq_task *pq_task = task->pq_task;
	struct mml_pq_chan *chan = &pq_mbox->comp_config_chan;
	int ret;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s called, job_id[%d] sub_task[%p]", __func__,
		task->job.jobid, &pq_task->comp_config);
	ret = set_sub_task(task, &pq_task->comp_config, chan,
			   &task->pq_param[0], false);
	mml_pq_trace_ex_end();
	return ret;
}

static bool set_hist(struct mml_pq_sub_task *sub_task,
		     bool dual, u8 pipe, u32 *phist, s32 engine)
{
	bool ready;

	mutex_lock(&sub_task->lock);
	if (dual && pipe) {
		if (engine == MML_PQ_AAL)
			memcpy(sub_task->readback_data.pipe1_hist, phist,
				sizeof(u32)*(AAL_HIST_NUM+AAL_DUAL_INFO_NUM));
		else if (engine == MML_PQ_HDR)
			memcpy(sub_task->readback_data.pipe1_hist, phist,
				sizeof(u32)*HDR_HIST_NUM);
	} else {
		if (engine == MML_PQ_AAL)
			memcpy(sub_task->readback_data.pipe0_hist, phist,
				sizeof(u32)*(AAL_HIST_NUM+AAL_DUAL_INFO_NUM));
		else if (engine == MML_PQ_HDR)
			memcpy(sub_task->readback_data.pipe0_hist, phist,
				sizeof(u32)*HDR_HIST_NUM);
	}

	atomic_inc(&sub_task->readback_data.pipe_cnt);
	ready = (dual && atomic_read(&sub_task->readback_data.pipe_cnt) == MML_PIPE_CNT) ||
		!dual;

	mutex_unlock(&sub_task->lock);
	return ready;
}

int mml_pq_aal_readback(struct mml_task *task, u8 pipe, u32 *phist)
{
	struct mml_pq_task *pq_task = task->pq_task;
	struct mml_pq_sub_task *sub_task = &pq_task->aal_readback;
	struct mml_pq_chan *chan = &pq_mbox->aal_readback_chan;
	int ret = 0;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s called job_id[%d] pipe[%d] sub_task->job_id[%d]",
		__func__, task->job.jobid, pipe, sub_task->job_id);

	if (!pipe) {
		mml_pq_rb_msg("%s job_id[%d] hist[0~4]={%08x, %08x, %08x, %08x, %08x}",
			__func__, task->job.jobid, phist[0], phist[1],
			phist[2], phist[3], phist[4]);

		mml_pq_rb_msg("%s job_id[%d] hist[10~14]={%08x, %08x, %08x, %08x, %08x}",
			__func__, task->job.jobid, phist[10], phist[11],
			phist[12], phist[13], phist[14]);
	} else {
		mml_pq_rb_msg("%s job_id[%d] hist[600~604]={%08x, %08x, %08x, %08x, %08x}",
			__func__, task->job.jobid, phist[600], phist[601],
			phist[602], phist[603], phist[604]);

		mml_pq_rb_msg("%s job_id[%d] hist[610~614]={%08x, %08x, %08x, %08x, %08x}",
			__func__, task->job.jobid, phist[610], phist[611],
			phist[612], phist[613], phist[614]);
	}

	if (set_hist(sub_task, task->config->dual, pipe, phist, MML_PQ_AAL))
		ret = set_sub_task(task, sub_task, chan, &task->pq_param[0], true);

	mml_pq_msg("%s end", __func__);
	mml_pq_trace_ex_end();
	return ret;
}

int mml_pq_hdr_readback(struct mml_task *task, u8 pipe, u32 *phist)
{
	struct mml_pq_task *pq_task = task->pq_task;
	struct mml_pq_sub_task *sub_task = &pq_task->hdr_readback;
	struct mml_pq_chan *chan = &pq_mbox->hdr_readback_chan;
	int ret = 0;

	mml_pq_msg("%s called pipe[%d]\n", __func__, pipe);
	if (unlikely(!task))
		return -EINVAL;

	mml_pq_msg("%s called pipe[%d] job_id[%d]\n", __func__, pipe, task->job.jobid);
	dump_pq_param(&task->pq_param[0]);

	if (set_hist(sub_task, task->config->dual, pipe, phist, MML_PQ_HDR))
		ret = set_sub_task(task, sub_task, chan, &task->pq_param[0], true);

	mml_pq_msg("%s end\n", __func__);
	return ret;
}

int mml_pq_rsz_callback(struct mml_task *task)
{
	struct mml_pq_task *pq_task = task->pq_task;
	struct mml_pq_sub_task *sub_task = &pq_task->rsz_callback;
	struct mml_pq_chan *chan = &pq_mbox->rsz_callback_chan;
	int ret = 0;


	mml_pq_msg("%s second outoput done.\n", __func__);
	ret = set_sub_task(task, sub_task, chan, &task->pq_param[1], true);
	mml_pq_msg("%s end\n", __func__);
	return ret;
}

int mml_pq_get_comp_config_result(struct mml_task *task, u32 timeout_ms)
{
	struct mml_pq_task *pq_task = task->pq_task;
	s32 ret;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s called, %d job_id[%d]", __func__,
		timeout_ms, task->job.jobid);

	ret = get_sub_task_result(pq_task, &pq_task->comp_config, timeout_ms,
				  dump_comp_config);
	mml_pq_trace_ex_end();
	return ret;
}

void mml_pq_put_comp_config_result(struct mml_task *task)
{
	struct mml_pq_chan *chan = &pq_mbox->comp_config_chan;

	mml_pq_msg("%s called job_id[%d]\n", __func__,
		task->job.jobid);

	put_sub_task_result(&task->pq_task->comp_config, chan);
}

static void handle_sub_task_result(struct mml_pq_sub_task *sub_task,
	void *result, void (*release_result)(void *result))
{
	mutex_lock(&sub_task->lock);
	if (atomic_read(&sub_task->result_ref)) {
		/* old result still in-use, abandon new result or else? */
		release_result(result);
	} else {
		if (sub_task->result)	/* destroy old result */
			release_result(sub_task->result);
		sub_task->result = result;
	}
	atomic_dec_if_positive(&sub_task->queued);
	mutex_unlock(&sub_task->lock);
}

static void wake_up_sub_task(struct mml_pq_sub_task *sub_task,
			     struct mml_pq_task *pq_task)
{
	mutex_lock(&sub_task->lock);
	wake_up(&sub_task->wq);
	mutex_unlock(&sub_task->lock);

	/* decrease pq_task ref after finish the result */
	kref_put(&pq_task->ref, release_pq_task);
}

static struct mml_pq_sub_task *wait_next_sub_task(struct mml_pq_chan *chan)
{
	struct mml_pq_sub_task *sub_task = NULL;
	s32 ret;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s called msg_cnt[%d]", __func__,
		atomic_read(&chan->msg_cnt));
	for (;;) {
		mml_pq_msg("start wait event!");

		ret = wait_event_interruptible(chan->msg_wq,
			(atomic_read(&chan->msg_cnt) > 0));
		if (ret) {
			if (ret != -ERESTARTSYS)
				mml_pq_log("%s wakeup wait_result[%d], msg_cnt[%d]",
					__func__, ret, atomic_read(&chan->msg_cnt));
			break;
		}

		mml_pq_msg("%s finish wait event! wait_result[%d], msg_cnt[%d]",
			__func__, ret, atomic_read(&chan->msg_cnt));

		ret = dequeue_msg(chan, &sub_task);
		if (unlikely(ret)) {
			mml_pq_err("err: dequeue msg failed: %d", ret);
			continue;
		}

		mml_pq_msg("after dequeue msg!");
		break;
	}
	mml_pq_msg("%s end %d task=%p", __func__, ret, sub_task);
	mml_pq_trace_ex_end();
	return sub_task;
}

static void cancel_sub_task(struct mml_pq_sub_task *sub_task)
{
	mutex_lock(&sub_task->lock);
	sub_task->job_cancelled = true;
	wake_up(&sub_task->wq);
	mutex_unlock(&sub_task->lock);
}

static void handle_tile_init_result(struct mml_pq_chan *chan,
				    struct mml_pq_tile_init_job *job)
{
	struct mml_pq_sub_task *sub_task = NULL;
	struct mml_pq_tile_init_result *result;
	struct mml_pq_reg *rsz_regs[MML_MAX_OUTPUTS];
	s32 ret;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_log("%s called, %d", __func__, job->result_job_id);
	ret = find_sub_task(chan, job->result_job_id, &sub_task);
	if (unlikely(ret)) {
		mml_pq_err("finish tile sub_task failed!: %d id: %d", ret,
			job->result_job_id);
		return;
	}

	result = kmalloc(sizeof(*result), GFP_KERNEL);
	if (unlikely(!result)) {
		mml_pq_err("err: create result failed");
		goto wake_up_prev_tile_init_task;
	}

	ret = copy_from_user(result, job->result, sizeof(*result));
	if (unlikely(ret)) {
		mml_pq_err("copy job result failed!: %d", ret);
		goto free_tile_init_result;
	}

	if (unlikely(result->rsz_param_cnt > MML_MAX_OUTPUTS)) {
		mml_pq_err("invalid rsz param count!: %d",
			result->rsz_param_cnt);
		goto free_tile_init_result;
	}

	rsz_regs[0] = kmalloc_array(result->rsz_reg_cnt[0], sizeof(struct mml_pq_reg),
				GFP_KERNEL);
	if (unlikely(!rsz_regs[0])) {
		mml_pq_err("err: create rsz_regs failed, size:%d",
			result->rsz_reg_cnt[0]);
		goto free_tile_init_result;
	}

	ret = copy_from_user(rsz_regs[0], result->rsz_regs[0],
		result->rsz_reg_cnt[0] * sizeof(struct mml_pq_reg));
	if (unlikely(ret)) {
		mml_pq_err("copy rsz config failed!: %d", ret);
		goto free_rsz_regs_0;
	}
	result->rsz_regs[0] = rsz_regs[0];

	/* second output */
	if (result->rsz_param_cnt == MML_MAX_OUTPUTS) {
		rsz_regs[1] = kmalloc_array(result->rsz_reg_cnt[1], sizeof(struct mml_pq_reg),
					GFP_KERNEL);
		if (unlikely(!rsz_regs[1])) {
			mml_pq_err("err: create rsz_regs failed, size:%d",
				result->rsz_reg_cnt[1]);
			goto free_rsz_regs_0;
		}

		ret = copy_from_user(rsz_regs[1], result->rsz_regs[1],
			result->rsz_reg_cnt[1] * sizeof(struct mml_pq_reg));
		if (unlikely(ret)) {
			mml_pq_err("copy rsz config failed!: %d", ret);
			goto free_rsz_regs_1;
		}
		result->rsz_regs[1] = rsz_regs[1];
	}

	handle_sub_task_result(sub_task, result, release_tile_init_result);
	goto wake_up_prev_tile_init_task;
free_rsz_regs_1:
	kfree(rsz_regs[1]);
free_rsz_regs_0:
	kfree(rsz_regs[0]);
free_tile_init_result:
	kfree(result);
wake_up_prev_tile_init_task:
	wake_up_sub_task(sub_task, from_tile_init(sub_task));
	mml_pq_msg("%s end, %d", __func__, ret);
	mml_pq_trace_ex_end();
}

static int mml_pq_tile_init_ioctl(unsigned long data)
{
	struct mml_pq_chan *chan = &pq_mbox->tile_init_chan;
	struct mml_pq_sub_task *new_sub_task = NULL;
	struct mml_pq_task *new_pq_task = NULL;
	struct mml_pq_tile_init_job *job;
	struct mml_pq_tile_init_job *user_job;
	u32 new_job_id;
	s32 ret;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s called", __func__);
	user_job = (struct mml_pq_tile_init_job *)data;
	if (unlikely(!user_job))
		return -EINVAL;

	job = kmalloc(sizeof(*job), GFP_KERNEL);
	if (unlikely(!job))
		return -ENOMEM;

	ret = copy_from_user(job, user_job, sizeof(*job));
	if (unlikely(ret)) {
		mml_pq_err("copy_from_user failed: %d", ret);
		kfree(job);
		return -EINVAL;
	}
	mml_pq_msg("%s result_job_id[%d]", __func__, job->result_job_id);
	if (job->result_job_id)
		handle_tile_init_result(chan, job);
	kfree(job);

	new_sub_task = wait_next_sub_task(chan);
	if (!new_sub_task) {
		mml_pq_msg("%s Get sub task failed", __func__);
		return -ERESTARTSYS;
	}

	new_pq_task = from_tile_init(new_sub_task);
	new_job_id = new_sub_task->job_id;

	ret = copy_to_user(&user_job->new_job_id, &new_job_id, sizeof(u32));

	ret = copy_to_user(&user_job->info, &new_sub_task->frame_data.info,
		sizeof(struct mml_frame_info));
	if (unlikely(ret)) {
		mml_pq_err("err: fail to copy to user frame info: %d", ret);
		goto wake_up_tile_init_task;
	}

	ret = copy_to_user(user_job->dst, new_sub_task->frame_data.frame_out,
		MML_MAX_OUTPUTS * sizeof(struct mml_frame_size));
	if (unlikely(ret)) {
		mml_pq_err("err: fail to copy to user frame out: %d", ret);
		goto wake_up_tile_init_task;
	}

	ret = copy_to_user(user_job->param, new_sub_task->frame_data.pq_param,
		MML_MAX_OUTPUTS * sizeof(struct mml_pq_param));
	if (unlikely(ret)) {
		mml_pq_err("err: fail to copy to user pq param: %d", ret);
		goto wake_up_tile_init_task;
	}
	mml_pq_msg("%s end", __func__);
	mml_pq_trace_ex_end();
	return 0;

wake_up_tile_init_task:
	cancel_sub_task(new_sub_task);
	mml_pq_msg("%s end %d", __func__, ret);
	mml_pq_trace_ex_end();
	return ret;
}

static void handle_comp_config_result(struct mml_pq_chan *chan,
				      struct mml_pq_comp_config_job *job)
{
	struct mml_pq_sub_task *sub_task = NULL;
	struct mml_pq_comp_config_result *result;
	struct mml_pq_aal_config_param *aal_param;
	struct mml_pq_reg *aal_regs;
	struct mml_pq_reg *hdr_regs;
	struct mml_pq_reg *ds_regs;
	struct mml_pq_reg *color_regs;
	u32 *aal_curve;
	u32 *hdr_curve;
	s32 ret;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s called, %d", __func__, job->result_job_id);
	ret = find_sub_task(chan, job->result_job_id, &sub_task);
	if (unlikely(ret)) {
		mml_pq_err("finish comp sub_task failed!: %d id: %d", ret,
			job->result_job_id);
		return;
	}
	mml_pq_msg("%s end %d task=%p sub_task->id[%d]", __func__, ret,
			sub_task, sub_task->job_id);

	result = kmalloc(sizeof(*result), GFP_KERNEL);
	if (unlikely(!result)) {
		mml_pq_err("err: create result failed");
		goto wake_up_prev_comp_config_task;
	}

	ret = copy_from_user(result, job->result, sizeof(*result));
	if (unlikely(ret)) {
		mml_pq_err("copy job result failed!: %d", ret);
		goto free_comp_config_result;
	}

	if (unlikely(result->param_cnt > MML_MAX_OUTPUTS)) {
		mml_pq_err("invalid param count!: %d",
			result->param_cnt);
		goto free_comp_config_result;
	}

	aal_param = kmalloc_array(result->param_cnt, sizeof(*aal_param),
				  GFP_KERNEL);
	if (unlikely(!aal_param)) {
		mml_pq_err("err: create aal_param failed, size:%d",
			result->param_cnt);
		goto free_comp_config_result;
	}

	ret = copy_from_user(aal_param, result->aal_param,
		result->param_cnt * sizeof(*aal_param));
	if (unlikely(ret)) {
		mml_pq_err("copy aal param failed!: %d", ret);
		goto free_aal_param;
	}

	aal_regs = kmalloc_array(result->aal_reg_cnt, sizeof(*aal_regs),
				 GFP_KERNEL);
	if (unlikely(!aal_regs)) {
		mml_pq_err("err: create aal_regs failed, size:%d",
			result->aal_reg_cnt);
		goto free_aal_param;
	}

	ret = copy_from_user(aal_regs, result->aal_regs,
		result->aal_reg_cnt * sizeof(*aal_regs));
	if (unlikely(ret)) {
		mml_pq_err("copy aal config failed!: %d", ret);
		goto free_aal_regs;
	}

	aal_curve = kmalloc_array(AAL_CURVE_NUM, sizeof(u32), GFP_KERNEL);
	if (unlikely(!aal_curve)) {
		mml_pq_err("err: create aal_curve failed, size:%d",
			AAL_CURVE_NUM);
		goto free_aal_regs;
	}

	ret = copy_from_user(aal_curve, result->aal_curve,
		AAL_CURVE_NUM * sizeof(u32));
	if (unlikely(ret)) {
		mml_pq_err("copy aal curve failed!: %d", ret);
		goto free_aal_curve;
	}

	hdr_regs = kmalloc_array(result->hdr_reg_cnt, sizeof(*hdr_regs),
				 GFP_KERNEL);
	if (unlikely(!hdr_regs)) {
		mml_pq_err("err: create hdr_regs failed, size:%d\n",
			result->hdr_reg_cnt);
		goto free_aal_curve;
	}

	ret = copy_from_user(hdr_regs, result->hdr_regs,
		result->hdr_reg_cnt * sizeof(*hdr_regs));
	if (unlikely(ret)) {
		mml_pq_err("copy aal config failed!: %d\n", ret);
		goto free_hdr_regs;
	}

	hdr_curve = kmalloc_array(HDR_CURVE_NUM, sizeof(u32),
				  GFP_KERNEL);
	if (unlikely(!hdr_curve)) {
		mml_pq_err("err: create hdr_curve failed, size:%d\n",
			HDR_CURVE_NUM);
		goto free_hdr_regs;
	}

	ret = copy_from_user(hdr_curve, result->hdr_curve,
		HDR_CURVE_NUM * sizeof(u32));
	if (unlikely(ret)) {
		mml_pq_err("copy hdr curve failed!: %d\n", ret);
		goto free_hdr_curve;
	}

	ds_regs = kmalloc_array(result->ds_reg_cnt, sizeof(*ds_regs),
				GFP_KERNEL);
	if (unlikely(!ds_regs)) {
		mml_pq_err("err: create ds_regs failed, size:%d\n",
			result->ds_reg_cnt);
		goto free_aal_curve;
	}

	ret = copy_from_user(ds_regs, result->ds_regs,
		result->ds_reg_cnt * sizeof(*ds_regs));
	if (unlikely(ret)) {
		mml_pq_err("copy ds config failed!: %d\n", ret);
		goto free_ds_regs;
	}

	color_regs = kmalloc_array(result->color_reg_cnt, sizeof(*color_regs),
				   GFP_KERNEL);
	if (unlikely(!color_regs)) {
		mml_pq_err("err: create color_regs failed, size:%d\n",
			result->ds_reg_cnt);
		goto free_ds_regs;
	}

	ret = copy_from_user(color_regs, result->color_regs,
		result->color_reg_cnt * sizeof(*color_regs));
	if (unlikely(ret)) {
		mml_pq_err("copy color config failed!: %d\n", ret);
		goto free_color_regs;
	}

	result->aal_param = aal_param;
	result->aal_regs = aal_regs;
	result->aal_curve = aal_curve;
	result->hdr_regs = hdr_regs;
	result->hdr_curve = hdr_curve;
	result->ds_regs = ds_regs;
	result->color_regs = color_regs;

	handle_sub_task_result(sub_task, result, release_comp_config_result);
	mml_pq_msg("%s result end, result_id[%d] sub_task[%p]",
		__func__, job->result_job_id, sub_task);
	goto wake_up_prev_comp_config_task;
free_hdr_curve:
	kfree(hdr_curve);
free_hdr_regs:
	kfree(hdr_regs);
free_ds_regs:
	kfree(ds_regs);
free_color_regs:
	kfree(color_regs);
free_aal_curve:
	kfree(aal_curve);
free_aal_regs:
	kfree(aal_regs);
free_aal_param:
	kfree(aal_param);
free_comp_config_result:
	kfree(result);
wake_up_prev_comp_config_task:
	wake_up_sub_task(sub_task, from_comp_config(sub_task));
	mml_pq_msg("%s end, %d", __func__, ret);
	mml_pq_trace_ex_end();
}

static int mml_pq_comp_config_ioctl(unsigned long data)
{
	struct mml_pq_chan *chan = &pq_mbox->comp_config_chan;
	struct mml_pq_sub_task *new_sub_task = NULL;
	struct mml_pq_task *new_pq_task = NULL;
	struct mml_pq_comp_config_job *job;
	struct mml_pq_comp_config_job *user_job;
	u32 new_job_id;
	s32 ret;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s called", __func__);
	user_job = (struct mml_pq_comp_config_job *)data;
	if (unlikely(!user_job))
		return -EINVAL;

	job = kmalloc(sizeof(*job), GFP_KERNEL);
	if (unlikely(!job))
		return -ENOMEM;

	ret = copy_from_user(job, user_job, sizeof(*job));
	if (unlikely(ret)) {
		mml_pq_err("copy_from_user failed: %d", ret);
		kfree(job);
		return -EINVAL;
	}
	mml_pq_msg("%s new_job_id[%d] result_job_id[%d]", __func__,
		job->new_job_id, job->result_job_id);

	if (job->result_job_id)
		handle_comp_config_result(chan, job);
	kfree(job);

	new_sub_task = wait_next_sub_task(chan);
	if (!new_sub_task) {
		mml_pq_msg("%s Get sub task failed", __func__);
		return -ERESTARTSYS;
	}

	new_pq_task = from_comp_config(new_sub_task);
	new_job_id = new_sub_task->job_id;

	ret = copy_to_user(&user_job->new_job_id, &new_job_id, sizeof(u32));

	ret = copy_to_user(&user_job->info, &new_sub_task->frame_data.info,
		sizeof(struct mml_frame_info));
	if (unlikely(ret)) {
		mml_pq_err("err: fail to copy to user frame info: %d", ret);
		goto wake_up_comp_config_task;
	}

	ret = copy_to_user(user_job->dst, new_sub_task->frame_data.frame_out,
		MML_MAX_OUTPUTS * sizeof(struct mml_frame_size));
	if (unlikely(ret)) {
		mml_pq_err("err: fail to copy to user frame out: %d", ret);
		goto wake_up_comp_config_task;
	}

	ret = copy_to_user(user_job->param, new_sub_task->frame_data.pq_param,
		MML_MAX_OUTPUTS * sizeof(struct mml_pq_param));
	if (unlikely(ret)) {
		mml_pq_err("err: fail to copy to user pq param: %d", ret);
		goto wake_up_comp_config_task;
	}
	mml_pq_msg("%s end", __func__);
	mml_pq_trace_ex_end();
	return 0;

wake_up_comp_config_task:
	cancel_sub_task(new_sub_task);
	mml_pq_msg("%s end %d", __func__, ret);
	mml_pq_trace_ex_end();
	return ret;
}

static int mml_pq_aal_readback_ioctl(unsigned long data)
{
	struct mml_pq_chan *chan = &pq_mbox->aal_readback_chan;
	struct mml_pq_sub_task *new_sub_task = NULL;
	struct mml_pq_task *new_pq_task = NULL;
	struct mml_pq_aal_readback_job *job;
	struct mml_pq_aal_readback_job *user_job;
	struct mml_pq_aal_readback_result *readback = NULL;
	u32 new_job_id;
	s32 ret;

	mml_pq_msg("%s called\n", __func__);
	user_job = (struct mml_pq_aal_readback_job *)data;
	if (unlikely(!user_job))
		return -EINVAL;

	job = kmalloc(sizeof(*job), GFP_KERNEL);
	if (unlikely(!job))
		return -ENOMEM;

	ret = copy_from_user(job, user_job, sizeof(*job));
	if (unlikely(ret)) {
		mml_pq_err("copy_from_user failed: %d", ret);
		kfree(job);
		return -EINVAL;
	}

	readback = kmalloc(sizeof(*readback), GFP_KERNEL);
	if (unlikely(!readback)) {
		kfree(job);
		return -ENOMEM;
	}

	ret = copy_from_user(readback, job->result, sizeof(*readback));
	if (unlikely(ret)) {
		mml_pq_err("copy_from_user failed: %d\n", ret);
		kfree(job);
		kfree(readback);
		return -EINVAL;
	}

	new_sub_task = wait_next_sub_task(chan);
	if (!new_sub_task) {
		kfree(job);
		kfree(readback);
		mml_pq_msg("%s Get sub task failed", __func__);
		return -ERESTARTSYS;
	}

	new_pq_task = from_aal_readback(new_sub_task);
	new_job_id = new_sub_task->job_id;

	ret = copy_to_user(&user_job->new_job_id, &new_job_id, sizeof(u32));

	ret = copy_to_user(&user_job->info, &new_sub_task->frame_data.info,
		sizeof(struct mml_frame_info));
	if (unlikely(ret)) {
		mml_pq_err("err: fail to copy to user frame info: %d\n", ret);
		goto wake_up_aal_readback_task;
	}

	ret = copy_to_user(user_job->param, new_sub_task->frame_data.pq_param,
		MML_MAX_OUTPUTS * sizeof(struct mml_pq_param));
	if (unlikely(ret)) {
		mml_pq_err("err: fail to copy to user pq param: %d\n", ret);
		goto wake_up_aal_readback_task;
	}

	readback->is_dual = new_sub_task->readback_data.is_dual;
	readback->cut_pos_x =
		(new_sub_task->frame_data.info.dest[0].crop.r.width/2) +
		new_sub_task->frame_data.info.dest[0].crop.r.left-1;

	mml_pq_msg("%s is_dual[%d] cut_pos_x[%d]", __func__,
		readback->is_dual, readback->cut_pos_x);

	ret = copy_to_user(&job->result->is_dual, &readback->is_dual, sizeof(bool));
	if (unlikely(ret)) {
		mml_pq_err("err: fail to copy to width: %d\n", ret);
		goto wake_up_aal_readback_task;
	}

	ret = copy_to_user(&job->result->cut_pos_x, &readback->cut_pos_x, sizeof(u32));
	if (unlikely(ret)) {
		mml_pq_err("err: fail to copy to width: %d\n", ret);
		goto wake_up_aal_readback_task;
	}

	if (new_sub_task->readback_data.pipe0_hist) {
		ret = copy_to_user(readback->aal_pipe0_hist,
			new_sub_task->readback_data.pipe0_hist,
			(AAL_HIST_NUM + AAL_DUAL_INFO_NUM) * sizeof(u32));
		atomic_dec_if_positive(&new_sub_task->readback_data.pipe_cnt);
		if (unlikely(ret)) {
			mml_pq_err("err: fail to copy to aal_pipe0_hist: %d\n", ret);
			goto wake_up_aal_readback_task;
		}
	} else {
		mml_pq_err("err: fail to copy to aal_pipe0_hist because of null pointer");
	}

	if (readback->is_dual && new_sub_task->readback_data.pipe1_hist) {
		ret = copy_to_user(readback->aal_pipe1_hist,
			new_sub_task->readback_data.pipe1_hist,
			(AAL_HIST_NUM + AAL_DUAL_INFO_NUM) * sizeof(u32));
		atomic_dec_if_positive(&new_sub_task->readback_data.pipe_cnt);
		if (unlikely(ret)) {
			mml_pq_err("err: fail to copy to aal_pipe1_hist: %d\n", ret);
			goto wake_up_aal_readback_task;
		}
	} else if (!readback->is_dual) {
		mml_pq_msg("single pipe");
	}

	atomic_dec_if_positive(&new_sub_task->queued);
	remove_sub_task(chan, new_sub_task->job_id);

	mml_pq_msg("%s end job_id[%d]\n", __func__, job->new_job_id);
	kfree(job);
	kfree(readback);
	return 0;

wake_up_aal_readback_task:
	atomic_dec_if_positive(&new_sub_task->queued);
	remove_sub_task(chan, new_sub_task->job_id);
	kfree(readback);
	kfree(job);
	cancel_sub_task(new_sub_task);
	mml_pq_msg("%s end %d", __func__, ret);
	return ret;
}

static int mml_pq_hdr_readback_ioctl(unsigned long data)
{
	struct mml_pq_chan *chan = &pq_mbox->hdr_readback_chan;
	struct mml_pq_sub_task *new_sub_task = NULL;
	struct mml_pq_task *new_pq_task = NULL;
	struct mml_pq_hdr_readback_job *job;
	struct mml_pq_hdr_readback_job *user_job;
	struct mml_pq_hdr_readback_result *readback = NULL;
	u32 new_job_id;
	s32 ret = 0;

	mml_pq_msg("%s called\n", __func__);
	user_job = (struct mml_pq_hdr_readback_job *)data;
	if (unlikely(!user_job))
		return -EINVAL;

	job = kmalloc(sizeof(*job), GFP_KERNEL);
	if (unlikely(!job))
		return -ENOMEM;

	ret = copy_from_user(job, user_job, sizeof(*job));
	if (unlikely(ret)) {
		mml_pq_err("copy_from_user failed: %d\n", ret);
		kfree(job);
		return -EINVAL;
	}

	readback = kmalloc(sizeof(*readback), GFP_KERNEL);
	if (unlikely(!readback)) {
		kfree(job);
		return -ENOMEM;
	}

	ret = copy_from_user(readback, job->result, sizeof(*readback));

	if (unlikely(ret)) {
		mml_pq_err("copy_from_user failed: %d\n", ret);
		kfree(job);
		kfree(readback);
		return -EINVAL;
	}

	new_sub_task = wait_next_sub_task(chan);

	if (!new_sub_task) {
		kfree(job);
		kfree(readback);
		mml_pq_msg("%s Get sub task failed", __func__);
		return -ERESTARTSYS;
	}

	new_pq_task = from_hdr_readback(new_sub_task);
	new_job_id = new_sub_task->job_id;

	ret = copy_to_user(&user_job->new_job_id, &new_job_id, sizeof(u32));

	ret = copy_to_user(&user_job->info, &new_sub_task->frame_data.info,
		sizeof(struct mml_frame_info));
	if (unlikely(ret)) {
		mml_pq_err("err: fail to copy to user frame info: %d\n", ret);
		goto wake_up_hdr_readback_task;
	}

	ret = copy_to_user(user_job->param, new_sub_task->frame_data.pq_param,
		MML_MAX_OUTPUTS * sizeof(struct mml_pq_param));
	if (unlikely(ret)) {
		mml_pq_err("err: fail to copy to user pq param: %d\n", ret);
		goto wake_up_hdr_readback_task;
	}

	readback->is_dual = new_sub_task->readback_data.is_dual;
	readback->cut_pos_x = new_sub_task->frame_data.info.src.width / 2; //fix me

	ret = copy_to_user(&job->result->is_dual, &readback->is_dual, sizeof(bool));
	if (unlikely(ret)) {
		mml_pq_err("err: fail to copy to width: %d\n", ret);
		goto wake_up_hdr_readback_task;
	}

	ret = copy_to_user(&job->result->cut_pos_x, &readback->cut_pos_x, sizeof(u32));
	if (unlikely(ret)) {
		mml_pq_err("err: fail to copy to width: %d\n", ret);
		goto wake_up_hdr_readback_task;
	}

	if (new_sub_task->readback_data.pipe0_hist) {
		ret = copy_to_user(readback->hdr_pipe0_hist,
			new_sub_task->readback_data.pipe0_hist,
			HDR_HIST_NUM * sizeof(u32));
		atomic_dec_if_positive(&new_sub_task->readback_data.pipe_cnt);
		if (unlikely(ret)) {
			mml_pq_err("err: fail to copy to hdr_pipe0_hist: %d\n", ret);
			goto wake_up_hdr_readback_task;
		}
	} else {
		mml_pq_err("err: fail to copy to hdr_pipe0_hist because of null pointer");
	}

	if (readback->is_dual && new_sub_task->readback_data.pipe1_hist) {
		ret = copy_to_user(readback->hdr_pipe1_hist,
			new_sub_task->readback_data.pipe1_hist,
			HDR_HIST_NUM * sizeof(u32));
		atomic_dec_if_positive(&new_sub_task->readback_data.pipe_cnt);
		if (unlikely(ret)) {
			mml_pq_err("err: fail to copy to hdr_pipe1_hist: %d\n", ret);
			goto wake_up_hdr_readback_task;
		}
	} else {
		mml_pq_msg("single pipe");
	}

	atomic_dec_if_positive(&new_sub_task->queued);
	remove_sub_task(chan, new_sub_task->job_id);

	mml_pq_msg("%s end job_id[%d]\n", __func__, job->new_job_id);
	kfree(job);
	kfree(readback);
	return 0;

wake_up_hdr_readback_task:
	atomic_dec_if_positive(&new_sub_task->queued);
	remove_sub_task(chan, new_sub_task->job_id);
	kfree(job);
	kfree(readback);
	cancel_sub_task(new_sub_task);
	mml_pq_msg("%s end %d\n", __func__, ret);
	return ret;
}

static int mml_pq_rsz_callback_ioctl(unsigned long data)
{
	struct mml_pq_chan *chan = &pq_mbox->rsz_callback_chan;
	struct mml_pq_sub_task *new_sub_task = NULL;
	struct mml_pq_task *new_pq_task = NULL;
	struct mml_pq_rsz_callback_job *job;
	struct mml_pq_rsz_callback_job *user_job;
	u32 new_job_id;
	s32 ret = 0;

	mml_pq_msg("%s called\n", __func__);
	user_job = (struct mml_pq_rsz_callback_job *)data;
	if (unlikely(!user_job))
		return -EINVAL;

	job = kmalloc(sizeof(*job), GFP_KERNEL);
	if (unlikely(!job))
		return -ENOMEM;

	ret = copy_from_user(job, user_job, sizeof(*job));
	if (unlikely(ret)) {
		mml_pq_err("copy_from_user failed: %d\n", ret);
		kfree(job);
		return -EINVAL;
	}

	new_sub_task = wait_next_sub_task(chan);

	if (!new_sub_task) {
		kfree(job);
		mml_pq_msg("%s Get sub task failed", __func__);
		return -ERESTARTSYS;
	}

	new_pq_task = from_rsz_callback(new_sub_task);
	new_job_id = new_sub_task->job_id;

	ret = copy_to_user(&user_job->new_job_id, &new_job_id, sizeof(u32));

	ret = copy_to_user(&user_job->info, &new_sub_task->frame_data.info,
		sizeof(struct mml_frame_info));
	if (unlikely(ret)) {
		mml_pq_err("err: fail to copy to user frame info: %d\n", ret);
		goto wake_up_rsz_callback_task;
	}

	ret = copy_to_user(user_job->param, new_sub_task->frame_data.pq_param,
		MML_MAX_OUTPUTS * sizeof(struct mml_pq_param));
	if (unlikely(ret)) {
		mml_pq_err("err: fail to copy to user pq param: %d\n", ret);
		goto wake_up_rsz_callback_task;
	}

	atomic_dec_if_positive(&new_sub_task->queued);
	remove_sub_task(chan, new_sub_task->job_id);

	mml_pq_msg("%s end job_id[%d]\n", __func__, job->new_job_id);
	kfree(job);
	return 0;

wake_up_rsz_callback_task:
	atomic_dec_if_positive(&new_sub_task->queued);
	remove_sub_task(chan, new_sub_task->job_id);
	kfree(job);
	cancel_sub_task(new_sub_task);
	mml_pq_msg("%s end %d\n", __func__, ret);
	return ret;
}

static long mml_pq_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	mml_pq_msg("%s called %#x", __func__, cmd);
	mml_pq_msg("%s tile_init=%#x comp_config=%#x",
		__func__, MML_PQ_IOC_TILE_INIT, MML_PQ_IOC_COMP_CONFIG);
	switch (cmd) {
	case MML_PQ_IOC_TILE_INIT:
		return mml_pq_tile_init_ioctl(arg);
	case MML_PQ_IOC_COMP_CONFIG:
		return mml_pq_comp_config_ioctl(arg);
	case MML_PQ_IOC_AAL_READBACK:
		return mml_pq_aal_readback_ioctl(arg);
	case MML_PQ_IOC_HDR_READBACK:
		return mml_pq_hdr_readback_ioctl(arg);
	case MML_PQ_IOC_RSZ_CALLBACK:
		return mml_pq_rsz_callback_ioctl(arg);
	default:
		return -ENOTTY;
	}
}

static long mml_pq_compat_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	mml_pq_msg("%s called %#x", __func__, cmd);
	mml_pq_msg("%s tile_init=%#x comp_config=%#x",
		__func__, MML_PQ_IOC_TILE_INIT, MML_PQ_IOC_COMP_CONFIG);
	return -EFAULT;
}

const struct file_operations mml_pq_fops = {
	.unlocked_ioctl = mml_pq_ioctl,
	.compat_ioctl	= mml_pq_compat_ioctl,
};

static struct miscdevice mml_pq_dev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "mml_pq",
	.fops	= &mml_pq_fops,
};

int mml_pq_core_init(void)
{
	s32 ret;
	s32 buf_idx = 0;


	INIT_LIST_HEAD(&rb_buf_list);
	mutex_init(&rb_buf_list_mutex);
	mutex_init(&rb_buf_pool_mutex);

	pq_mbox = kzalloc(sizeof(*pq_mbox), GFP_KERNEL);
	if (unlikely(!pq_mbox))
		return -ENOMEM;
	init_pq_chan(&pq_mbox->tile_init_chan);
	init_pq_chan(&pq_mbox->comp_config_chan);
	init_pq_chan(&pq_mbox->aal_readback_chan);
	init_pq_chan(&pq_mbox->hdr_readback_chan);
	init_pq_chan(&pq_mbox->rsz_callback_chan);
	buffer_num = 0;

	for (buf_idx = 0; buf_idx < TOTAL_RB_BUF_NUM; buf_idx++)
		rb_buf_pool[buf_idx] = buf_idx*4096;

	ret = misc_register(&mml_pq_dev);
	mml_pq_log("%s result: %d", __func__, ret);
	if (unlikely(ret)) {
		kfree(pq_mbox);
		pq_mbox = NULL;
	}
	return ret;
}

void mml_pq_core_uninit(void)
{
	misc_deregister(&mml_pq_dev);
	kfree(pq_mbox);
	pq_mbox = NULL;
}

static s32 ut_case;
static bool ut_inited;
static struct list_head ut_mml_tasks;
static u32 ut_task_cnt;

static void ut_init()
{
	if (ut_inited)
		return;

	INIT_LIST_HEAD(&ut_mml_tasks);
	ut_inited = true;
}

static void destroy_ut_task(struct mml_task *task)
{
	mml_pq_msg("destroy mml_task for PQ UT [%lu.%lu]",
		task->end_time.tv_sec, task->end_time.tv_nsec);
	list_del(&task->entry);
	ut_task_cnt--;
	kfree(task);
}

static int run_ut_task_threaded(void *data)
{
	struct mml_task *task = data;
	s32 ret;

	pr_notice("start run mml_task for PQ UT [%lu.%lu]\n",
		task->end_time.tv_sec, task->end_time.tv_nsec);

	ret = mml_pq_set_tile_init(task);
	pr_notice("tile_init result: %d\n", ret);

	ret = mml_pq_get_tile_init_result(task, 100);
	pr_notice("get result result: %d\n", ret);

	mml_pq_put_tile_init_result(task);

	destroy_ut_task(task);
	return 0;
}

static void create_ut_task(const char *case_name)
{
	struct mml_task *task = mml_core_create_task();
	struct task_struct *thr;

	pr_notice("start create task for %s\n", case_name);
	INIT_LIST_HEAD(&task->entry);
	ktime_get_ts64(&task->end_time);
	list_add_tail(&task->entry, &ut_mml_tasks);
	ut_task_cnt++;

	pr_notice("created mml_task for PQ UT [%lu.%lu]\n",
		task->end_time.tv_sec, task->end_time.tv_nsec);
	thr = kthread_run(run_ut_task_threaded, task, case_name);
	if (IS_ERR(thr)) {
		pr_notice("create thread failed, thread:%s\n", case_name);
		destroy_ut_task(task);
	}
}

static s32 ut_set(const char *val, const struct kernel_param *kp)
{
	s32 result;

	ut_init();
	result = sscanf(val, "%d", &ut_case);
	if (result != 1) {
		pr_notice("invalid input: %s, result(%d)\n", val, result);
		return -EINVAL;
	}
	pr_notice("%s: case_id=%d\n", __func__, ut_case);

	switch (ut_case) {
	case 0:
		create_ut_task("basic_pq");
		break;
	default:
		pr_notice("invalid case_id: %d\n", ut_case);
		break;
	}

	pr_notice("%s END\n", __func__);
	return 0;
}

static s32 ut_get(char *buf, const struct kernel_param *kp)
{
	s32 length = 0;
	u32 i = 0;
	struct mml_task *task;

	ut_init();
	switch (ut_case) {
	case 0:
		length += snprintf(buf + length, PAGE_SIZE - length,
			"current UT task count: %d\n", ut_task_cnt);
		list_for_each_entry(task, &ut_mml_tasks, entry) {
			length += snprintf(buf + length, PAGE_SIZE - length,
				"  - [%d] task submit time: %lu.%lu\n", i,
				task->end_time.tv_sec, task->end_time.tv_nsec);
		}
		break;
	default:
		pr_notice("not support read for case_id: %d\n", ut_case);
		break;
	}
	buf[length] = '\0';

	return length;
}

static struct kernel_param_ops ut_param_ops = {
	.set = ut_set,
	.get = ut_get,
};
module_param_cb(pq_ut_case, &ut_param_ops, NULL, 0644);
MODULE_PARM_DESC(pq_ut_case, "mml pq core UT test case");

MODULE_DESCRIPTION("MTK MML PQ Core Driver");
MODULE_AUTHOR("Aaron Wu<ycw.wu@mediatek.com>");
MODULE_LICENSE("GPL");
