// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/cpumask.h>
#include <linux/sched/task.h>
#include <linux/sched/cputime.h>

#include <mt-plat/fpsgo_common.h>

#include "fpsgo_base.h"
#include "fbt_cpu.h"
#include "xgf.h"
#include "sched/sched.h"

static unsigned long long last_vsync_ts;

static void uboost_get_runtime(pid_t tid, u64 *runtime)
{
	struct task_struct *p;

	if (unlikely(!tid))
		return;

	rcu_read_lock();
	p = find_task_by_vpid(tid);
	if (!p) {
		xgf_trace(" %5d not found to get runtime", tid);
		rcu_read_unlock();
		return;
	}
	get_task_struct(p);
	rcu_read_unlock();

	*runtime = (u64)fpsgo_task_sched_runtime(p);
	put_task_struct(p);
}

void fpsgo_base2uboost_compute(
	struct render_info *render, unsigned long long ts)
{
	struct uboost *boost;
	struct hrtimer *timer;
	unsigned long long t_sched_runtime = 0;
	unsigned long long t_vsyc_period = 0;
	unsigned long long timer_period = 0;
	int frame_idx = 0;
	int get_result = 0;

	fpsgo_lockprove(__func__);

	if (!render || !ts)
		return;

	get_result = uboost2xgf_get_info(render->pid,
		render->buffer_id, &timer_period, &frame_idx);
	render->ux = get_result;

	if (!get_result)
		return;

	boost = &(render->uboost_info);
	if (ts > last_vsync_ts)
		t_vsyc_period = ts - last_vsync_ts;
	last_vsync_ts = ts;
	uboost_get_runtime(render->tgid, &t_sched_runtime);
	boost->vsync_u_runtime = t_sched_runtime;
	boost->checkp_u_runtime = 0;

	/* set initial value for first UB_BEGIN_FRAME frames */
	if (frame_idx < UB_BEGIN_FRAME && t_vsyc_period)
		timer_period = t_vsyc_period >> 2;
	else if (timer_period > t_vsyc_period && t_vsyc_period)
		timer_period = t_vsyc_period * 2;

	/* set max time period */
	if (timer_period > TIME_50MS)
		timer_period = TIME_50MS;

	boost->timer_period = timer_period;

	timer = &(boost->timer);

	if (timer) {
		/* hrtimer_cancel(timer); */

		if (boost->uboosting == 0 && timer_period) {
			hrtimer_start(timer, ns_to_ktime(timer_period), HRTIMER_MODE_REL);
			boost->uboosting = 1;
		}
	}
	fpsgo_systrace_c_fbt(render->pid, render->buffer_id, timer_period, "t2ub");
}

static void do_uboost(struct work_struct *work)
{
	int cpu, is_running = 0;
	int tofree = 0;
	unsigned long flags;
	struct task_struct *p;
	struct uboost *boost;
	struct render_info *render;
	unsigned long long t_sched_runtime = 0;
	unsigned long long runtime_lowerbound = 0;
	unsigned long long threshold = 0;

	boost = container_of(work, struct uboost, work);
	if (!boost) {
		FPSGO_LOGE("ERROR %d\n", __LINE__);
		return;
	}
	render = container_of(boost, struct render_info, uboost_info);
	if (!render) {
		FPSGO_LOGE("ERROR %d\n", __LINE__);
		return;
	}

	fpsgo_render_tree_lock(__func__);
	fpsgo_thread_lock(&(render->thr_mlock));

	if (render->linger != 0)
		goto out;

	uboost_get_runtime(render->tgid, &t_sched_runtime);
	boost->checkp_u_runtime = t_sched_runtime;

	for_each_possible_cpu(cpu) {
		raw_spin_lock_irqsave(&cpu_rq(cpu)->lock, flags);
		list_for_each_entry(p, &cpu_rq(cpu)->cfs_tasks, se.group_node) {
			if (p->pid == render->tgid)
				is_running = 1;
		}
		raw_spin_unlock_irqrestore(&cpu_rq(cpu)->lock, flags);
	}

	runtime_lowerbound = boost->timer_period - (boost->timer_period >> 3);
	if ((boost->checkp_u_runtime - boost->vsync_u_runtime) > runtime_lowerbound)
		threshold = 2;

	fpsgo_systrace_c_fbt(render->pid, render->buffer_id, (is_running+threshold), "ubv");

	if (is_running || threshold)
		fpsgo_uboost2fbt_uboost(render);

out:
	boost->uboosting = 0;

	if (render->linger > 0 && fpsgo_base_is_finished(render)) {
		tofree = 1;
		fpsgo_del_linger(render);
	}

	fpsgo_thread_unlock(&(render->thr_mlock));

	if (tofree)
		kfree(render);

	fpsgo_render_tree_unlock(__func__);
}

static enum hrtimer_restart uboost_tfn(struct hrtimer *timer)
{
	struct uboost *boost;

	boost = container_of(timer, struct uboost, timer);
	schedule_work(&boost->work);
	return HRTIMER_NORESTART;
}

void fpsgo_base2uboost_cancel(struct render_info *obj)
{
	if (!obj)
		return;

	hrtimer_cancel(&(obj->uboost_info.timer));
}

void fpsgo_base2uboost_init(struct render_info *obj)
{
	struct uboost *boost;

	if (!obj)
		return;

	boost = &(obj->uboost_info);

	boost->vsync_u_runtime = 0;
	boost->checkp_u_runtime = 0;
	boost->timer_period = 0;
	boost->uboosting = 0;
	hrtimer_init(&boost->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	boost->timer.function = &uboost_tfn;
	INIT_WORK(&boost->work, do_uboost);
}

int __init fpsgo_uboost_init(void)
{
	return 0;
}

int __exit fpsgo_uboost_exit(void)
{
	return 0;
}

