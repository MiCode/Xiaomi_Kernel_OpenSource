// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/wait.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/string.h>
#include <linux/average.h>
#include <linux/topology.h>
#include <linux/vmalloc.h>
#include <linux/sched/clock.h>
#include <linux/sched.h>
#include <asm/div64.h>
#include <mt-plat/fpsgo_common.h>
#include "../fbt/include/fbt_cpu.h"
#include "../fbt/include/xgf.h"
#include "fpsgo_base.h"
#include "fpsgo_sysfs.h"
#include "fstb.h"
#include "fstb_usedext.h"
#include "fpsgo_usedext.h"

int (*fpsgo2msync_hint_frameinfo_fp)(unsigned int render_tid, unsigned int reader_bufID,
		unsigned int target_fps, unsigned long q2q_time, unsigned long q2q_time2);
EXPORT_SYMBOL(fpsgo2msync_hint_frameinfo_fp);

#if IS_ENABLED(CONFIG_MTK_GPU_SUPPORT)
#include "ged_kpi.h"
#endif

#define mtk_fstb_dprintk_always(fmt, args...) \
	pr_debug("[FSTB]" fmt, ##args)

#define mtk_fstb_dprintk(fmt, args...) \
	do { \
		if (fstb_fps_klog_on == 1) \
			pr_debug("[FSTB]" fmt, ##args); \
	} while (0)

static struct kobject *fstb_kobj;
static int max_fps_limit = DEFAULT_DFPS;
static int dfps_ceiling = DEFAULT_DFPS;
static int min_fps_limit = CFG_MIN_FPS_LIMIT;
static int fps_error_threshold = 10;
static int QUANTILE = 50;
static int margin_mode = 2;
static int margin_mode_gpu = 2;
static int margin_mode_dbnc_a = 9;
static int margin_mode_dbnc_b = 1;
static int margin_mode_gpu_dbnc_a = 9;
static int margin_mode_gpu_dbnc_b = 1;
static int RESET_TOLERENCE = DEFAULT_RESET_TOLERENCE;
static int JUMP_CHECK_NUM = DEFAULT_JUMP_CHECK_NUM;
static int JUMP_CHECK_Q_PCT = DEFAULT_JUMP_CHECK_Q_PCT;
static int adopt_low_fps = 1;
static int condition_get_fps;
static int condition_fstb_active;
static long long FRAME_TIME_WINDOW_SIZE_US = USEC_PER_SEC;

DECLARE_WAIT_QUEUE_HEAD(queue);
DECLARE_WAIT_QUEUE_HEAD(active_queue);

static void fstb_fps_stats(struct work_struct *work);
static DECLARE_WORK(fps_stats_work,
		(void *) fstb_fps_stats);
static HLIST_HEAD(fstb_frame_infos);
static HLIST_HEAD(fstb_render_target_fps);

static struct hrtimer hrt;
static struct workqueue_struct *wq;

static struct fps_level fps_global_level;

static int fstb_fps_klog_on;
static int fstb_enable, fstb_active, fstb_active_dbncd, fstb_idle_cnt;
static int fstb_self_ctrl_fps_enable;
static long long last_update_ts;
static int fps_bypass_max = 150;
static int fps_bypass_min = 50;
static int total_fstb_policy_cmd_num;

static void reset_fps_level(void);
static int set_soft_fps_level(struct fps_level level);

static DEFINE_MUTEX(fstb_lock);
static DEFINE_MUTEX(fstb_fps_active_time);

static struct rb_root video_pid_tree;
static DEFINE_MUTEX(fstb_video_pid_tree_lock);

static struct rb_root fstb_policy_cmd_tree;
static DEFINE_MUTEX(fstb_policy_cmd_lock);

void (*gbe_fstb2gbe_poll_fp)(struct hlist_head *list);

static void enable_fstb_timer(void)
{
	ktime_t ktime;

	ktime = ktime_set(0,
			FRAME_TIME_WINDOW_SIZE_US * 1000);
	hrtimer_start(&hrt, ktime, HRTIMER_MODE_REL);
}

static void disable_fstb_timer(void)
{
	hrtimer_cancel(&hrt);
}

static enum hrtimer_restart mt_fstb(struct hrtimer *timer)
{
	if (wq)
		queue_work(wq, &fps_stats_work);

	return HRTIMER_NORESTART;
}

int is_fstb_enable(void)
{
	return fstb_enable;
}

int is_fstb_active(long long time_diff)
{
	int active = 0;
	ktime_t cur_time;
	long long cur_time_us;

	cur_time = ktime_get();
	cur_time_us = ktime_to_us(cur_time);

	mutex_lock(&fstb_fps_active_time);

	if (cur_time_us - last_update_ts < time_diff)
		active = 1;

	mutex_unlock(&fstb_fps_active_time);

	return active;
}
EXPORT_SYMBOL(is_fstb_active);

struct k_list {
	struct list_head queue_list;
	int fpsgo2pwr_pid;
	int fpsgo2pwr_fps;
};
static LIST_HEAD(head);
static DEFINE_MUTEX(fpsgo2pwr_lock);
static DECLARE_WAIT_QUEUE_HEAD(pwr_queue);
static void fstb_sentcmd(int pid, int fps)
{
	static struct k_list *node;

	mutex_lock(&fpsgo2pwr_lock);
	node = kmalloc(sizeof(*node), GFP_KERNEL);
	if (node == NULL)
		goto out;
	node->fpsgo2pwr_pid = pid;
	node->fpsgo2pwr_fps = fps;
	list_add_tail(&node->queue_list, &head);
	condition_get_fps = 1;
out:
	mutex_unlock(&fpsgo2pwr_lock);
	wake_up_interruptible(&pwr_queue);
}

void fpsgo_ctrl2fstb_get_fps(int *pid, int *fps)
{
	static struct k_list *node;

	wait_event_interruptible(pwr_queue, condition_get_fps);
	mutex_lock(&fpsgo2pwr_lock);
	if (!list_empty(&head)) {
		node = list_first_entry(&head, struct k_list, queue_list);
		*pid = node->fpsgo2pwr_pid;
		*fps = node->fpsgo2pwr_fps;
		list_del(&node->queue_list);
		kfree(node);
	}
	if (list_empty(&head))
		condition_get_fps = 0;
	mutex_unlock(&fpsgo2pwr_lock);
}

int fpsgo_ctrl2fstb_wait_fstb_active(void)
{
	wait_event_interruptible(active_queue, condition_fstb_active);
	mutex_lock(&fstb_lock);
	condition_fstb_active = 0;
	mutex_unlock(&fstb_lock);
	return 0;
}

int fpsgo_ctrl2fstb_switch_fstb(int enable)
{
	struct FSTB_FRAME_INFO *iter;
	struct hlist_node *t;

	mutex_lock(&fstb_lock);
	if (fstb_enable == enable) {
		mutex_unlock(&fstb_lock);
		return 0;
	}

	fstb_enable = enable;
	fpsgo_systrace_c_fstb(-200, 0, fstb_enable, "fstb_enable");

	mtk_fstb_dprintk_always("%s %d\n", __func__, fstb_enable);
	if (!fstb_enable) {
		hlist_for_each_entry_safe(iter, t,
				&fstb_frame_infos, hlist) {
			hlist_del(&iter->hlist);
			vfree(iter);
		}
	} else {
		if (wq) {
			struct work_struct *psWork =
				kmalloc(sizeof(struct work_struct), GFP_ATOMIC);

			if (psWork) {
				INIT_WORK(psWork, fstb_fps_stats);
				queue_work(wq, psWork);
			}
		}
	}

	mutex_unlock(&fstb_lock);

	return 0;
}

static void switch_fstb_active(void)
{
	fpsgo_systrace_c_fstb(-200, 0,
			fstb_active, "fstb_active");
	fpsgo_systrace_c_fstb(-200, 0,
			fstb_active_dbncd, "fstb_active_dbncd");

	mtk_fstb_dprintk_always("%s %d %d\n",
			__func__, fstb_active, fstb_active_dbncd);
	enable_fstb_timer();
}

int switch_fps_range(int nr_level, struct fps_level *level)
{
	struct fps_level global_level;

	if (nr_level != 1)
		return 1;

	global_level.start = level->start;
	global_level.end = level->end;

	if (!set_soft_fps_level(global_level))
		return 0;
	else
		return 1;
}
static struct FSTB_FRAME_INFO *add_new_frame_info(int pid, unsigned long long bufID,
	int hwui_flag)
{
	struct task_struct *tsk = NULL, *gtsk = NULL;
		struct FSTB_FRAME_INFO *new_frame_info;

	new_frame_info = vmalloc(sizeof(*new_frame_info));
	if (new_frame_info == NULL)
		goto out;

	new_frame_info->pid = pid;
	new_frame_info->target_fps = max_fps_limit;
	new_frame_info->target_fps_v2 = max_fps_limit;
	new_frame_info->target_fps_margin_v2 = 0;
	new_frame_info->target_fps_margin = 0;
	new_frame_info->target_fps_margin_gpu = 0;
	new_frame_info->target_fps_margin2 = 0;
	new_frame_info->target_fps_margin_dbnc_a = margin_mode_dbnc_a;
	new_frame_info->target_fps_margin_dbnc_b = margin_mode_dbnc_b;
	new_frame_info->target_fps_margin_gpu_dbnc_a = margin_mode_gpu_dbnc_a;
	new_frame_info->target_fps_margin_gpu_dbnc_b = margin_mode_gpu_dbnc_b;
	new_frame_info->target_fps_diff = 0;
	new_frame_info->target_fps_notifying = 0;
	new_frame_info->queue_fps = max_fps_limit;
	new_frame_info->bufid = bufID;
	new_frame_info->queue_time_begin = 0;
	new_frame_info->queue_time_end = 0;
	new_frame_info->weighted_cpu_time_begin = 0;
	new_frame_info->weighted_cpu_time_end = 0;
	new_frame_info->weighted_gpu_time_begin = 0;
	new_frame_info->weighted_gpu_time_end = 0;
	new_frame_info->quantile_cpu_time = -1;
	new_frame_info->quantile_gpu_time = -1;
	new_frame_info->fps_raise_flag = 0;
	new_frame_info->vote_i = 0;
	new_frame_info->render_idle_cnt = 0;
	new_frame_info->hwui_flag = hwui_flag;
	new_frame_info->sbe_state = 0;
	new_frame_info->self_ctrl_fps_enable = 0;
	new_frame_info->tfb_enable = 0;
	new_frame_info->notify_target_fps = 0;

	rcu_read_lock();
	tsk = find_task_by_vpid(pid);
	if (tsk) {
		get_task_struct(tsk);
		gtsk = find_task_by_vpid(tsk->tgid);
		put_task_struct(tsk);
		if (gtsk)
			get_task_struct(gtsk);
	}
	rcu_read_unlock();

	if (gtsk) {
		strncpy(new_frame_info->proc_name, gtsk->comm, 16);
		new_frame_info->proc_name[15] = '\0';
		new_frame_info->proc_id = gtsk->pid;
		put_task_struct(gtsk);
	} else {
		new_frame_info->proc_name[0] = '\0';
		new_frame_info->proc_id = 0;
	}

out:
	return new_frame_info;
}

static void fstb_update_policy_cmd(struct FSTB_FRAME_INFO *iter,
	struct FSTB_POLICY_CMD *policy, unsigned long long ts)
{
	if (policy) {
		iter->self_ctrl_fps_enable = policy->self_ctrl_fps_enable;
		iter->tfb_enable = policy->tfb_enable;
		iter->notify_target_fps = policy->notify_target_fps;
		if (policy->self_ctrl_fps_enable || policy->tfb_enable ||
			policy->notify_target_fps)
			policy->ts = ts;
	} else {
		iter->self_ctrl_fps_enable = fstb_self_ctrl_fps_enable ? 1 : 0;
		iter->tfb_enable = 0;
		iter->notify_target_fps = 0;
	}
}

static void fstb_delete_policy_cmd(struct FSTB_POLICY_CMD *iter)
{
	unsigned long long min_ts = ULLONG_MAX;
	struct FSTB_POLICY_CMD *tmp_iter, *min_iter;
	struct rb_node *rbn;

	if (iter) {
		if (!iter->self_ctrl_fps_enable &&
			!iter->tfb_enable && !iter->notify_target_fps) {
			min_iter = iter;
			goto delete;
		} else
			return;
	}

	if (RB_EMPTY_ROOT(&fstb_policy_cmd_tree))
		return;

	rbn = rb_first(&fstb_policy_cmd_tree);
	while (rbn) {
		tmp_iter = rb_entry(rbn, struct FSTB_POLICY_CMD, rb_node);
		if (tmp_iter->ts < min_ts) {
			min_ts = tmp_iter->ts;
			min_iter = tmp_iter;
		}
		rbn = rb_next(rbn);
	}

	if (!min_iter)
		return;

delete:
	rb_erase(&min_iter->rb_node, &fstb_policy_cmd_tree);
	kfree(iter);
	total_fstb_policy_cmd_num--;
}

static struct FSTB_POLICY_CMD *fstb_get_policy_cmd(int tgid,
	unsigned long long ts, int force)
{
	struct rb_node **p = &fstb_policy_cmd_tree.rb_node;
	struct rb_node *parent;
	struct FSTB_POLICY_CMD *iter;

	while (*p) {
		parent = *p;
		iter = rb_entry(parent, struct FSTB_POLICY_CMD, rb_node);

		if (tgid < iter->tgid)
			p = &(*p)->rb_left;
		else if (tgid > iter->tgid)
			p = &(*p)->rb_right;
		else
			return iter;
	}

	if (!force)
		return NULL;

	if (total_fstb_policy_cmd_num > MAX_FSTB_POLICY_CMD_NUM)
		fstb_delete_policy_cmd(NULL);

	iter = kzalloc(sizeof(*iter), GFP_KERNEL);
	if (!iter)
		return NULL;

	iter->tgid = tgid;
	iter->self_ctrl_fps_enable = 0;
	iter->tfb_enable = 0;
	iter->notify_target_fps = 0;
	iter->ts = ts;

	rb_link_node(&iter->rb_node, parent, p);
	rb_insert_color(&iter->rb_node, &fstb_policy_cmd_tree);

	total_fstb_policy_cmd_num++;

	return iter;
}

static void fstb_set_policy_cmd(int cmd, int value, int tgid,
	unsigned long long ts, int op)
{
	struct FSTB_POLICY_CMD *iter;

	iter = fstb_get_policy_cmd(tgid, ts, op);
	if (iter) {
		if (cmd == 0)
			iter->self_ctrl_fps_enable = value;
		else if (cmd == 2)
			iter->notify_target_fps = value;

		if (!op)
			fstb_delete_policy_cmd(iter);
	}
}

int switch_thread_max_fps(int pid, int set_max)
{
	struct FSTB_FRAME_INFO *iter;
	int ret = 0;

	mutex_lock(&fstb_lock);
	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter->pid != pid)
			continue;

		iter->sbe_state = set_max;
		fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
				set_max, "sbe_set_max_fps");
		fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
				iter->sbe_state, "sbe_state");
	}

	mutex_unlock(&fstb_lock);

	return ret;
}

static int switch_redner_fps_range(char *proc_name,
	pid_t pid, int nr_level, struct fps_level *level)
{
	int ret = 0;
	int i;
	int mode;
	struct FSTB_RENDER_TARGET_FPS *rtfiter = NULL;
	struct hlist_node *n;

	if (nr_level > MAX_NR_RENDER_FPS_LEVELS || nr_level < 0)
		return -EINVAL;

	/* check if levels are interleaving */
	for (i = 0; i < nr_level; i++) {
		if (level[i].end > level[i].start ||
				(i > 0 && level[i].start > level[i - 1].end)) {
			return -EINVAL;
		}
	}

	if (proc_name != NULL && pid == 0)
		/*process mode*/
		mode = 0;
	else if (proc_name == NULL && pid > 0)
		/*thread mode*/
		mode = 1;
	else
		return -EINVAL;

	mutex_lock(&fstb_lock);

	hlist_for_each_entry_safe(rtfiter, n, &fstb_render_target_fps, hlist) {
		if ((mode == 0 && !strncmp(
				proc_name, rtfiter->process_name, 16)) ||
				(mode == 1 && pid == rtfiter->pid)) {
			if (nr_level == 0) {
				/* delete render target fps*/
				hlist_del(&rtfiter->hlist);
				kfree(rtfiter);
			} else {
				/* reassign render target fps */
				rtfiter->nr_level = nr_level;
				memcpy(rtfiter->level, level,
					nr_level * sizeof(struct fps_level));
			}
			break;
		}
	}

	if (rtfiter == NULL && nr_level) {
		/* create new render target fps */
		struct FSTB_RENDER_TARGET_FPS *new_render_target_fps;

		new_render_target_fps =
			kzalloc(sizeof(*new_render_target_fps), GFP_KERNEL);
		if (new_render_target_fps == NULL) {
			ret = -ENOMEM;
			goto err;
		}

		if (mode == 0) {
			new_render_target_fps->pid = 0;
			if (!strncpy(
					new_render_target_fps->process_name,
					proc_name, 16)) {
				kfree(new_render_target_fps);
				ret = -ENOMEM;
				goto err;
			}
			new_render_target_fps->process_name[15] = '\0';
		} else if (mode == 1) {
			new_render_target_fps->pid = pid;
			new_render_target_fps->process_name[0] = '\0';
		}
		new_render_target_fps->nr_level = nr_level;
		memcpy(new_render_target_fps->level,
			level, nr_level * sizeof(struct fps_level));

		hlist_add_head(&new_render_target_fps->hlist,
			&fstb_render_target_fps);
	}

err:
	mutex_unlock(&fstb_lock);
	return ret;

}

int switch_process_fps_range(char *proc_name,
		int nr_level, struct fps_level *level)
{
	return switch_redner_fps_range(proc_name, 0, nr_level, level);
}

int switch_thread_fps_range(pid_t pid, int nr_level, struct fps_level *level)
{
	return switch_redner_fps_range(NULL, pid, nr_level, level);
}

static int cmplonglong(const void *a, const void *b)
{
	return *(long long *)a - *(long long *)b;
}

void fpsgo_ctrl2fstb_dfrc_fps(int fps)
{
	mutex_lock(&fstb_lock);

	if (fps <= CFG_MAX_FPS_LIMIT && fps >= CFG_MIN_FPS_LIMIT) {
		dfps_ceiling = fps;
		max_fps_limit = min(dfps_ceiling,
				fps_global_level.start);
		min_fps_limit =	min(dfps_ceiling,
				fps_global_level.end);
	}

	mutex_unlock(&fstb_lock);
}

void gpu_time_update(long long t_gpu, unsigned int cur_freq,
		unsigned int cur_max_freq, u64 ulID)
{
	struct FSTB_FRAME_INFO *iter;

	ktime_t cur_time;
	long long cur_time_us;

	mutex_lock(&fstb_lock);

	if (!fstb_enable) {
		mutex_unlock(&fstb_lock);
		return;
	}

	if (!fstb_active)
		fstb_active = 1;

	if (!fstb_active_dbncd) {
		fstb_active_dbncd = 1;
		switch_fstb_active();
	}

	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter->bufid == ulID)
			break;
	}

	if (iter == NULL) {
		mutex_unlock(&fstb_lock);
		return;
	}

	iter->gpu_time = t_gpu;
	iter->gpu_freq = cur_freq;

	if (iter->weighted_gpu_time_begin < 0 ||
	iter->weighted_gpu_time_end < 0 ||
	iter->weighted_gpu_time_begin > iter->weighted_gpu_time_end ||
	iter->weighted_gpu_time_end >= FRAME_TIME_BUFFER_SIZE) {

		/* purge all data */
		iter->weighted_gpu_time_begin = iter->weighted_gpu_time_end = 0;
	}

	/*get current time*/
	cur_time = ktime_get();
	cur_time_us = ktime_to_us(cur_time);

	/*remove old entries*/
	while (iter->weighted_gpu_time_begin < iter->weighted_gpu_time_end) {
		if (iter->weighted_gpu_time_ts[iter->weighted_gpu_time_begin] <
				cur_time_us - FRAME_TIME_WINDOW_SIZE_US)
			iter->weighted_gpu_time_begin++;
		else
			break;
	}

	if (iter->weighted_gpu_time_begin == iter->weighted_gpu_time_end &&
	iter->weighted_gpu_time_end == FRAME_TIME_BUFFER_SIZE - 1)
		iter->weighted_gpu_time_begin = iter->weighted_gpu_time_end = 0;

	/*insert entries to weighted_gpu_time*/
	/*if buffer full --> move array align first*/
	if (iter->weighted_gpu_time_begin < iter->weighted_gpu_time_end &&
	iter->weighted_gpu_time_end == FRAME_TIME_BUFFER_SIZE - 1) {

		memmove(iter->weighted_gpu_time,
		&(iter->weighted_gpu_time[iter->weighted_gpu_time_begin]),
		sizeof(long long) *
		(iter->weighted_gpu_time_end - iter->weighted_gpu_time_begin));
		memmove(iter->weighted_gpu_time_ts,
		&(iter->weighted_gpu_time_ts[iter->weighted_gpu_time_begin]),
		sizeof(long long) *
		(iter->weighted_gpu_time_end - iter->weighted_gpu_time_begin));

		/*reset index*/
		iter->weighted_gpu_time_end =
		iter->weighted_gpu_time_end - iter->weighted_gpu_time_begin;
		iter->weighted_gpu_time_begin = 0;
	}

	if (cur_max_freq > 0 && cur_max_freq >= cur_freq
			&& t_gpu > 0LL && t_gpu < 1000000000LL) {
		iter->weighted_gpu_time[iter->weighted_gpu_time_end] =
			t_gpu * cur_freq;
		do_div(iter->weighted_gpu_time[iter->weighted_gpu_time_end],
				cur_max_freq);
		iter->weighted_gpu_time_ts[iter->weighted_gpu_time_end] =
			cur_time_us;
		fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
		(int)iter->weighted_gpu_time[iter->weighted_gpu_time_end],
		"weighted_gpu_time");
		iter->weighted_gpu_time_end++;
	}

	mtk_fstb_dprintk(
	"fstb: time %lld %lld t_gpu %lld cur_freq %u cur_max_freq %u\n",
	cur_time_us, ktime_to_us(ktime_get())-cur_time_us,
	t_gpu, cur_freq, cur_max_freq);

	fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid, (int)t_gpu, "t_gpu");
	fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
			(int)cur_freq, "cur_gpu_cap");
	fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
			(int)cur_max_freq, "max_gpu_cap");

	mutex_unlock(&fstb_lock);
}

static int get_gpu_frame_time(struct FSTB_FRAME_INFO *iter)
{
	int ret = INT_MAX;
	/*copy entries to temp array*/
	/*sort this array*/
	if (iter->weighted_gpu_time_end - iter->weighted_gpu_time_begin
		> 0 &&
		iter->weighted_gpu_time_end - iter->weighted_gpu_time_begin
		< FRAME_TIME_BUFFER_SIZE) {
		memcpy(iter->sorted_weighted_gpu_time,
		&(iter->weighted_gpu_time[iter->weighted_gpu_time_begin]),
		sizeof(long long) *
		(iter->weighted_gpu_time_end - iter->weighted_gpu_time_begin));
		sort(iter->sorted_weighted_gpu_time,
		iter->weighted_gpu_time_end - iter->weighted_gpu_time_begin,
		sizeof(long long), cmplonglong, NULL);
	}

	/*update nth value*/
	if (iter->weighted_gpu_time_end - iter->weighted_gpu_time_begin) {
		if (
			iter->sorted_weighted_gpu_time[
				QUANTILE*
				(iter->weighted_gpu_time_end-
				 iter->weighted_gpu_time_begin)/100]
			> INT_MAX)
			ret = INT_MAX;
		else
			ret =
				iter->sorted_weighted_gpu_time[
					QUANTILE*
					(iter->weighted_gpu_time_end-
					 iter->weighted_gpu_time_begin)/100];
	} else
		ret = -1;

	fpsgo_systrace_c_fstb(iter->pid, iter->bufid, ret,
			"quantile_weighted_gpu_time");
	return ret;

}

void eara2fstb_get_tfps(int max_cnt, int *is_camera, int *pid, unsigned long long *buf_id,
				int *tfps, int *rfps, int *hwui, char name[][16])
{
	int count = 0;
	struct FSTB_FRAME_INFO *iter;
	struct hlist_node *n;

	mutex_lock(&fstb_lock);
	*is_camera = 0;

	hlist_for_each_entry_safe(iter, n, &fstb_frame_infos, hlist) {
		if (count == max_cnt)
			break;

		if (!iter->target_fps || iter->target_fps == -1)
			continue;

		pid[count] = iter->pid;
		hwui[count] = iter->hwui_flag;
		buf_id[count] = iter->bufid;
		rfps[count] = iter->queue_fps;
		if (!iter->target_fps_notifying
			|| iter->target_fps_notifying == -1) {
			if (!iter->self_ctrl_fps_enable)
				tfps[count] = iter->target_fps;
			else
				tfps[count] = iter->target_fps_v2;
		}
		else
			tfps[count] = iter->target_fps_notifying;
		if (name)
			strncpy(name[count], iter->proc_name, 16);
		count++;
	}

	mutex_unlock(&fstb_lock);
}
EXPORT_SYMBOL(eara2fstb_get_tfps);

void eara2fstb_tfps_mdiff(int pid, unsigned long long buf_id, int diff,
					int tfps)
{
	int tmp_target_fps;
	struct FSTB_FRAME_INFO *iter;
	struct hlist_node *n;

	mutex_lock(&fstb_lock);

	hlist_for_each_entry_safe(iter, n, &fstb_frame_infos, hlist) {
		if (pid == iter->pid && buf_id == iter->bufid) {
			if (!iter->self_ctrl_fps_enable)
				tmp_target_fps = iter->target_fps;
			else
				tmp_target_fps = iter->target_fps_v2;

			if (tfps != iter->target_fps_notifying
				&& tfps != tmp_target_fps)
				break;

			iter->target_fps_diff = diff;
			fpsgo_systrace_c_fstb_man(pid, buf_id, diff, "eara_diff");

			if (iter->target_fps_notifying
				&& tfps == iter->target_fps_notifying) {
				if (!iter->self_ctrl_fps_enable)
					iter->target_fps = iter->target_fps_notifying;
				else
					iter->target_fps_v2 = iter->target_fps_notifying;
				iter->target_fps_notifying = 0;
				fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
					iter->target_fps_v2, "fstb_target_fps1");
				fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
					0, "fstb_notifying");
			}
			break;
		}
	}

	mutex_unlock(&fstb_lock);
}
EXPORT_SYMBOL(eara2fstb_tfps_mdiff);

int (*eara_pre_change_fp)(void);
EXPORT_SYMBOL(eara_pre_change_fp);
int (*eara_pre_change_single_fp)(int pid, unsigned long long bufID,
			int target_fps);
EXPORT_SYMBOL(eara_pre_change_single_fp);

static void fstb_change_tfps(struct FSTB_FRAME_INFO *iter, int target_fps,
		int notify_eara)
{
	int ret = -1;

	if (notify_eara && eara_pre_change_single_fp)
		ret = eara_pre_change_single_fp(iter->pid, iter->bufid, target_fps);

	if ((notify_eara && (ret == -1))
		|| iter->target_fps_notifying == target_fps) {
		if (!iter->self_ctrl_fps_enable)
			iter->target_fps = target_fps;
		else
			iter->target_fps_v2 = target_fps;
		iter->target_fps_notifying = 0;
		fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
					0, "fstb_notifying");
		fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
					iter->target_fps_v2, "fstb_target_fps1");
	} else {
		iter->target_fps_notifying = target_fps;
		fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
			iter->target_fps_notifying, "fstb_notifying");
	}
}

int fpsgo_fbt2fstb_update_cpu_frame_info(
		int pid,
		unsigned long long bufID,
		int tgid,
		int frame_type,
		unsigned long long Q2Q_time,
		long long Runnging_time,
		unsigned int Curr_cap,
		unsigned int Max_cap,
		unsigned long long enqueue_length,
		unsigned long long dequeue_length)
{
	long long cpu_time_ns = (long long)Runnging_time;
	unsigned int max_current_cap = Curr_cap;
	unsigned int max_cpu_cap = Max_cap;
	unsigned long long wct = 0;
	int eara_fps, tolerence_fps;

	struct FSTB_FRAME_INFO *iter;

	ktime_t cur_time;
	long long cur_time_us;

	mutex_lock(&fstb_lock);

	if (!fstb_enable) {
		mutex_unlock(&fstb_lock);
		return 0;
	}

	if (!fstb_active)
		fstb_active = 1;

	if (!fstb_active_dbncd) {
		fstb_active_dbncd = 1;
		switch_fstb_active();
	}

	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter->pid == pid && iter->bufid == bufID)
			break;
	}

	if (iter == NULL) {
		mutex_unlock(&fstb_lock);
		return 0;
	}

	mtk_fstb_dprintk(
	"pid %d Q2Q_time %lld Runnging_time %lld Curr_cap %u Max_cap %u\n",
	pid, Q2Q_time, Runnging_time, Curr_cap, Max_cap);

	if (iter->weighted_cpu_time_begin < 0 ||
		iter->weighted_cpu_time_end < 0 ||
		iter->weighted_cpu_time_begin > iter->weighted_cpu_time_end ||
		iter->weighted_cpu_time_end >= FRAME_TIME_BUFFER_SIZE) {

		/* purge all data */
		iter->weighted_cpu_time_begin = iter->weighted_cpu_time_end = 0;
	}

	/*get current time*/
	cur_time = ktime_get();
	cur_time_us = ktime_to_us(cur_time);

	/*remove old entries*/
	while (iter->weighted_cpu_time_begin < iter->weighted_cpu_time_end) {
		if (iter->weighted_cpu_time_ts[iter->weighted_cpu_time_begin] <
				cur_time_us - FRAME_TIME_WINDOW_SIZE_US)
			iter->weighted_cpu_time_begin++;
		else
			break;
	}

	if (iter->weighted_cpu_time_begin == iter->weighted_cpu_time_end &&
		iter->weighted_cpu_time_end == FRAME_TIME_BUFFER_SIZE - 1)
		iter->weighted_cpu_time_begin = iter->weighted_cpu_time_end = 0;

	/*insert entries to weighted_cpu_time*/
	/*if buffer full --> move array align first*/
	if (iter->weighted_cpu_time_begin < iter->weighted_cpu_time_end &&
		iter->weighted_cpu_time_end == FRAME_TIME_BUFFER_SIZE - 1) {

		memmove(iter->weighted_cpu_time,
		&(iter->weighted_cpu_time[iter->weighted_cpu_time_begin]),
		sizeof(long long) *
		(iter->weighted_cpu_time_end -
		 iter->weighted_cpu_time_begin));
		memmove(iter->weighted_cpu_time_ts,
		&(iter->weighted_cpu_time_ts[iter->weighted_cpu_time_begin]),
		sizeof(long long) *
		(iter->weighted_cpu_time_end -
		 iter->weighted_cpu_time_begin));

		/*reset index*/
		iter->weighted_cpu_time_end =
		iter->weighted_cpu_time_end - iter->weighted_cpu_time_begin;
		iter->weighted_cpu_time_begin = 0;
	}

	if (max_cpu_cap > 0 && Max_cap > Curr_cap
		&& cpu_time_ns > 0LL && cpu_time_ns < 1000000000LL) {
		wct = cpu_time_ns * max_current_cap;
		do_div(wct, max_cpu_cap);
	} else
		goto out;

	fpsgo_systrace_c_fstb_man(pid, iter->bufid, (int)wct,
		"weighted_cpu_time");

	iter->weighted_cpu_time[iter->weighted_cpu_time_end] =
		wct;

	iter->weighted_cpu_time_ts[iter->weighted_cpu_time_end] =
		cur_time_us;
	iter->weighted_cpu_time_end++;

out:
	mtk_fstb_dprintk(
	"pid %d fstb: time %lld %lld cpu_time_ns %lld max_current_cap %u max_cpu_cap %u\n"
	, pid, cur_time_us, ktime_to_us(ktime_get())-cur_time_us,
	cpu_time_ns, max_current_cap, max_cpu_cap);


	/* parse cpu time of each frame to ged_kpi */
	iter->cpu_time = cpu_time_ns;

	if (!iter->self_ctrl_fps_enable) {
		eara_fps = iter->target_fps;
		if (iter->target_fps && iter->target_fps != -1 && iter->target_fps_diff
			&& !iter->target_fps_margin && !iter->target_fps_margin_gpu) {
			eara_fps = iter->target_fps * 1000 + iter->target_fps_diff;
			eara_fps /= 1000;
			eara_fps = clamp(eara_fps, min_fps_limit, max_fps_limit);
		}
		tolerence_fps = iter->target_fps_margin_gpu;
	} else {
		eara_fps = iter->target_fps_v2;
		if (iter->target_fps_v2 && iter->target_fps_v2 != -1 && iter->target_fps_diff
			&& !iter->target_fps_margin_v2) {
			eara_fps = iter->target_fps_v2 * 1000 + iter->target_fps_diff;
			eara_fps /= 1000;
			eara_fps = clamp(eara_fps, min_fps_limit, max_fps_limit);
		}
		tolerence_fps = iter->target_fps_margin_v2;
	}
	switch (iter->sbe_state) {
	case 0:
		break;
	case 1:
		eara_fps = max_fps_limit;
		tolerence_fps = 0;
		break;
	default:
		break;
	}
	ged_kpi_set_target_FPS_margin(iter->bufid, eara_fps, tolerence_fps,
		iter->target_fps_diff, iter->cpu_time);

	if (fpsgo2msync_hint_frameinfo_fp)
		fpsgo2msync_hint_frameinfo_fp((unsigned int)iter->pid, iter->bufid,
			iter->target_fps, Q2Q_time, Q2Q_time - enqueue_length - dequeue_length);

	fpsgo_systrace_c_fstb_man(pid, iter->bufid, (int)cpu_time_ns, "t_cpu");
	fpsgo_systrace_c_fstb(pid, iter->bufid, (int)max_current_cap,
			"cur_cpu_cap");
	fpsgo_systrace_c_fstb(pid, iter->bufid, (int)max_cpu_cap,
			"max_cpu_cap");

	mutex_unlock(&fstb_lock);
	return 0;
}

static void fstb_calculate_target_fps(int pid, unsigned long long bufID,
	unsigned long long cur_queue_end_ts)
{
	int i, target_fps, margin = 0, eara_is_active = 0;
	int target_fps_old = max_fps_limit, target_fps_new = max_fps_limit;
	struct FSTB_FRAME_INFO *iter;
	struct FSTB_RENDER_TARGET_FPS *rtfiter;

	mutex_lock(&fstb_lock);

	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter->pid == pid && iter->bufid == bufID)
			break;
	}

	if (!iter)
		goto out;

	margin = iter->target_fps_margin_v2;
	if (iter->target_fps_diff)
		eara_is_active = 1;
	else
		eara_is_active = 0;

	mutex_unlock(&fstb_lock);

	target_fps = fpsgo_fstb2xgf_get_target_fps(pid, bufID,
		&margin, cur_queue_end_ts, eara_is_active);

	mutex_lock(&fstb_lock);

	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter->pid == pid && iter->bufid == bufID)
			break;
	}

	if (!iter)
		goto out;

	if (target_fps <= 0) {
		fstb_change_tfps(iter, iter->target_fps, 1);
		fpsgo_main_trace("[fstb][%d][0x%llx] | back to v1 (%d)",
			iter->pid, iter->bufid, iter->target_fps);
	} else {
		target_fps_old = iter->target_fps_v2;
		target_fps_new = target_fps;

		if (iter->notify_target_fps > 0) {
			target_fps_new = iter->notify_target_fps;
			margin = 0;
		}

		hlist_for_each_entry(rtfiter, &fstb_render_target_fps, hlist) {
			if (!strncmp(iter->proc_name, rtfiter->process_name, 16) ||
				rtfiter->pid == iter->pid) {
				for (i = rtfiter->nr_level-1; i >= 0; i--) {
					if (rtfiter->level[i].start >= target_fps_new) {
						target_fps_new =
							target_fps_new >= rtfiter->level[i].end ?
							target_fps_new : rtfiter->level[i].end;
						break;
					}
				}
				if (i < 0)
					target_fps_new = rtfiter->level[0].start;

				if (target_fps_new == rtfiter->level[0].start && margin)
					margin = 0;
			}
		}

		if (target_fps_new > max_fps_limit)
			target_fps_new = max_fps_limit;
		if (target_fps_new < min_fps_limit)
			target_fps_new = min_fps_limit;

		if (target_fps_old != target_fps_new)
			fstb_change_tfps(iter, target_fps_new, 1);
	}
	iter->target_fps_margin_v2 = margin;

	fpsgo_main_trace("[fstb][%d][0x%llx] | target_fps:%d(%d)(%d)(%d)(%d)",
		iter->pid, iter->bufid, iter->target_fps_v2,
		target_fps_old, target_fps_new, target_fps, iter->notify_target_fps);
	fpsgo_main_trace("[fstb][%d][0x%llx] | dfrc:%d eara:%d margin:%d",
		iter->pid, iter->bufid,
		dfps_ceiling, eara_is_active, iter->target_fps_margin_v2);
	fpsgo_systrace_c_fstb(iter->pid, iter->bufid, iter->target_fps_v2, "target_fps_v2");
	fpsgo_systrace_c_fstb(iter->pid, iter->bufid, iter->target_fps_margin_v2,
		"target_fps_margin_v2");

out:
	mutex_unlock(&fstb_lock);
}

#define FSTB_CONTAINER_OF(ptr, type, member) \
	((type *)(((char *)ptr) - offsetof(type, member)))

static void fstb_notifier_wq_cb(struct work_struct *psWork)
{
	struct FSTB_NOTIFIER_PUSH_TAG *vpPush =
		FSTB_CONTAINER_OF(psWork,
				struct FSTB_NOTIFIER_PUSH_TAG, sWork);

	if (!vpPush)
		return;

	fstb_calculate_target_fps(vpPush->pid, vpPush->bufid,
		vpPush->cur_queue_end_ts);

	kfree(vpPush);
}

void fpsgo_comp2fstb_prepare_calculate_target_fps(int pid, unsigned long long bufID,
	unsigned long long cur_queue_end_ts)
{
	struct FSTB_FRAME_INFO *iter;
	struct FSTB_NOTIFIER_PUSH_TAG *vpPush;

	mutex_lock(&fstb_lock);

	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter->pid == pid && iter->bufid == bufID)
			break;
	}

	if (!iter)
		goto out;

	if (!iter->self_ctrl_fps_enable)
		iter->self_ctrl_fps_enable = fstb_self_ctrl_fps_enable ? 1 : 0;

	if (!iter->self_ctrl_fps_enable)
		goto out;

	vpPush =
		(struct FSTB_NOTIFIER_PUSH_TAG *)
		kmalloc(sizeof(struct FSTB_NOTIFIER_PUSH_TAG), GFP_ATOMIC);

	if (!vpPush)
		goto out;

	if (!wq) {
		kfree(vpPush);
		goto out;
	}

	vpPush->pid = pid;
	vpPush->bufid = bufID;
	vpPush->cur_queue_end_ts = cur_queue_end_ts;

	INIT_WORK(&vpPush->sWork, fstb_notifier_wq_cb);
	queue_work(wq, &vpPush->sWork);

out:
	mutex_unlock(&fstb_lock);
}

static long long get_cpu_frame_time(struct FSTB_FRAME_INFO *iter)
{
	long long ret = INT_MAX;
	/*copy entries to temp array*/
	/*sort this array*/
	if (iter->weighted_cpu_time_end - iter->weighted_cpu_time_begin > 0 &&
		iter->weighted_cpu_time_end - iter->weighted_cpu_time_begin <
		FRAME_TIME_BUFFER_SIZE) {
		memcpy(iter->sorted_weighted_cpu_time,
		&(iter->weighted_cpu_time[iter->weighted_cpu_time_begin]),
		sizeof(long long) *
		(iter->weighted_cpu_time_end -
		 iter->weighted_cpu_time_begin));
		sort(iter->sorted_weighted_cpu_time,
		iter->weighted_cpu_time_end -
		iter->weighted_cpu_time_begin,
		sizeof(long long), cmplonglong, NULL);
	}

	/*update nth value*/
	if (iter->weighted_cpu_time_end - iter->weighted_cpu_time_begin) {
		if (
			iter->sorted_weighted_cpu_time[
				QUANTILE*
				(iter->weighted_cpu_time_end-
				 iter->weighted_cpu_time_begin)/100]
			> INT_MAX)
			ret = INT_MAX;
		else
			ret =
				iter->sorted_weighted_cpu_time[
					QUANTILE*
					(iter->weighted_cpu_time_end-
					 iter->weighted_cpu_time_begin)/100];
	} else
		ret = -1;

	fpsgo_systrace_c_fstb(iter->pid, iter->bufid, ret,
		"quantile_weighted_cpu_time");
	return ret;

}

void fstb_set_video_pid(int pid)
{
	struct video_info *video_info_instance;

	if (pid == 0)
		return;

	mutex_lock(&fstb_video_pid_tree_lock);
	video_info_instance = fstb_search_and_add_video_info(pid, 1);
	mutex_unlock(&fstb_video_pid_tree_lock);
	if (video_info_instance)
		fpsgo_systrace_c_fstb_man(-100, 0,
			video_info_instance->count_instance, "video_pid[%d]", pid);
	fpsgo_main_trace("[FSTB_Video]: pid=%d, %s", pid, __func__);
}

void fstb_clear_video_pid(int pid)
{
	if (pid == 0)
		return;

	mutex_lock(&fstb_video_pid_tree_lock);
	fstb_delete_video_info(pid);
	mutex_unlock(&fstb_video_pid_tree_lock);

	fpsgo_main_trace("[FSTB_Video]: pid=%d, %s", pid, __func__);
}

static int fstb_get_queue_fps2(struct FSTB_FRAME_INFO *iter)
{
	unsigned long long retval = 0;
	unsigned long long duration = 0;
	int tmp_jump_check_num = 0;

	tmp_jump_check_num = min(JUMP_CHECK_NUM,
			iter->target_fps * JUMP_CHECK_Q_PCT / 100);
	tmp_jump_check_num =
		tmp_jump_check_num <= 1 ? 2 : tmp_jump_check_num;

	duration =
		iter->queue_time_ts[iter->queue_time_end - 1] -
		iter->queue_time_ts[iter->queue_time_end - tmp_jump_check_num];
	do_div(duration, tmp_jump_check_num - 1);

	retval = 1000000000ULL;
	do_div(retval, duration);

	return (int)retval;
}


static int calculate_fps_limit(struct FSTB_FRAME_INFO *iter, int target_fps);

static int mode(int a[], int n)
{
	int maxValue = 0, maxCount = 0, i, j;

	for (i = 0; i < n; ++i) {
		int count = 0;

		for (j = 0; j < n; ++j) {
			if (a[j] == a[i])
				++count;
		}

		if (count > maxCount) {
			maxCount = count;
			maxValue = a[i];
		}
	}

	if (maxCount)
		return maxValue;
	else
		return a[n-1];
}

void fpsgo_comp2fstb_queue_time_update(int pid, unsigned long long bufID,
	int frame_type, unsigned long long ts,
	int api, int hwui_flag)
{
	struct FSTB_FRAME_INFO *iter;
	struct FSTB_POLICY_CMD *policy;
	ktime_t cur_time;
	long long cur_time_us = 0;
	int tmp_jump_check_num = 0;

	mutex_lock(&fstb_lock);

	if (!fstb_enable) {
		mutex_unlock(&fstb_lock);
		return;
	}

	if (!fstb_active)
		fstb_active = 1;

	if (!fstb_active_dbncd) {
		fstb_active_dbncd = 1;
		switch_fstb_active();
	}

	cur_time = ktime_get();
	cur_time_us = ktime_to_us(cur_time);

	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter->pid == pid && (iter->bufid == bufID ||
				iter->bufid == 0))
			break;
	}


	if (iter == NULL) {
		struct FSTB_FRAME_INFO *new_frame_info;

		new_frame_info = add_new_frame_info(pid, bufID, hwui_flag);
		if (new_frame_info == NULL)
			goto out;

		iter = new_frame_info;
		hlist_add_head(&iter->hlist, &fstb_frame_infos);
	}

	if (iter->bufid == 0)
		iter->bufid = bufID;

	iter->hwui_flag = hwui_flag;

	if (wq_has_sleeper(&active_queue)) {
		condition_fstb_active = 1;
		wake_up_interruptible(&active_queue);
	}

	mutex_lock(&fstb_policy_cmd_lock);
	policy = fstb_get_policy_cmd(iter->proc_id, ts, 0);
	if (!policy)
		fstb_update_policy_cmd(iter, NULL, ts);
	else
		fstb_update_policy_cmd(iter, policy, ts);
	mutex_unlock(&fstb_policy_cmd_lock);

	if (iter->queue_time_begin < 0 ||
			iter->queue_time_end < 0 ||
			iter->queue_time_begin > iter->queue_time_end ||
			iter->queue_time_end >= FRAME_TIME_BUFFER_SIZE) {
		/* purge all data */
		iter->queue_time_begin = iter->queue_time_end = 0;
	}

	/*remove old entries*/
	while (iter->queue_time_begin < iter->queue_time_end) {
		if (iter->queue_time_ts[iter->queue_time_begin] < ts -
				(long long)FRAME_TIME_WINDOW_SIZE_US * 1000)
			iter->queue_time_begin++;
		else
			break;
	}

	if (iter->queue_time_begin == iter->queue_time_end &&
			iter->queue_time_end == FRAME_TIME_BUFFER_SIZE - 1)
		iter->queue_time_begin = iter->queue_time_end = 0;

	/*insert entries to weighted_display_time*/
	/*if buffer full --> move array align first*/
	if (iter->queue_time_begin < iter->queue_time_end &&
			iter->queue_time_end == FRAME_TIME_BUFFER_SIZE - 1) {
		memmove(iter->queue_time_ts,
		&(iter->queue_time_ts[iter->queue_time_begin]),
		sizeof(unsigned long long) *
		(iter->queue_time_end - iter->queue_time_begin));
		/*reset index*/
		iter->queue_time_end =
			iter->queue_time_end - iter->queue_time_begin;
		iter->queue_time_begin = 0;
	}

	iter->queue_time_ts[iter->queue_time_end] = ts;
	iter->queue_time_end++;

	if (!JUMP_CHECK_NUM)
		goto out;

	tmp_jump_check_num = min(JUMP_CHECK_NUM,
			iter->target_fps * JUMP_CHECK_Q_PCT / 100);
	tmp_jump_check_num =
		tmp_jump_check_num <= 1 ? 2 : tmp_jump_check_num;

	if (iter->queue_time_end - iter->queue_time_begin >= tmp_jump_check_num) {
		int tmp_q_fps = fstb_get_queue_fps2(iter);
		int tmp_target_fps = iter->target_fps;
		int tmp_vote_fps = iter->target_fps;

		fpsgo_systrace_c_fstb(iter->pid, iter->bufid, tmp_q_fps,
			"tmp_q_fps");
		fpsgo_systrace_c_fstb(iter->pid, iter->bufid, tmp_jump_check_num,
			"tmp_jump_check_num");

		if (tmp_q_fps >= iter->target_fps +
			iter->target_fps_margin2) {
			tmp_target_fps =
				calculate_fps_limit(iter, tmp_q_fps);
		}

		if (iter->vote_i < JUMP_VOTE_MAX_I) {
			iter->vote_fps[iter->vote_i] = tmp_target_fps;
			iter->vote_i++;
		} else {
			memmove(iter->vote_fps,
			&(iter->vote_fps[JUMP_VOTE_MAX_I - tmp_jump_check_num + 1]),
			sizeof(int) * (tmp_jump_check_num - 1));
			iter->vote_i = tmp_jump_check_num - 1;
			iter->vote_fps[iter->vote_i] = tmp_target_fps;
			iter->vote_i++;
		}

		if (iter->vote_i >= tmp_jump_check_num) {
			tmp_vote_fps =
			mode(
			&(iter->vote_fps[iter->vote_i - tmp_jump_check_num]),
			tmp_jump_check_num);
			fpsgo_main_trace("fstb_vote_target_fps %d",
			tmp_vote_fps);
		}

		if (tmp_vote_fps > iter->target_fps) {
			iter->fps_raise_flag = 1;
			if (!iter->self_ctrl_fps_enable)
				fstb_change_tfps(iter, tmp_vote_fps, 1);
			else
				iter->target_fps = tmp_vote_fps;
		}
	}

out:
	mutex_unlock(&fstb_lock);

	mutex_lock(&fstb_fps_active_time);
	if (cur_time_us)
		last_update_ts = cur_time_us;
	mutex_unlock(&fstb_fps_active_time);
}

static int fstb_get_queue_fps1(struct FSTB_FRAME_INFO *iter,
		long long interval, int *is_fps_update)
{
	int i = iter->queue_time_begin, j;
	unsigned long long queue_fps;
	unsigned long long frame_interval_count = 0;
	unsigned long long avg_frame_interval = 0;
	unsigned long long retval = 0;
	int frame_count = 0;

	/* remove old entries */
	while (i < iter->queue_time_end) {
		if (iter->queue_time_ts[i] < sched_clock() - interval * 1000)
			i++;
		else
			break;
	}

	/* filter and asfc evaluation*/
	for (j = i + 1; j < iter->queue_time_end; j++) {
		if ((iter->queue_time_ts[j] -
					iter->queue_time_ts[j - 1]) <
				DISPLAY_FPS_FILTER_NS) {
			avg_frame_interval +=
				(iter->queue_time_ts[j] -
				 iter->queue_time_ts[j - 1]);
			frame_interval_count++;
		}
		frame_count++;
	}

	*is_fps_update = frame_count ? 1 : 0;

	queue_fps = (long long)(iter->queue_time_end - i) * 1000000LL;
	do_div(queue_fps, (unsigned long long)interval);

	if (avg_frame_interval != 0) {
		retval = 1000000000ULL * frame_interval_count;
		do_div(retval, avg_frame_interval);
		if (frame_interval_count < DISPLAY_FPS_FILTER_NUM)
			retval = -1;
		fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid, (int)retval,
			"queue_fps");
		return retval;
	}
	if (iter->queue_fps == -1 || frame_count)
		retval = -1;
	else
		retval = 0;
	fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
			retval, "queue_fps");

	return retval;
}

static int fps_update(struct FSTB_FRAME_INFO *iter)
{
	int is_fps_update = 0;

	iter->queue_fps =
		fstb_get_queue_fps1(iter, FRAME_TIME_WINDOW_SIZE_US, &is_fps_update);

	return is_fps_update;
}

/* Calculate FPS limit:
 * @retval new fps limit
 *
 * search in ascending order
 * For discrete range:
 *  same as before, we select the upper one level that
 *  is just larger than current target.
 * For contiguous range:
 *  if the new limit is between [start,end], use new limit
 */
static int calculate_fps_limit(struct FSTB_FRAME_INFO *iter, int target_fps)
{
	int ret_fps = target_fps;
	int asfc_turn = 0;
	int i;
	struct FSTB_RENDER_TARGET_FPS *rtfiter = NULL;

	if (iter->notify_target_fps > 0)
		ret_fps = iter->notify_target_fps;

	hlist_for_each_entry(rtfiter, &fstb_render_target_fps, hlist) {
		mtk_fstb_dprintk("%s %s %d %s %d\n",
				__func__, iter->proc_name, iter->pid,
				rtfiter->process_name, rtfiter->pid);

		if (!strncmp(iter->proc_name, rtfiter->process_name, 16)
				|| rtfiter->pid == iter->pid) {

			for (i = rtfiter->nr_level - 1; i >= 0; i--) {
				if (rtfiter->level[i].start >= target_fps) {
					ret_fps =
						target_fps >=
						rtfiter->level[i].end ?
						target_fps :
						rtfiter->level[i].end;
					break;
				}
			}

			if (i < 0)
				ret_fps = rtfiter->level[0].start;
			else if (i && ret_fps == rtfiter->level[i].start)
				asfc_turn = 1;

			break;
		}
	}

	if (ret_fps == 30 && max_fps_limit > 30) {
		if (rtfiter && rtfiter->level[0].start > 30)
			asfc_turn = 1;
		else if (!rtfiter)
			asfc_turn = 1;
	} else if (ret_fps == 60 && max_fps_limit > 60) {
		if (rtfiter && rtfiter->level[0].start > 60)
			asfc_turn = 1;
		else if (!rtfiter)
			asfc_turn = 1;
	} else if (ret_fps == 90 && max_fps_limit > 90) {
		if (rtfiter && rtfiter->level[0].start > 90)
			asfc_turn = 1;
		else if (!rtfiter)
			asfc_turn = 1;
	}

	switch (margin_mode) {
	case 0:
		iter->target_fps_margin =
			ret_fps >= max_fps_limit ? 0 : RESET_TOLERENCE;
		break;
	case 1:
		if (ret_fps >= max_fps_limit)
			iter->target_fps_margin = 0;
		else if (asfc_turn)
			iter->target_fps_margin = RESET_TOLERENCE;
		else
			iter->target_fps_margin = 0;
		break;
	case 2:
		if (ret_fps >= max_fps_limit) {
			iter->target_fps_margin = 0;
			iter->target_fps_margin_dbnc_a =
				margin_mode_dbnc_a;
			iter->target_fps_margin_dbnc_b =
				margin_mode_dbnc_b;
		} else if (asfc_turn) {
			if (iter->target_fps_margin_dbnc_a > 0) {
				iter->target_fps_margin = 0;
				iter->target_fps_margin_dbnc_a--;
			} else if (iter->target_fps_margin_dbnc_b > 0) {
				iter->target_fps_margin = RESET_TOLERENCE;
				iter->target_fps_margin_dbnc_b--;
				if (iter->target_fps_margin_dbnc_b <= 0) {
					iter->target_fps_margin_dbnc_a =
						margin_mode_dbnc_a;
					iter->target_fps_margin_dbnc_b =
						margin_mode_dbnc_b;
				}
			} else {
				iter->target_fps_margin = RESET_TOLERENCE;
				iter->target_fps_margin_dbnc_a =
					margin_mode_dbnc_a;
				iter->target_fps_margin_dbnc_b =
					margin_mode_dbnc_b;
			}
		} else {
			iter->target_fps_margin = 0;
			iter->target_fps_margin_dbnc_a =
				margin_mode_dbnc_a;
			iter->target_fps_margin_dbnc_b =
				margin_mode_dbnc_b;
		}
		break;
	default:
		iter->target_fps_margin =
			ret_fps >= max_fps_limit ? 0 : RESET_TOLERENCE;
		break;
	}

	switch (margin_mode_gpu) {
	case 0:
		iter->target_fps_margin_gpu =
			ret_fps >= max_fps_limit ? 0 : RESET_TOLERENCE;
		break;
	case 1:
		if (ret_fps >= max_fps_limit)
			iter->target_fps_margin_gpu = 0;
		else if (asfc_turn)
			iter->target_fps_margin_gpu = RESET_TOLERENCE;
		else
			iter->target_fps_margin_gpu = 0;
		break;
	case 2:
		if (ret_fps >= max_fps_limit) {
			iter->target_fps_margin_gpu = 0;
			iter->target_fps_margin_gpu_dbnc_a =
				margin_mode_gpu_dbnc_a;
			iter->target_fps_margin_gpu_dbnc_b =
				margin_mode_gpu_dbnc_b;
		} else if (asfc_turn) {
			if (iter->target_fps_margin_gpu_dbnc_a > 0) {
				iter->target_fps_margin_gpu = 0;
				iter->target_fps_margin_gpu_dbnc_a--;
			} else if (iter->target_fps_margin_gpu_dbnc_b > 0) {
				iter->target_fps_margin_gpu = RESET_TOLERENCE;
				iter->target_fps_margin_gpu_dbnc_b--;
				if (iter->target_fps_margin_gpu_dbnc_b <= 0) {
					iter->target_fps_margin_gpu_dbnc_a =
						margin_mode_gpu_dbnc_a;
					iter->target_fps_margin_gpu_dbnc_b =
						margin_mode_gpu_dbnc_b;
				}
			} else {
				iter->target_fps_margin_gpu = RESET_TOLERENCE;
				iter->target_fps_margin_gpu_dbnc_a =
					margin_mode_gpu_dbnc_a;
				iter->target_fps_margin_gpu_dbnc_b =
					margin_mode_gpu_dbnc_b;
			}
		} else {
			iter->target_fps_margin_gpu = 0;
			iter->target_fps_margin_gpu_dbnc_a =
				margin_mode_gpu_dbnc_a;
			iter->target_fps_margin_gpu_dbnc_b =
				margin_mode_gpu_dbnc_b;
		}
		break;
	default:
		iter->target_fps_margin_gpu =
			ret_fps >= max_fps_limit ? 0 : RESET_TOLERENCE;
		break;
	}

	iter->target_fps_margin2 = asfc_turn ? RESET_TOLERENCE : 0;

	if (iter->notify_target_fps > 0) {
		if (iter->target_fps_margin)
			iter->target_fps_margin = 0;
		if (iter->target_fps_margin_gpu)
			iter->target_fps_margin_gpu = 0;
	}

	if (ret_fps >= max_fps_limit)
		return max_fps_limit;

	if (ret_fps <= min_fps_limit)
		return min_fps_limit;

	return ret_fps;
}

static int cal_target_fps(struct FSTB_FRAME_INFO *iter)
{
	long long target_limit = max_fps_limit;
	int cur_cpu_time, cur_gpu_time;

	cur_cpu_time = get_cpu_frame_time(iter);
	cur_gpu_time = get_gpu_frame_time(iter);
	iter->quantile_cpu_time = cur_cpu_time;
	iter->quantile_gpu_time = cur_gpu_time;


	if (iter->fps_raise_flag == 1) {
		target_limit = iter->target_fps;
		/*decrease*/
	} else if (iter->target_fps - iter->queue_fps >
			iter->target_fps * fps_error_threshold / 100) {

		target_limit = iter->queue_fps;

		fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
				(int)target_limit, "tmp_target_limit");
	} else {
		target_limit = iter->target_fps;
	}

	iter->fps_raise_flag = 0;

	return target_limit;

}

#define FSTB_SEC_DIVIDER 1000000000
#define FSTB_MSEC_DIVIDER 1000000000000ULL
void fpsgo_fbt2fstb_query_fps(int pid, unsigned long long bufID,
		int *target_fps, int *target_cpu_time, int *fps_margin,
		int tgid, int *quantile_cpu_time,
		int *quantile_gpu_time, int *target_fpks, int *cooler_on)
{
	struct FSTB_FRAME_INFO *iter = NULL;
	unsigned long long total_time, v_c_time;
	int tolerence_fps = 0;
	int eara_fps = max_fps_limit;

	mutex_lock(&fstb_lock);

	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter->pid == pid && iter->bufid == bufID)
			break;
	}

	*cooler_on = 0;

	if (!iter) {
		(*quantile_cpu_time) = -1;
		(*quantile_gpu_time) = -1;
		*target_fps = max_fps_limit;
		tolerence_fps = 0;
		total_time = (int)FSTB_SEC_DIVIDER;
		total_time =
			div64_u64(total_time,
			(*target_fps) + tolerence_fps > max_fps_limit ?
			max_fps_limit : (*target_fps) + tolerence_fps);
		v_c_time = total_time;
		eara_fps = *target_fps * 1000;

	} else {
		(*quantile_cpu_time) = iter->quantile_cpu_time;
		(*quantile_gpu_time) = iter->quantile_gpu_time;

		if (!iter->self_ctrl_fps_enable) {
			if (iter->target_fps && iter->target_fps != -1
				&& iter->target_fps_diff
				&& !iter->target_fps_margin
				&& !iter->target_fps_margin_gpu) {
				int max_mlimit = max_fps_limit * 1000;
				int min_mlimit = min_fps_limit * 1000;

				eara_fps = iter->target_fps * 1000;
				fpsgo_systrace_c_fstb_man(pid, iter->bufid,
						iter->target_fps_diff, "eara_diff");

				eara_fps += iter->target_fps_diff;
				eara_fps = clamp(eara_fps, min_mlimit, max_mlimit);
				*cooler_on = 1;

				*target_fps = eara_fps / 1000;
				tolerence_fps = iter->target_fps_margin * 1000;
				total_time = (unsigned long long)FSTB_MSEC_DIVIDER;
				total_time =
					div64_u64(total_time,
					(eara_fps + tolerence_fps) > max_mlimit ?
					max_mlimit : (eara_fps + tolerence_fps));

			} else {
				switch (iter->sbe_state) {
				case 1:
					*target_fps = max_fps_limit;
					tolerence_fps = 0;
					break;
				case 0:
				default:
					*target_fps = iter->target_fps;
					tolerence_fps = iter->target_fps_margin;
					if ((iter->queue_fps >
						fps_global_level.start * fps_bypass_max / 100 ||
						iter->queue_fps <
						fps_global_level.end *
						fps_bypass_min / 100) &&
						iter->queue_fps > 0)
						*target_fps = -1;
					break;
				}
				eara_fps = *target_fps * 1000;
				total_time = (int)FSTB_SEC_DIVIDER;
				total_time =
					div64_u64(total_time,
					(*target_fps) + tolerence_fps > max_fps_limit ?
					max_fps_limit : (*target_fps) + tolerence_fps);
			}
		} else {
			if (iter->target_fps_v2 && iter->target_fps_v2 != -1
				&& iter->target_fps_diff
				&& !iter->target_fps_margin_v2) {
				int max_mlimit = max_fps_limit * 1000;
				int min_mlimit = min_fps_limit * 1000;

				eara_fps = iter->target_fps_v2 * 1000;
				fpsgo_systrace_c_fstb_man(pid, iter->bufid,
						iter->target_fps_diff, "eara_diff");

				eara_fps += iter->target_fps_diff;
				eara_fps = clamp(eara_fps, min_mlimit, max_mlimit);
				*cooler_on = 1;

				*target_fps = eara_fps / 1000;
				tolerence_fps = iter->target_fps_margin_v2 * 1000;
				total_time = (unsigned long long)FSTB_MSEC_DIVIDER;
				total_time =
					div64_u64(total_time,
					(eara_fps + tolerence_fps) > max_mlimit ?
					max_mlimit : (eara_fps + tolerence_fps));

			} else {
				*target_fps = iter->target_fps_v2;
				tolerence_fps = iter->target_fps_margin_v2;
				total_time = (int)FSTB_SEC_DIVIDER;
				total_time =
					div64_u64(total_time,
					(*target_fps) + tolerence_fps > max_fps_limit ?
					max_fps_limit : (*target_fps) + tolerence_fps);
				eara_fps = *target_fps * 1000;
			}
		}

		v_c_time = total_time;

		if (!adopt_low_fps && iter->queue_fps == -1)
			*target_fps = -1;
	}

	*target_cpu_time = v_c_time;
	*fps_margin = tolerence_fps;
	*target_fpks = eara_fps;

	mutex_unlock(&fstb_lock);
}

static int fpsgo_power2fstb_get_target_fps(struct FSTB_FRAME_INFO *iter)
{
	int i;
	int ret = -1;
	int target_fps_arb_arr[4];

	if (!iter)
		goto out;

	if (iter->self_ctrl_fps_enable || fstb_self_ctrl_fps_enable) {
		target_fps_arb_arr[0] = 2;
		target_fps_arb_arr[1] = iter->target_fps_v2;
	} else {
		target_fps_arb_arr[0] = 1;
		target_fps_arb_arr[1] = iter->target_fps;
	}

	target_fps_arb_arr[2] = -1;
	if (iter->sbe_state == 1)
		target_fps_arb_arr[2] = max_fps_limit;

	if (fps_global_level.start == CFG_MAX_FPS_LIMIT &&
		fps_global_level.end == CFG_MIN_FPS_LIMIT)
		target_fps_arb_arr[3] = -1;
	else
		target_fps_arb_arr[3] = fps_global_level.start;

	if (target_fps_arb_arr[0] == 2)
		ret = target_fps_arb_arr[1];
	for (i = 2; i < 4; i++) {
		if (target_fps_arb_arr[i] > 0)
			ret = target_fps_arb_arr[i];
	}

out:
	return ret;
}

static int cmp_powerfps(const void *x1, const void *x2)
{
	const struct FSTB_POWERFPS_LIST *r1 = x1;
	const struct FSTB_POWERFPS_LIST *r2 = x2;

	if (r1->pid == 0)
		return 1;
	else if (r1->pid == -1)
		return 1;
	else if (r1->pid < r2->pid)
		return -1;
	else if (r1->pid == r2->pid && r1->fps < r2->fps)
		return -1;
	else if (r1->pid == r2->pid && r1->fps == r2->fps)
		return 0;
	return 1;
}
struct FSTB_POWERFPS_LIST powerfps_arrray[64];
void fstb_cal_powerhal_fps(void)
{
	struct FSTB_FRAME_INFO *iter;
	int i = 0, j = 0;

	memset(powerfps_arrray, 0, 64 * sizeof(struct FSTB_POWERFPS_LIST));
	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		powerfps_arrray[i].pid = iter->proc_id;
		powerfps_arrray[i].fps = fpsgo_power2fstb_get_target_fps(iter);

		i++;
		if (i >= 64) {
			i = 63;
			break;
		}
	}
	powerfps_arrray[i].pid = -1;

	sort(powerfps_arrray, i, sizeof(struct FSTB_POWERFPS_LIST), cmp_powerfps, NULL);

	for (j = 0; j < i; j++) {
		if (powerfps_arrray[j].pid != powerfps_arrray[j + 1].pid) {
			mtk_fstb_dprintk_always("%s %d %d %d\n",
				__func__, j, powerfps_arrray[j].pid, powerfps_arrray[j].fps);
			fstb_sentcmd(powerfps_arrray[j].pid, powerfps_arrray[j].fps);
		}
	}

}

static void fstb_fps_stats(struct work_struct *work)
{
	struct FSTB_FRAME_INFO *iter;
	struct hlist_node *n;
	int target_fps = max_fps_limit;
	int target_fps_old, target_fps_new;
	int idle = 1;
	int fstb_active2xgf;
	int max_target_fps = -1;
	int eara_ret = -1;

	if (work != &fps_stats_work)
		kfree(work);

	mutex_lock(&fstb_lock);

	if (gbe_fstb2gbe_poll_fp)
		gbe_fstb2gbe_poll_fp(&fstb_frame_infos);


	hlist_for_each_entry_safe(iter, n, &fstb_frame_infos, hlist) {
		/* if this process did queue buffer while last polling window */
		if (fps_update(iter)) {

			idle = 0;

			target_fps = cal_target_fps(iter);
			if (!iter->self_ctrl_fps_enable) {
				target_fps_old = iter->target_fps;
				target_fps_new = calculate_fps_limit(iter, target_fps);
				if (target_fps_old != target_fps_new)
					fstb_change_tfps(iter, target_fps_new, 1);
			} else
				iter->target_fps = calculate_fps_limit(iter, target_fps);

			iter->vote_i = 0;
			fpsgo_systrace_c_fstb_man(iter->pid, 0,
					dfps_ceiling, "dfrc");
			fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
				iter->target_fps, "fstb_target_fps1");
			fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
				iter->target_fps_margin, "target_fps_margin");
			fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
				iter->target_fps_margin_gpu, "target_fps_margin_gpu");
			fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
				iter->target_fps_margin2, "target_fps_thrs");
			fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
				iter->target_fps_margin_dbnc_a,
				"target_fps_margin_dbnc_a");
			fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
				iter->target_fps_margin_dbnc_b,
				"target_fps_margin_dbnc_b");
			fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
				iter->target_fps_margin_gpu_dbnc_a,
				"target_fps_margin_gpu_dbnc_a");
			fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
				iter->target_fps_margin_gpu_dbnc_b,
				"target_fps_margin_gpu_dbnc_b");

			mtk_fstb_dprintk(
			"%s pid:%d target_fps:%d\n",
			__func__, iter->pid,
			iter->target_fps);

			if (max_target_fps < iter->target_fps)
				max_target_fps = iter->target_fps;

			iter->render_idle_cnt = 0;
			/* if queue fps == 0, we delete that frame_info */
		} else {
			iter->render_idle_cnt++;
			if (iter->render_idle_cnt < FSTB_IDLE_DBNC) {

				iter->target_fps = calculate_fps_limit(iter, target_fps);
				mtk_fstb_dprintk(
						"%s pid:%d target_fps:%d\n",
						__func__, iter->pid,
						iter->target_fps);
				continue;
			}

			fpsgo_fstb2xgf_notify_recycle(iter->pid, iter->bufid);

			hlist_del(&iter->hlist);


			vfree(iter);
		}
	}

	fstb_cal_powerhal_fps();

	/* check idle twice to avoid fstb_active ping-pong */
	if (idle)
		fstb_idle_cnt++;
	else
		fstb_idle_cnt = 0;

	if (fstb_idle_cnt >= FSTB_IDLE_DBNC) {
		fstb_active_dbncd = 0;
		fstb_idle_cnt = 0;
	} else if (fstb_idle_cnt >= 2) {
		fstb_active = 0;
	}

	if (fstb_active)
		fstb_active2xgf = 1;
	else
		fstb_active2xgf = 0;

	if (fstb_enable && fstb_active_dbncd)
		enable_fstb_timer();
	else
		disable_fstb_timer();

	mutex_unlock(&fstb_lock);

	if (eara_pre_change_fp)
		eara_ret = eara_pre_change_fp();

	if (eara_ret == -1) {
		mutex_lock(&fstb_lock);
		hlist_for_each_entry_safe(iter, n, &fstb_frame_infos, hlist) {
			if (!iter->target_fps_notifying
				|| iter->target_fps_notifying == -1)
				continue;
			if (!iter->self_ctrl_fps_enable)
				iter->target_fps = iter->target_fps_notifying;
			else
				iter->target_fps_v2 = iter->target_fps_notifying;
			iter->target_fps_notifying = 0;
			fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
					0, "fstb_notifying");
			fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
					iter->target_fps_v2, "fstb_target_fps1");
		}
		mutex_unlock(&fstb_lock);
	}


	fpsgo_check_thread_status();
	fpsgo_fstb2xgf_do_recycle(fstb_active2xgf);
}

struct video_info *fstb_search_and_add_video_info(int pid, int add_node)
{
	struct rb_node **rb_ptr = &video_pid_tree.rb_node;
	struct rb_node *parent = NULL;
	struct video_info *ptr_video_info = NULL;

	WARN_ON(!mutex_is_locked(&fstb_video_pid_tree_lock));

	while (*rb_ptr) {
		parent = *rb_ptr;
		ptr_video_info = rb_entry(parent, struct video_info, entry);

		if (pid < ptr_video_info->pid)
			rb_ptr = &(*rb_ptr)->rb_left;
		else if (pid > ptr_video_info->pid)
			rb_ptr = &(*rb_ptr)->rb_right;
		else
			break;
	}

	/* search only */
	if (!add_node) {
		if (*rb_ptr)
			return ptr_video_info;
		else
			return NULL;
	}

	/* add node */
	if (*rb_ptr) {
		ptr_video_info->count_instance++;
		return ptr_video_info;
	}

	ptr_video_info = kzalloc(sizeof(*ptr_video_info), GFP_KERNEL);
	ptr_video_info->pid = pid;
	ptr_video_info->count_instance = 1U;
	rb_link_node(&ptr_video_info->entry, parent, rb_ptr);
	rb_insert_color(&ptr_video_info->entry, &video_pid_tree);

	return ptr_video_info;
}

void fstb_delete_video_info(int pid)
{
	struct video_info *video_info_instance;

	WARN_ON(!mutex_is_locked(&fstb_video_pid_tree_lock));

	video_info_instance = fstb_search_and_add_video_info(pid, 0);

	if (!video_info_instance)
		return;

	if (video_info_instance->count_instance > 1) {
		video_info_instance->count_instance--;
		fpsgo_systrace_c_fstb_man(-100, 0,
			video_info_instance->count_instance, "video_pid[%d]", pid);
		return;
	}
	fpsgo_systrace_c_fstb_man(-100, 0, 0, "video_pid[%d]", pid);
	rb_erase(&video_info_instance->entry, &video_pid_tree);
	kfree(video_info_instance);
}

static int set_soft_fps_level(struct fps_level level)
{
	mutex_lock(&fstb_lock);

	if (level.end > level.start)
		goto set_fps_level_err;

	fps_global_level.start = level.start;
	fps_global_level.end = level.end;

	max_fps_limit = min(dfps_ceiling, fps_global_level.start);
	min_fps_limit = min(dfps_ceiling, fps_global_level.end);

	mutex_unlock(&fstb_lock);

	return 0;
set_fps_level_err:
	mutex_unlock(&fstb_lock);
	return -EINVAL;
}

static void reset_fps_level(void)
{
	struct fps_level global_level;

	global_level.start = CFG_MAX_FPS_LIMIT;
	global_level.end = CFG_MIN_FPS_LIMIT;

	set_soft_fps_level(global_level);
}

#define FSTB_SYSFS_READ(name, show, variable); \
static ssize_t name##_show(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		char *buf) \
{ \
	if ((show)) \
		return scnprintf(buf, PAGE_SIZE, "%d\n", (variable)); \
	else \
		return 0; \
}

#define FSTB_SYSFS_WRITE_VALUE(name, variable, min, max); \
static ssize_t name##_store(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		const char *buf, size_t count) \
{ \
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE]; \
	int arg; \
\
	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) { \
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) { \
			if (kstrtoint(acBuffer, 0, &arg) == 0) { \
				if (arg >= (min) && arg <= (max)) { \
					mutex_lock(&fstb_lock); \
					(variable) = arg; \
					mutex_unlock(&fstb_lock); \
				} \
			} \
		} \
	} \
\
	return count; \
}

#define FSTB_SYSFS_WRITE_POLICY_CMD(name, cmd, min, max); \
static ssize_t name##_store(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		const char *buf, size_t count) \
{ \
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE]; \
	int tgid; \
	int arg; \
	unsigned long long ts = fpsgo_get_time(); \
\
	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) { \
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) { \
			if (sscanf(acBuffer, "%d %d", &tgid, &arg) == 2) { \
				mutex_lock(&fstb_policy_cmd_lock); \
				if (arg >= (min) && arg <= (max)) \
					fstb_set_policy_cmd(cmd, arg, tgid, ts, 1); \
				else \
					fstb_set_policy_cmd(cmd, 0, tgid, ts, 0); \
				mutex_unlock(&fstb_policy_cmd_lock); \
			} \
		} \
	} \
\
	return count; \
}

/*
 *	PowerHAL API for videoplayback hint.
 *
 *	Two write-only nodes to set/clear video pid to the list:
 *		1. set_video_pid
 *		2. clear_video_pid
 *		e.g.
 *			echo $pid > set_video_pid
 *	One read-only node to show the video pid list:
 *		1. fstb_video_pid_list
 *		e.g.
 *			cat fstb_video_pid_list
 */

static ssize_t fstb_video_pid_list_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct rb_node *n;
	struct video_info *video_instance;
	char pid_str[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int pos = 0;
	int length;

	length = scnprintf(pid_str + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
	"tgid\tcount_instance\n");
	pos += length;

	rcu_read_lock();
	for (n = rb_first(&video_pid_tree); n != NULL; n = rb_next(n)) {
		video_instance = rb_entry(n, struct video_info, entry);
		length = scnprintf(pid_str + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%d\t%u\n", video_instance->pid, video_instance->count_instance);
		pos += length;
	}
	rcu_read_unlock();
	return scnprintf(buf, PAGE_SIZE, "%s", pid_str);
}
static KOBJ_ATTR_RO(fstb_video_pid_list);

static ssize_t set_video_pid_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg = -1;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) != 0)
				goto out;
			mtk_fstb_dprintk_always("%s %d\n", __func__, arg);
			if (arg != -1)
				fstb_set_video_pid(arg);
		}
	}

out:
	return count;
}

static ssize_t set_video_pid_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct rb_node *n;
	struct video_info *video_instance;
	char pid_str[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int pos = 0;
	int length;

	length = scnprintf(pid_str + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
	"tgid\tcount_instance\n");
	pos += length;

	rcu_read_lock();
	for (n = rb_first(&video_pid_tree); n != NULL; n = rb_next(n)) {
		video_instance = rb_entry(n, struct video_info, entry);
		length = scnprintf(pid_str + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%d\t%u\n", video_instance->pid, video_instance->count_instance);
		pos += length;
	}
	rcu_read_unlock();
	return scnprintf(buf, PAGE_SIZE, "%s", pid_str);
}

static KOBJ_ATTR_RW(set_video_pid);

static ssize_t clear_video_pid_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg = -1;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) != 0)
				goto out;
			mtk_fstb_dprintk_always("%s %d\n", __func__, arg);
			if (arg != -1)
				fstb_clear_video_pid(arg);
		}
	}

out:
	return count;
}

static ssize_t clear_video_pid_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct rb_node *n;
	struct video_info *video_instance;
	char pid_str[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int pos = 0;
	int length;

	length = scnprintf(pid_str + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
	"tgid\tcount_instance\n");
	pos += length;

	rcu_read_lock();
	for (n = rb_first(&video_pid_tree); n != NULL; n = rb_next(n)) {
		video_instance = rb_entry(n, struct video_info, entry);
		length = scnprintf(pid_str + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%d\t%u\n", video_instance->pid, video_instance->count_instance);
		pos += length;
	}
	rcu_read_unlock();
	return scnprintf(buf, PAGE_SIZE, "%s", pid_str);
}

static KOBJ_ATTR_RW(clear_video_pid);

FSTB_SYSFS_READ(set_render_max_fps, 0, 0);

static ssize_t set_render_max_fps_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;
	int ret = 0;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) != 0)
				goto out;
			mtk_fstb_dprintk_always("%s %d\n", __func__, arg);
			fpsgo_systrace_c_fstb_man(arg > 0 ? arg : -arg,
				0, arg > 0, "force_max_fps");
			if (arg > 0)
				ret = switch_thread_max_fps(arg, 1);
			else
				ret = switch_thread_max_fps(-arg, 0);
		}
	}

out:
	return count;
}

static KOBJ_ATTR_RW(set_render_max_fps);

static ssize_t fstb_soft_level_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "1 %d-%d\n",
		fps_global_level.start, fps_global_level.end);
}

static ssize_t fstb_soft_level_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	char *sepstr, *substr;
	int start_fps, end_fps;
	struct fps_level new_fps_global_level;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			acBuffer[count] = '\0';
			sepstr = acBuffer;

			substr = strsep(&sepstr, " ");
			substr = sepstr;
			if (!substr)
				goto err;

			if (strchr(substr, '-')) {
				if (sscanf(substr, "%d-%d",
					&start_fps, &end_fps) != 2)
					goto err;
				new_fps_global_level.start = start_fps;
				new_fps_global_level.end = end_fps;
			} else {
				if (kstrtoint(substr,
					10, &start_fps) != 0)
					goto err;
				new_fps_global_level.start = start_fps;
				new_fps_global_level.end = start_fps;
			}

			set_soft_fps_level(new_fps_global_level);
		}
	}

err:
	return count;
}

static KOBJ_ATTR_RW(fstb_soft_level);

static ssize_t fstb_fps_list_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int i;
	struct FSTB_RENDER_TARGET_FPS *rtfiter = NULL;
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE] = "";
	int pos = 0;
	int length;


	hlist_for_each_entry(rtfiter, &fstb_render_target_fps, hlist) {
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%s %d %d ",
			rtfiter->process_name,
			rtfiter->pid,
			rtfiter->nr_level);
		pos += length;
		for (i = 0; i < rtfiter->nr_level; i++) {
			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"%d-%d ",
				rtfiter->level[i].start,
				rtfiter->level[i].end);
			pos += length;
		}
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"\n");
		pos += length;
	}

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static ssize_t fstb_fps_list_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	char *sepstr, *substr;
	char proc_name[16];
	int i;
	int  nr_level, start_fps, end_fps;
	int mode = 1;
	int pid = 0;
	int ret = 0;
	struct fps_level level[MAX_NR_RENDER_FPS_LEVELS];

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			acBuffer[count] = '\0';
			sepstr = acBuffer;

			substr = strsep(&sepstr, " ");
			if (!substr || !strncpy(proc_name, substr, 16)) {
				ret = -EINVAL;
				goto err;
			}
			proc_name[15] = '\0';

			if (kstrtoint(proc_name, 10, &pid) != 0)
				mode = 0; /* process mode*/

			substr = strsep(&sepstr, " ");

			if (!substr || kstrtoint(substr, 10, &nr_level) != 0 ||
					nr_level > MAX_NR_RENDER_FPS_LEVELS ||
					nr_level < 0) {
				ret = -EINVAL;
				goto err;
			}

			for (i = 0; i < nr_level; i++) {
				substr = strsep(&sepstr, " ");
				if (!substr) {
					ret = -EINVAL;
					goto err;
				}

				if (sscanf(substr, "%d-%d",
					&start_fps, &end_fps) != 2) {
					ret = -EINVAL;
					goto err;
				}
				level[i].start = start_fps;
				level[i].end = end_fps;
			}

			if (mode == 0) {
				if (switch_process_fps_range(proc_name,
					nr_level, level))
					ret = -EINVAL;
			} else {
				if (switch_thread_fps_range(pid,
					nr_level, level))
					ret = -EINVAL;
			}

		}
	}

err:
	return count;
}

static KOBJ_ATTR_RW(fstb_fps_list);

FSTB_SYSFS_READ(jump_check_q_pct, 1, JUMP_CHECK_Q_PCT);
FSTB_SYSFS_WRITE_VALUE(jump_check_q_pct, JUMP_CHECK_Q_PCT, 1, 100);

static KOBJ_ATTR_RW(jump_check_q_pct);

FSTB_SYSFS_READ(jump_check_num, 1, JUMP_CHECK_NUM);
FSTB_SYSFS_WRITE_VALUE(jump_check_num, JUMP_CHECK_NUM, 0, JUMP_VOTE_MAX_I);

static KOBJ_ATTR_RW(jump_check_num);

FSTB_SYSFS_READ(margin_mode, 1, margin_mode);
FSTB_SYSFS_WRITE_VALUE(margin_mode, margin_mode, 1, INT_MAX);

static KOBJ_ATTR_RW(margin_mode);

FSTB_SYSFS_READ(margin_mode_dbnc_a, 1, margin_mode_dbnc_a);
FSTB_SYSFS_WRITE_VALUE(margin_mode_dbnc_a, margin_mode_dbnc_a, 1, INT_MAX);

static KOBJ_ATTR_RW(margin_mode_dbnc_a);

FSTB_SYSFS_READ(margin_mode_dbnc_b, 1, margin_mode_dbnc_b);
FSTB_SYSFS_WRITE_VALUE(margin_mode_dbnc_b, margin_mode_dbnc_b, 1, INT_MAX);

static KOBJ_ATTR_RW(margin_mode_dbnc_b);

FSTB_SYSFS_READ(margin_mode_gpu, 1, margin_mode_gpu);
FSTB_SYSFS_WRITE_VALUE(margin_mode_gpu, margin_mode_gpu, 1, INT_MAX);

static KOBJ_ATTR_RW(margin_mode_gpu);

FSTB_SYSFS_READ(margin_mode_gpu_dbnc_a, 1, margin_mode_gpu_dbnc_a);
FSTB_SYSFS_WRITE_VALUE(margin_mode_gpu_dbnc_a, margin_mode_gpu_dbnc_a, 1, INT_MAX);

static KOBJ_ATTR_RW(margin_mode_gpu_dbnc_a);

FSTB_SYSFS_READ(margin_mode_gpu_dbnc_b, 1, margin_mode_gpu_dbnc_b);
FSTB_SYSFS_WRITE_VALUE(margin_mode_gpu_dbnc_b, margin_mode_gpu_dbnc_b, 1, INT_MAX);

static KOBJ_ATTR_RW(margin_mode_gpu_dbnc_b);

FSTB_SYSFS_READ(fstb_reset_tolerence, 1, RESET_TOLERENCE);
FSTB_SYSFS_WRITE_VALUE(fstb_reset_tolerence, RESET_TOLERENCE, 0, INT_MAX);

static KOBJ_ATTR_RW(fstb_reset_tolerence);

FSTB_SYSFS_READ(fstb_tune_quantile, 1, QUANTILE);
FSTB_SYSFS_WRITE_VALUE(fstb_tune_quantile, QUANTILE, 0, 100);

static KOBJ_ATTR_RW(fstb_tune_quantile);

FSTB_SYSFS_READ(fstb_tune_error_threshold, 1, fps_error_threshold);
FSTB_SYSFS_WRITE_VALUE(fstb_tune_error_threshold, fps_error_threshold, 0, 100);

static KOBJ_ATTR_RW(fstb_tune_error_threshold);

FSTB_SYSFS_READ(adopt_low_fps, 1, adopt_low_fps);
FSTB_SYSFS_WRITE_VALUE(adopt_low_fps, adopt_low_fps, 0, 1);

static KOBJ_ATTR_RW(adopt_low_fps);

FSTB_SYSFS_READ(fstb_fps_bypass_min, 1, fps_bypass_min);
FSTB_SYSFS_WRITE_VALUE(fstb_fps_bypass_min, fps_bypass_min, 0, 200);

static KOBJ_ATTR_RW(fstb_fps_bypass_min);

FSTB_SYSFS_READ(fstb_fps_bypass_max, 1, fps_bypass_max);
FSTB_SYSFS_WRITE_VALUE(fstb_fps_bypass_max, fps_bypass_max, 0, 200);

static KOBJ_ATTR_RW(fstb_fps_bypass_max);

FSTB_SYSFS_READ(fstb_self_ctrl_fps_enable, 1, fstb_self_ctrl_fps_enable);
FSTB_SYSFS_WRITE_VALUE(fstb_self_ctrl_fps_enable, fstb_self_ctrl_fps_enable, 0, 1);

static KOBJ_ATTR_RW(fstb_self_ctrl_fps_enable);

static ssize_t fstb_debug_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int pos = 0;
	int length;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"fstb_enable %d\n", fstb_enable);
	pos += length;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"fstb_log %d\n", fstb_fps_klog_on);
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"fstb_active %d\n", fstb_active);
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"fstb_active_dbncd %d\n", fstb_active_dbncd);
	pos += length;
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"fstb_idle_cnt %d\n", fstb_idle_cnt);
	pos += length;

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static ssize_t fstb_debug_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int k_enable, klog_on;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (sscanf(acBuffer, "%d %d",
				&k_enable, &klog_on) >= 1) {
				if (k_enable == 0 || k_enable == 1)
					fpsgo_ctrl2fstb_switch_fstb(k_enable);

				if (klog_on == 0 || klog_on == 1)
					fstb_fps_klog_on = klog_on;
			}
		}
	}

	return count;
}
static KOBJ_ATTR_RW(fstb_debug);


static ssize_t fpsgo_status_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct FSTB_FRAME_INFO *iter;
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int pos = 0;
	int length;

	mutex_lock(&fstb_lock);

	if (!fstb_enable) {
		mutex_unlock(&fstb_lock);
		return 0;
	}

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
	"tid\tbufID\t\tname\t\tcurrentFPS\ttargetFPS\tFPS_margin\tFPS_margin_GPU\tFPS_margin_thrs\tsbe_state\tHWUI\tt_gpu\t\tpolicy\n");
	pos += length;

	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter) {
			length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
					"%d\t0x%llx\t%s\t%d\t\t%d\t\t%d\t\t%d\t\t%d\t\t%d\t\t%d\t%lld\t\t(%d,%d,%d)\n",
					iter->pid,
					iter->bufid,
					iter->proc_name,
					iter->queue_fps > max_fps_limit ?
					max_fps_limit : iter->queue_fps,
					iter->self_ctrl_fps_enable ?
					iter->target_fps_v2 : iter->target_fps,
					iter->self_ctrl_fps_enable ?
					iter->target_fps_margin_v2 : iter->target_fps_margin,
					iter->self_ctrl_fps_enable ?
					-1 : iter->target_fps_margin_gpu,
					iter->self_ctrl_fps_enable ?
					iter->target_fps_margin_v2 : iter->target_fps_margin2,
					iter->sbe_state,
					iter->hwui_flag,
					iter->gpu_time,
					iter->self_ctrl_fps_enable,
					iter->tfb_enable,
					iter->notify_target_fps);
			pos += length;
		}
	}

	mutex_unlock(&fstb_lock);

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"dfps_ceiling:%d\n", dfps_ceiling);
	pos += length;

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}
static KOBJ_ATTR_ROO(fpsgo_status);

static ssize_t fstb_policy_cmd_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE] = "";
	int pos = 0;
	int length;
	struct FSTB_POLICY_CMD *iter;
	struct rb_root *rbr;
	struct rb_node *rbn;

	mutex_lock(&fstb_policy_cmd_lock);

	rbr = &fstb_policy_cmd_tree;
	for (rbn = rb_first(rbr); rbn; rbn = rb_next(rbn)) {
		iter = rb_entry(rbn, struct FSTB_POLICY_CMD, rb_node);
		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"tgid:%d\tfstb_self_ctrl_fps_enable:%d\ttfb_enable:%d\tnotify_target_fps:%d\tts:%llu\n",
			iter->tgid,
			iter->self_ctrl_fps_enable,
			iter->tfb_enable,
			iter->notify_target_fps,
			iter->ts);
		pos += length;
	}

	mutex_unlock(&fstb_policy_cmd_lock);

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static KOBJ_ATTR_RO(fstb_policy_cmd);

FSTB_SYSFS_READ(fstb_self_ctrl_fps_enable_by_pid, 0, 0);
FSTB_SYSFS_WRITE_POLICY_CMD(fstb_self_ctrl_fps_enable_by_pid, 0, 1, 1);

static KOBJ_ATTR_RW(fstb_self_ctrl_fps_enable_by_pid);

FSTB_SYSFS_READ(notify_fstb_target_fps_by_pid, 0, 0);
FSTB_SYSFS_WRITE_POLICY_CMD(notify_fstb_target_fps_by_pid, 2, 1, CFG_MAX_FPS_LIMIT);

static KOBJ_ATTR_RW(notify_fstb_target_fps_by_pid);

int mtk_fstb_init(void)
{
	mtk_fstb_dprintk_always("init\n");
#if defined(CONFIG_MTK_GPU_COMMON_DVFS_SUPPORT)
	ged_kpi_output_gfx_info2_fp = gpu_time_update;
#endif

	if (!fpsgo_sysfs_create_dir(NULL, "fstb", &fstb_kobj)) {
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fpsgo_status);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_debug);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_tune_error_threshold);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_tune_quantile);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_margin_mode_dbnc_b);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_margin_mode_dbnc_a);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_margin_mode);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_margin_mode_gpu_dbnc_b);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_margin_mode_gpu_dbnc_a);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_margin_mode_gpu);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_reset_tolerence);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_fps_list);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_soft_level);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_jump_check_num);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_jump_check_q_pct);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_adopt_low_fps);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_set_render_max_fps);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_self_ctrl_fps_enable);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_fps_bypass_max);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_fps_bypass_min);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_set_video_pid);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_clear_video_pid);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_video_pid_list);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_policy_cmd);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_self_ctrl_fps_enable_by_pid);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_notify_fstb_target_fps_by_pid);
	}

	reset_fps_level();

	wq = alloc_ordered_workqueue("%s", WQ_MEM_RECLAIM | WQ_HIGHPRI, "mt_fstb");
	if (!wq)
		goto err;

	hrtimer_init(&hrt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrt.function = &mt_fstb;

	fstb_policy_cmd_tree = RB_ROOT;

	mtk_fstb_dprintk_always("init done\n");

	return 0;

err:
	return -1;
}

int __exit mtk_fstb_exit(void)
{
	mtk_fstb_dprintk("exit\n");

	disable_fstb_timer();

	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fpsgo_status);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_debug);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_tune_error_threshold);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_tune_quantile);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_margin_mode_dbnc_b);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_margin_mode_dbnc_a);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_margin_mode);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_margin_mode_gpu_dbnc_b);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_margin_mode_gpu_dbnc_a);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_margin_mode_gpu);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_reset_tolerence);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_fps_list);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_soft_level);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_jump_check_num);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_jump_check_q_pct);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_adopt_low_fps);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_set_render_max_fps);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_self_ctrl_fps_enable);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_fps_bypass_max);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_fps_bypass_min);
	fpsgo_sysfs_remove_file(fstb_kobj,
				&kobj_attr_set_video_pid);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_clear_video_pid);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_video_pid_list);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_policy_cmd);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_self_ctrl_fps_enable_by_pid);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_notify_fstb_target_fps_by_pid);

	fpsgo_sysfs_remove_dir(&fstb_kobj);

	return 0;
}
