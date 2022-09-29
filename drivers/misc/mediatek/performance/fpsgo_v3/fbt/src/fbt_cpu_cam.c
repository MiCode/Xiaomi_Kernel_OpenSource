// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/hardirq.h>
#include <linux/init.h>
#include <linux/kallsyms.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/sort.h>
#include <linux/string.h>
#include <asm/div64.h>
#include <linux/topology.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/bsearch.h>
#include <linux/sched/task.h>
#include <sched/sched.h>
#include <linux/cpufreq.h>
#include "sugov/cpufreq.h"

#include <mt-plat/fpsgo_common.h>

#include "fpsgo_base.h"
#include "fpsgo_sysfs.h"
#include "fpsgo_usedext.h"
#include "fbt_usedext.h"
#include "fbt_cpu.h"
#include "fbt_cpu_platform.h"
#include "fbt_cpu_ctrl.h"
#include "xgf.h"
#include "mini_top.h"
#include "fps_composer.h"
#include "fbt_cpu_cam.h"

#define NSEC_PER_HUSEC 100000
#define IDLE_DBNC 10
#define MAX_PID_DIGIT 7
#define MAIN_LOG_SIZE 256
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

static atomic_t fbt_cam_uclamp_boost_enable;
static atomic_t fbt_cam_gcc_enable;
static int fbt_cam_active;
static int fbt_cam_idle_cnt;
static int cluster_num;
static DEFINE_MUTEX(fbt_cam_frame_lock);
static DEFINE_MUTEX(fbt_cam_tree_lock);
static struct kobject *fbt_cam_kobj;
static struct rb_root fbt_cam_thread_tree;
static struct hrtimer hrt;
static struct workqueue_struct *jerk_wq;
static struct workqueue_struct *recycle_wq;
static void do_recycle(struct work_struct *work);
static DECLARE_WORK(recycle_work, (void *) do_recycle);

static int fbt_cam_rescue_pct_1 = 100;
static int fbt_cam_rescue_f_1 = 25;
static int fbt_cam_rescue_pct_2 = 200;
static int fbt_cam_rescue_f_2 = 25;
static int fbt_cam_gcc_std_filter = 2;
static int fbt_cam_gcc_history_window = 60;
static int fbt_cam_gcc_up_check = 15;
static int fbt_cam_gcc_up_thrs = 100;
static int fbt_cam_gcc_up_step = 5;
static int fbt_cam_gcc_down_check = 60;
static int fbt_cam_gcc_down_thrs = 5;
static int fbt_cam_gcc_down_step = 10;

HLIST_HEAD(fbt_cam_frames);

static int nsec_to_100usec(unsigned long long nsec)
{
	unsigned long long husec;

	husec = div64_u64(nsec, (unsigned long long)NSEC_PER_HUSEC);

	return (int)husec;
}

static void enable_fbt_cpu_cam_timer(void)
{
	hrtimer_start(&hrt, ktime_set(0, NSEC_PER_SEC), HRTIMER_MODE_REL);
}

static void disable_fbt_cpu_cam_timer(void)
{
	hrtimer_cancel(&hrt);
}

static int fbt_cam_jerk_is_finish(struct fbt_cam_frame *iter)
{
	int i;

	for (i = 0; i < RESCUE_TIMER_NUM; i++) {
		if (iter->proc_1.jerks[i].jerking)
			return 0;
		if (iter->proc_2.jerks[i].jerking)
			return 0;
	}

	return 1;
}

static struct fbt_cam_group *get_fbt_cam_group(struct fbt_cam_thread *iter1,
	int group_id, int blc_wt, int force)
{
	struct rb_node **p = &iter1->group_tree.rb_node;
	struct rb_node *parent = NULL;
	struct fbt_cam_group *iter2 = NULL;

	while (*p) {
		parent = *p;
		iter2 = rb_entry(parent, struct fbt_cam_group, rb_node);

		if (group_id < iter2->group_id)
			p = &(*p)->rb_left;
		else if (group_id > iter2->group_id)
			p = &(*p)->rb_right;
		else
			return iter2;
	}

	if (!force)
		return NULL;

	iter2 = kzalloc(sizeof(*iter2), GFP_KERNEL);
	if (!iter2)
		return NULL;
	iter2->group_id = group_id;
	iter2->blc_wt = blc_wt;

	rb_link_node(&iter2->rb_node, parent, p);
	rb_insert_color(&iter2->rb_node, &iter1->group_tree);

	return iter2;
}

static struct fbt_cam_thread *get_fbt_cam_thread(int tid, int force)
{
	struct rb_node **p = &fbt_cam_thread_tree.rb_node;
	struct rb_node *parent = NULL;
	struct fbt_cam_thread *iter = NULL;

	while (*p) {
		parent = *p;
		iter = rb_entry(parent, struct fbt_cam_thread, rb_node);

		if (tid < iter->tid)
			p = &(*p)->rb_left;
		else if (tid > iter->tid)
			p = &(*p)->rb_right;
		else
			return iter;
	}

	if (!force)
		return NULL;

	iter = kzalloc(sizeof(*iter), GFP_KERNEL);
	if (!iter)
		return NULL;
	iter->group_tree = RB_ROOT;
	iter->tid = tid;

	rb_link_node(&iter->rb_node, parent, p);
	rb_insert_color(&iter->rb_node, &fbt_cam_thread_tree);

	fbt_xgff_dep_thread_notify(tid, 1);

	return iter;
}

static void do_recycle(struct work_struct *work)
{
	int i;
	unsigned long long diff;
	unsigned long long period = NSEC_PER_SEC;
	unsigned long long cur_ts = fpsgo_get_time();
	struct fbt_cam_frame *iter1;
	struct fbt_cam_thread *iter2;
	struct fbt_cam_group *iter3;
	struct hlist_node *h;

	mutex_lock(&fbt_cam_frame_lock);

	if (hlist_empty(&fbt_cam_frames)) {
		fbt_cam_idle_cnt++;
		if (fbt_cam_idle_cnt >= IDLE_DBNC) {
			fbt_cam_active = 0;
			goto out;
		}
	}

	hlist_for_each_entry_safe(iter1, h, &fbt_cam_frames, hlist) {
		diff = cur_ts - iter1->ts;
		if (diff >= period && fbt_cam_jerk_is_finish(iter1)) {
			mutex_lock(&fbt_cam_tree_lock);
			for (i = 0; i < iter1->dep_list_num; i++) {
				iter2 = get_fbt_cam_thread(iter1->dep_list[i], 0);
				if (iter2) {
					iter3 = get_fbt_cam_group(iter2, iter1->group_id,
						iter1->blc_wt, 0);
					if (iter3) {
						rb_erase(&iter3->rb_node, &iter2->group_tree);
						kfree(iter3);
					}
				}
				if (RB_EMPTY_ROOT(&iter2->group_tree)) {
					fbt_xgff_dep_thread_notify(iter2->tid, -1);
					rb_erase(&iter2->rb_node, &fbt_cam_thread_tree);
					kfree(iter2);
				}
			}
			mutex_unlock(&fbt_cam_tree_lock);

			kfree(iter1->dep_list);
			kfree(iter1->area);
			fbt_xgff_list_blc_del(iter1->p_blc);
			hlist_del(&iter1->hlist);
			kfree(iter1);
		}
	}

	enable_fbt_cpu_cam_timer();

out:
	mutex_unlock(&fbt_cam_frame_lock);
}

static enum hrtimer_restart recycle_fbt_cam_frame(struct hrtimer *timer)
{
	if (recycle_wq)
		queue_work(recycle_wq, &recycle_work);
	else
		schedule_work(&recycle_work);

	return HRTIMER_NORESTART;
}

static int fbt_cam_blc_wt_arb(int tid, int group_id, int blc_wt)
{
	int ret = 0, max_blc_wt = 0;
	struct fbt_cam_thread *iter1;
	struct fbt_cam_group *iter2;
	struct rb_root *rbr;
	struct rb_node *rbn;

	mutex_lock(&fbt_cam_tree_lock);

	iter1 = get_fbt_cam_thread(tid, 0);
	if (!iter1)
		goto out;

	rbr = &iter1->group_tree;
	for (rbn = rb_first(rbr); rbn; rbn = rb_next(rbn)) {
		iter2 = rb_entry(rbn, struct fbt_cam_group, rb_node);
		if (iter2->group_id == group_id)
			iter2->blc_wt = blc_wt;
		if (iter2->blc_wt > max_blc_wt)
			max_blc_wt = iter2->blc_wt;
	}

	if (blc_wt == max_blc_wt)
		ret = 1;

	fpsgo_systrace_c_fbt_debug(tid, group_id, ret, "arb_flag");

out:
	mutex_unlock(&fbt_cam_tree_lock);
	return ret;
}

static void fbt_cam_blc_wt_reset(int group_id)
{
	int i;
	int all_zero_flag = 1;
	int related_flag = 0;
	int clear_list[100];
	int clear_list_num = 0;
	struct fbt_cam_thread *iter1;
	struct fbt_cam_group *iter2;
	struct rb_root *rbr1, *rbr2;
	struct rb_node *rbn1, *rbn2;

	mutex_lock(&fbt_cam_tree_lock);

	rbr1 = &fbt_cam_thread_tree;
	for (rbn1 = rb_first(rbr1); rbn1; rbn1 = rb_next(rbn1)) {
		iter1 = rb_entry(rbn1, struct fbt_cam_thread, rb_node);
		rbr2 = &iter1->group_tree;
		for (rbn2 = rb_first(rbr2); rbn2; rbn2 = rb_next(rbn2)) {
			iter2 = rb_entry(rbn2, struct fbt_cam_group, rb_node);
			if (iter2->group_id == group_id) {
				iter2->blc_wt = 0;
				related_flag = 1;
			}
			if (iter2->blc_wt > 0)
				all_zero_flag = 0;
		}
		if (all_zero_flag && related_flag) {
			clear_list[clear_list_num] = iter1->tid;
			clear_list_num++;
		}
		all_zero_flag = 1;
		related_flag = 0;
	}

	if (clear_list_num > 0) {
		for (i = 0; i < clear_list_num; i++) {
			fbt_set_per_task_cap(clear_list[i], 0, 100);
			fpsgo_systrace_c_fbt_debug(clear_list[i], group_id, 1, "clear_uclamp_flag");
			fpsgo_systrace_c_fbt_debug(clear_list[i], group_id, 0, "clear_uclamp_flag");
		}
	}

	mutex_unlock(&fbt_cam_tree_lock);
}

static void fbt_cam_all_blc_wt_reset(void)
{
	struct fbt_cam_thread *iter1;
	struct fbt_cam_group *iter2;
	struct rb_root *rbr1, *rbr2;
	struct rb_node *rbn1, *rbn2;

	mutex_lock(&fbt_cam_tree_lock);

	rbr1 = &fbt_cam_thread_tree;
	for (rbn1 = rb_first(rbr1); rbn1; rbn1 = rb_next(rbn1)) {
		iter1 = rb_entry(rbn1, struct fbt_cam_thread, rb_node);
		rbr2 = &iter1->group_tree;
		for (rbn2 = rb_first(rbr2); rbn2; rbn2 = rb_next(rbn2)) {
			iter2 = rb_entry(rbn2, struct fbt_cam_group, rb_node);
			iter2->blc_wt = 0;
		}
		fbt_set_per_task_cap(iter1->tid, 0, 100);
	}

	mutex_unlock(&fbt_cam_tree_lock);
}

static int calculate_gcc_quota(struct fbt_cam_frame *iter)
{
	int i, tmp_idx;
	int avg = 0;
	int quota = 0;
	long long std_square = 0;

	tmp_idx = iter->quota_raw_idx;
	for (i = 0; i < iter->gcc_history_window; i++) {
		quota += iter->quota_raw[tmp_idx];
		tmp_idx--;
		if (tmp_idx < 0)
			tmp_idx = QUOTA_RAW_SIZE - 1;
	}

	if (iter->gcc_std_filter > 0) {
		avg = quota / iter->gcc_history_window;
		tmp_idx = iter->quota_raw_idx;
		for (i = 0; i < iter->gcc_history_window; i++) {
			std_square += (long long)(iter->quota_raw[tmp_idx] - avg) *
					(long long)(iter->quota_raw[tmp_idx] - avg);
			tmp_idx--;
			if (tmp_idx < 0)
				tmp_idx = QUOTA_RAW_SIZE - 1;

		}
		do_div(std_square, iter->gcc_history_window);

		quota = 0;
		tmp_idx = iter->quota_raw_idx;
		for (i = 0; i < iter->gcc_history_window; i++) {
			if ((long long)(iter->quota_raw[tmp_idx] - avg) *
				(long long)(iter->quota_raw[tmp_idx] - avg) <
				(long long)(iter->gcc_std_filter * iter->gcc_std_filter
								* std_square) ||
				iter->quota_raw[tmp_idx] + iter->target_time > 0)
				quota += iter->quota_raw[tmp_idx];
			tmp_idx--;
			if (tmp_idx < 0)
				tmp_idx = QUOTA_RAW_SIZE - 1;
		}

		fpsgo_main_trace("[fbt_cam] %s group_id=%d avg:%d std:%lld quota:%d",
			__func__, iter->group_id, avg, std_square, quota);
	}

	return quota;
}

static void do_fbt_cam_gcc(struct fbt_cam_frame *iter, int blc_wt, int max_cap)
{
	int i;
	int tmp_idx;
	int quota;
	int update_quota_flag = 0;
	int check_flag = 0;

	tmp_idx = iter->quota_raw_idx + 1;
	if (tmp_idx >= QUOTA_RAW_SIZE)
		tmp_idx = 0;

	iter->quota_raw[tmp_idx] = iter->target_time - iter->q2q_time;
	iter->quota_raw_idx = tmp_idx;
	fpsgo_systrace_c_fbt(iter->group_id, 0, iter->quota_raw[tmp_idx], "quota_raw");

	if (iter->gcc_target_time != iter->target_time) {
		for (i = 0; i < QUOTA_RAW_SIZE; i++)
			iter->quota_raw[i] = 0;
		iter->quota_raw_idx = 0;
		iter->prev_total_quota = 0;
		iter->cur_total_quota = 0;
		iter->gcc_correction = 0;
		iter->gcc_target_time = iter->target_time;
	}

	if (iter->frame_count % iter->gcc_up_check == 0 &&
		iter->frame_count % iter->gcc_down_check == 0) {
		check_flag = 100;
		quota = calculate_gcc_quota(iter);
		update_quota_flag = 1;
		if (quota * 100 >= iter->target_time * iter->gcc_down_thrs)
			goto check_deboost;
		if (quota * 100 + iter->target_time * iter->gcc_up_thrs <= 0)
			goto check_boost;
		goto done;
	}

check_deboost:
	if (iter->frame_count % iter->gcc_down_check == 0) {
		check_flag += 10;
		if (!update_quota_flag) {
			quota = calculate_gcc_quota(iter);
			update_quota_flag = 1;
		}
		if (quota * 100 < iter->target_time * iter->gcc_down_thrs) {
			check_flag += 2;
			goto done;
		}
		if (iter->prev_total_quota > quota) {
			check_flag += 3;
			goto done;
		}
		iter->gcc_correction -= iter->gcc_down_step;
		goto done;
	}

check_boost:
	if (iter->frame_count % iter->gcc_up_check == 0) {
		check_flag += 20;
		if (!update_quota_flag) {
			quota = calculate_gcc_quota(iter);
			update_quota_flag = 1;
		}
		if (quota * 100 + iter->target_time * iter->gcc_up_thrs > 0) {
			check_flag += 2;
			goto done;
		}
		if (iter->prev_total_quota < quota) {
			check_flag += 3;
			goto done;
		}
		iter->gcc_correction += iter->gcc_up_step;
		goto done;
	}

done:
	if (update_quota_flag) {
		iter->prev_total_quota = iter->cur_total_quota;
		iter->cur_total_quota = quota;
		fpsgo_systrace_c_fbt_debug(iter->group_id, 0,
			iter->prev_total_quota, "prev_total_quota");
		fpsgo_systrace_c_fbt_debug(iter->group_id, 0,
			iter->cur_total_quota, "cur_total_quota");
	}

	if (iter->gcc_correction + blc_wt > max_cap)
		iter->gcc_correction = max_cap - blc_wt;
	else if (iter->gcc_correction + blc_wt < 0)
		iter->gcc_correction = -blc_wt;
	fpsgo_systrace_c_fbt(iter->group_id, 0, check_flag, "gcc_boost");
	fpsgo_systrace_c_fbt(iter->group_id, 0, blc_wt, "before correction");
	fpsgo_systrace_c_fbt(iter->group_id, 0, iter->gcc_correction, "correction");

	if (unlikely(iter->frame_count == INT_MAX))
		iter->frame_count = 0;
	else
		iter->frame_count += 1;
}

static void fbt_cam_set_min_cap(struct fbt_cam_frame *iter, int min_cap, int jerk)
{
	char *dep_str = NULL;
	int i;
	int bhr_opp_local;
	int bhr_local;
	int max_cap;

	if (!atomic_read(&fbt_cam_uclamp_boost_enable))
		return;

	if (!iter->dep_list)
		return;

	if (!min_cap) {
		fbt_cam_blc_wt_reset(iter->group_id);
		fpsgo_systrace_c_fbt(iter->group_id, 0,
			0,	"perf idx");
		fpsgo_systrace_c_fbt(iter->group_id, 0,
			100,	"perf_idx_max");
		return;
	}

	if (!jerk) {
		bhr_opp_local = fbt_cpu_get_bhr_opp();
		bhr_local = fbt_cpu_get_bhr();
	} else {
		bhr_opp_local = fbt_cpu_get_rescue_opp_c();
		bhr_local = 0;
	}

	max_cap = fbt_get_max_cap(min_cap, bhr_opp_local, bhr_local,
		iter->group_id, 0);

	dep_str = kcalloc(iter->dep_list_num + 1, MAX_PID_DIGIT * sizeof(char),
				GFP_KERNEL);
	if (!dep_str)
		return;

	for (i = 0; i < iter->dep_list_num; i++) {
		char temp[MAX_PID_DIGIT] = {"\0"};

		if (fbt_cam_blc_wt_arb(iter->dep_list[i], iter->group_id, min_cap))
			fbt_set_per_task_cap(iter->dep_list[i], min_cap, max_cap);

		if (strlen(dep_str) == 0)
			snprintf(temp, sizeof(temp), "%d", iter->dep_list[i]);
		else
			snprintf(temp, sizeof(temp), ",%d", iter->dep_list[i]);

		if (strlen(dep_str) + strlen(temp) < MAIN_LOG_SIZE)
			strncat(dep_str, temp, strlen(temp));
	}

	fpsgo_main_trace("[%d] dep-list %s", iter->group_id, dep_str);
	kfree(dep_str);

	fpsgo_systrace_c_fbt(iter->group_id, 0, min_cap, "perf_idx");
	fpsgo_systrace_c_fbt(iter->group_id, 0, max_cap, "perf_idx_max");
}

static int fbt_cam_get_next_jerk(int cur_id)
{
	int ret_id;

	ret_id = cur_id + 1;
	if (ret_id >= RESCUE_TIMER_NUM)
		ret_id = 0;

	return ret_id;
}

static void fbt_cam_do_jerk_locked(struct fbt_cam_frame *iter, int num)
{
	int rescue_opp_c_local = fbt_cpu_get_rescue_opp_c();
	unsigned int blc_wt = 0;
	struct cpu_ctrl_data *pld;

	blc_wt = iter->blc_wt;
	if (!blc_wt)
		return;

	pld = kcalloc(cluster_num,
			sizeof(struct cpu_ctrl_data), GFP_KERNEL);
	if (!pld)
		return;

	blc_wt = fbt_get_new_base_blc(pld, blc_wt,
		iter->rescue_f_1, 0, rescue_opp_c_local);
	if (!blc_wt)
		return;

	blc_wt = fbt_limit_capacity(blc_wt, 1);

	fpsgo_systrace_c_fbt(iter->group_id, 0, num, "jerk_need");
	fpsgo_systrace_c_fbt(iter->group_id, 0, 0, "jerk_need");

	iter->blc_wt = blc_wt;
	fbt_cam_set_min_cap(iter, blc_wt, 1);
	fbt_set_ceiling(pld, iter->group_id, 0);

	kfree(pld);
}

static void fbt_cam_cancel_jerk(struct fbt_cam_frame *iter, int num)
{
	int i;
	struct fbt_jerk *jerk;

	for (i = 0; i < RESCUE_TIMER_NUM; i++) {
		if (num == 1)
			jerk = &iter->proc_1.jerks[i];
		else
			jerk = &iter->proc_2.jerks[i];

		if (jerk->jerking) {
			hrtimer_cancel(&(jerk->timer));
			jerk->jerking = 0;
		}
	}
}

static int fbt_cam_set_jerk(struct fbt_cam_frame *iter, int num)
{
	int active_jerk_id;
	unsigned long long t2wnt;
	struct hrtimer *timer;

	t2wnt = iter->target_time;
	if (num == 1)
		t2wnt = t2wnt * iter->rescue_pct_1 / 100;
	else
		t2wnt = t2wnt * iter->rescue_pct_2 / 100;
	t2wnt = MAX(1ULL, t2wnt);
	fpsgo_systrace_c_fbt(iter->group_id, 0, t2wnt, "t2wnt");
	fpsgo_systrace_c_fbt(iter->group_id, 0, 0, "t2wnt");

	if (num == 1) {
		active_jerk_id = fbt_cam_get_next_jerk(iter->proc_1.active_jerk_id);
		iter->proc_1.active_jerk_id = active_jerk_id;
		if (t2wnt == 1) {
			fbt_cam_do_jerk_locked(iter, 1);
			return 0;
		}
		timer = &(iter->proc_1.jerks[active_jerk_id].timer);
		if (timer) {
			if (iter->proc_1.jerks[active_jerk_id].jerking == 0) {
				iter->proc_1.jerks[active_jerk_id].jerking = 1;
				hrtimer_start(timer, ns_to_ktime(t2wnt), HRTIMER_MODE_REL);
			}
		}
	} else {
		active_jerk_id = fbt_cam_get_next_jerk(iter->proc_2.active_jerk_id);
		iter->proc_2.active_jerk_id = active_jerk_id;
		if (t2wnt == 1) {
			fbt_cam_do_jerk_locked(iter, 2);
			return 0;
		}
		timer = &(iter->proc_2.jerks[active_jerk_id].timer);
		if (timer) {
			if (iter->proc_2.jerks[active_jerk_id].jerking == 0) {
				iter->proc_2.jerks[active_jerk_id].jerking = 1;
				hrtimer_start(timer, ns_to_ktime(t2wnt), HRTIMER_MODE_REL);
			}
		}
	}

	return 1;
}

static void fbt_cam_do_jerk1(struct work_struct *work)
{
	struct fbt_jerk *jerk;
	struct fbt_proc *proc_1;
	struct fbt_cam_frame *iter;

	jerk = container_of(work, struct fbt_jerk, work);
	if (!jerk || jerk->id < 0 || jerk->id > RESCUE_TIMER_NUM - 1)
		return;

	proc_1 = container_of(jerk, struct fbt_proc, jerks[jerk->id]);
	if (!proc_1 || proc_1->active_jerk_id < 0 ||
		proc_1->active_jerk_id > RESCUE_TIMER_NUM - 1)
		return;

	iter = container_of(proc_1, struct fbt_cam_frame, proc_1);
	if (!iter)
		return;

	mutex_lock(&fbt_cam_frame_lock);
	if (jerk->id == proc_1->active_jerk_id)
		fbt_cam_do_jerk_locked(iter, 1);

	jerk->jerking = 0;
	mutex_unlock(&fbt_cam_frame_lock);
}

static void fbt_cam_do_jerk2(struct work_struct *work)
{
	struct fbt_jerk *jerk;
	struct fbt_proc *proc_2;
	struct fbt_cam_frame *iter;

	jerk = container_of(work, struct fbt_jerk, work);
	if (!jerk || jerk->id < 0 || jerk->id > RESCUE_TIMER_NUM - 1)
		return;

	proc_2 = container_of(jerk, struct fbt_proc, jerks[jerk->id]);
	if (!proc_2 || proc_2->active_jerk_id < 0 ||
		proc_2->active_jerk_id > RESCUE_TIMER_NUM - 1)
		return;

	iter = container_of(proc_2, struct fbt_cam_frame, proc_2);
	if (!iter)
		return;

	mutex_lock(&fbt_cam_frame_lock);
	if (jerk->id == proc_2->active_jerk_id)
		fbt_cam_do_jerk_locked(iter, 2);

	jerk->jerking = 0;
	mutex_unlock(&fbt_cam_frame_lock);
}

static enum hrtimer_restart fbt_cam_jerk_tfn(struct hrtimer *timer)
{
	struct fbt_jerk *jerk;

	jerk = container_of(timer, struct fbt_jerk, timer);
	if (jerk_wq)
		queue_work(jerk_wq, &jerk->work);
	else
		schedule_work(&jerk->work);
	return HRTIMER_NORESTART;
}

static void fbt_cam_init_jerk(struct fbt_cam_frame *iter)
{
	int i;
	struct fbt_jerk *jerk;

	for (i = 0; i < RESCUE_TIMER_NUM; i++) {
		jerk = &iter->proc_1.jerks[i];
		jerk->id = i;
		jerk->jerking = 0;
		jerk->postpone = 0;
		hrtimer_init(&jerk->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		jerk->timer.function = &fbt_cam_jerk_tfn;
		INIT_WORK(&jerk->work, fbt_cam_do_jerk1);

		jerk = &iter->proc_2.jerks[i];
		jerk->id = i;
		jerk->jerking = 0;
		jerk->postpone = 0;
		hrtimer_init(&jerk->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		jerk->timer.function = &fbt_cam_jerk_tfn;
		INIT_WORK(&jerk->work, fbt_cam_do_jerk2);
	}
}

static int find_fbt_cam_frame(int group_id, struct fbt_cam_frame **ret)
{
	struct fbt_cam_frame *iter;
	struct hlist_node *h;

	hlist_for_each_entry_safe(iter, h, &fbt_cam_frames, hlist) {
		if (iter->group_id != group_id)
			continue;

		if (ret)
			*ret = iter;
		return 0;
	}

	return -EINVAL;
}

static int new_fbt_cam_frame(int group_id, int *dep_list, int dep_list_num,
	unsigned long long ts, struct fbt_cam_frame **ret)
{
	int i;
	unsigned long long tmp_runtime = 0, total_runtime = 0;
	struct fbt_cam_frame *iter;

	if (!dep_list || dep_list_num <= 0)
		return -EINVAL;

	iter = kzalloc(sizeof(*iter), GFP_KERNEL);

	if (!iter)
		return -ENOMEM;

	iter->dep_list = kcalloc(dep_list_num, sizeof(int), GFP_KERNEL);
	if (!iter->dep_list) {
		kfree(iter);
		return -ENOMEM;
	}

	for (i = 0; i < dep_list_num; i++) {
		iter->dep_list[i] = dep_list[i];

		tmp_runtime = 0;
		xgf_get_runtime(iter->dep_list[i], &tmp_runtime);
		total_runtime += tmp_runtime;
	}

	iter->area = kzalloc(cluster_num * sizeof(long), GFP_KERNEL);
	if (!iter->area) {
		kfree(iter->dep_list);
		kfree(iter);
		return -ENOMEM;
	}

	iter->p_blc = fbt_xgff_list_blc_add(group_id, 0);
	if (iter->p_blc == NULL) {
		kfree(iter->area);
		kfree(iter->dep_list);
		kfree(iter);
		return -ENOMEM;
	}

	iter->ploading = fbt_xgff_list_loading_add(group_id, 0, ts);

	fbt_cam_init_jerk(iter);

	iter->group_id = group_id;
	iter->dep_list_num = dep_list_num;
	iter->blc_wt = 0;
	iter->ts = ts;
	iter->last_runtime = total_runtime;
	iter->cpu_time = 0;
	iter->q2q_time = 0;
	iter->target_time = 0;
	iter->frame_count = 0;
	iter->rescue_pct_1 = fbt_cam_rescue_pct_1;
	iter->rescue_f_1 = fbt_cam_rescue_f_1;
	iter->proc_1.active_jerk_id = -1;
	iter->rescue_pct_2 = fbt_cam_rescue_pct_2;
	iter->rescue_f_2 = fbt_cam_rescue_f_2;
	iter->proc_2.active_jerk_id = -1;
	iter->quota_raw_idx = 0;
	iter->prev_total_quota = 0;
	iter->cur_total_quota = 0;
	iter->gcc_std_filter = fbt_cam_gcc_std_filter;
	iter->gcc_history_window = fbt_cam_gcc_history_window;
	iter->gcc_up_check = fbt_cam_gcc_up_check;
	iter->gcc_up_thrs = fbt_cam_gcc_up_thrs;
	iter->gcc_up_step = fbt_cam_gcc_up_step;
	iter->gcc_down_check = fbt_cam_gcc_down_check;
	iter->gcc_down_thrs = fbt_cam_gcc_down_thrs;
	iter->gcc_down_step = fbt_cam_gcc_down_step;
	iter->gcc_correction = 0;
	iter->gcc_target_time = 0;

	if (ret)
		*ret = iter;
	return 0;
}

static void fbt_cam_frame_set_param(struct fbt_cam_frame *iter, int *param)
{
	if (param[0] <= 0)
		iter->rescue_pct_1 = fbt_cam_rescue_pct_1;
	else
		iter->rescue_pct_1 = param[0];
	if (param[1] <= 0)
		iter->rescue_f_1 = fbt_cam_rescue_f_1;
	else
		iter->rescue_f_1 = param[1];
	if (param[2] <= 0)
		iter->rescue_pct_2 = fbt_cam_rescue_pct_2;
	else
		iter->rescue_pct_2 = param[2];
	if (param[3] <= 0)
		iter->rescue_f_2 = fbt_cam_rescue_f_2;
	else
		iter->rescue_f_2 = param[3];
	if (param[4] <= 0)
		iter->gcc_std_filter = fbt_cam_gcc_std_filter;
	else
		iter->gcc_std_filter = param[4];
	if (param[5] <= 0)
		iter->gcc_history_window = fbt_cam_gcc_history_window;
	else
		iter->gcc_history_window = param[5];
	if (param[6] <= 0)
		iter->gcc_up_check = fbt_cam_gcc_up_check;
	else
		iter->gcc_up_check = param[6];
	if (param[7] <= 0)
		iter->gcc_up_thrs = fbt_cam_gcc_up_thrs;
	else
		iter->gcc_up_thrs = param[7];
	if (param[8] <= 0)
		iter->gcc_up_step = fbt_cam_gcc_up_step;
	else
		iter->gcc_up_step = param[8];
	if (param[9] <= 0)
		iter->gcc_down_check = fbt_cam_gcc_down_check;
	else
		iter->gcc_down_check = param[9];
	if (param[10] <= 0)
		iter->gcc_down_thrs = fbt_cam_gcc_down_thrs;
	else
		iter->gcc_down_thrs = param[10];
	if (param[11] <= 0)
		iter->gcc_down_step = fbt_cam_gcc_down_step;
	else
		iter->gcc_down_step = param[11];
}

void fpsgo_fbt2cam_notify_uclamp_boost_enable(int enable)
{
	atomic_set(&fbt_cam_uclamp_boost_enable, enable);

	if (!enable)
		fbt_cam_all_blc_wt_reset();
}

static int _fbt_cam_frame_start(int group_id, int *dep_list, int dep_list_num,
	int *param, unsigned long long ts,
	unsigned long long *cpu_time, unsigned long long *q2q_time, long *area)
{
	int i;
	int ret = XGF_NOTIFY_OK;
	unsigned long long tmp_runtime = 0, total_runtime = 0;
	struct fbt_cam_frame *r, **rframe;
	struct fbt_cam_thread *iter1;
	struct fbt_cam_group *iter2;

	mutex_lock(&fbt_cam_frame_lock);
	rframe = &r;

	if (find_fbt_cam_frame(group_id, rframe)) {
		if (new_fbt_cam_frame(group_id, dep_list, dep_list_num, ts, rframe)) {
			ret = XGF_PARAM_ERR;
			goto out;
		}

		if (r->dep_list) {
			mutex_lock(&fbt_cam_tree_lock);
			for (i = 0; i < r->dep_list_num; i++) {
				iter1 = get_fbt_cam_thread(r->dep_list[i], 1);
				if (iter1)
					iter2 = get_fbt_cam_group(iter1, group_id, r->blc_wt, 1);
			}
			mutex_unlock(&fbt_cam_tree_lock);
		}

		hlist_add_head(&r->hlist, &fbt_cam_frames);
	} else {
		if (r->dep_list) {
			for (i = 0; i < r->dep_list_num; i++) {
				tmp_runtime = 0;
				xgf_get_runtime(r->dep_list[i], &tmp_runtime);
				total_runtime += tmp_runtime;
			}
		}
		if (r->area) {
			memmove(area, r->area, cluster_num * sizeof(long));
			memset(r->area, 0, cluster_num * sizeof(long));
			fbt_xgff_get_loading_by_cluster(&r->ploading, ts, 0, 1, r->area);
		}
		r->blc_wt = 0;
		r->ts = ts;
		r->last_runtime = total_runtime;
		*cpu_time = r->cpu_time;
		*q2q_time = r->q2q_time;
	}

	fbt_cam_frame_set_param(r, param);

out:
	xgf_trace("[fbt_cam] %s result:%d group_id:%d", __func__, ret, group_id);
	mutex_unlock(&fbt_cam_frame_lock);
	return ret;
}

static int _fbt_cam_frame_end(unsigned int group_id, unsigned long long ts)
{
	int i;
	int ret = XGF_NOTIFY_OK;
	unsigned long long tmp_runtime = 0, total_runtime = 0;
	struct fbt_cam_frame *r, **rframe;

	mutex_lock(&fbt_cam_frame_lock);
	rframe = &r;
	if (find_fbt_cam_frame(group_id, rframe)) {
		ret = XGF_THREAD_NOT_FOUND;
		mutex_unlock(&fbt_cam_frame_lock);
		goto out;
	}

	if (r->area)
		fbt_xgff_get_loading_by_cluster(&r->ploading, ts, 0, 0, r->area);

	if (r->dep_list) {
		for (i = 0; i < r->dep_list_num; i++) {
			tmp_runtime = 0;
			xgf_get_runtime(r->dep_list[i], &tmp_runtime);
			total_runtime += tmp_runtime;
		}
		r->cpu_time = total_runtime - r->last_runtime;
	} else
		r->cpu_time = 0;
	r->q2q_time = ts - r->ts;
	r->ts = ts;
	r->blc_wt = 0;
	fbt_cam_cancel_jerk(r, 1);
	fbt_cam_cancel_jerk(r, 2);
	mutex_unlock(&fbt_cam_frame_lock);

	fbt_cam_blc_wt_reset(group_id);

out:
	xgf_trace("[fbt_cam] %s result:%d group_id:%d", __func__, ret, group_id);
	return ret;
}

static int fbt_cam_frame_startend(unsigned int startend,
		int group_id,
		int *dep_list,
		int dep_list_num,
		int *param,
		unsigned long long *cputime,
		unsigned long long *q2qtime,
		long *area)
{
	unsigned long long cur_ts = fpsgo_get_time();

	fpsgo_systrace_c_fbt(group_id, 0, startend, "fbt_cam_queueid");

	if (startend)
		return _fbt_cam_frame_start(group_id, dep_list, dep_list_num,
			param, cur_ts, cputime, q2qtime, area);

	return _fbt_cam_frame_end(group_id, cur_ts);
}

static int fbt_cam_calculate_original_blc_wt(int group_id, int prefer_cluster, long *area,
	unsigned long long cpu_time, unsigned long long q2q_time, unsigned long long target_time)
{
	int i;
	int blc_wt = 0;
	int final_cid = -1, user_max_cid = -1, default_max_cid = -1;
	long new_aa, final_aa, user_max_aa = 0, default_max_aa = 0;
	unsigned long tmp_prefer_cluster = prefer_cluster;
	unsigned long long temp_blc;
	unsigned long long t1, t2, t3;

	t1 = nsec_to_100usec(cpu_time);
	t2 = nsec_to_100usec(q2q_time);
	t3 = nsec_to_100usec(target_time);

	for (i = 0; i < cluster_num; i++) {
		if (test_bit(i, &tmp_prefer_cluster) && area[i] > user_max_aa) {
			user_max_cid = i;
			user_max_aa = area[i];
		}
		if (area[i] > default_max_aa) {
			default_max_cid = i;
			default_max_aa = area[i];
		}
	}

	if (user_max_cid >= 0 && user_max_cid < cluster_num &&
		user_max_aa > 0) {
		final_cid = user_max_cid;
		final_aa = user_max_aa;
	} else if (default_max_cid >= 0 && default_max_cid < cluster_num &&
		default_max_aa > 0) {
		final_cid = default_max_cid;
		final_aa = default_max_aa;
	} else {
		final_cid = -1;
		final_aa = 0;
	}
	fpsgo_systrace_c_fbt_debug(group_id, 0, user_max_cid, "cid_user");
	fpsgo_systrace_c_fbt_debug(group_id, 0, user_max_aa, "aa_user");
	fpsgo_systrace_c_fbt_debug(group_id, 0, default_max_cid, "cid_default");
	fpsgo_systrace_c_fbt_debug(group_id, 0, default_max_aa, "aa_default");
	fpsgo_systrace_c_fbt_debug(group_id, 0, final_cid, "cid_final");
	fpsgo_systrace_c_fbt_debug(group_id, 0, final_aa, "aa_final");

	if (final_aa <= 0) {
		blc_wt = 0;
	} else if (t1 > 0 && t2 > 0) {
		new_aa = final_aa * t1;
		new_aa = div64_s64(new_aa, t2);
		final_aa = new_aa;
		temp_blc = new_aa;
		do_div(temp_blc, (unsigned int)t3);
		blc_wt = (unsigned int)temp_blc;
	} else {
		temp_blc = final_aa;
		do_div(temp_blc, (unsigned int)t3);
		blc_wt = (unsigned int)temp_blc;
	}
	blc_wt = clamp(blc_wt, 1, 100);

	xgf_trace("[fbt_cam] %s perf_index=%d aa=%lld run=%llu Q2Q=%llu target=%llu ",
		__func__, blc_wt, final_aa, t1, t2, t3);
	fpsgo_systrace_c_fbt_debug(group_id, 0, final_aa, "aa_running_ratio");
	fpsgo_systrace_c_fbt(group_id, 0, cpu_time, "t_cpu");
	fpsgo_systrace_c_fbt(group_id, 0, q2q_time, "q2q_time");
	fpsgo_systrace_c_fbt(group_id, 0, target_time, "target_time");

	return blc_wt;
}

static int xgff_boost_startend(unsigned int startend, int group_id,
	int *dep_list, int dep_list_num, int prefer_cluster,
	unsigned long long target_time, int *param)
{
	int i;
	int xgff_ret;
	unsigned long long cpu_time = 0;
	unsigned long long q2q_time = 0;
	long *area;
	int blc_wt = 0;
	struct fbt_cam_frame *r, **rframe;
	struct fpsgo_loading *tmp_dep_list;

	if (!fpsgo_is_enable())
		return XGF_DISABLE;

	if (fbt_cam_idle_cnt) {
		fbt_cam_idle_cnt = 0;
		if (!fbt_cam_active) {
			fbt_cam_active = 1;
			enable_fbt_cpu_cam_timer();
		}
	}

	if (!startend) {
		xgff_ret = fbt_cam_frame_startend(0, group_id, NULL, 0,
			NULL, NULL, NULL, NULL);
		fpsgo_systrace_c_fbt(group_id, 0, xgff_ret, "xgff_ret");
		return 0;
	}

	area = kzalloc(cluster_num * sizeof(long), GFP_KERNEL);
	if (!area)
		return -ENOMEM;

	if (!dep_list)
		return -ENOMEM;

	xgff_ret = fbt_cam_frame_startend(1, group_id, dep_list, dep_list_num,
		param, &cpu_time, &q2q_time, area);
	fpsgo_systrace_c_fbt(group_id, 0, xgff_ret, "xgff_ret");
	if (!q2q_time) {
		kfree(area);
		return -EINVAL;
	}

	blc_wt = fbt_cam_calculate_original_blc_wt(group_id, prefer_cluster, area,
		cpu_time, q2q_time, target_time);
	kfree(area);

	blc_wt = fbt_limit_capacity(blc_wt, 0);

	mutex_lock(&fbt_cam_frame_lock);
	rframe = &r;
	if (find_fbt_cam_frame(group_id, rframe)) {
		mutex_unlock(&fbt_cam_frame_lock);
		return XGF_THREAD_NOT_FOUND;
	}

	r->target_time = target_time;
	if (atomic_read(&fbt_cam_gcc_enable)) {
		do_fbt_cam_gcc(r, blc_wt, 100);
		blc_wt = clamp(blc_wt + r->gcc_correction, 1, 100);
		blc_wt = fbt_limit_capacity(blc_wt, 0);
	}

	r->blc_wt = blc_wt;
	fbt_xgff_blc_set(r->p_blc, blc_wt, r->dep_list_num, r->dep_list);

	tmp_dep_list = kcalloc(r->dep_list_num, sizeof(struct fpsgo_loading), GFP_KERNEL);
	if (r->dep_list && tmp_dep_list) {
		for (i = 0; i < r->dep_list_num; i++)
			tmp_dep_list[i].pid = r->dep_list[i];
		fbt_set_limit(group_id, blc_wt, group_id, 0,
			r->dep_list_num, tmp_dep_list, NULL, cpu_time);
	} else {
		fbt_set_limit(group_id, blc_wt, group_id, 0,
			0, NULL, NULL, cpu_time);
	}
	kfree(tmp_dep_list);

	fbt_cam_set_min_cap(r, blc_wt, 0);
	fbt_cam_set_jerk(r, 1);
	fbt_cam_set_jerk(r, 2);
	mutex_unlock(&fbt_cam_frame_lock);

	return 0;
}

#define FBT_CAM_SYSFS_READ(name, show, variable); \
static ssize_t name##_show(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		char *buf) \
{ \
	if ((show)) \
		return scnprintf(buf, PAGE_SIZE, "%d\n", (variable)); \
	else \
		return 0; \
}

#define FBT_CAM_SYSFS_WRITE_VALUE(name, variable, min, max); \
static ssize_t name##_store(struct kobject *kobj, \
		struct kobj_attribute *attr, \
		const char *buf, size_t count) \
{ \
	char *acBuffer = NULL; \
	int arg; \
\
	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL); \
	if (!acBuffer) \
		goto out; \
\
	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) { \
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) { \
			if (kstrtoint(acBuffer, 0, &arg) == 0) { \
				if (arg >= (min) && arg <= (max)) { \
					mutex_lock(&fbt_cam_frame_lock); \
					(variable) = arg; \
					mutex_unlock(&fbt_cam_frame_lock); \
				} \
			} \
		} \
	} \
\
out: \
	kfree(acBuffer); \
	return count; \
}

static ssize_t fbt_cam_uclamp_boost_enable_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&fbt_cam_uclamp_boost_enable));
}

static KOBJ_ATTR_RO(fbt_cam_uclamp_boost_enable);

static ssize_t fbt_cam_gcc_enable_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&fbt_cam_gcc_enable));
}

static ssize_t fbt_cam_gcc_enable_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char *acBuffer = NULL;
	int arg;

	acBuffer = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!acBuffer)
		goto out;

	if ((count > 0) && (count < FPSGO_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, FPSGO_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				atomic_set(&fbt_cam_gcc_enable, !!arg);
		}
	}

out:
	kfree(acBuffer);
	return count;
}

static KOBJ_ATTR_RW(fbt_cam_gcc_enable);

FBT_CAM_SYSFS_READ(fbt_cam_rescue_pct_1, 1, fbt_cam_rescue_pct_1);
FBT_CAM_SYSFS_WRITE_VALUE(fbt_cam_rescue_pct_1, fbt_cam_rescue_pct_1, 0, 200);

static KOBJ_ATTR_RW(fbt_cam_rescue_pct_1);

FBT_CAM_SYSFS_READ(fbt_cam_rescue_f_1, 1, fbt_cam_rescue_f_1);
FBT_CAM_SYSFS_WRITE_VALUE(fbt_cam_rescue_f_1, fbt_cam_rescue_f_1, 0, 100);

static KOBJ_ATTR_RW(fbt_cam_rescue_f_1);

FBT_CAM_SYSFS_READ(fbt_cam_rescue_pct_2, 1, fbt_cam_rescue_pct_2);
FBT_CAM_SYSFS_WRITE_VALUE(fbt_cam_rescue_pct_2, fbt_cam_rescue_pct_2, 0, 200);

static KOBJ_ATTR_RW(fbt_cam_rescue_pct_2);

FBT_CAM_SYSFS_READ(fbt_cam_rescue_f_2, 1, fbt_cam_rescue_f_2);
FBT_CAM_SYSFS_WRITE_VALUE(fbt_cam_rescue_f_2, fbt_cam_rescue_f_2, 0, 100);

static KOBJ_ATTR_RW(fbt_cam_rescue_f_2);

FBT_CAM_SYSFS_READ(fbt_cam_gcc_std_filter, 1, fbt_cam_gcc_std_filter);
FBT_CAM_SYSFS_WRITE_VALUE(fbt_cam_gcc_std_filter, fbt_cam_gcc_std_filter, 0, INT_MAX);

static KOBJ_ATTR_RW(fbt_cam_gcc_std_filter);

FBT_CAM_SYSFS_READ(fbt_cam_gcc_history_window, 1, fbt_cam_gcc_history_window);
FBT_CAM_SYSFS_WRITE_VALUE(fbt_cam_gcc_history_window, fbt_cam_gcc_history_window, 1, INT_MAX);

static KOBJ_ATTR_RW(fbt_cam_gcc_history_window);

FBT_CAM_SYSFS_READ(fbt_cam_gcc_up_check, 1, fbt_cam_gcc_up_check);
FBT_CAM_SYSFS_WRITE_VALUE(fbt_cam_gcc_up_check, fbt_cam_gcc_up_check, 1, INT_MAX);

static KOBJ_ATTR_RW(fbt_cam_gcc_up_check);

FBT_CAM_SYSFS_READ(fbt_cam_gcc_up_thrs, 1, fbt_cam_gcc_up_thrs);
FBT_CAM_SYSFS_WRITE_VALUE(fbt_cam_gcc_up_thrs, fbt_cam_gcc_up_thrs, 0, INT_MAX);

static KOBJ_ATTR_RW(fbt_cam_gcc_up_thrs);

FBT_CAM_SYSFS_READ(fbt_cam_gcc_up_step, 1, fbt_cam_gcc_up_step);
FBT_CAM_SYSFS_WRITE_VALUE(fbt_cam_gcc_up_step, fbt_cam_gcc_up_step, 0, INT_MAX);

static KOBJ_ATTR_RW(fbt_cam_gcc_up_step);

FBT_CAM_SYSFS_READ(fbt_cam_gcc_down_check, 1, fbt_cam_gcc_down_check);
FBT_CAM_SYSFS_WRITE_VALUE(fbt_cam_gcc_down_check, fbt_cam_gcc_down_check, 1, INT_MAX);

static KOBJ_ATTR_RW(fbt_cam_gcc_down_check);

FBT_CAM_SYSFS_READ(fbt_cam_gcc_down_thrs, 1, fbt_cam_gcc_down_thrs);
FBT_CAM_SYSFS_WRITE_VALUE(fbt_cam_gcc_down_thrs, fbt_cam_gcc_down_thrs, 0, INT_MAX);

static KOBJ_ATTR_RW(fbt_cam_gcc_down_thrs);

FBT_CAM_SYSFS_READ(fbt_cam_gcc_down_step, 1, fbt_cam_gcc_down_step);
FBT_CAM_SYSFS_WRITE_VALUE(fbt_cam_gcc_down_step, fbt_cam_gcc_down_step, 0, INT_MAX);

static KOBJ_ATTR_RW(fbt_cam_gcc_down_step);

static ssize_t thread_status_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	char *temp = NULL;
	int pos = 0;
	int length = 0;
	struct fbt_cam_thread *iter1;
	struct fbt_cam_group *iter2;
	struct rb_root *rbr1, *rbr2;
	struct rb_node *rbn1, *rbn2;

	temp = kcalloc(FPSGO_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

	mutex_lock(&fbt_cam_tree_lock);

	rbr1 = &fbt_cam_thread_tree;
	for (rbn1 = rb_first(rbr1); rbn1; rbn1 = rb_next(rbn1)) {
		iter1 = rb_entry(rbn1, struct fbt_cam_thread, rb_node);

		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"tid:%d\t", iter1->tid);
		pos += length;

		rbr2 = &iter1->group_tree;
		for (rbn2 = rb_first(rbr2); rbn2; rbn2 = rb_next(rbn2)) {
			iter2 = rb_entry(rbn2, struct fbt_cam_group, rb_node);

			length = scnprintf(temp + pos,
				FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
				"group:%d(%d)\t", iter2->group_id, iter2->blc_wt);
			pos += length;
		}

		length = scnprintf(temp + pos,
			FPSGO_SYSFS_MAX_BUFF_SIZE - pos,
			"\n");
		pos += length;
	}

	mutex_unlock(&fbt_cam_tree_lock);

	length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(thread_status);

void __exit fbt_cpu_cam_exit(void)
{
	disable_fbt_cpu_cam_timer();

	fpsgo_sysfs_remove_file(fbt_cam_kobj,
			&kobj_attr_fbt_cam_uclamp_boost_enable);
	fpsgo_sysfs_remove_file(fbt_cam_kobj,
			&kobj_attr_fbt_cam_gcc_enable);
	fpsgo_sysfs_remove_file(fbt_cam_kobj,
			&kobj_attr_fbt_cam_rescue_pct_1);
	fpsgo_sysfs_remove_file(fbt_cam_kobj,
			&kobj_attr_fbt_cam_rescue_f_1);
	fpsgo_sysfs_remove_file(fbt_cam_kobj,
			&kobj_attr_fbt_cam_rescue_pct_2);
	fpsgo_sysfs_remove_file(fbt_cam_kobj,
			&kobj_attr_fbt_cam_rescue_f_2);
	fpsgo_sysfs_remove_file(fbt_cam_kobj,
			&kobj_attr_fbt_cam_gcc_std_filter);
	fpsgo_sysfs_remove_file(fbt_cam_kobj,
			&kobj_attr_fbt_cam_gcc_history_window);
	fpsgo_sysfs_remove_file(fbt_cam_kobj,
			&kobj_attr_fbt_cam_gcc_up_check);
	fpsgo_sysfs_remove_file(fbt_cam_kobj,
			&kobj_attr_fbt_cam_gcc_up_thrs);
	fpsgo_sysfs_remove_file(fbt_cam_kobj,
			&kobj_attr_fbt_cam_gcc_up_step);
	fpsgo_sysfs_remove_file(fbt_cam_kobj,
			&kobj_attr_fbt_cam_gcc_down_check);
	fpsgo_sysfs_remove_file(fbt_cam_kobj,
			&kobj_attr_fbt_cam_gcc_down_thrs);
	fpsgo_sysfs_remove_file(fbt_cam_kobj,
			&kobj_attr_fbt_cam_gcc_down_step);
	fpsgo_sysfs_remove_file(fbt_cam_kobj,
			&kobj_attr_thread_status);

	fpsgo_sysfs_remove_dir(&fbt_cam_kobj);
}

int __init fbt_cpu_cam_init(void)
{
	atomic_set(&fbt_cam_uclamp_boost_enable, 1);
	atomic_set(&fbt_cam_gcc_enable, 1);
	cluster_num = fpsgo_arch_nr_clusters();
	fbt_cam_thread_tree = RB_ROOT;

	xgff_boost_startend_fp = xgff_boost_startend;

	recycle_wq = alloc_ordered_workqueue("fbt_cam_recycle", WQ_HIGHPRI | WQ_MEM_RECLAIM);
	if (!recycle_wq)
		goto err;

	jerk_wq = alloc_workqueue("fbt_cam_jerk", WQ_HIGHPRI | WQ_MEM_RECLAIM | WQ_UNBOUND, 0);
	if (!jerk_wq)
		goto err;

	hrtimer_init(&hrt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrt.function = &recycle_fbt_cam_frame;

	enable_fbt_cpu_cam_timer();

	if (!fpsgo_sysfs_create_dir(NULL, "fbt_cam", &fbt_cam_kobj)) {
		fpsgo_sysfs_create_file(fbt_cam_kobj,
				&kobj_attr_fbt_cam_uclamp_boost_enable);
		fpsgo_sysfs_create_file(fbt_cam_kobj,
				&kobj_attr_fbt_cam_gcc_enable);
		fpsgo_sysfs_create_file(fbt_cam_kobj,
				&kobj_attr_fbt_cam_rescue_pct_1);
		fpsgo_sysfs_create_file(fbt_cam_kobj,
				&kobj_attr_fbt_cam_rescue_f_1);
		fpsgo_sysfs_create_file(fbt_cam_kobj,
				&kobj_attr_fbt_cam_rescue_pct_2);
		fpsgo_sysfs_create_file(fbt_cam_kobj,
				&kobj_attr_fbt_cam_rescue_f_2);
		fpsgo_sysfs_create_file(fbt_cam_kobj,
				&kobj_attr_fbt_cam_gcc_std_filter);
		fpsgo_sysfs_create_file(fbt_cam_kobj,
				&kobj_attr_fbt_cam_gcc_history_window);
		fpsgo_sysfs_create_file(fbt_cam_kobj,
				&kobj_attr_fbt_cam_gcc_up_check);
		fpsgo_sysfs_create_file(fbt_cam_kobj,
				&kobj_attr_fbt_cam_gcc_up_thrs);
		fpsgo_sysfs_create_file(fbt_cam_kobj,
				&kobj_attr_fbt_cam_gcc_up_step);
		fpsgo_sysfs_create_file(fbt_cam_kobj,
				&kobj_attr_fbt_cam_gcc_down_check);
		fpsgo_sysfs_create_file(fbt_cam_kobj,
				&kobj_attr_fbt_cam_gcc_down_thrs);
		fpsgo_sysfs_create_file(fbt_cam_kobj,
				&kobj_attr_fbt_cam_gcc_down_step);
		fpsgo_sysfs_create_file(fbt_cam_kobj,
				&kobj_attr_thread_status);
	}

	return 0;

err:
	return -1;
}

