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
	struct mutex task_link_lock;
	struct mml_pq_chan tile_init_chan;
	struct mml_pq_chan comp_config_chan;
};

static struct mml_pq_mbox *pq_mbox;

static void init_pq_chan(struct mml_pq_chan *chan)
{
	mml_pq_trace_ex_begin("%s", __func__);

	init_waitqueue_head(&chan->msg_wq);
	atomic_set(&chan->msg_cnt, 0);
	INIT_LIST_HEAD(&chan->msg_list);
	mutex_init(&chan->msg_lock);

	INIT_LIST_HEAD(&chan->job_list);
	chan->job_idx = 1;
	mutex_init(&chan->job_lock);

	mml_pq_trace_ex_end();
}

static void queue_msg(struct mml_pq_chan *chan,
			struct mml_pq_sub_task *sub_task)
{
	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s sub_task[%p] sub_task->mbox_list[%d] chan[%p] chan->msg_list[%p]\n",
		__func__, sub_task, &sub_task->mbox_list, chan, &chan->msg_list);

	mutex_lock(&chan->msg_lock);
	list_add_tail(&sub_task->mbox_list, &chan->msg_list);
	atomic_inc(&chan->msg_cnt);
	mutex_unlock(&chan->msg_lock);

	mml_pq_msg("%s wake up channel message queue\n", __func__);
	wake_up_interruptible(&chan->msg_wq);

	mml_pq_trace_ex_end();
}

static s32 dequeue_msg(struct mml_pq_chan *chan,
			struct mml_pq_sub_task **out_sub_task)
{
	struct mml_pq_sub_task *temp = NULL;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s chan[%p] chan->msg_list[%p]\n", __func__, chan, &chan->msg_list);

	mutex_lock(&chan->msg_lock);
	temp = list_first_entry_or_null(&chan->msg_list,
		typeof(*temp), mbox_list);
	if (temp)
		atomic_dec(&chan->msg_cnt);
	list_del(&temp->mbox_list);
	mutex_unlock(&chan->msg_lock);

	if (!temp) {
		mml_pq_err("%s temp is null\n", __func__);
		return -ENOENT;
	}

	if (temp->result) {
		mml_pq_log("%s result is exit\n", __func__);
		return -EFAULT;
	}

	mml_pq_msg("%s temp[%p] temp->result[%p] sub_task->job_id[%d] chan[%p] chan_job_id[%d]\n",
		__func__, temp, temp->result, temp->job_id, chan, chan->job_idx);
	mml_pq_msg("%s chan_job_id[%d] temp->mbox_list[%p] chan->job_list[%p]\n", __func__,
			chan->job_idx, &temp->mbox_list, &chan->job_list);

	mutex_lock(&chan->job_lock);
	list_add_tail(&temp->mbox_list, &chan->job_list);
	temp->job_id = chan->job_idx++;
	mutex_unlock(&chan->job_lock);
	*out_sub_task = temp;

	mml_pq_trace_ex_end();
	return 0;
}

static s32 find_sub_task(struct mml_pq_chan *chan, u64 job_id,
			struct mml_pq_sub_task **out_sub_task)
{
	struct mml_pq_sub_task *sub_task = NULL, *tmp = NULL;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s chan[%p] job_id[%d]\n", __func__, chan, job_id);

	mutex_lock(&chan->job_lock);
	list_for_each_entry_safe(sub_task, tmp, &chan->job_list, mbox_list) {
		mml_pq_msg("%s sub_task[%p] chan->job_list[%p] sub_job_id[%d]\n", __func__,
				sub_task, chan->job_list, sub_task->job_id);
		if (sub_task->job_id == job_id) {
			*out_sub_task = sub_task;
			mml_pq_msg("%s find sub_task:%p id:%llx",
					__func__, sub_task, job_id);
			list_del(&sub_task->mbox_list);
			break;
		}
	}
	mutex_unlock(&chan->job_lock);

	mml_pq_trace_ex_end();
	return 0;
}

static s32 create_pq_task(struct mml_task *task)
{
	struct mml_pq_task *pq_task;

	mml_pq_trace_ex_begin("%s", __func__);
	if (likely(task->pq_task))
		return 0;

	pq_task = kmalloc(sizeof(*pq_task), GFP_KERNEL);

	if (unlikely(!pq_task)) {
		mml_pq_err("%s create pq_task failed\n", __func__);
		return -ENOMEM;
	}
	mml_pq_msg("%s create pq_task\n", __func__);
	pq_task->task = task;
	atomic_set(&pq_task->ref_cnt, 1);
	mutex_init(&pq_task->lock);
	mutex_init(&pq_task->init_pq_sub_task_lock);
	pq_task->tile_init.inited = false;
	pq_task->comp_config.inited = false;
	task->pq_task = pq_task;

	mml_pq_trace_ex_end();
	return 0;
}

void destroy_pq_task(struct mml_task *task)
{
	struct mml_pq_task *pq_task = task->pq_task;

	if (unlikely(!pq_task))
		return;

	mml_pq_trace_ex_begin("%s", __func__);
	mutex_lock(&pq_task->lock);
	pq_task->task = NULL;
	task->pq_task = NULL;
	atomic_dec(&pq_task->ref_cnt);
	mutex_unlock(&pq_task->lock);

	mml_pq_trace_ex_end();
}

static bool init_pq_sub_task(struct mml_pq_sub_task *sub_task)
{
	if (likely(sub_task->inited))
		return false;

	mml_pq_trace_ex_begin("%s", __func__);

	mml_pq_msg("%s\n", __func__);
	mutex_init(&sub_task->lock);
	sub_task->result = NULL;
	init_waitqueue_head(&sub_task->wq);
	INIT_LIST_HEAD(&sub_task->mbox_list);
	sub_task->job_cancelled = false;
	sub_task->job_id = 0;
	sub_task->inited = true;

	mml_pq_trace_ex_end();
	return true;
}

static struct mml_pq_task *from_tile_init(struct mml_pq_sub_task *sub_task)
{
	return container_of(sub_task,
		struct mml_pq_task, tile_init);
}

static struct mml_pq_task *from_comp_config(struct mml_pq_sub_task *sub_task)
{
	return container_of(sub_task,
		struct mml_pq_task, comp_config);
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

static void dump_tile_result(void *data)
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
		mml_pq_dump("[tile][%u] vertical_first=%u\n", i,
				result->rsz_param[i].vertical_first);
		mml_pq_dump("[tile][%u] ver_cubic_trunc=%u\n", i,
				result->rsz_param[i].ver_cubic_trunc);
	}
	mml_pq_dump("[tile] count=%u", result->rsz_reg_cnt);
}

int mml_pq_tile_init(struct mml_task *task)
{
	s32 ret;
	bool inited = false;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s job_id[%d] called", __func__, task->job.jobid);
	if (unlikely(!task))
		return -EINVAL;

	dump_pq_param(&task->pq_param[0]);
	ret = create_pq_task(task);
	if (unlikely(ret))
		return ret;

	mutex_lock(&task->pq_task->init_pq_sub_task_lock);
	inited = init_pq_sub_task(&task->pq_task->tile_init);
	mutex_unlock(&task->pq_task->init_pq_sub_task_lock);

	if (inited)
		queue_msg(&pq_mbox->tile_init_chan, &task->pq_task->tile_init);

	mml_pq_msg("%s job_id[%d] end\n", __func__, task->job.jobid);
	mml_pq_trace_ex_end();

	return 0;
}

int mml_pq_get_tile_init_result(struct mml_task *task, u32 timeout_ms)
{
	struct mml_pq_sub_task *sub_task;
	s32 ret;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s job_id[%d] called, %d", __func__, task->job.jobid, timeout_ms);
	if (unlikely(!task || !task->pq_task ||
		!task->pq_task->tile_init.inited))
		return -EINVAL;

	sub_task = &task->pq_task->tile_init;
	mml_pq_msg("begin wait for tile init result");
	ret = wait_event_timeout(sub_task->wq, sub_task->result,
			msecs_to_jiffies(timeout_ms));
	mml_pq_msg("wait for tile init result: %d", ret);
	if (ret)
		dump_tile_result(sub_task->result);

	mml_pq_msg("%s job_id[%d] end:%d", __func__, task->job.jobid, ret);
	mml_pq_trace_ex_end();
	if (ret)
		return 0;
	else
		return -EBUSY;
}

static void dump_comp_config(void *data)
{
	u32 i;
	struct mml_pq_comp_config_result *result = (struct mml_pq_comp_config_result *)data;

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

int mml_pq_comp_config(struct mml_task *task)
{
	s32 ret;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s called", __func__);
	if (unlikely(!task))
		return -EINVAL;

	dump_pq_param(&task->pq_param[0]);
	if (!task->pq_task) {
		ret = create_pq_task(task);
		if (unlikely(ret))
			return ret;
	}

	if (init_pq_sub_task(&task->pq_task->comp_config))
		queue_msg(&pq_mbox->comp_config_chan, &task->pq_task->comp_config);
	mml_pq_msg("%s end", __func__);

	mml_pq_trace_ex_end();
	return 0;
}

int mml_pq_get_comp_config_result(struct mml_task *task, u32 timeout_ms)
{
	struct mml_pq_sub_task *sub_task;
	s32 ret;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s called, %d", __func__, timeout_ms);
	if (unlikely(!task || !task->pq_task ||
		!task->pq_task->comp_config.inited))
		return -EINVAL;

	sub_task = &task->pq_task->comp_config;
	mml_pq_msg("begin wait for comp config result");

	ret = wait_event_timeout(sub_task->wq, sub_task->result,
			msecs_to_jiffies(timeout_ms));
	mml_pq_msg("wait for tile comp config: %d", ret);
	if (ret)
		dump_comp_config(sub_task->result);

	mml_pq_msg("%s end:%d", __func__, ret);

	mml_pq_trace_ex_end();

	if (ret)
		return 0;
	else
		return -EBUSY;
}

static void handle_tile_init_result(struct mml_pq_chan *chan,
				struct mml_pq_tile_init_job *job)
{
	struct mml_pq_sub_task *sub_task = NULL;
	struct mml_pq_task *pq_task = NULL;
	struct mml_pq_tile_init_result *result;
	struct mml_pq_rsz_tile_init_param *rsz_param;
	struct mml_pq_reg *rsz_regs;
	s32 ret;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_log("%s called, %d", __func__, job->result_job_id);
	ret = find_sub_task(chan, job->result_job_id, &sub_task);
	if (unlikely(ret)) {
		mml_pq_err("finish tile sub_task failed!: %d", ret);
		return;
	}

	if (unlikely(sub_task->result)) {
		mml_pq_err("err: sub_task has existed result!");
		goto wake_up_prev_tile_init_task;
	}

	result = kmalloc(sizeof(result), GFP_KERNEL);
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

	rsz_param = kmalloc(result->rsz_param_cnt*sizeof(*rsz_param),
			GFP_KERNEL);
	if (unlikely(!rsz_param)) {
		mml_pq_err("err: create rsz_param failed, size:%d",
			result->rsz_param_cnt);
		goto free_tile_init_result;
	}

	ret = copy_from_user(rsz_param, result->rsz_param,
		result->rsz_param_cnt*sizeof(*rsz_param));
	if (unlikely(ret)) {
		mml_pq_err("copy rsz param failed!: %d", ret);
		goto free_rsz_param;
	}

	rsz_regs = kmalloc(result->rsz_reg_cnt*sizeof(*rsz_regs),
			GFP_KERNEL);
	if (unlikely(!rsz_regs)) {
		mml_pq_err("err: create rsz_regs failed, size:%d",
			result->rsz_reg_cnt);
		goto free_rsz_param;
	}

	ret = copy_from_user(rsz_regs, result->rsz_regs,
		result->rsz_reg_cnt*sizeof(*rsz_regs));
	if (unlikely(ret)) {
		mml_pq_err("copy rsz config failed!: %d", ret);
		goto free_rsz_regs;
	}

	result->rsz_param = rsz_param;
	result->rsz_regs = rsz_regs;
	mutex_lock(&sub_task->lock);
	sub_task->result = result;
	mutex_unlock(&sub_task->lock);
	goto wake_up_prev_tile_init_task;
free_rsz_regs:
	kfree(rsz_regs);
free_rsz_param:
	kfree(rsz_param);
free_tile_init_result:
	kfree(result);
wake_up_prev_tile_init_task:
	mutex_lock(&sub_task->lock);
	wake_up(&sub_task->wq);
	mutex_unlock(&sub_task->lock);

	/* decrease pq_task ref_cnt after finish the result */
	pq_task = from_tile_init(sub_task);
	mutex_lock(&pq_task->lock);
	atomic_dec(&pq_task->ref_cnt);
	mutex_unlock(&pq_task->lock);
	mml_pq_msg("%s end, %d", __func__, ret);
	mml_pq_trace_ex_end();
}

static struct mml_pq_sub_task *wait_next_sub_task(struct mml_pq_chan *chan)
{
	struct mml_pq_sub_task *sub_task = NULL;
	int wait_result = 0;
	s32 ret;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s called", __func__);
	for (;;) {
		mml_pq_msg("start wait event!");
		wait_result = wait_event_interruptible(chan->msg_wq,
			(atomic_read(&chan->msg_cnt) > 0));

		if (wait_result) {
			mml_pq_log("%s wakeup wait_result[%d], msg_cnt[%d]",
				__func__, wait_result, atomic_read(&chan->msg_cnt));
			return NULL;
		}

		mml_pq_msg("%s finish wait event! wait_result[%d], msg_cnt[%d]",
			__func__, wait_result, atomic_read(&chan->msg_cnt));

		ret = dequeue_msg(chan, &sub_task);

		if (unlikely(ret)) {
			mml_pq_err("err: dequeue msg failed: %d", ret);
			continue;
		}

		mml_pq_msg("after dequene msg!");
		break;
	}
	mml_pq_msg("%s end %d task=%p", __func__, ret, sub_task);
	mml_pq_trace_ex_end();
	return sub_task;
}

static int mml_pq_tile_init_ioctl(unsigned long data)
{
	struct mml_pq_chan *chan = &pq_mbox->tile_init_chan;
	struct mml_pq_sub_task *new_sub_task = NULL;
	struct mml_pq_task *new_pq_task = NULL;
	struct mml_pq_tile_init_job *job;
	struct mml_pq_tile_init_job *user_job;
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
	if (ret) {
		mml_pq_err("copy_from_user failed: %d", ret);
		return -EINVAL;
	}
	mml_pq_msg("%s result_job_id[%d]", __func__, job->result_job_id);
	if (job->result_job_id)
		handle_tile_init_result(chan, job);

	new_sub_task = wait_next_sub_task(chan);

	if (NULL == new_sub_task) {
		kfree(job);
		mml_pq_log("%s Get sub task failed", __func__);
		return -ERESTARTSYS;
	}

	new_pq_task = from_tile_init(new_sub_task);
	job->new_job_id = new_sub_task->job_id;
	mutex_lock(&new_pq_task->lock);
	if (unlikely(!atomic_read(&new_pq_task->ref_cnt))) {
		mml_pq_err("err: pq_task ref_cnt is 0");
		mutex_unlock(&new_pq_task->lock);
		return -ENOENT;
	}

	ret = copy_to_user(&user_job->new_job_id,
			&job->new_job_id, sizeof(u32));

	ret = copy_to_user(&user_job->info, &new_pq_task->task->config->info,
		sizeof(struct mml_frame_info));
	if (unlikely(ret)) {
		mml_pq_err("err: fail to copy to user frame info: %d", ret);
		goto wake_up_tile_init_task;
	}

	ret = copy_to_user(user_job->dst, new_pq_task->task->config->frame_out,
		MML_MAX_OUTPUTS*sizeof(struct mml_frame_size));
	if (unlikely(ret)) {
		mml_pq_err("err: fail to copy to user frame out: %d", ret);
		goto wake_up_tile_init_task;
	}

	ret = copy_to_user(user_job->param, new_pq_task->task->pq_param,
		MML_MAX_OUTPUTS*sizeof(struct mml_pq_param));
	if (unlikely(ret)) {
		mml_pq_err("err: fail to copy to user pq param: %d", ret);
		goto wake_up_tile_init_task;
	}
	atomic_inc(&new_pq_task->ref_cnt);
	mutex_unlock(&new_pq_task->lock);
	mml_pq_msg("%s end", __func__);
	mml_pq_trace_ex_end();
	return 0;

wake_up_tile_init_task:
	mutex_unlock(&new_pq_task->lock);
	mutex_lock(&new_sub_task->lock);
	new_sub_task->job_cancelled = true;
	wake_up(&new_sub_task->wq);
	mutex_unlock(&new_sub_task->lock);
	mml_pq_msg("%s end %d", __func__, ret);
	mml_pq_trace_ex_end();
	return ret;
}

static void handle_comp_config_result(struct mml_pq_chan *chan,
				struct mml_pq_comp_config_job *job)
{
	struct mml_pq_sub_task *sub_task = NULL;
	struct mml_pq_task *pq_task = NULL;
	struct mml_pq_comp_config_result *result = NULL;
	struct mml_pq_aal_config_param *aal_param = NULL;
	struct mml_pq_reg *aal_regs = NULL;
	u32 *aal_curve = NULL;
	s32 ret = 0;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s called, %d", __func__, job->result_job_id);
	ret = find_sub_task(chan, job->result_job_id, &sub_task);
	if (unlikely(ret)) {
		mml_pq_err("finish tile sub_task failed!: %d", ret);
		return;
	}
	mml_pq_msg("%s end %d task=%p sub_task->id[%d]", __func__, ret,
			sub_task, sub_task->job_id);
	if (unlikely(sub_task->result)) {
		mml_pq_err("err: sub_task has existed result!");
		goto wake_up_prev_comp_config_task;
	}

	result = kmalloc(sizeof(result), GFP_KERNEL);
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
		mml_pq_err("invalid rsz param count!: %d",
			result->param_cnt);
		goto free_comp_config_result;
	}

	aal_param = kmalloc(result->param_cnt*sizeof(*aal_param),
			GFP_KERNEL);
	if (unlikely(!aal_param)) {
		mml_pq_err("err: create aal_param failed, size:%d",
			result->param_cnt);
		goto free_comp_config_result;
	}

	ret = copy_from_user(aal_param, result->aal_param,
		result->param_cnt*sizeof(*aal_param));
	if (unlikely(ret)) {
		mml_pq_err("copy aal param failed!: %d", ret);
		goto free_aal_param;
	}

	aal_regs = kmalloc(result->aal_reg_cnt*sizeof(*aal_regs),
			GFP_KERNEL);
	if (unlikely(!aal_regs)) {
		mml_pq_err("err: create aal_regs failed, size:%d",
			result->aal_reg_cnt);
		goto free_aal_param;
	}

	ret = copy_from_user(aal_regs, result->aal_regs,
		result->aal_reg_cnt*sizeof(*aal_regs));
	if (unlikely(ret)) {
		mml_pq_err("copy aal config failed!: %d", ret);
		goto free_aal_regs;
	}

	aal_curve = kmalloc(AAL_CURVE_NUM*sizeof(u32),
			GFP_KERNEL);
	if (unlikely(!aal_curve)) {
		mml_pq_err("err: create aal_curve failed, size:%d",
			AAL_CURVE_NUM);
		goto free_aal_regs;
	}

	ret = copy_from_user(aal_curve, result->aal_curve,
		AAL_CURVE_NUM*sizeof(u32));
	if (unlikely(ret)) {
		mml_pq_err("copy aal curve failed!: %d", ret);
		goto free_aal_curve;
	}

	result->aal_param = aal_param;
	result->aal_regs = aal_regs;
	result->aal_curve = aal_curve;
	mutex_lock(&sub_task->lock);
	sub_task->result = result;
	mutex_unlock(&sub_task->lock);
	mml_pq_msg("%s result end, result_id[%d] sub_task[%p]",
		__func__, job->result_job_id, sub_task);
	goto wake_up_prev_comp_config_task;
free_aal_curve:
	kfree(aal_curve);
free_aal_regs:
	kfree(aal_regs);
free_aal_param:
	kfree(aal_param);
free_comp_config_result:
	kfree(result);
wake_up_prev_comp_config_task:
	mutex_lock(&sub_task->lock);
	wake_up(&sub_task->wq);
	mutex_unlock(&sub_task->lock);

	/* decrease pq_task ref_cnt after finish the result */
	pq_task = from_comp_config(sub_task);
	mutex_lock(&pq_task->lock);
	atomic_dec(&pq_task->ref_cnt);
	mutex_unlock(&pq_task->lock);
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
	s32 ret = 0;

	mml_pq_trace_ex_begin("%s", __func__);
	mml_pq_msg("%s called", __func__);
	user_job = (struct mml_pq_comp_config_job *)data;
	if (unlikely(!user_job))
		return -EINVAL;

	job = kmalloc(sizeof(*job), GFP_KERNEL);
	if (unlikely(!job))
		return -ENOMEM;

	ret = copy_from_user(job, user_job, sizeof(*job));
	if (ret) {
		mml_pq_err("copy_from_user failed: %d", ret);
		return -EINVAL;
	}
	mml_pq_msg("%s new_job_id[%d] result_job_id[%d]", __func__, job->new_job_id,
		job->result_job_id);

	if (job->result_job_id)
		handle_comp_config_result(chan, job);

	new_sub_task = wait_next_sub_task(chan);

	if (NULL == new_sub_task) {
		kfree(job);
		mml_pq_log("%s Get sub task failed", __func__);
		return -ERESTARTSYS;
	}

	new_pq_task = from_comp_config(new_sub_task);
	job->new_job_id = new_sub_task->job_id;
	mutex_lock(&new_pq_task->lock);
	if (unlikely(!atomic_read(&new_pq_task->ref_cnt))) {
		mml_pq_err("err: pq_task ref_cnt is 0");
		mutex_unlock(&new_pq_task->lock);
		return -ENOENT;
	}

	ret = copy_to_user(&user_job->new_job_id,
			&job->new_job_id, sizeof(u32));

	ret = copy_to_user(&user_job->info, &new_pq_task->task->config->info,
		sizeof(struct mml_frame_info));
	if (unlikely(ret)) {
		mml_pq_err("err: fail to copy to user frame info: %d", ret);
		goto wake_up_comp_config_task;
	}

	ret = copy_to_user(user_job->param, new_pq_task->task->pq_param,
		MML_MAX_OUTPUTS*sizeof(struct mml_pq_param));
	if (unlikely(ret)) {
		mml_pq_err("err: fail to copy to user pq param: %d", ret);
		goto wake_up_comp_config_task;
	}
	atomic_inc(&new_pq_task->ref_cnt);
	mutex_unlock(&new_pq_task->lock);
	mml_pq_msg("%s end", __func__);
	mml_pq_trace_ex_end();
	return 0;

wake_up_comp_config_task:
	mutex_unlock(&new_pq_task->lock);
	mutex_lock(&new_sub_task->lock);
	new_sub_task->job_cancelled = true;
	wake_up(&new_sub_task->wq);
	mutex_unlock(&new_sub_task->lock);
	mml_pq_msg("%s end %d", __func__, ret);
	mml_pq_trace_ex_end();
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

void mml_pq_core_init(void)
{
	s32 ret;
	pq_mbox = kmalloc(sizeof(*pq_mbox), GFP_KERNEL);
	mutex_init(&pq_mbox->task_link_lock);
	init_pq_chan(&pq_mbox->tile_init_chan);
	init_pq_chan(&pq_mbox->comp_config_chan);
	ret = misc_register(&mml_pq_dev);
	mml_pq_log("%s result: %d", __func__, ret);
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

	ret = mml_pq_tile_init(task);
	pr_notice("tile_init result: %d\n", ret);

	ret = mml_pq_get_tile_init_result(task, 100);
	pr_notice("get result result: %d\n", ret);

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
