// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
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
#include "fstb.h"
#include "fstb_usedext.h"
#include "../fbt/include/fbt_fteh.h"
#include "fpsgo_usedext.h"


#define API_READY 0
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

#define fpsgo_systrace_c_fstb_man(pid, val, fmt...) \
	fpsgo_systrace_c(FPSGO_DEBUG_MANDATORY, pid, val, fmt)

#define API_READY 0

static int max_fps_limit = DEFAULT_DFPS;
static int dfps_ceiling = DEFAULT_DFPS;
static int min_fps_limit = CFG_MIN_FPS_LIMIT;
static int fps_error_threshold = 10;
static int QUANTILE = 50;
static long long FRAME_TIME_WINDOW_SIZE_US = 1000000;
static long long ADJUST_INTERVAL_US = 1000000;
static int margin_mode;
static int margin_mode_dbnc_a = 9;
static int margin_mode_dbnc_b = 1;

static void fstb_fps_stats(struct work_struct *work);
static DECLARE_WORK(fps_stats_work,
		(void *) fstb_fps_stats);
static HLIST_HEAD(fstb_frame_infos);
static HLIST_HEAD(fstb_render_target_fps);
static HLIST_HEAD(fstb_fteh_list);

static struct hrtimer hrt;
static struct workqueue_struct *wq;
static struct dentry *fstb_debugfs_dir;

static struct fps_level fps_levels[MAX_NR_FPS_LEVELS];
static int nr_fps_levels = MAX_NR_FPS_LEVELS;

static int fstb_fps_klog_on;
static int fstb_enable, fstb_active, fstb_idle_cnt;
static long long last_update_ts;

static void reset_fps_level(void);
static int set_soft_fps_level(int nr_level,
		struct fps_level *level);

static DEFINE_MUTEX(fstb_lock);
static DEFINE_MUTEX(fstb_fps_active_time);
static DEFINE_MUTEX(fstb_cam_active_time);

void (*gbe_fstb2gbe_poll_fp)(struct hlist_head *list);

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

	fpsgo_systrace_c_fstb(tid, start, "gblock");

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
	fpsgo_systrace_c_fstb(-200, fstb_enable, "fstb_enable");

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
	fpsgo_systrace_c_fstb(-200,
			fstb_active, "fstb_active");

	mtk_fstb_dprintk_always("%s %d\n",
			__func__, fstb_active);
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

int switch_fps_range(int nr_level, struct fps_level *level)
{
	if (nr_level != 1)
		return 1;

	if (!set_soft_fps_level(nr_level, level))
		return 0;
	else
		return 1;
}

int fpsgo_fbt2fstb_query_fteh_list(int tid)
{
	struct FSTB_FTEH_LIST *iter;
	struct task_struct *tsk, *gtsk;
	char proc_name[16], thrd_name[16];
	int ret = 0;

	rcu_read_lock();
	tsk = find_task_by_vpid(tid);
	if (tsk) {
		get_task_struct(tsk);
		gtsk = find_task_by_vpid(tsk->tgid);
		if (!strncpy(thrd_name, tsk->comm, 16)) {
			put_task_struct(tsk);
			goto out;
		}
		put_task_struct(tsk);
		if (gtsk) {
			get_task_struct(gtsk);
			if (!strncpy(proc_name, gtsk->comm, 16)) {
				put_task_struct(gtsk);
				goto out;
			}
			put_task_struct(gtsk);
		} else
			goto out;
	} else
		goto out;
	rcu_read_unlock();

	hlist_for_each_entry(iter, &fstb_fteh_list, hlist) {
		if (!strncmp(proc_name, iter->process_name,
				strlen(iter->process_name)) &&
			!strncmp(thrd_name, iter->thread_name,
				strlen(iter->thread_name))) {
			ret = 1;
			break;
		}
	}

	return ret;

out:
	rcu_read_unlock();
	return ret;
}

#define MAX_FTEH_LENGTH 20
static int fteh_list_length;
static int set_fteh_list(char *proc_name,
	char *thrd_name)
{
	struct FSTB_FTEH_LIST *new_fteh_list;

	if (fteh_list_length >= MAX_FTEH_LENGTH)
		return -ENOMEM;

	new_fteh_list =
		kzalloc(sizeof(*new_fteh_list), GFP_KERNEL);
	if (new_fteh_list == NULL)
		return -ENOMEM;

	if (!strncpy(
				new_fteh_list->process_name,
				proc_name, 16)) {
		kfree(new_fteh_list);
		return -ENOMEM;
	}

	if (!strncpy(
				new_fteh_list->thread_name,
				thrd_name, 16)) {
		kfree(new_fteh_list);
		return -ENOMEM;
	}

	hlist_add_head(&new_fteh_list->hlist,
			&fstb_fteh_list);

	fteh_list_length++;

	return 0;

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

	if (fps <= CFG_MAX_FPS_LIMIT && fps >= CFG_MIN_FPS_LIMIT) {
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

	if (!fstb_active) {
		fstb_active = 1;
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
		fpsgo_systrace_c_fstb_man(iter->pid,
		(int)iter->weighted_gpu_time[iter->weighted_gpu_time_end],
		"weighted_gpu_time");
		iter->weighted_gpu_time_end++;
	}

	mtk_fstb_dprintk(
	"fstb: time %lld %lld t_gpu %lld cur_freq %u cur_max_freq %u\n",
	cur_time_us, ktime_to_us(ktime_get())-cur_time_us,
	t_gpu, cur_freq, cur_max_freq);

	fpsgo_systrace_c_fstb_man(iter->pid, (int)t_gpu, "t_gpu");
	fpsgo_systrace_c_fstb(iter->pid, (int)cur_freq, "cur_gpu_cap");
	fpsgo_systrace_c_fstb(iter->pid, (int)cur_max_freq, "max_gpu_cap");

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

	fpsgo_systrace_c_fstb_man(iter->pid, ret, "quantile_weighted_gpu_time");
	return ret;

}

void (*eara_thrm_frame_start_fp)(int pid,
	int cpu_time, int vpu_time, int mdla_time,
	int cpu_cap, int vpu_cap, int mdla_cap,
	int queuefps, unsigned long long q2q_time,
	int AI_cross_vpu, int AI_cross_mdla, int AI_bg_vpu,
	int AI_bg_mdla, ktime_t cur_time);

int fpsgo_fbt2fstb_update_cpu_frame_info(
		int pid,
		int tgid,
		int frame_type,
		unsigned long long Q2Q_time,
		unsigned long long Runnging_time,
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

	struct FSTB_FRAME_INFO *iter;

	ktime_t cur_time;
	long long cur_time_us;

	mutex_lock(&fstb_lock);

	if (!fstb_enable) {
		mutex_unlock(&fstb_lock);
		return 0;
	}

	if (!fstb_active) {
		fstb_active = 1;
		switch_fstb_active();
	}

	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter->pid == pid)
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

	if (max_cpu_cap > 0 && Max_cap > Curr_cap) {
		wct = cpu_time_ns * max_current_cap;
		do_div(wct, max_cpu_cap);
	} else
		wct = cpu_time_ns;

	fpsgo_systrace_c_fstb_man(pid, (int)wct, "weighted_cpu_time");

	if (vpu_time_ns && vpu_boost > 0 && vpu_boost <= VPU_MAX_CAP) {
		wvt = vpu_time_ns * vpu_boost;
		do_div(wvt, VPU_MAX_CAP);
		fpsgo_systrace_c_fstb_man(pid, (int)wvt, "weighted_vpu_time");
	}

	if (mdla_time_ns && mdla_boost > 0 && mdla_boost <= MDLA_MAX_CAP) {
		wmt = mdla_time_ns * mdla_boost;
		do_div(wmt, MDLA_MAX_CAP);
		fpsgo_systrace_c_fstb_man(pid, (int)wmt, "weighted_mdla_time");
	}

	iter->weighted_cpu_time[iter->weighted_cpu_time_end] =
		wct + wvt + wmt;

	iter->weighted_cpu_time_ts[iter->weighted_cpu_time_end] =
		cur_time_us;
	iter->weighted_cpu_time_end++;

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

	fpsgo_systrace_c_fstb_man(pid, (int)cpu_time_ns, "t_cpu");
	fpsgo_systrace_c_fstb(pid, (int)max_current_cap, "cur_cpu_cap");
	fpsgo_systrace_c_fstb(pid, (int)max_cpu_cap, "max_cpu_cap");

	if (mid) {
		fpsgo_systrace_c_fstb_man(pid, (int)vpu_time_ns, "t_vpu");
		fpsgo_systrace_c_fstb(pid, (int)vpu_boost, "cur_vpu_cap");
		fpsgo_systrace_c_fstb_man(pid, (int)mdla_time_ns, "t_mdla");
		fpsgo_systrace_c_fstb(pid, (int)mdla_boost, "cur_mdla_cap");
		fpsgo_systrace_c_fstb(pid, (int)iter->m_c_time,
			"avg_cpu_time");
		fpsgo_systrace_c_fstb(pid, (int)iter->m_c_cap, "avg_cpu_cap");
		fpsgo_systrace_c_fstb(pid, (int)iter->m_v_time,
			"avg_vpu_time");
		fpsgo_systrace_c_fstb(pid, (int)iter->m_v_cap, "avg_vpu_cap");
		fpsgo_systrace_c_fstb(pid, (int)iter->m_m_time,
			"avg_mdla_time");
		fpsgo_systrace_c_fstb(pid, (int)iter->m_m_cap, "avg_mdla_cap");
	}

	if (eara_thrm_frame_start_fp) {
		eara_thrm_frame_start_fp(pid, (int)Runnging_time,
			(int)vpu_time_ns, (int)mdla_time_ns, Curr_cap,
			vpu_boost, mdla_boost, iter->queue_fps,
			Q2Q_time, 0, 0,
			0, 0, cur_time);
	}

	mutex_unlock(&fstb_lock);
	return 0;
}

void (*eara_thrm_enqueue_end_fp)(int pid, int gpu_time, int gpu_freq,
	unsigned long long enq);
int fpsgo_comp2fstb_enq_end(int pid, unsigned long long enq)
{
	struct FSTB_FRAME_INFO *iter;

	mutex_lock(&fstb_lock);

	if (!fstb_enable) {
		mutex_unlock(&fstb_lock);
		return 0;
	}

	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter->pid == pid)
			break;
	}

	if (iter == NULL) {
		mutex_unlock(&fstb_lock);
		return 0;
	}

	if (eara_thrm_enqueue_end_fp)
		eara_thrm_enqueue_end_fp(pid,
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

	fpsgo_systrace_c_fstb_man(iter->pid, ret, "quantile_weighted_cpu_time");
	return ret;

}

/*
 * check if camera is active
 * if yes, apply g block c boost
 */
long long fstb_cam_active_ts;
int fstb_is_cam_active;
void fpsgo_comp2fstb_camera_active(int pid)
{
	mutex_lock(&fstb_cam_active_time);
	fstb_cam_active_ts = ktime_to_us(ktime_get());
	mutex_unlock(&fstb_cam_active_time);

	mtk_fstb_dprintk("camera_api pid %d\n", pid);
	fpsgo_systrace_c_fstb(pid, pid, "camera_active");
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

void fpsgo_comp2fstb_queue_time_update(int pid,
	int frame_type, unsigned long long ts,
	unsigned long long bufferid, int api)
{
	struct FSTB_FRAME_INFO *iter;
	ktime_t cur_time;
	long long cur_time_us = 0;
	struct task_struct *tsk = NULL, *gtsk = NULL;

	mutex_lock(&fstb_lock);

	if (!fstb_enable) {
		mutex_unlock(&fstb_lock);
		return;
	}

	if (!fstb_active) {
		fstb_active = 1;
		switch_fstb_active();
	}

	cur_time = ktime_get();
	cur_time_us = ktime_to_us(cur_time);

	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter->pid == pid)
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
		new_frame_info->target_fps_margin2 = 0;
		new_frame_info->target_fps_margin_dbnc_a = margin_mode_dbnc_a;
		new_frame_info->target_fps_margin_dbnc_b = margin_mode_dbnc_b;
		new_frame_info->queue_fps = max_fps_limit;
		new_frame_info->bufid = bufferid;
		new_frame_info->queue_time_begin = 0;
		new_frame_info->queue_time_end = 0;
		new_frame_info->weighted_cpu_time_begin = 0;
		new_frame_info->weighted_cpu_time_end = 0;
		new_frame_info->weighted_gpu_time_begin = 0;
		new_frame_info->weighted_gpu_time_end = 0;
		new_frame_info->new_info = 1;
		new_frame_info->m_c_time = 0;
		new_frame_info->m_c_cap = 0;
		new_frame_info->m_v_time = 0;
		new_frame_info->m_v_cap = 0;
		new_frame_info->m_m_time = 0;
		new_frame_info->m_m_cap = 0;
		new_frame_info->gblock_b = 0ULL;
		new_frame_info->gblock_time = 0ULL;

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
			put_task_struct(gtsk);
		} else {
			new_frame_info->proc_name[0] = '\0';
		}

		iter = new_frame_info;
		hlist_add_head(&iter->hlist, &fstb_frame_infos);
	} else {
		iter->bufid = bufferid;
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

out:
	mutex_unlock(&fstb_lock);

	mutex_lock(&fstb_fps_active_time);
	if (cur_time_us)
		last_update_ts = cur_time_us;
	mutex_unlock(&fstb_fps_active_time);
}

static int fstb_get_queue_fps(struct FSTB_FRAME_INFO *iter,
		long long interval)
{
	int i = iter->queue_time_begin, j;
	unsigned long long queue_fps;
	unsigned long long frame_interval_count = 0;
	unsigned long long avg_frame_interval = 0;
	unsigned long long retval = 0;

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
	}

	queue_fps = (long long)(iter->queue_time_end - i) * 1000000LL;
	do_div(queue_fps, (unsigned long long)interval);

	if (avg_frame_interval != 0) {
		retval = 1000000000ULL * frame_interval_count;
		do_div(retval, avg_frame_interval);
		mtk_fstb_dprintk("%s  %d %llu\n",
				__func__, iter->pid, retval);
		fpsgo_systrace_c_fstb_man(iter->pid, (int)retval, "queue_fps");
		return retval;
	}
	mtk_fstb_dprintk("%s  %d %d\n", __func__, iter->pid, 0);
	fpsgo_systrace_c_fstb_man(iter->pid, 0, "queue_fps");

	return 0;
}

static int fps_update(struct FSTB_FRAME_INFO *iter)
{
	iter->queue_fps =
		fstb_get_queue_fps(iter, FRAME_TIME_WINDOW_SIZE_US);

	return iter->queue_fps;
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
		if (ret_fps >= max_fps_limit)
			iter->target_fps_margin = 0;
		else if (asfc_turn) {
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
	unsigned long long tmp_target_limit = max_fps_limit;
	int cur_cpu_time, cur_gpu_time;
	long long cur_pipe_time;

	cur_cpu_time = get_cpu_frame_time(iter);
	cur_gpu_time = get_gpu_frame_time(iter);

#if API_READY
	if (iter->new_info == 0) {
		struct pob_fpsgo_fpsstats_info pffi = {0};

		pffi.quantile_weighted_cpu_time = cur_cpu_time;
		pffi.quantile_weighted_gpu_time = cur_gpu_time;

		pob_fpsgo_fstb_stats_update(POB_FPSGO_FSTB_STATS_UPDATE, &pffi);
	}
#endif

	if (iter->new_info == 1) {
		iter->new_info = 0;
		target_limit = max_fps_limit;
		/*decrease*/
	} else if (iter->target_fps - iter->queue_fps >
			iter->target_fps * fps_error_threshold / 100) {

		if (iter->queue_fps < iter->target_fps)
			target_limit = iter->queue_fps;
		else
			target_limit = iter->target_fps;
		fpsgo_systrace_c_fstb(iter->pid,
				(int)target_limit, "tmp_target_limit");
		/*increase*/
	} else if (iter->queue_fps >=
		iter->target_fps + iter->target_fps_margin2) {

		tmp_target_limit = 1000000000LL;
		cur_pipe_time = (long long)max(cur_cpu_time, cur_gpu_time);

		if (cur_pipe_time > 0)
			do_div(tmp_target_limit, cur_pipe_time);
		else
			tmp_target_limit = max_fps_limit;

		fpsgo_systrace_c_fstb(iter->pid,
				(int)tmp_target_limit, "tmp_target_limit");

		if ((int)tmp_target_limit - iter->target_fps >
				iter->target_fps * fps_error_threshold / 100) {
			if (iter->target_fps >= 48)
				target_limit = clamp((int)tmp_target_limit,
						48, CFG_MAX_FPS_LIMIT);
			else if (iter->target_fps >= 45)
				target_limit = clamp((int)tmp_target_limit,
						45, 48);
			else if (iter->target_fps >= 36)
				target_limit = clamp((int)tmp_target_limit,
						36, 45);
			else if (iter->target_fps > 30)
				target_limit = clamp((int)tmp_target_limit,
						30, 36);
			else if (iter->target_fps == 30)
				target_limit = 60;
			else if (iter->target_fps >= 27)
				target_limit = clamp((int)tmp_target_limit,
						27, 30);
			else if (iter->target_fps >= 24)
				target_limit = clamp((int)tmp_target_limit,
						24, 27);
			else
				target_limit = clamp((int)tmp_target_limit,
						CFG_MIN_FPS_LIMIT, 24);

		} else
			target_limit = iter->target_fps;

		/*stable state*/
	} else {
		target_limit = iter->target_fps;
	}

	return target_limit;

}

void (*eara_thrm_gblock_bypass_fp)(int pid, int bypass);
#define FSTB_SEC_DIVIDER 1000000000
void fpsgo_fbt2fstb_query_fps(int pid, int *target_fps,
	int *target_cpu_time, int tgid, unsigned long long mid)
{
	struct FSTB_FRAME_INFO *iter = NULL;
	unsigned long long total_time, v_c_time;
	int tolerence_fps = 0;

	mutex_lock(&fstb_lock);

	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		if (iter->pid == pid)
			break;
	}

	if (!iter) {
		*target_fps = max_fps_limit;
		tolerence_fps = 0;
		total_time = (int)FSTB_SEC_DIVIDER;
		total_time =
			div64_u64(total_time,
			(*target_fps) + tolerence_fps > max_fps_limit ?
			max_fps_limit : (*target_fps) + tolerence_fps);
		v_c_time = total_time;

	} else {

		*target_fps = iter->target_fps;
		tolerence_fps = iter->target_fps_margin;
		total_time = (int)FSTB_SEC_DIVIDER;
		total_time =
			div64_u64(total_time,
			(*target_fps) + tolerence_fps > max_fps_limit ?
			max_fps_limit : (*target_fps) + tolerence_fps);

		if (total_time > 1000000ULL + iter->gblock_time &&
				iter->gblock_time > 1000000ULL) {
			fpsgo_systrace_c_fstb(pid,
					iter->gblock_time, "gblock_time");
			total_time -= iter->gblock_time;

			if (eara_thrm_gblock_bypass_fp)
				eara_thrm_gblock_bypass_fp(iter->pid, 1);
		} else {
			if (eara_thrm_gblock_bypass_fp)
				eara_thrm_gblock_bypass_fp(iter->pid, 0);
		}

		iter->gblock_time = 0ULL;

			v_c_time = total_time;

	}

	*target_cpu_time = v_c_time;

	mutex_unlock(&fstb_lock);
}

static void fstb_fps_stats(struct work_struct *work)
{
	struct FSTB_FRAME_INFO *iter;
	struct hlist_node *n;
	int target_fps = max_fps_limit;
	int idle = 1;
	int fstb_active2xgf;

	if (work != &fps_stats_work)
		kfree(work);

	mutex_lock(&fstb_lock);


#if API_READY
	pob_fpsgo_fstb_stats_update(POB_FPSGO_FSTB_STATS_START, NULL);
#endif

	hlist_for_each_entry_safe(iter, n, &fstb_frame_infos, hlist) {
		/* if this process did queue buffer while last polling window */
		if (fps_update(iter)) {

			idle = 0;

			target_fps = cal_target_fps(iter);

			iter->target_fps =
				calculate_fps_limit(iter, target_fps);
			fpsgo_systrace_c_fstb_man(iter->pid,
					dfps_ceiling, "dfrc");
			fpsgo_systrace_c_fstb(iter->pid,
				iter->target_fps_margin, "target_fps_margin");
			fpsgo_systrace_c_fstb(iter->pid,
				iter->target_fps_margin2, "target_fps_margin2");
			fpsgo_systrace_c_fstb(iter->pid,
				iter->target_fps_margin_dbnc_a,
				"target_fps_margin_dbnc_a");
			fpsgo_systrace_c_fstb(iter->pid,
				iter->target_fps_margin_dbnc_b,
				"target_fps_margin_dbnc_b");

			// ged_kpi_set_target_FPS_margin(iter->bufid,
			// iter->target_fps, iter->target_fps_margin);

			mtk_fstb_dprintk(
			"%s pid:%d target_fps:%d\n",
			__func__, iter->pid,
			iter->target_fps);
			/* if queue fps == 0, we delete that frame_info */
		} else {
			hlist_del(&iter->hlist);

			vfree(iter);
		}
	}

	/* check idle twice to avoid fstb_active ping-pong */
	if (idle)
		fstb_idle_cnt++;
	else
		fstb_idle_cnt = 0;

	if (fstb_idle_cnt >= 2) {
		fstb_active = 0;
		fstb_idle_cnt = 0;
	}

	if (fstb_active)
		fstb_active2xgf = 1;
	else
		fstb_active2xgf = 0;

	if (fstb_enable && fstb_active)
		enable_fstb_timer();
	else
		disable_fstb_timer();


	mutex_unlock(&fstb_lock);

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

static int fstb_soft_level_read(struct seq_file *m, void *v)
{
	int i;

	seq_printf(m, "%d ", nr_fps_levels);
	for (i = 0; i < nr_fps_levels; i++)
		seq_printf(m, "%d-%d ", fps_levels[i].start, fps_levels[i].end);
	seq_puts(m, "\n");

	return 0;
}

/* format example: 1 60-45
 * compatible: 1 45
 */
static ssize_t fstb_soft_level_write(struct file *file,
		const char __user *buffer,
		size_t count, loff_t *data)
{
	char *buf, *sepstr, *substr;
	int ret = -EINVAL, new_nr_fps_levels, i, start_fps, end_fps;
	struct fps_level *new_levels;

	/* we do not allow change fps_level during fps throttling,
	 * because fps_levels would be changed.
	 */

	if (count > 256)
		return -ENOMEM;

	buf = kmalloc(count + 1, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	new_levels = kmalloc(sizeof(fps_levels), GFP_KERNEL);
	if (new_levels == NULL) {
		ret = -ENOMEM;
		goto err_freebuf;
	}

	if (copy_from_user(buf, buffer, count)) {
		ret = -EFAULT;
		goto err;
	}
	buf[count] = '\0';
	sepstr = buf;

	substr = strsep(&sepstr, " ");
	if (!substr || kstrtoint(substr, 10, &new_nr_fps_levels) != 0 ||
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
		if (strchr(substr, '-')) { /* maybe contiguous */
			if (sscanf(substr, "%d-%d",
				&start_fps, &end_fps) != 2) {
				ret = -EINVAL;
				goto err;
			}
			new_levels[i].start = start_fps;
			new_levels[i].end = end_fps;
		} else { /* discrete */
			if (kstrtoint(substr, 10, &start_fps) != 0) {
				ret = -EINVAL;
				goto err;
			}
			new_levels[i].start = start_fps;
			new_levels[i].end = start_fps;
		}
	}

	ret = !set_soft_fps_level(new_nr_fps_levels, new_levels);

	if (ret == 1)
		ret = count;
	else
		ret = -EINVAL;

err:
	kfree(new_levels);
err_freebuf:
	kfree(buf);

	return ret;
}

static int fstb_soft_level_open(struct inode *inode, struct file *file)
{
	return single_open(file, fstb_soft_level_read, NULL);
}

static const struct file_operations fstb_soft_level_fops = {
	.owner = THIS_MODULE,
	.open = fstb_soft_level_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = fstb_soft_level_write,
	.release = single_release,
};

static int fstb_fteh_list_read(struct seq_file *m, void *v)
{
	struct FSTB_FTEH_LIST *ftehiter = NULL;

	hlist_for_each_entry(ftehiter, &fstb_fteh_list, hlist) {
		seq_printf(m, "%s %s\n",
				ftehiter->process_name, ftehiter->thread_name);
	}

	return 0;
}

static ssize_t fstb_fteh_list_write(struct file *file,
		const char __user *buffer,
		size_t count, loff_t *data)
{
	int ret = count;
	char *buf;
	char proc_name[16], thrd_name[16];

	if (count > 256)
		return -ENOMEM;

	buf = kmalloc(count + 1, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	if (copy_from_user(buf, buffer, count)) {
		ret = -EFAULT;
		goto err;
	}
	buf[count] = '\0';

	if (sscanf(buf, "%15s %15s", proc_name, thrd_name) != 2) {
		ret = -EINVAL;
		goto err;
	}

	if (set_fteh_list(proc_name, thrd_name))
		ret = -EINVAL;

err:
	kfree(buf);
	return ret;
}

static int fstb_fteh_list_open(struct inode *inode, struct file *file)
{
	return single_open(file, fstb_fteh_list_read, NULL);
}

static const struct file_operations fstb_fteh_list_fops = {
	.owner = THIS_MODULE,
	.open = fstb_fteh_list_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = fstb_fteh_list_write,
	.release = single_release,
};

static int fstb_fps_list_read(struct seq_file *m, void *v)
{
	int i;
	struct FSTB_RENDER_TARGET_FPS *rtfiter = NULL;

	hlist_for_each_entry(rtfiter, &fstb_render_target_fps, hlist) {
		seq_printf(m, "%s %d %d ",
			rtfiter->process_name, rtfiter->pid, rtfiter->nr_level);
		for (i = 0; i < rtfiter->nr_level; i++)
			seq_printf(m, "%d-%d ",
				rtfiter->level[i].start, rtfiter->level[i].end);
		seq_puts(m, "\n");
	}

	return 0;
}

static ssize_t fstb_fps_list_write(struct file *file,
		const char __user *buffer,
		size_t count, loff_t *data)
{
	int ret = count;
	char *sepstr, *substr, *buf;
	char proc_name[16];
	int i;
	int  nr_level, start_fps, end_fps;
	int mode = 1;
	int pid = 0;
	struct fps_level level[MAX_NR_RENDER_FPS_LEVELS];

	if (count > 256)
		return -ENOMEM;

	buf = kmalloc(count + 1, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;


	if (copy_from_user(buf, buffer, count)) {
		ret = -EFAULT;
		goto err;
	}
	buf[count] = '\0';
	sepstr = buf;

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

		if (sscanf(substr, "%d-%d", &start_fps, &end_fps) != 2) {
			ret = -EINVAL;
			goto err;
		}
		level[i].start = start_fps;
		level[i].end = end_fps;
	}

	if (mode == 0) {
		if (switch_process_fps_range(proc_name, nr_level, level))
			ret = -EINVAL;
	} else {
		if (switch_thread_fps_range(pid, nr_level, level))
			ret = -EINVAL;
	}

err:
	kfree(buf);
	return ret;
}

static int fstb_fps_list_open(struct inode *inode, struct file *file)
{
	return single_open(file, fstb_fps_list_read, NULL);
}

static const struct file_operations fstb_fps_list_fops = {
	.owner = THIS_MODULE,
	.open = fstb_fps_list_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = fstb_fps_list_write,
	.release = single_release,
};

static int fstb_tune_window_size_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%lld ", FRAME_TIME_WINDOW_SIZE_US);
	return 0;
}

static ssize_t fstb_tune_window_size_write(struct file *file,
		const char __user *buffer,
		size_t count, loff_t *data)
{
	int ret;
	long long arg;

	if (!kstrtoll_from_user(buffer, count, 0, &arg))
		ret = switch_sample_window(arg);
	else
		ret = -EINVAL;

	return (ret < 0) ? ret : count;
}

static int fstb_tune_window_size_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, fstb_tune_window_size_read, NULL);
}

static const struct file_operations fstb_tune_window_size_fops = {
	.owner = THIS_MODULE,
	.open = fstb_tune_window_size_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = fstb_tune_window_size_write,
	.release = single_release,
};

static int fstb_margin_mode_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", margin_mode);
	return 0;
}

static ssize_t fstb_margin_mode_write(struct file *file,
		const char __user *buffer,
		size_t count, loff_t *data)
{
	int ret;
	int arg;

	if (!kstrtoint_from_user(buffer, count, 0, &arg))
		ret = switch_margin_mode(arg);
	else
		ret = -EINVAL;

	return (ret < 0) ? ret : count;
}

static int fstb_margin_mode_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, fstb_margin_mode_read, NULL);
}

static const struct file_operations fstb_margin_mode_fops = {
	.owner = THIS_MODULE,
	.open = fstb_margin_mode_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = fstb_margin_mode_write,
	.release = single_release,
};

static int fstb_margin_mode_dbnc_a_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", margin_mode_dbnc_a);
	return 0;
}

static ssize_t fstb_margin_mode_dbnc_a_write(struct file *file,
		const char __user *buffer,
		size_t count, loff_t *data)
{
	int ret;
	int arg;

	if (!kstrtoint_from_user(buffer, count, 0, &arg))
		ret = switch_margin_mode_dbnc_a(arg);
	else
		ret = -EINVAL;

	return (ret < 0) ? ret : count;
}

static int fstb_margin_mode_dbnc_a_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, fstb_margin_mode_dbnc_a_read, NULL);
}

static const struct file_operations fstb_margin_mode_dbnc_a_fops = {
	.owner = THIS_MODULE,
	.open = fstb_margin_mode_dbnc_a_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = fstb_margin_mode_dbnc_a_write,
	.release = single_release,
};

static int fstb_margin_mode_dbnc_b_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", margin_mode_dbnc_b);
	return 0;
}

static ssize_t fstb_margin_mode_dbnc_b_write(struct file *file,
		const char __user *buffer,
		size_t count, loff_t *data)
{
	int ret;
	int arg;

	if (!kstrtoint_from_user(buffer, count, 0, &arg))
		ret = switch_margin_mode_dbnc_b(arg);
	else
		ret = -EINVAL;

	return (ret < 0) ? ret : count;
}

static int fstb_margin_mode_dbnc_b_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, fstb_margin_mode_dbnc_b_read, NULL);
}

static const struct file_operations fstb_margin_mode_dbnc_b_fops = {
	.owner = THIS_MODULE,
	.open = fstb_margin_mode_dbnc_b_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = fstb_margin_mode_dbnc_b_write,
	.release = single_release,
};

static int fstb_tune_quantile_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d ", QUANTILE);
	return 0;
}

static ssize_t fstb_tune_quantile_write(struct file *file,
		const char __user *buffer,
		size_t count, loff_t *data)
{
	int ret;
	int arg;

	if (!kstrtoint_from_user(buffer, count, 0, &arg))
		ret = switch_percentile_frametime(arg);
	else
		ret = -EINVAL;

	return (ret < 0) ? ret : count;
}

static int fstb_tune_quantile_open(struct inode *inode, struct file *file)
{
	return single_open(file, fstb_tune_quantile_read, NULL);
}

static const struct file_operations fstb_tune_quantile_fops = {
	.owner = THIS_MODULE,
	.open = fstb_tune_quantile_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = fstb_tune_quantile_write,
	.release = single_release,
};

static int fstb_tune_error_threshold_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d ", fps_error_threshold);
	return 0;
}

static ssize_t fstb_tune_error_threshold_write(struct file *file,
		const char __user *buffer,
		size_t count, loff_t *data)
{
	int ret;
	int arg;

	if (!kstrtoint_from_user(buffer, count, 0, &arg))
		ret = switch_fps_error_threhosld(arg);
	else
		ret = -EINVAL;

	return (ret < 0) ? ret : count;
}

static int fstb_tune_error_threshold_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, fstb_tune_error_threshold_read, NULL);
}

static const struct file_operations fstb_tune_error_threshold_fops = {
	.owner = THIS_MODULE,
	.open = fstb_tune_error_threshold_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = fstb_tune_error_threshold_write,
	.release = single_release,
};

static int mtk_fstb_fps_proc_read(struct seq_file *m, void *v)
{
	/**
	 * The format to print out:
	 *  fstb_enable <0 or 1>
	 *  kernel_log <0 or 1>
	 */
	seq_printf(m, "fstb_enable %d\n", fstb_enable);
	seq_printf(m, "fstb_log %d\n", fstb_fps_klog_on);
	seq_printf(m, "fstb_active %d\n", fstb_active);
	seq_printf(m, "fstb_idle_cnt %d\n", fstb_idle_cnt);

	return 0;
}

static ssize_t mtk_fstb_fps_proc_write(struct file *filp,
		const char __user *buffer, size_t count, loff_t *data)
{
	int len = 0;
	char desc[128];
	int k_enable, klog_on;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	/**
	 * sscanf format <fstb_enable> <klog_on>
	 * <klog_on> can only be 0 or 1
	 */

	if (data == NULL) {
		mtk_fstb_dprintk("[%s] null data\n", __func__);
		return -EINVAL;
	}

	if (sscanf(desc, "%d %d", &k_enable, &klog_on) >= 1) {
		if (k_enable == 0 || k_enable == 1)
			fpsgo_ctrl2fstb_switch_fstb(k_enable);

		if (klog_on == 0 || klog_on == 1)
			fstb_fps_klog_on = klog_on;

		return count;
	}

	mtk_fstb_dprintk("[%s] bad arg\n", __func__);
	return -EINVAL;
}

static int mtk_fstb_fps_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_fstb_fps_proc_read, NULL);
}

static const struct file_operations fstb_fps_fops = {
	.owner = THIS_MODULE,
	.open = mtk_fstb_fps_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtk_fstb_fps_proc_write,
	.release = single_release,
};

static ssize_t fpsgo_status_write(struct file *filp,
		const char __user *buf, size_t len, loff_t *data)
{
	char tmp[32] = {0};

	len = (len < (sizeof(tmp) - 1)) ? len : (sizeof(tmp) - 1);
	return len;
}

static int fpsgo_status_read(struct seq_file *m, void *v)
{
	struct FSTB_FRAME_INFO *iter;
	int fteh_pid;
	int fteh_state;

	mutex_lock(&fstb_lock);

	if (!fstb_enable) {
		mutex_unlock(&fstb_lock);
		return 0;
	}

	seq_puts(m,
	"tid\tname\t\tcurrentFPS\ttargetFPS\tFPS_margin\tfteh_list\n"
	);

	hlist_for_each_entry(iter, &fstb_frame_infos, hlist) {
		seq_printf(m, "%d\t", iter->pid);
		seq_printf(m, "%s\t", iter->proc_name);

		seq_printf(m, "%d\t\t",
				iter->queue_fps > max_fps_limit ?
				max_fps_limit : iter->queue_fps);

		seq_printf(m, "%d\t\t", iter->target_fps);

		seq_printf(m, "%d\t\t", iter->target_fps_margin);

		seq_printf(m, "%d\t\t",
			fpsgo_fbt2fstb_query_fteh_list(iter->pid));

		seq_puts(m, "\n");

	}

	mutex_unlock(&fstb_lock);

	fteh_state = fpsgo_fteh_get_state(&fteh_pid);
	seq_puts(m, "\nFTEH\tstate\tcur_pid\n");
	seq_printf(m, "\t%d\t%d\n", fteh_state, fteh_pid);

	seq_printf(m, "fstb_is_cam_active:%d\n", fstb_is_cam_active);
	seq_printf(m, "dfps_ceiling:%d\n", dfps_ceiling);

	return 0;
}

static int fpsgo_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, fpsgo_status_read, NULL);
}

static const struct file_operations fpsgo_status_fops = {
	.owner = THIS_MODULE,
	.open = fpsgo_status_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = fpsgo_status_write,
	.release = single_release,
};

int mtk_fstb_init(void)
{
	int num_cluster = 0;

	mtk_fstb_dprintk_always("init\n");

#if API_READY
	num_cluster = arch_get_nr_clusters();
#else
	num_cluster = 2;
#endif
	ged_kpi_output_gfx_info2_fp = gpu_time_update;

	/* create debugfs file */
	if (!fpsgo_debugfs_dir)
		goto err;

	fstb_debugfs_dir = debugfs_create_dir("fstb",
			fpsgo_debugfs_dir);
	if (!fstb_debugfs_dir) {
		mtk_fstb_dprintk_always(
				"[%s]: mkdir /sys/kernel/debug/fpsgo/fstb failed\n",
				__func__);
		goto err;
	}

	debugfs_create_file("fpsgo_status",
			0664,
			fstb_debugfs_dir,
			NULL,
			&fpsgo_status_fops);

	debugfs_create_file("fstb_debug",
			0664,
			fstb_debugfs_dir,
			NULL,
			&fstb_fps_fops);

	debugfs_create_file("fstb_soft_level",
			0664,
			fstb_debugfs_dir,
			NULL,
			&fstb_soft_level_fops);

	debugfs_create_file("fstb_fps_list",
			0664,
			fstb_debugfs_dir,
			NULL,
			&fstb_fps_list_fops);

	debugfs_create_file("fstb_fteh_list",
			0664,
			fstb_debugfs_dir,
			NULL,
			&fstb_fteh_list_fops);

	debugfs_create_file("fstb_tune_error_threshold",
			0664,
			fstb_debugfs_dir,
			NULL,
			&fstb_tune_error_threshold_fops);

	debugfs_create_file("fstb_tune_quantile",
			0664,
			fstb_debugfs_dir,
			NULL,
			&fstb_tune_quantile_fops);

	debugfs_create_file("fstb_tune_window_size",
			0664,
			fstb_debugfs_dir,
			NULL,
			&fstb_tune_window_size_fops);

	debugfs_create_file("margin_mode",
			0664,
			fstb_debugfs_dir,
			NULL,
			&fstb_margin_mode_fops);

	debugfs_create_file("margin_mode_dbnc_a",
			0664,
			fstb_debugfs_dir,
			NULL,
			&fstb_margin_mode_dbnc_a_fops);

	debugfs_create_file("margin_mode_dbnc_b",
			0664,
			fstb_debugfs_dir,
			NULL,
			&fstb_margin_mode_dbnc_b_fops);

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

	/* remove the debugfs file */
	debugfs_remove_recursive(fstb_debugfs_dir);
	return 0;
}
