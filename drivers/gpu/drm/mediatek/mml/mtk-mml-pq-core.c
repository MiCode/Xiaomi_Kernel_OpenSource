/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/miscdevice.h>

#include "mtk-mml-pq.h"
#include "mtk-mml-core.h"
#include "mtk-mml-pq-core.h"

#undef pr_fmt
#define pr_fmt(fmt) "[mml_pq_core]" fmt

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
};

static struct mml_pq_mbox *pq_mbox;

static void init_pq_chan(struct mml_pq_chan *chan)
{
	init_waitqueue_head(&chan->msg_wq);
	atomic_set(&chan->msg_cnt, 0);
	INIT_LIST_HEAD(&chan->msg_list);
	mutex_init(&chan->msg_lock);

	INIT_LIST_HEAD(&chan->job_list);
	chan->job_idx = 1;
	mutex_init(&chan->job_lock);
}

static void queue_msg(struct mml_pq_chan *chan,
			struct mml_pq_sub_task *sub_task)
{
	mutex_lock(&chan->msg_lock);
	list_add_tail(&sub_task->mbox_list, &chan->msg_list);
	atomic_inc(&chan->msg_cnt);
	mutex_unlock(&chan->msg_lock);

	pr_notice("wake up channel message queue\n");
	wake_up(&chan->msg_wq);
}

static s32 dequeue_msg(struct mml_pq_chan *chan,
			struct mml_pq_sub_task **out_sub_task)
{
	struct mml_pq_sub_task *temp = NULL;

	mutex_lock(&chan->msg_lock);
	temp = list_first_entry_or_null(&chan->msg_list,
		typeof(*temp), mbox_list);
	if (temp)
		atomic_dec(&chan->msg_cnt);
	list_del(&temp->mbox_list);
	mutex_unlock(&chan->msg_lock);

	if (!temp)
		return -ENOENT;

	if (temp->result)
		return -EFAULT;

	pr_notice("dequeue from message queue and queue to job queue\n");
	mutex_lock(&chan->job_lock);
	list_add_tail(&temp->mbox_list, &chan->job_list);
	temp->job_id = chan->job_idx++;
	mutex_unlock(&chan->job_lock);
	*out_sub_task = temp;
	return 0;
}

static s32 find_sub_task(struct mml_pq_chan *chan, u64 job_id,
			struct mml_pq_sub_task **out_sub_task)
{
	struct mml_pq_sub_task *sub_task = NULL, *tmp = NULL;

	mutex_lock(&chan->job_lock);
	list_for_each_entry_safe(sub_task, tmp, &chan->job_list, mbox_list) {
		if (sub_task->job_id == job_id) {
			*out_sub_task = sub_task;
			pr_notice("find sub_task:%p id:%llx",
					sub_task, job_id);
			list_del(&sub_task->mbox_list);
			break;
		}
	}
	mutex_unlock(&chan->job_lock);

	return 0;
}

static s32 create_pq_task(struct mml_task *task)
{
	struct mml_pq_task *pq_task;

	if (likely(task->pq_task))
		return 0;

	pq_task = kmalloc(sizeof(*pq_task), GFP_KERNEL);

	if (unlikely(!pq_task)) {
		pr_notice("err: create pq_task failed\n");
		return -ENOMEM;
	}
	pr_notice("create pq_task\n");
	pq_task->task = task;
	atomic_set(&pq_task->ref_cnt, 1);
	mutex_init(&pq_task->lock);
	pq_task->tile_init.inited = false;
	task->pq_task = pq_task;
	return 0;
}

void destroy_pq_task(struct mml_task *task)
{
	struct mml_pq_task *pq_task = task->pq_task;

	if (unlikely(!pq_task))
		return;

	mutex_lock(&pq_task->lock);
	pq_task->task = NULL;
	task->pq_task = NULL;
	atomic_dec(&pq_task->ref_cnt);
	mutex_unlock(&pq_task->lock);
}

static void init_pq_sub_task(struct mml_pq_sub_task *sub_task)
{
	if (likely(sub_task->inited))
		return;

	pr_notice("init sub task\n");
	mutex_init(&sub_task->lock);
	sub_task->result = NULL;
	init_waitqueue_head(&sub_task->wq);
	INIT_LIST_HEAD(&sub_task->mbox_list);
	sub_task->job_cancelled = false;
	sub_task->job_id = 0;
	sub_task->inited = true;
}

static struct mml_pq_task *from_tile_init(struct mml_pq_sub_task *sub_task)
{
	return container_of(sub_task,
		struct mml_pq_task, tile_init);
}

static void dump_pq_param(struct mml_pq_param *pq_param)
{
	if (!pq_param)
		return;

	pr_notice("[param] enable=%u\n", pq_param->enable);
	pr_notice("[param] time_stamp=%u\n", pq_param->time_stamp);
	pr_notice("[param] scenario=%u\n", pq_param->scenario);
	pr_notice("[param] layer_id=%u disp_id=%d\n",
		pq_param->layer_id, pq_param->disp_id);
	pr_notice("[param] src_gamut=%u dst_gamut\n",
		pq_param->src_gamut, pq_param->dst_gamut);
	pr_notice("[param] video_mode=%u\n", pq_param->src_hdr_video_mode);
	pr_notice("[param] user=%u\n", pq_param->user_info);
}

static void dump_tile_result(void *data)
{
	u32 i;
	struct mml_pq_tile_init_result *result = (struct mml_pq_tile_init_result *)data;
	if (!result)
		return;

	pr_notice("[tile][param] count=%u\n", result->rsz_param_cnt);
	for (i = 0; i < result->rsz_param_cnt && i < MML_MAX_OUTPUTS; i++) {
		pr_notice("[tile][param][%u] coeff_step_x=%u\n", i, result->rsz_param[i].coeff_step_x);
		pr_notice("[tile][param][%u] coeff_step_y=%u\n", i, result->rsz_param[i].coeff_step_y);
		pr_notice("[tile][param][%u] precision_x=%u\n", i, result->rsz_param[i].precision_x);
		pr_notice("[tile][param][%u] precision_y=%u\n", i, result->rsz_param[i].precision_y);
		pr_notice("[tile][param][%u] crop_offset_x=%u\n", i, result->rsz_param[i].crop_offset_x);
		pr_notice("[tile][param][%u] crop_subpix_x=%u\n", i, result->rsz_param[i].crop_subpix_x);
		pr_notice("[tile][param][%u] crop_offset_y=%u\n", i, result->rsz_param[i].crop_offset_y);
		pr_notice("[tile][param][%u] crop_subpix_y=%u\n", i, result->rsz_param[i].crop_subpix_y);
		pr_notice("[tile][param][%u] hor_dir_scale=%u\n", i, result->rsz_param[i].hor_dir_scale);
		pr_notice("[tile][param][%u] hor_algorithm=%u\n", i, result->rsz_param[i].hor_algorithm);
		pr_notice("[tile][param][%u] ver_dir_scale=%u\n", i, result->rsz_param[i].ver_dir_scale);
		pr_notice("[tile][param][%u] ver_algorithm=%u\n", i, result->rsz_param[i].ver_algorithm);
		pr_notice("[tile][param][%u] vertical_first=%u\n", i, result->rsz_param[i].vertical_first);
		pr_notice("[tile][param][%u] ver_cubic_trunc=%u\n", i, result->rsz_param[i].ver_cubic_trunc);
	}
	pr_notice("[tile][reg] count=%u\n", result->rsz_reg_cnt);
}

int mml_pq_tile_init(struct mml_task *task)
{
	s32 ret;

	pr_notice("%s called\n", __func__);
	if (unlikely(!task))
		return -EINVAL;

	dump_pq_param(&task->pq_param[0]);
	ret = create_pq_task(task); 
	if (unlikely(ret))
		return ret;

	init_pq_sub_task(&task->pq_task->tile_init);
	queue_msg(&pq_mbox->tile_init_chan, &task->pq_task->tile_init);
	pr_notice("%s end\n", __func__);
	return 0;
}
EXPORT_SYMBOL_GPL(mml_pq_tile_init);

int mml_pq_get_tile_init_result(struct mml_task *task, u32 timeout_ms)
{
	struct mml_pq_sub_task *sub_task;
	s32 ret;

	pr_notice("%s called, %d\n", __func__, timeout_ms);
	if (unlikely(!task || !task->pq_task ||
		!task->pq_task->tile_init.inited))
		return -EINVAL;

	sub_task = &task->pq_task->tile_init;
	pr_notice("begin wait for tile init result\n");
	ret = wait_event_timeout(sub_task->wq, sub_task->result,
			msecs_to_jiffies(timeout_ms));
	pr_notice("wait for tile init result: %d\n", ret);
	if (ret)
		dump_tile_result(sub_task->result);

	pr_notice("%s end:%d\n", __func__, ret);
	if (ret)
		return 0;
	else
		return -EBUSY;
}
EXPORT_SYMBOL_GPL(mml_pq_get_tile_init_result);

static void handle_tile_init_result(struct mml_pq_chan *chan,
				struct mml_pq_tile_init_job *job)
{
	struct mml_pq_sub_task *sub_task = NULL;
	struct mml_pq_task *pq_task = NULL;
	struct mml_pq_tile_init_result *result;
	struct mml_pq_rsz_tile_init_param *rsz_param;
	struct mml_pq_reg *rsz_regs;
	s32 ret;

	pr_notice("%s called, %d\n", __func__, job->result_job_id);
	ret = find_sub_task(chan, job->result_job_id, &sub_task); 
	if (unlikely(ret)) {
		pr_notice("finish tile sub_task failed!: %d\n", ret);
		return;
	}

	if (unlikely(sub_task->result)) {
		pr_notice("err: sub_task has existed result!\n");
		goto wake_up_prev_tile_init_task;
	}

	result = kmalloc(sizeof(result), GFP_KERNEL);
	if (unlikely(!result)) {
		pr_notice("err: create result failed\n");
		goto wake_up_prev_tile_init_task;
	}

	ret = copy_from_user(result, job->result, sizeof(*result));
	if (unlikely(ret)) {
		pr_notice("copy job result failed!: %d\n", ret);
		goto free_tile_init_result;
	}

	if (unlikely(result->rsz_param_cnt > MML_MAX_OUTPUTS)) {
		pr_notice("invalid rsz param count!: %d\n",
			result->rsz_param_cnt);
		goto free_tile_init_result;
	}

	rsz_param = kmalloc(result->rsz_param_cnt*sizeof(*rsz_param),
			GFP_KERNEL);
	if (unlikely(!rsz_param)) {
		pr_notice("err: create rsz_param failed, size:%d\n",
			result->rsz_param_cnt);
		goto free_tile_init_result;
	}

	ret = copy_from_user(rsz_param, result->rsz_param,
		result->rsz_param_cnt*sizeof(*rsz_param));
	if (unlikely(ret)) {
		pr_notice("copy rsz param failed!: %d\n", ret);
		goto free_rsz_param;
	}

	rsz_regs = kmalloc(result->rsz_reg_cnt*sizeof(*rsz_regs),
			GFP_KERNEL);
	if (unlikely(!rsz_regs)) {
		pr_notice("err: create rsz_regs failed, size:%d\n",
			result->rsz_reg_cnt);
		goto free_rsz_param;
	}

	ret = copy_from_user(rsz_regs, result->rsz_regs,
		result->rsz_reg_cnt*sizeof(*rsz_regs));
	if (unlikely(ret)) {
		pr_notice("copy rsz config failed!: %d\n", ret);
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
	pr_notice("%s end, %d\n", __func__, ret);
}

static struct mml_pq_sub_task *wait_next_sub_task(struct mml_pq_chan *chan)
{
	struct mml_pq_sub_task *sub_task = NULL;
	s32 ret;

	pr_notice("%s called\n", __func__);
	for (;;) {
		pr_notice("start wait event!\n");
		wait_event(chan->msg_wq, (atomic_read(&chan->msg_cnt) > 0));
		pr_notice("finish wait event!\n");
		ret = dequeue_msg(chan, &sub_task);

		if (unlikely(ret)) {
			pr_notice("err: dequeue msg failed: %d\n", ret);
			continue;
		}

		pr_notice("after dequene msg!\n");
		break;
	}
	pr_notice("%s end %d task=%p\n", __func__, ret, sub_task);
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

	pr_notice("%s called\n", __func__);
	user_job = (struct mml_pq_tile_init_job *)data;
	if (unlikely(!user_job))
		return -EINVAL;

	job = kmalloc(sizeof(*job), GFP_KERNEL);
	if (unlikely(!job))
		return -ENOMEM;

	ret = copy_from_user(job, user_job, sizeof(*job));
	if (ret) {
		pr_notice("copy_from_user failed: %d\n", ret);
		return -EINVAL;
	}
	if (job->result_job_id)
		handle_tile_init_result(chan, job);

	new_sub_task = wait_next_sub_task(chan);
	new_pq_task = from_tile_init(new_sub_task);
	job->new_job_id = new_sub_task->job_id;
	mutex_lock(&new_pq_task->lock);
	if (unlikely(!atomic_read(&new_pq_task->ref_cnt))) {
		pr_notice("err: pq_task ref_cnt is 0\n");
		mutex_unlock(&new_pq_task->lock);
		return -ENOENT;
	}

	ret = copy_to_user(&user_job->new_job_id,
			&job->new_job_id, sizeof(u32));

	ret = copy_to_user(&user_job->info, &new_pq_task->task->config->info,
		sizeof(struct mml_frame_info));
	if (unlikely(ret)) {
		pr_notice("err: fail to copy to user frame info: %d\n", ret);
		goto wake_up_tile_init_task;
	}

	ret = copy_to_user(user_job->param, new_pq_task->task->pq_param,
		MML_MAX_OUTPUTS*sizeof(struct mml_pq_param));
	if (unlikely(ret)) {
		pr_notice("err: fail to copy to user pq param: %d\n", ret);
		goto wake_up_tile_init_task;
	}
	atomic_inc(&new_pq_task->ref_cnt);
	mutex_unlock(&new_pq_task->lock);
	pr_notice("%s end\n", __func__);
	return 0;

wake_up_tile_init_task:
	mutex_unlock(&new_pq_task->lock);
	mutex_lock(&new_sub_task->lock);
	new_sub_task->job_cancelled = true;
	wake_up(&new_sub_task->wq);
	mutex_unlock(&new_sub_task->lock);
	pr_notice("%s end %d\n", __func__, ret);
	return ret;
}

static long mml_pq_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	pr_notice("%s called %#x\n", __func__, cmd);
	pr_notice("%s MML_PQ_IOC_tile_init=%#x\n",
		__func__, MML_PQ_IOC_TILE_INIT);
	switch (cmd) {
	case MML_PQ_IOC_TILE_INIT:
		return mml_pq_tile_init_ioctl(arg);
	default:
		return -ENOTTY;
	}
}

static long mml_pq_compat_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	pr_notice("%s called %#x\n", __func__, cmd);
	pr_notice("%s MML_PQ_IOC_TILE_INIT=%#x\n",
		__func__, MML_PQ_IOC_TILE_INIT);
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
	ret = misc_register(&mml_pq_dev);
	pr_notice("%s result: %d\n", __func__, ret);
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
	pr_notice("destroy mml_task for PQ UT [%lu.%lu]\n",
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