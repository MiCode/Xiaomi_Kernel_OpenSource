// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
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
#include <asm/div64.h>
#include <mt-plat/fpsgo_common.h>
#include "../fbt/include/fbt_cpu.h"
#include "../fbt/include/xgf.h"
#include "fpsgo_base.h"
#include "fpsgo_sysfs.h"
#include "fstb.h"
#include "fstb_usedext.h"
#include "eara_job.h"
#include "fpsgo_usedext.h"
#include "tchbst.h"
#include "syslimiter.h"

#include <mt-plat/mtk_perfobserver.h>

#ifdef CONFIG_MTK_GPU_SUPPORT
#include "ged_kpi.h"
#endif

#define mtk_fstb_dprintk_always(fmt, args...) \
	pr_debug("[FSTB]" fmt, ##args)

#define mtk_fstb_dprintk(fmt, args...) \
	do { \
		if (fstb_fps_klog_on == 1) \
			pr_debug("[FSTB]" fmt, ##args); \
	} while (0)

#define fpsgo_systrace_c_fstb_man(pid, val, fmt...) \
	fpsgo_systrace_c(FPSGO_DEBUG_MANDATORY, pid, val, fmt)


static struct kobject *fstb_kobj;
static int max_fps_limit = DEFAULT_DFPS;
static int dfps_ceiling = DEFAULT_DFPS;
static int min_fps_limit = CFG_MIN_FPS_LIMIT;
static int fps_error_threshold = 10;
static int QUANTILE = 50;
static long long FRAME_TIME_WINDOW_SIZE_US = 1000000;
static long long ADJUST_INTERVAL_US = 1000000;
static int margin_mode;
static int margin_mode_gpu;
static int margin_mode_dbnc_a = 9;
static int margin_mode_dbnc_b = 1;
static int margin_mode_gpu_dbnc_a = 9;
static int margin_mode_gpu_dbnc_b = 1;
static int JUMP_CHECK_NUM = DEFAULT_JUMP_CHECK_NUM;
static int JUMP_CHECK_Q_PCT = DEFAULT_JUMP_CHECK_Q_PCT;
static int adopt_low_fps = 1;
static int condition_get_fps;

DECLARE_WAIT_QUEUE_HEAD(queue);

static void fstb_fps_stats(struct work_struct *work);
static DECLARE_WORK(fps_stats_work,
		(void *) fstb_fps_stats);
static HLIST_HEAD(fstb_frame_infos);
static HLIST_HEAD(fstb_render_target_fps);

static struct hrtimer hrt;
static struct workqueue_struct *wq;

static struct fps_level fps_levels[MAX_NR_FPS_LEVELS];
static int nr_fps_levels = MAX_NR_FPS_LEVELS;

static int fstb_fps_klog_on;
static int fstb_enable, fstb_active, fstb_active_dbncd, fstb_idle_cnt;
static long long last_update_ts;

static void reset_fps_level(void);
static int set_soft_fps_level(int nr_level,
		struct fps_level *level);

static DEFINE_MUTEX(fstb_lock);
static DEFINE_MUTEX(fstb_fps_active_time);
static DEFINE_MUTEX(fstb_cam_active_time);

void (*gbe_fstb2gbe_poll_fp)(struct hlist_head *list);

static int arch_get_nr_clusters(void)
{
	return arch_nr_clusters();
}

static void enable_fstb_timer(void)
{
	ktime_t ktime;

	ktime = ktime_set(0,
			ADJUST_INTERVAL_US * 1000);
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



int fpsgo_ctrl2fstb_gblock(int tid, int start)
{
	struct FSTB_FRAME_INFO *iter;
	ktime_t cur_time;
	unsigned long long cur_time_ns;

	cur_time = ktime_get();
	cur_time_ns = ktime_to_ns(cur_time);

	mutex_lock(&fstb_lock);

	if (!fstb_enable) {
		mutex_unlock(&fstb_lock);
		return 0;
	}

	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter->pid == tid)
			break;
	}

	if (iter == NULL) {
		mutex_unlock(&fstb_lock);
		return 0;
	}

	fpsgo_systrace_c_fstb(tid, 0, start, "gblock");

	/* end */
	if (!start && iter->gblock_b)
		iter->gblock_time += cur_time_ns - iter->gblock_b;

	/* start */
	if (start)
		iter->gblock_b = cur_time_ns;

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
		syslimiter_update_dfrc_fps(-1);
		dram_ctl_update_dfrc_fps(dfps_ceiling);
		hlist_for_each_entry_safe(iter, t,
				&fstb_frame_infos, hlist) {
			hlist_del(&iter->hlist);
			vfree(iter);
		}
		pob_fpsgo_qtsk_update(POB_FPSGO_QTSK_DELALL, NULL);
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

int switch_sample_window(long long time_usec)
{
	if (time_usec < 0 ||
			time_usec > 1000000 * FRAME_TIME_BUFFER_SIZE / 120)
		return -EINVAL;

	FRAME_TIME_WINDOW_SIZE_US = ADJUST_INTERVAL_US = time_usec;

	return 0;
}

int switch_margin_mode(int mode)
{
	if (mode > 2 || mode < 0)
		return -EINVAL;

	mutex_lock(&fstb_lock);
	if (mode != margin_mode)
		margin_mode = mode;
	mutex_unlock(&fstb_lock);

	return 0;
}

int switch_margin_mode_gpu(int mode)
{
	if (mode > 2 || mode < 0)
		return -EINVAL;

	mutex_lock(&fstb_lock);
	if (mode != margin_mode_gpu)
		margin_mode_gpu = mode;
	mutex_unlock(&fstb_lock);

	return 0;
}

int switch_margin_mode_dbnc_a(int val)
{
	if (val < 1)
		return -EINVAL;

	mutex_lock(&fstb_lock);
	if (val != margin_mode_dbnc_a)
		margin_mode_dbnc_a = val;
	mutex_unlock(&fstb_lock);

	return 0;
}

int switch_margin_mode_gpu_dbnc_a(int val)
{
	if (val < 1)
		return -EINVAL;

	mutex_lock(&fstb_lock);
	if (val != margin_mode_gpu_dbnc_a)
		margin_mode_gpu_dbnc_a = val;
	mutex_unlock(&fstb_lock);

	return 0;
}

int switch_margin_mode_dbnc_b(int val)
{
	if (val < 1)
		return -EINVAL;

	mutex_lock(&fstb_lock);
	if (val != margin_mode_dbnc_b)
		margin_mode_dbnc_b = val;
	mutex_unlock(&fstb_lock);

	return 0;
}


int switch_margin_mode_gpu_dbnc_b(int val)
{
	if (val < 1)
		return -EINVAL;

	mutex_lock(&fstb_lock);
	if (val != margin_mode_gpu_dbnc_b)
		margin_mode_gpu_dbnc_b = val;
	mutex_unlock(&fstb_lock);

	return 0;
}

int switch_jump_check_num(int val)
{
	if (val < 1 || val > JUMP_VOTE_MAX_I)
		return -EINVAL;

	mutex_lock(&fstb_lock);
	if (val != JUMP_CHECK_NUM)
		JUMP_CHECK_NUM = val;
	mutex_unlock(&fstb_lock);

	return 0;
}

int switch_jump_check_q_pct(int val)
{
	if (val < 1 || val > 100)
		return -EINVAL;

	mutex_lock(&fstb_lock);
	if (val != JUMP_CHECK_Q_PCT)
		JUMP_CHECK_Q_PCT = val;
	mutex_unlock(&fstb_lock);

	return 0;
}

int switch_fps_range(int nr_level, struct fps_level *level)
{
	if (nr_level != 1)
		return 1;

	if (!set_soft_fps_level(nr_level, level))
		return 0;
	else
		return 1;
}

int switch_thread_max_fps(int pid, int set_max)
{
	struct FSTB_FRAME_INFO *iter;
	int ret = 0;

	mutex_lock(&fstb_lock);
	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter->pid != pid)
			continue;

		switch (iter->sbe_state) {
		case -1:
			ret = -1;
			break;
		case 0:
		case 1:
			iter->sbe_state = set_max;
			break;
		default:
			break;
		}
		fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
				set_max, "sbe_set_max_fps");
		fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
				iter->sbe_state, "sbe_state");
	}

	mutex_unlock(&fstb_lock);

	return ret;
}

int switch_thread_no_ctrl(int pid, int set_no_ctrl)
{
	struct FSTB_FRAME_INFO *iter;
	int ret = 0;

	mutex_lock(&fstb_lock);
	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter->pid != pid)
			continue;
		switch (iter->sbe_state) {
		case -1:
		case 0:
			iter->sbe_state = set_no_ctrl ? -1 : 0;
			break;
		case 1:
			iter->sbe_state = set_no_ctrl ? -1 : 1;
			break;
		default:
			break;
		}
		fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
				set_no_ctrl, "sbe_set_no_ctrl");
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

int switch_fps_error_threhosld(int threshold)
{
	if (threshold < 0 || threshold > 100)
		return -EINVAL;

	fps_error_threshold = threshold;

	return 0;
}

int switch_percentile_frametime(int ratio)
{
	if (ratio < 0 ||  ratio > 100)
		return -EINVAL;

	QUANTILE = ratio;

	return 0;
}

static int cmplonglong(const void *a, const void *b)
{
	return *(long long *)a - *(long long *)b;
}

void fpsgo_ctrl2fstb_dfrc_fps(int fps)
{
	mutex_lock(&fstb_lock);

	if (fps <= CFG_MAX_FPS_LIMIT && fps >= CFG_MIN_FPS_LIMIT &&
		nr_fps_levels >= 1) {
		dfps_ceiling = fps;
		max_fps_limit = min(dfps_ceiling,
				fps_levels[0].start);
		min_fps_limit =	min(dfps_ceiling,
				fps_levels[nr_fps_levels - 1].end);

		mutex_unlock(&fstb_lock);
	} else
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

	{
		struct pob_fpsgo_qtsk_info pfqi = {0};

		pfqi.tskid = iter->pid;
		pfqi.cur_gpu_cap = cur_freq;
		pfqi.max_gpu_cap = cur_max_freq;

		pob_fpsgo_qtsk_update(POB_FPSGO_QTSK_GPUCAP_UPDATE, &pfqi);
	}

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

	fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid, ret,
			"quantile_weighted_gpu_time");
	return ret;

}

int fstb_is_cam_active;
void eara2fstb_get_tfps(int max_cnt, int *is_camera, int *pid, unsigned long long *buf_id,
						int *tfps, int *rfps, int *hwui, char name[][16])
{
	int count = 0;
	struct FSTB_FRAME_INFO *iter;
	struct hlist_node *n;

	mutex_lock(&fstb_lock);
	*is_camera = fstb_is_cam_active;

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
			|| iter->target_fps_notifying == -1)
			tfps[count] = iter->target_fps;
		else
			tfps[count] = iter->target_fps_notifying;
		if (name)
			strncpy(name[count], iter->proc_name, 16);
		count++;
	}

	mutex_unlock(&fstb_lock);
}

void eara2fstb_tfps_mdiff(int pid, unsigned long long buf_id, int diff,
				int tfps)
{
	struct FSTB_FRAME_INFO *iter;
	struct hlist_node *n;

	mutex_lock(&fstb_lock);

	hlist_for_each_entry_safe(iter, n, &fstb_frame_infos, hlist) {
		if (pid == iter->pid && buf_id == iter->bufid) {
			if (tfps != iter->target_fps_notifying &&
				 tfps != iter->target_fps)
				break;

			iter->target_fps_diff = diff;
			fpsgo_systrace_c_fstb_man(pid, buf_id, diff, "eara_diff");

			if (iter->target_fps_notifying
				&& tfps == iter->target_fps_notifying) {
				iter->target_fps = iter->target_fps_notifying;
				iter->target_fps_notifying = 0;
				fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
					iter->target_fps, "fstb_target_fps1");
				fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
					0, "fstb_notifying");
			}
			break;
		}
	}

	mutex_unlock(&fstb_lock);
}

int (*eara_pre_change_fp)(void);
int (*eara_pre_change_single_fp)(int pid, unsigned long long bufID,
			int target_fps);
static void fstb_change_tfps(struct FSTB_FRAME_INFO *iter, int target_fps,
		int notify_eara)
{
	int ret = -1;

	if (notify_eara && eara_pre_change_single_fp)
		ret = eara_pre_change_single_fp(iter->pid, iter->bufid, target_fps);

	if ((notify_eara && (ret == -1))
		|| iter->target_fps_notifying == target_fps) {
		iter->target_fps = target_fps;
		iter->target_fps_notifying = 0;
		fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
					0, "fstb_notifying");
		fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
					iter->target_fps, "fstb_target_fps1");
	} else {
		iter->target_fps_notifying = target_fps;
		fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
			iter->target_fps_notifying, "fstb_notifying");
	}
}

void (*eara_thrm_frame_start_fp)(int pid, unsigned long long bufID,
	int cpu_time, int vpu_time, int mdla_time,
	int cpu_cap, int vpu_cap, int mdla_cap,
	int queuefps, unsigned long long q2q_time,
	int AI_cross_vpu, int AI_cross_mdla, int AI_bg_vpu,
	int AI_bg_mdla, ktime_t cur_time);

int fpsgo_fbt2fstb_update_cpu_frame_info(
		int pid,
		unsigned long long bufID,
		int tgid,
		int frame_type,
		unsigned long long Q2Q_time,
		long long Runnging_time,
		unsigned int Curr_cap,
		unsigned int Max_cap,
		unsigned long long mid)
{
	long long cpu_time_ns = (long long)Runnging_time;
	unsigned int max_current_cap = Curr_cap;
	unsigned int max_cpu_cap = Max_cap;
	unsigned long long wct = 0, wvt = 0, wmt = 0;
	long long vpu_time_ns = 0, mdla_time_ns = 0;
	int vpu_boost = 0, mdla_boost = 0;
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

	{
		struct pob_fpsgo_qtsk_info pfqi = {0};

		pfqi.tskid = pid;
		pfqi.cur_cpu_cap = Curr_cap;
		pfqi.max_cpu_cap = Max_cap;
		pfqi.rescue_cpu = 0;

		pob_fpsgo_qtsk_update(POB_FPSGO_QTSK_CPUCAP_UPDATE, &pfqi);
	}

	if (Curr_cap)
		notify_touch(3);

	if (mid && cpu_time_ns >= 0) {
		fpsgo_fstb2eara_get_exec_time(tgid, mid,
			&vpu_time_ns, &mdla_time_ns);
		fpsgo_fstb2eara_get_boost_value(tgid, mid,
			&vpu_boost, &mdla_boost);
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

	if (vpu_time_ns && vpu_boost > 0 && vpu_boost <= VPU_MAX_CAP) {
		wvt = vpu_time_ns * vpu_boost;
		do_div(wvt, VPU_MAX_CAP);
		fpsgo_systrace_c_fstb_man(pid, iter->bufid, (int)wvt,
			"weighted_vpu_time");
	}

	if (mdla_time_ns && mdla_boost > 0 && mdla_boost <= MDLA_MAX_CAP) {
		wmt = mdla_time_ns * mdla_boost;
		do_div(wmt, MDLA_MAX_CAP);
		fpsgo_systrace_c_fstb_man(pid, iter->bufid, (int)wmt,
			"weighted_mdla_time");
	}

	iter->weighted_cpu_time[iter->weighted_cpu_time_end] =
		wct + wvt + wmt;

	iter->weighted_cpu_time_ts[iter->weighted_cpu_time_end] =
		cur_time_us;
	iter->weighted_cpu_time_end++;

out:
	mtk_fstb_dprintk(
	"pid %d fstb: time %lld %lld cpu_time_ns %lld max_current_cap %u max_cpu_cap %u\n"
	, pid, cur_time_us, ktime_to_us(ktime_get())-cur_time_us,
	cpu_time_ns, max_current_cap, max_cpu_cap);

	iter->m_c_time = (iter->m_c_time + cpu_time_ns) / 2;
	iter->m_c_cap = (iter->m_c_cap + max_current_cap) / 2;
	iter->m_v_time = (iter->m_v_time + vpu_time_ns) / 2;
	iter->m_v_cap = (iter->m_v_cap + vpu_boost) / 2;
	iter->m_m_time = (iter->m_m_time + mdla_time_ns) / 2;
	iter->m_m_cap = (iter->m_m_cap + mdla_boost) / 2;

	/* parse cpu time of each frame to ged_kpi */
	iter->cpu_time = cpu_time_ns;

	eara_fps = iter->target_fps;
	if (iter->target_fps && iter->target_fps != -1
		&& iter->target_fps_diff
		&& !iter->target_fps_margin_gpu
		&& !iter->target_fps_margin) {
		eara_fps = iter->target_fps * 1000 + iter->target_fps_diff;
		eara_fps /= 1000;
		eara_fps = clamp(eara_fps, min_fps_limit, max_fps_limit);
	}
	tolerence_fps = iter->target_fps_margin_gpu;

	switch (iter->sbe_state) {
	case -1:
	case 0:
		break;
	case 1:
		eara_fps = max_fps_limit;
		tolerence_fps = 0;
		break;
	default:
		break;
	}

	ged_kpi_set_target_FPS_margin(iter->bufid,
		iter->target_fps, iter->target_fps_margin, iter->cpu_time);

	fpsgo_systrace_c_fstb_man(pid, iter->bufid, (int)cpu_time_ns, "t_cpu");
	fpsgo_systrace_c_fstb(pid, iter->bufid, (int)max_current_cap,
			"cur_cpu_cap");
	fpsgo_systrace_c_fstb(pid, iter->bufid, (int)max_cpu_cap,
			"max_cpu_cap");

	if (mid && cpu_time_ns >= 0) {
		fpsgo_systrace_c_fstb_man(pid, iter->bufid, (int)vpu_time_ns,
			"t_vpu");
		fpsgo_systrace_c_fstb(pid, iter->bufid, (int)vpu_boost,
			"cur_vpu_cap");
		fpsgo_systrace_c_fstb_man(pid, iter->bufid, (int)mdla_time_ns,
			"t_mdla");
		fpsgo_systrace_c_fstb(pid, iter->bufid, (int)mdla_boost,
			"cur_mdla_cap");
		fpsgo_systrace_c_fstb(pid, iter->bufid, (int)iter->m_c_time,
			"avg_cpu_time");
		fpsgo_systrace_c_fstb(pid, iter->bufid, (int)iter->m_c_cap,
			"avg_cpu_cap");
		fpsgo_systrace_c_fstb(pid, iter->bufid, (int)iter->m_v_time,
			"avg_vpu_time");
		fpsgo_systrace_c_fstb(pid, iter->bufid, (int)iter->m_v_cap,
			"avg_vpu_cap");
		fpsgo_systrace_c_fstb(pid, iter->bufid, (int)iter->m_m_time,
			"avg_mdla_time");
		fpsgo_systrace_c_fstb(pid, iter->bufid, (int)iter->m_m_cap,
			"avg_mdla_cap");
	}

	if (eara_thrm_frame_start_fp) {
		int vpu_cross, mdla_cross, vpu_bg, mdla_bg;

		fpsgo_fstb2eara_get_jobs_status(&vpu_cross,
			&mdla_cross, &vpu_bg, &mdla_bg);

		eara_thrm_frame_start_fp(pid, bufID, (int)Runnging_time,
			(int)vpu_time_ns, (int)mdla_time_ns, Curr_cap,
			vpu_boost, mdla_boost, iter->queue_fps,
			Q2Q_time, vpu_cross, mdla_cross,
			vpu_bg, mdla_bg, cur_time);
	}

	mutex_unlock(&fstb_lock);
	return 0;
}

void (*eara_thrm_enqueue_end_fp)(int pid, unsigned long long bufID,
	int gpu_time, int gpu_freq, unsigned long long enq);
int fpsgo_comp2fstb_enq_end(int pid, unsigned long long bufID,
		unsigned long long enq)
{
	struct FSTB_FRAME_INFO *iter;

	mutex_lock(&fstb_lock);

	if (!fstb_enable) {
		mutex_unlock(&fstb_lock);
		return 0;
	}

	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter->pid == pid && iter->bufid == bufID)
			break;
	}

	if (iter == NULL) {
		mutex_unlock(&fstb_lock);
		return 0;
	}

	if (eara_thrm_enqueue_end_fp)
		eara_thrm_enqueue_end_fp(pid, bufID,
			iter->gpu_time, iter->gpu_freq, enq);

	mutex_unlock(&fstb_lock);
	return 0;
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

	fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid, ret,
		"quantile_weighted_cpu_time");
	return ret;

}

/*
 * check if camera is active
 * if yes, apply g block c boost
 */
long long fstb_cam_active_ts;
void fpsgo_comp2fstb_camera_active(int pid)
{
	mutex_lock(&fstb_cam_active_time);
	fstb_cam_active_ts = ktime_to_us(ktime_get());
	mutex_unlock(&fstb_cam_active_time);

	mtk_fstb_dprintk("camera_api pid %d\n", pid);
	fpsgo_systrace_c_fstb(pid, 0, pid, "camera_active");
}

static void fstb_set_cam_active(int active)
{
	if (fstb_is_cam_active == active)
		return;

	fstb_is_cam_active = active;
	fpsgo_gpu_block_boost_enable_camera(active ? 0 : -1);
}

static void fstb_check_cam_status(void)
{
	mutex_lock(&fstb_cam_active_time);
	if (ktime_to_us(ktime_get()) - fstb_cam_active_ts < 1000000LL)
		fstb_set_cam_active(1);
	else
		fstb_set_cam_active(0);
	mutex_unlock(&fstb_cam_active_time);
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
	ktime_t cur_time;
	long long cur_time_us = 0;
	struct task_struct *tsk = NULL, *gtsk = NULL;
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
		if (iter->pid == pid && iter->bufid == bufID)
			break;
	}


	if (iter == NULL) {
		struct FSTB_FRAME_INFO *new_frame_info;

		new_frame_info = vmalloc(sizeof(*new_frame_info));
		if (new_frame_info == NULL)
			goto out;

		new_frame_info->pid = pid;
		new_frame_info->target_fps = max_fps_limit;
		new_frame_info->target_fps_margin = 0;
		new_frame_info->target_fps_margin_gpu = 0;
		new_frame_info->target_fps_margin2 = 0;
		new_frame_info->target_fps_margin_dbnc_a = margin_mode_dbnc_a;
		new_frame_info->target_fps_margin_dbnc_b = margin_mode_dbnc_b;
		new_frame_info->target_fps_margin_gpu_dbnc_a = margin_mode_gpu_dbnc_a;
		new_frame_info->target_fps_margin_gpu_dbnc_b = margin_mode_gpu_dbnc_b;
		new_frame_info->target_fps_diff = 0;
		new_frame_info->sbe_state = 0;
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
		new_frame_info->new_info = 1;
		new_frame_info->m_c_time = 0;
		new_frame_info->m_c_cap = 0;
		new_frame_info->m_v_time = 0;
		new_frame_info->m_v_cap = 0;
		new_frame_info->m_m_time = 0;
		new_frame_info->m_m_cap = 0;
		new_frame_info->gblock_b = 0ULL;
		new_frame_info->gblock_time = 0ULL;
		new_frame_info->fps_raise_flag = 0;
		new_frame_info->vote_i = 0;
		new_frame_info->render_idle_cnt = 0;
		new_frame_info->hwui_flag = hwui_flag;

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

		iter = new_frame_info;
		hlist_add_head(&iter->hlist, &fstb_frame_infos);
		{
			struct pob_fpsgo_qtsk_info pffi = {iter->pid};

			pob_fpsgo_qtsk_update(POB_FPSGO_QTSK_ADD, &pffi);
		}
	}

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

		fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid, tmp_q_fps,
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

			if (tmp_vote_fps != iter->target_fps)
				fstb_change_tfps(iter, tmp_vote_fps, 1);
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


	{
		struct pob_fpsgo_fpsstats_info pffi = {0};

		pffi.quantile_weighted_cpu_time = cur_cpu_time;
		pffi.quantile_weighted_gpu_time = cur_gpu_time;

		pob_fpsgo_fstb_stats_update(POB_FPSGO_FSTB_STATS_UPDATE, &pffi);
	}

	if (iter->fps_raise_flag == 1) {
		target_limit = iter->target_fps;
		/*decrease*/
	} else if (iter->target_fps - iter->queue_fps >
			iter->target_fps * fps_error_threshold / 100) {
		fpsgo_fstb2eara_notify_fps_bound();

		target_limit = iter->queue_fps;

		fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
				(int)target_limit, "tmp_target_limit");
	} else {
		target_limit = iter->target_fps;
	}

	iter->fps_raise_flag = 0;

	return target_limit;

}

void (*eara_thrm_gblock_bypass_fp)(int pid, unsigned long long bufid,
					int bypass);
#define FSTB_SEC_DIVIDER 1000000000
#define FSTB_MSEC_DIVIDER 1000000000000ULL
void fpsgo_fbt2fstb_query_fps(int pid, unsigned long long bufID,
		int *target_fps, int *target_cpu_time, int *fps_margin,
		int tgid, unsigned long long mid, int *quantile_cpu_time,
		int *quantile_gpu_time)
{
	struct FSTB_FRAME_INFO *iter = NULL;
	unsigned long long total_time, v_c_time;
	int tolerence_fps = 0;

	mutex_lock(&fstb_lock);

	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter->pid == pid && iter->bufid == bufID)
			break;
	}

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

	} else {
		(*quantile_cpu_time) = iter->quantile_cpu_time;
		(*quantile_gpu_time) = iter->quantile_gpu_time;

		if (iter->target_fps && iter->target_fps != -1
			&& iter->target_fps_diff
			&& !iter->target_fps_margin
			&& !iter->target_fps_margin_gpu) {
			int eara_fps = iter->target_fps * 1000;
			int max_mlimit = max_fps_limit * 1000;
			int min_mlimit = min_fps_limit * 1000;

			fpsgo_systrace_c_fstb_man(pid, iter->bufid,
					iter->target_fps_diff, "eara_diff");

			eara_fps += iter->target_fps_diff;
			eara_fps = clamp(eara_fps, min_mlimit, max_mlimit);

			*target_fps = eara_fps / 1000;
			tolerence_fps = iter->target_fps_margin * 1000;
			total_time = (unsigned long long)FSTB_MSEC_DIVIDER;
			total_time =
				div64_u64(total_time,
				(eara_fps + tolerence_fps) > max_mlimit ?
				max_mlimit : (eara_fps + tolerence_fps));

		} else {
			switch (iter->sbe_state) {
			case -1:
				*target_fps = -1;
				tolerence_fps = 0;
				break;
			case 0:
				*target_fps = iter->target_fps;
				tolerence_fps = iter->target_fps_margin;
				break;
			case 1:
				*target_fps = max_fps_limit;
				tolerence_fps = 0;
				break;
			default:
				*target_fps = iter->target_fps;
				tolerence_fps = iter->target_fps_margin;
				break;
			}
			total_time = (int)FSTB_SEC_DIVIDER;
			total_time =
				div64_u64(total_time,
				(*target_fps) + tolerence_fps > max_fps_limit ?
				max_fps_limit : (*target_fps) + tolerence_fps);
		}

		if (total_time > 1000000ULL + iter->gblock_time &&
				iter->gblock_time > 1000000ULL) {
			fpsgo_systrace_c_fstb(pid, iter->bufid,
					iter->gblock_time, "gblock_time");
			total_time -= iter->gblock_time;
			if (eara_thrm_gblock_bypass_fp)
				eara_thrm_gblock_bypass_fp(iter->pid,
							iter->bufid, 1);
		} else {
			if (eara_thrm_gblock_bypass_fp)
				eara_thrm_gblock_bypass_fp(iter->pid,
							iter->bufid, 0);
		}

		iter->gblock_time = 0ULL;

		if (mid && iter->queue_fps > 0)
			fpsgo_fstb2eara_optimize_power(mid, tgid,
				&v_c_time, total_time, iter->m_c_time,
				iter->m_v_time, iter->m_m_time, iter->m_c_cap,
				iter->m_v_cap, iter->m_m_cap);
		else
			v_c_time = total_time;

		if (!adopt_low_fps && iter->queue_fps == -1)
			*target_fps = -1;
	}

	*target_cpu_time = v_c_time;
	*fps_margin = tolerence_fps;

	mutex_unlock(&fstb_lock);
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
		powerfps_arrray[i].fps = iter->queue_fps > 0 ? iter->queue_fps : -1;
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

void (*eara_pre_active_fp)(int is_active);
static void fstb_fps_stats(struct work_struct *work)
{
	struct FSTB_FRAME_INFO *iter;
	struct hlist_node *n;
	int target_fps = max_fps_limit;
	int idle = 1;
	int fstb_active2xgf;
	int max_target_fps = -1;
	int eara_ret = -1;

	if (work != &fps_stats_work)
		kfree(work);

	mutex_lock(&fstb_lock);

	if (gbe_fstb2gbe_poll_fp)
		gbe_fstb2gbe_poll_fp(&fstb_frame_infos);

	pob_fpsgo_fstb_stats_update(POB_FPSGO_FSTB_STATS_START, NULL);

	hlist_for_each_entry_safe(iter, n, &fstb_frame_infos, hlist) {
		/* if this process did queue buffer while last polling window */
		if (fps_update(iter)) {

			idle = 0;

			target_fps = cal_target_fps(iter);
			target_fps = calculate_fps_limit(iter, target_fps);

			if (target_fps != iter->target_fps)
				fstb_change_tfps(iter, target_fps, 0);

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

				target_fps = calculate_fps_limit(iter, target_fps);

				if (target_fps != iter->target_fps)
					fstb_change_tfps(iter, target_fps, 0);

				mtk_fstb_dprintk(
						"%s pid:%d target_fps:%d\n",
						__func__, iter->pid,
						iter->target_fps);
				continue;
			}

			hlist_del(&iter->hlist);

			{
				struct pob_fpsgo_qtsk_info pffi = {iter->pid};

				pob_fpsgo_qtsk_update(POB_FPSGO_QTSK_DEL,
						&pffi);
			}

			vfree(iter);
		}
	}

	syslimiter_update_dfrc_fps(max_target_fps);
	fstb_cal_powerhal_fps();
	dram_ctl_update_dfrc_fps(max_target_fps);

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

	fpsgo_fstb2eara_notify_fps_active(fstb_active);
	pob_fpsgo_fstb_stats_update(POB_FPSGO_FSTB_STATS_END, NULL);
	if (fstb_active == 0)
		pob_fpsgo_qtsk_update(POB_FPSGO_QTSK_DELALL, NULL);

	mutex_unlock(&fstb_lock);

	if (eara_pre_change_fp)
		eara_ret = eara_pre_change_fp();

	if (eara_ret == -1) {
		mutex_lock(&fstb_lock);
		hlist_for_each_entry_safe(iter, n, &fstb_frame_infos, hlist) {
			if (!iter->target_fps_notifying
				|| iter->target_fps_notifying == -1)
				continue;
			iter->target_fps = iter->target_fps_notifying;
			iter->target_fps_notifying = 0;
			fpsgo_systrace_c_fstb_man(iter->pid, iter->bufid,
					0, "fstb_notifying");
			fpsgo_systrace_c_fstb(iter->pid, iter->bufid,
					iter->target_fps, "fstb_target_fps1");
		}
		mutex_unlock(&fstb_lock);
	}

	fstb_check_cam_status();

	fpsgo_check_thread_status();
	fpsgo_fstb2xgf_do_recycle(fstb_active2xgf);
	fpsgo_create_render_dep();

}

static int set_soft_fps_level(int nr_level, struct fps_level *level)
{
	mutex_lock(&fstb_lock);

	if (nr_level != 1)
		goto set_fps_level_err;

	if (level->end > level->start)
		goto set_fps_level_err;

	memcpy(fps_levels, level, nr_level * sizeof(struct fps_level));

	nr_fps_levels = nr_level;
	max_fps_limit = min(dfps_ceiling, fps_levels->start);
	min_fps_limit = min(dfps_ceiling, fps_levels->end);

	mutex_unlock(&fstb_lock);

	return 0;
set_fps_level_err:
	mutex_unlock(&fstb_lock);
	return -EINVAL;
}

static void reset_fps_level(void)
{
	struct fps_level level[1];

	level[0].start = CFG_MAX_FPS_LIMIT;
	level[0].end = CFG_MIN_FPS_LIMIT;

	set_soft_fps_level(1, level);
}

static ssize_t set_render_no_ctrl_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return 0;
}

static ssize_t set_render_no_ctrl_store(struct kobject *kobj,
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
				0, arg > 0, "force_no_ctrl");
			if (arg > 0)
				ret = switch_thread_no_ctrl(arg, 1);
			else
				ret = switch_thread_no_ctrl(-arg, 0);
		}
	}

out:
	return count;
}

static KOBJ_ATTR_RW(set_render_no_ctrl);

static ssize_t set_render_max_fps_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return 0;
}

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

static ssize_t jump_check_q_pct_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", JUMP_CHECK_Q_PCT);
}

static ssize_t jump_check_q_pct_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				switch_jump_check_q_pct(arg);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(jump_check_q_pct);

static ssize_t jump_check_num_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", JUMP_CHECK_NUM);
}

static ssize_t jump_check_num_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				switch_jump_check_num(arg);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(jump_check_num);

static ssize_t fstb_soft_level_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	char temp[FPSGO_SYSFS_MAX_BUFF_SIZE] = "";
	int pos = 0;
	int length;
	int i;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%d ", nr_fps_levels);
	pos += length;

	for (i = 0; i < nr_fps_levels; i++) {
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"%d-%d ", fps_levels[i].start, fps_levels[i].end);
		pos += length;
	}
	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"\n");
	pos += length;

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static ssize_t fstb_soft_level_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	char *sepstr, *substr;
	int ret = -EINVAL, new_nr_fps_levels, i, start_fps, end_fps;
	struct fps_level *new_levels;

	new_levels = kmalloc(sizeof(fps_levels), GFP_KERNEL);
	if (new_levels == NULL)
		return count;


	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			acBuffer[count] = '\0';
			sepstr = acBuffer;

			substr = strsep(&sepstr, " ");
			if (!substr ||
					kstrtoint(substr, 10,
					&new_nr_fps_levels) != 0 ||
					new_nr_fps_levels > MAX_NR_FPS_LEVELS) {
				ret = -EINVAL;
				goto err;
			}

			for (i = 0; i < new_nr_fps_levels; i++) {
				substr = strsep(&sepstr, " ");
				if (!substr) {
					ret = -EINVAL;
					goto err;
				}
				/* maybe contiguous */
				if (strchr(substr, '-')) {
					if (sscanf(substr, "%d-%d",
						&start_fps, &end_fps) != 2) {
						ret = -EINVAL;
						goto err;
					}
					new_levels[i].start = start_fps;
					new_levels[i].end = end_fps;
				} else { /* discrete */
					if (kstrtoint(substr,
						10, &start_fps) != 0) {
						ret = -EINVAL;
						goto err;
					}
					new_levels[i].start = start_fps;
					new_levels[i].end = start_fps;
				}
			}

			ret = !set_soft_fps_level(
				new_nr_fps_levels, new_levels);

			if (ret == 1)
				ret = count;
			else
				ret = -EINVAL;
		}
	}

err:
	kfree(new_levels);

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

static ssize_t fstb_tune_window_size_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", FRAME_TIME_WINDOW_SIZE_US);
}

static ssize_t fstb_tune_window_size_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				switch_sample_window(arg);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(fstb_tune_window_size);

static ssize_t margin_mode_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", margin_mode);
}

static ssize_t margin_mode_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				switch_margin_mode(arg);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(margin_mode);

static ssize_t margin_mode_dbnc_a_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", margin_mode_dbnc_a);
}

static ssize_t margin_mode_dbnc_a_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				switch_margin_mode_dbnc_a(arg);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(margin_mode_dbnc_a);

static ssize_t margin_mode_dbnc_b_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", margin_mode_dbnc_b);
}

static ssize_t margin_mode_dbnc_b_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				switch_margin_mode_dbnc_b(arg);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(margin_mode_dbnc_b);

static ssize_t margin_mode_gpu_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", margin_mode_gpu);
}

static ssize_t margin_mode_gpu_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				switch_margin_mode_gpu(arg);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(margin_mode_gpu);

static ssize_t margin_mode_gpu_dbnc_a_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", margin_mode_gpu_dbnc_a);
}

static ssize_t margin_mode_gpu_dbnc_a_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				switch_margin_mode_gpu_dbnc_a(arg);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(margin_mode_gpu_dbnc_a);

static ssize_t margin_mode_gpu_dbnc_b_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", margin_mode_gpu_dbnc_b);
}

static ssize_t margin_mode_gpu_dbnc_b_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				switch_margin_mode_gpu_dbnc_b(arg);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(margin_mode_gpu_dbnc_b);

static ssize_t fstb_tune_quantile_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", QUANTILE);
}

static ssize_t fstb_tune_quantile_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				switch_percentile_frametime(arg);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(fstb_tune_quantile);

static ssize_t fstb_tune_error_threshold_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", fps_error_threshold);
}

static ssize_t fstb_tune_error_threshold_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				switch_fps_error_threhosld(arg);
		}
	}

	return count;
}

static KOBJ_ATTR_RW(fstb_tune_error_threshold);

static ssize_t adopt_low_fps_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", adopt_low_fps);
}

static ssize_t adopt_low_fps_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[FPSGO_SYSFS_MAX_BUFF_SIZE];
	int arg;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0
				&& arg >= 0 && arg <= 1)
				adopt_low_fps = arg;
		}
	}

	return count;
}

static KOBJ_ATTR_RW(adopt_low_fps);

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
	"tid\tbufID\t\tname\t\tcurrentFPS\ttargetFPS\tFPS_margin\tFPS_margin_GPU\tFPS_margin_thrs\tsbe_state\n");
	pos += length;

	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"%d\t0x%llx\t%s\t%d\t\t%d\t\t%d\t\t%d\t\t%d\t\t%d\n",
				iter->pid,
				iter->bufid,
				iter->proc_name,
				iter->queue_fps > max_fps_limit ?
				max_fps_limit : iter->queue_fps,
				iter->target_fps,
				iter->target_fps_margin,
				iter->target_fps_margin_gpu,
				iter->target_fps_margin2,
				iter->sbe_state);
		pos += length;

	}

	mutex_unlock(&fstb_lock);

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"fstb_is_cam_active:%d\n", fstb_is_cam_active);
	pos += length;

	length = scnprintf(temp + pos, FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"dfps_ceiling:%d\n", dfps_ceiling);
	pos += length;


	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}
static KOBJ_ATTR_ROO(fpsgo_status);

int mtk_fstb_init(void)
{
	int num_cluster = 0;

	mtk_fstb_dprintk_always("init\n");

	num_cluster = arch_get_nr_clusters();

	ged_kpi_output_gfx_info2_fp = gpu_time_update;


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
				&kobj_attr_fstb_tune_window_size);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_fps_list);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_fstb_soft_level);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_jump_check_num);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_jump_check_q_pct);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_set_render_max_fps);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_set_render_no_ctrl);
		fpsgo_sysfs_create_file(fstb_kobj,
				&kobj_attr_adopt_low_fps);
	}

	reset_fps_level();

	wq = create_singlethread_workqueue("mt_fstb");
	if (!wq)
		goto err;

	hrtimer_init(&hrt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrt.function = &mt_fstb;

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
			&kobj_attr_fstb_tune_window_size);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_fps_list);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_fstb_soft_level);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_jump_check_num);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_jump_check_q_pct);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_set_render_max_fps);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_set_render_no_ctrl);
	fpsgo_sysfs_remove_file(fstb_kobj,
			&kobj_attr_adopt_low_fps);

	fpsgo_sysfs_remove_dir(&fstb_kobj);

	return 0;
}
