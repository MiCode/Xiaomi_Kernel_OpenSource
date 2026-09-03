// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2022, The Linux Foundation. All rights reserved.
 * This file has been modified by Unisoc (shanghai) Technologies Co., Ltd in 2022
 */

#include <linux/syscore_ops.h>
#include <trace/hooks/sched.h>
#include "uni_sched.h"

enum task_event {
	PUT_PREV_TASK   = 0,
	PICK_NEXT_TASK  = 1,
	TASK_WAKE       = 2,
	TASK_MIGRATE    = 3,
	TASK_UPDATE     = 4,
	IRQ_UPDATE	= 5,
};

#define WINDOW_STATS_RECENT		0
#define WINDOW_STATS_MAX		1
#define WINDOW_STATS_MAX_RECENT_AVG	2
#define WINDOW_STATS_AVG		3
#define WINDOW_STATS_INVALID_POLICY	4

#define WALT_FREQ_ACCOUNT_WAIT_TIME	0

static __read_mostly unsigned int walt_ravg_hist_size = 6;
static __read_mostly unsigned int walt_window_stats_policy = WINDOW_STATS_MAX;
__read_mostly unsigned int sysctl_walt_io_is_busy;
__read_mostly unsigned int sysctl_sched_walt_cpu_high_irqload =
							(10 * NSEC_PER_MSEC);

unsigned int sysctl_sched_walt_init_task_load_pct = 10;

/*
 * Window size (in ns). Adjust for the tick size so that the window
 * rollover occurs just before the tick boundary.
 */
__read_mostly unsigned int walt_ravg_window =
					    (16000000 / TICK_NSEC) * TICK_NSEC;

__read_mostly unsigned int sysctl_walt_busy_threshold = 50;
__read_mostly unsigned int sysctl_sched_walt_cross_window_util = 1;

static unsigned int sync_cpu;
static ktime_t ktime_last;
static __read_mostly bool walt_ktime_suspended;

unsigned long walt_cpu_util_freq(int cpu)
{
	u64 walt_cpu_util;
	struct rq *rq = cpu_rq(cpu);
	struct uni_rq *uni_rq = (struct uni_rq *) rq->android_vendor_data1;

	walt_cpu_util = uni_rq->cumulative_runnable_avg;
	walt_cpu_util <<= SCHED_CAPACITY_SHIFT;
	do_div(walt_cpu_util, walt_ravg_window);

	if (uni_rq->is_busy == CPU_BUSY_SET || sysctl_walt_io_is_busy != 0) {
		u64 prev_runnable_sum = uni_rq->prev_runnable_sum;

		prev_runnable_sum <<= SCHED_CAPACITY_SHIFT;
		do_div(prev_runnable_sum, walt_ravg_window);
		walt_cpu_util = max(walt_cpu_util, prev_runnable_sum);
	}

	if (sysctl_walt_boost_irqload)
		walt_cpu_util += div64_u64(capacity_orig_of(cpu) * uni_rq->avg_irqload,
						(u64)walt_ravg_window);

	return min_t(unsigned long, walt_cpu_util, capacity_orig_of(cpu));
}

static inline unsigned int walt_task_load(struct uni_task_struct *uni_tsk)
{
	return uni_tsk->demand;
}

static inline void fixup_cum_window_demand(struct rq *rq, s64 delta)
{
	struct uni_rq *uni_rq = (struct uni_rq *)rq->android_vendor_data1;

	uni_rq->cum_window_demand += delta;
	if (unlikely((s64)uni_rq->cum_window_demand < 0))
		uni_rq->cum_window_demand = 0;
}

void walt_inc_cumulative_runnable_avg(struct rq *rq, struct task_struct *p)
{
	struct uni_rq *uni_rq = (struct uni_rq *)rq->android_vendor_data1;
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *)p->android_vendor_data1;

	uni_rq->cumulative_runnable_avg += uni_tsk->demand;

	/*
	 * Add a task's contribution to the cumulative window demand when
	 *
	 * (1) task is enqueued with on_rq = 1 i.e migration,
	 *     prio/cgroup/class change.
	 * (2) task is waking for the first time in this window.
	 */
	if (p->on_rq || (uni_tsk->last_sleep_ts < uni_rq->window_start))
		fixup_cum_window_demand(rq, uni_tsk->demand);
}

void walt_dec_cumulative_runnable_avg(struct rq *rq, struct task_struct *p)
{
	struct uni_rq *uni_rq = (struct uni_rq *)rq->android_vendor_data1;
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *)p->android_vendor_data1;

	uni_rq->cumulative_runnable_avg -= uni_tsk->demand;
	BUG_ON((s64)uni_rq->cumulative_runnable_avg < 0);

	/*
	 * on_rq will be 1 for sleeping tasks. So check if the task
	 * is migrating or dequeuing in RUNNING state to change the
	 * prio/cgroup/class.
	 */
	if (task_on_rq_migrating(p) || task_is_running(p))
		fixup_cum_window_demand(rq, -(s64)uni_tsk->demand);
}

static void
walt_fixup_cumulative_runnable_avg(struct rq *rq, struct uni_task_struct *uni_tsk,
				   u64 new_task_load)
{
	struct uni_rq *uni_rq = (struct uni_rq *)rq->android_vendor_data1;
	s64 task_load_delta = (s64)new_task_load - walt_task_load(uni_tsk);

	uni_rq->cumulative_runnable_avg += task_load_delta;
	if (unlikely((s64)uni_rq->cumulative_runnable_avg < 0))
		panic("cra less than zero: tld: %lld, task_load(p) = %u\n",
			task_load_delta, walt_task_load(uni_tsk));

	fixup_cum_window_demand(rq, task_load_delta);
}

u64 sched_ktime_clock(void)
{
	if (unlikely(walt_ktime_suspended))
		return ktime_to_ns(ktime_last);
	return ktime_get_ns();
}

static void walt_resume(void)
{
	walt_ktime_suspended = false;
}

static int walt_suspend(void)
{
	ktime_last = ktime_get();
	walt_ktime_suspended = true;
	return 0;
}

static struct syscore_ops walt_syscore_ops = {
	.resume	= walt_resume,
	.suspend = walt_suspend
};

static inline bool exiting_task(struct task_struct *p)
{
	return !!(p->flags & PF_EXITING);
}

static u64 update_window_start(struct rq *rq, u64 wallclock)
{
	s64 delta;
	int nr_windows;
	struct uni_rq *uni_rq = (struct uni_rq *)rq->android_vendor_data1;
	u64 old_window_start = uni_rq->window_start;

	delta = wallclock - uni_rq->window_start;
	/* If the MPM global timer is cleared, set delta as 0 to avoid kernel BUG happening */
	if (delta < 0) {
		delta = 0;
		WARN_ONCE(1, "WALT wallclock appears to have gone backwards or reset\n");
	}

	if (delta < walt_ravg_window)
		return old_window_start;

	nr_windows = div64_u64(delta, walt_ravg_window);
	uni_rq->window_start += (u64)nr_windows * (u64)walt_ravg_window;

	uni_rq->cum_window_demand = uni_rq->cumulative_runnable_avg;

	return old_window_start;
}
/*
 * Translate absolute delta time accounted on a CPU
 * to a scale where 1024 is the capacity of the most
 * capable CPU running at FMAX
 */
static u64 scale_exec_time(u64 delta, struct rq *rq)
{
	u64 cap_curr = cap_scale(arch_scale_cpu_capacity(cpu_of(rq)),
				arch_scale_freq_capacity(cpu_of(rq)));

	delta = cap_scale(delta, cap_curr);

	return delta;
}

static inline int cpu_is_waiting_on_io(struct rq *rq)
{
	if (!sysctl_walt_io_is_busy)
		return 0;

	return atomic_read(&rq->nr_iowait);
}


static int account_busy_for_cpu_time(struct rq *rq, struct task_struct *p,
				     u64 irqtime, int event)
{
	if (is_idle_task(p)) {
		/* TASK_WAKE && TASK_MIGRATE is not possible on idle task! */
		if (event == PICK_NEXT_TASK)
			return 0;

		/* PUT_PREV_TASK, TASK_UPDATE && IRQ_UPDATE are left */
		return irqtime || cpu_is_waiting_on_io(rq);
	}

	if (event == TASK_WAKE)
		return 0;

	if (event == PUT_PREV_TASK || event == IRQ_UPDATE ||
				      event == TASK_UPDATE)
		return 1;

	/* Only TASK_MIGRATE && PICK_NEXT_TASK left */
	return WALT_FREQ_ACCOUNT_WAIT_TIME;
}

/*
 * Account cpu activity in its busy time counters (rq->curr/prev_runnable_sum)
 */
static void update_cpu_busy_time(struct task_struct *p, struct rq *rq,
	     int event, u64 wallclock, u64 irqtime)
{
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *)p->android_vendor_data1;
	struct uni_rq *uni_rq = (struct uni_rq *)rq->android_vendor_data1;
	int new_window, nr_full_windows = 0;
	int p_is_curr_task = (p == rq->curr);
	u64 mark_start = uni_tsk->mark_start;
	u64 window_start = uni_rq->window_start;
	u32 window_size = walt_ravg_window;
	u64 delta;

	new_window = mark_start < window_start;
	if (new_window)
		nr_full_windows = div64_u64((window_start - mark_start),
						window_size);

	/*
	 * Handle per-task window rollover. We don't care about the idle
	 * task or exiting tasks.
	 */
	if (new_window && !is_idle_task(p) && !exiting_task(p)) {
		u32 curr_window = 0;

		if (!nr_full_windows)
			curr_window = uni_tsk->curr_window;

		uni_tsk->prev_window = curr_window;
		uni_tsk->curr_window = 0;
	}

	if (!account_busy_for_cpu_time(rq, p, irqtime, event)) {
		/*
		 * account_busy_for_cpu_time() = 0, so no update to the
		 * task's current window needs to be made. This could be
		 * for example
		 *
		 *   - a wakeup event on a task within the current
		 *     window (!new_window below, no action required),
		 *   - switching to a new task from idle (PICK_NEXT_TASK)
		 *     in a new window where irqtime is 0 and we aren't
		 *     waiting on IO
		 */
		if (!new_window)
			return;

		/*
		 * A new window has started. The RQ demand must be rolled
		 * over if p is the current task.
		 */
		if (p_is_curr_task) {
			u64 prev_sum = 0;

			/* p is either idle task or an exiting task */
			if (!nr_full_windows)
				prev_sum = uni_rq->curr_runnable_sum;

			uni_rq->prev_runnable_sum = prev_sum;
			uni_rq->curr_runnable_sum = 0;
		}

		return;
	}

	if (!new_window) {
		/*
		 * account_busy_for_cpu_time() = 1 so busy time needs
		 * to be accounted to the current window. No rollover
		 * since we didn't start a new window. An example of this is
		 * when a task starts execution and then sleeps within the
		 * same window.
		 */

		if (!irqtime || !is_idle_task(p) || cpu_is_waiting_on_io(rq))
			delta = wallclock - mark_start;
		else
			delta = irqtime;
		delta = scale_exec_time(delta, rq);
		uni_rq->curr_runnable_sum += delta;
		if (!is_idle_task(p) && !exiting_task(p))
			uni_tsk->curr_window += delta;

		return;
	}

	if (!p_is_curr_task) {
		/*
		 * account_busy_for_cpu_time() = 1 so busy time needs
		 * to be accounted to the current window. A new window
		 * has also started, but p is not the current task, so the
		 * window is not rolled over - just split up and account
		 * as necessary into curr and prev. The window is only
		 * rolled over when a new window is processed for the current
		 * task.
		 *
		 * Irqtime can't be accounted by a task that isn't the
		 * currently running task.
		 */

		if (!nr_full_windows) {
			/*
			 * A full window hasn't elapsed, account partial
			 * contribution to previous completed window.
			 */
			delta = scale_exec_time(window_start - mark_start, rq);
			if (!exiting_task(p))
				uni_tsk->prev_window += delta;
		} else {
			/*
			 * Since at least one full window has elapsed,
			 * the contribution to the previous window is the
			 * full window (window_size).
			 */
			delta = scale_exec_time(window_size, rq);
			if (!exiting_task(p))
				uni_tsk->prev_window = delta;
		}
		uni_rq->prev_runnable_sum += delta;

		/* Account piece of busy time in the current window. */
		delta = scale_exec_time(wallclock - window_start, rq);
		uni_rq->curr_runnable_sum += delta;
		if (!exiting_task(p))
			uni_tsk->curr_window = delta;

		return;
	}

	if (!irqtime || !is_idle_task(p) || cpu_is_waiting_on_io(rq)) {
		/*
		 *account_busy_for_cpu_time() = 1 so busy time needs
		 * to be accounted to the current window. A new window
		 * has started and p is the current task so rollover is
		 * needed. If any of these three above conditions are true
		 * then this busy time can't be accounted as irqtime.
		 *
		 * Busy time for the idle task or exiting tasks need not
		 * be accounted.
		 *
		 * An example of this would be a task that starts execution
		 * and then sleeps once a new window has begun.
		 */

		if (!nr_full_windows) {
			/*
			 * A full window hasn't elapsed, account partial
			 * contribution to previous completed window.
			 */
			delta = scale_exec_time(window_start - mark_start, rq);
			if (!is_idle_task(p) && !exiting_task(p))
				uni_tsk->prev_window += delta;

			delta += uni_rq->curr_runnable_sum;
		} else {
			/*
			 * Since at least one full window has elapsed,
			 * the contribution to the previous window is the
			 * full window (window_size).
			 */
			delta = scale_exec_time(window_size, rq);
			if (!is_idle_task(p) && !exiting_task(p))
				uni_tsk->prev_window = delta;

		}
		/*
		 * Rollover for normal runnable sum is done here by overwriting
		 * the values in prev_runnable_sum and curr_runnable_sum.
		 * Rollover for new task runnable sum has completed by previous
		 * if-else statement.
		 */
		uni_rq->prev_runnable_sum = delta;

		/* Account piece of busy time in the current window. */
		delta = scale_exec_time(wallclock - window_start, rq);
		uni_rq->curr_runnable_sum = delta;
		if (!is_idle_task(p) && !exiting_task(p))
			uni_tsk->curr_window = delta;

		return;
	}

	if (irqtime) {
		/*
		 * account_busy_for_cpu_time() = 1 so busy time needs
		 * to be accounted to the current window. A new window
		 * has started and p is the current task so rollover is
		 * needed. The current task must be the idle task because
		 * irqtime is not accounted for any other task.
		 *
		 * Irqtime will be accounted each time we process IRQ activity
		 * after a period of idleness, so we know the IRQ busy time
		 * started at wallclock - irqtime.
		 */

		BUG_ON(!is_idle_task(p));
		mark_start = wallclock - irqtime;

		/*
		 * Roll window over. If IRQ busy time was just in the current
		 * window then that is all that need be accounted.
		 */
		uni_rq->prev_runnable_sum = uni_rq->curr_runnable_sum;
		if (mark_start > window_start) {
			uni_rq->curr_runnable_sum = scale_exec_time(irqtime, rq);
			return;
		}

		/*
		 * The IRQ busy time spanned multiple windows. Process the
		 * busy time preceding the current window start first.
		 */
		delta = window_start - mark_start;
		if (delta > window_size)
			delta = window_size;
		delta = scale_exec_time(delta, rq);
		uni_rq->prev_runnable_sum += delta;

		/* Process the remaining IRQ busy time in the current window. */
		delta = wallclock - window_start;
		uni_rq->curr_runnable_sum = scale_exec_time(delta, rq);

		return;
	}

	BUG();
}

static int account_busy_for_task_demand(struct task_struct *p, int event)
{
	/*
	 * No need to bother updating task demand for exiting tasks
	 * or the idle task.
	 */
	if (exiting_task(p) || is_idle_task(p))
		return 0;

	/*
	 * When a task is waking up it is completing a segment of non-busy
	 * time. Likewise, if wait time is not treated as busy time, then
	 * when a task begins to run or is migrated, it is not running and
	 * is completing a segment of non-busy time.
	 */
	if (event == TASK_WAKE || (!tg_account_wait_time(p) &&
			(event == PICK_NEXT_TASK || event == TASK_MIGRATE)))
		return 0;

	return 1;
}

/*
 * Called when new window is starting for a task, to record cpu usage over
 * recently concluded window(s). Normally 'samples' should be 1. It can be > 1
 * when, say, a real-time task runs without preemption for several windows at a
 * stretch.
 */
static void update_history(struct rq *rq, struct task_struct *p,
			 u32 runtime, int samples, int event)
{
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *)p->android_vendor_data1;
	u32 *hist = &uni_tsk->sum_history[0];
	int ridx, widx;
	u32 max = 0, avg, demand;
	u64 sum = 0;

	/* Ignore windows where task had no activity */
	if (!runtime || is_idle_task(p) || exiting_task(p) || !samples)
		goto done;

	/* Push new 'runtime' value onto stack */
	widx = walt_ravg_hist_size - 1;
	ridx = widx - samples;
	for (; ridx >= 0; --widx, --ridx) {
		hist[widx] = hist[ridx];
		sum += hist[widx];
		if (hist[widx] > max)
			max = hist[widx];
	}

	for (widx = 0; widx < samples && widx < walt_ravg_hist_size; widx++) {
		hist[widx] = runtime;
		sum += hist[widx];
		if (hist[widx] > max)
			max = hist[widx];
	}

	uni_tsk->sum = 0;

	if (walt_window_stats_policy == WINDOW_STATS_RECENT) {
		demand = runtime;
	} else if (walt_window_stats_policy == WINDOW_STATS_MAX) {
		demand = max;
	} else {
		avg = div64_u64(sum, walt_ravg_hist_size);
		if (walt_window_stats_policy == WINDOW_STATS_AVG)
			demand = avg;
		else
			demand = max(avg, runtime);
	}

	/*
	 * A throttled deadline sched class task gets dequeued without
	 * changing p->on_rq. Since the dequeue decrements hmp stats
	 * avoid decrementing it here again.
	 *
	 * When window is rolled over, the cumulative window demand
	 * is reset to the cumulative runnable average (contribution from
	 * the tasks on the runqueue). If the current task is dequeued
	 * already, it's demand is not included in the cumulative runnable
	 * average. So add the task demand separately to cumulative window
	 * demand.
	 */
	if (!task_has_dl_policy(p) || !p->dl.dl_throttled) {
		if (task_on_rq_queued(p))
			walt_fixup_cumulative_runnable_avg(rq, uni_tsk, demand);
		else if (rq->curr == p)
			fixup_cum_window_demand(rq, demand);
	}

	uni_tsk->demand = demand;
	uni_tsk->demand_scale = scale_demand(demand);

done:
	trace_walt_update_history(rq, p, uni_tsk, runtime, samples, event);
}

static void add_to_task_demand(struct rq *rq, struct task_struct *p,
				u64 delta)
{
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *)p->android_vendor_data1;

	delta = scale_exec_time(delta, rq);
	uni_tsk->sum += delta;
	if (unlikely(uni_tsk->sum > walt_ravg_window))
		uni_tsk->sum = walt_ravg_window;

	if (sysctl_sched_walt_cross_window_util) {
		uni_tsk->sum_latest += delta;
		if (unlikely(uni_tsk->sum_latest > walt_ravg_window))
			uni_tsk->sum_latest = walt_ravg_window;
	}
}

/*
 * Account cpu demand of task and/or update task's cpu demand history
 *
 * ms = p->ravg.mark_start;
 * wc = wallclock
 * ws = rq->window_start
 *
 * Three possibilities:
 *
 *	a) Task event is contained within one window.
 *		window_start < mark_start < wallclock
 *
 *		ws   ms  wc
 *		|    |   |
 *		V    V   V
 *		|---------------|
 *
 *	In this case, p->ravg.sum is updated *iff* event is appropriate
 *	(ex: event == PUT_PREV_TASK)
 *
 *	b) Task event spans two windows.
 *		mark_start < window_start < wallclock
 *
 *		ms   ws   wc
 *		|    |    |
 *		V    V    V
 *		-----|-------------------
 *
 *	In this case, p->ravg.sum is updated with (ws - ms) *iff* event
 *	is appropriate, then a new window sample is recorded followed
 *	by p->ravg.sum being set to (wc - ws) *iff* event is appropriate.
 *
 *	c) Task event spans more than two windows.
 *
 *		ms ws_tmp			   ws  wc
 *		|  |				   |   |
 *		V  V				   V   V
 *		---|-------|-------|-------|-------|------
 *		   |				   |
 *		   |<------ nr_full_windows ------>|
 *
 *	In this case, p->ravg.sum is updated with (ws_tmp - ms) first *iff*
 *	event is appropriate, window sample of p->ravg.sum is recorded,
 *	'nr_full_window' samples of window_size is also recorded *iff*
 *	event is appropriate and finally p->ravg.sum is set to (wc - ws)
 *	*iff* event is appropriate.
 *
 * IMPORTANT : Leave p->ravg.mark_start unchanged, as update_cpu_busy_time()
 * depends on it!
 */
static void update_task_demand(struct task_struct *p, struct rq *rq,
	     int event, u64 wallclock)
{
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *)p->android_vendor_data1;
	struct uni_rq *uni_rq = (struct uni_rq *)rq->android_vendor_data1;
	u64 mark_start = uni_tsk->mark_start;
	u64 delta, window_start = uni_rq->window_start;
	int new_window, nr_full_windows;
	u32 window_size = walt_ravg_window;
	u32 window_scale = scale_exec_time(window_size, rq);

	new_window = mark_start < window_start;

	if (!account_busy_for_task_demand(p, event)) {
		if (new_window) {
			/*
			 * If the time accounted isn't being accounted as
			 * busy time, and a new window started, only the
			 * previous window need be closed out with the
			 * pre-existing demand. Multiple windows may have
			 * elapsed, but since empty windows are dropped,
			 * it is not necessary to account those.
			 */
			update_history(rq, p, uni_tsk->sum, 1, event);
		}
		if (sysctl_sched_walt_cross_window_util)
			uni_tsk->sum_latest = 0;
		return;
	}

	if (!new_window) {
		/*
		 * The simple case - busy time contained within the existing
		 * window.
		 */
		add_to_task_demand(rq, p, wallclock - mark_start);

		goto done;
	}

	/*
	 * Busy time spans at least two windows. Temporarily rewind
	 * window_start to first window boundary after mark_start.
	 */
	delta = window_start - mark_start;
	nr_full_windows = div64_u64(delta, window_size);
	window_start -= (u64)nr_full_windows * (u64)window_size;

	/* Process (window_start - mark_start) first */
	add_to_task_demand(rq, p, window_start - mark_start);

	/* Push new sample(s) into task's demand history */
	update_history(rq, p, uni_tsk->sum, 1, event);
	if (sysctl_sched_walt_cross_window_util)
		uni_tsk->sum = uni_tsk->sum_latest;
	if (nr_full_windows) {
		update_history(rq, p, window_scale,
			       nr_full_windows, event);
		if (sysctl_sched_walt_cross_window_util) {
			uni_tsk->sum = window_scale;
			uni_tsk->sum_latest = window_scale;
		}
	}
	/*
	 * Roll window_start back to current to process any remainder
	 * in current window.
	 */
	window_start += (u64)nr_full_windows * (u64)window_size;

	/* Process (wallclock - window_start) next */
	mark_start = window_start;
	add_to_task_demand(rq, p, wallclock - mark_start);

done:
	/*
	 * Update task demand in current window when policy is
	 * WINDOW_STATS_MAX. The purpose is to create opportunity
	 * for rising cpu freq when cr_avg is used for cpufreq
	 */
	if (uni_tsk->sum > uni_tsk->demand && walt_window_stats_policy == WINDOW_STATS_MAX) {
		if (!task_has_dl_policy(p) || !p->dl.dl_throttled) {
			if (task_on_rq_queued(p))
				walt_fixup_cumulative_runnable_avg(rq, uni_tsk, uni_tsk->sum);
			else if (rq->curr == p)
				fixup_cum_window_demand(rq, uni_tsk->sum);
		}
		uni_tsk->demand = uni_tsk->sum;
		uni_tsk->demand_scale = scale_demand(uni_tsk->sum);
	}
}

/* Reflect task activity on its demand and cpu's busy time statistics */
static void walt_update_task_ravg(struct task_struct *p, struct rq *rq,
	     int event, u64 wallclock, u64 irqtime)
{
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *)p->android_vendor_data1;
	struct uni_rq *uni_rq = (struct uni_rq *)rq->android_vendor_data1;
	u64 old_window_start;

	if (unlikely(!uni_rq->window_start))
		return;

	lockdep_assert_rq_held(rq);

	old_window_start = update_window_start(rq, wallclock);

	if (!uni_tsk->mark_start)
		goto done;

	update_task_demand(p, rq, event, wallclock);
	update_cpu_busy_time(p, rq, event, wallclock, irqtime);

done:
	if (uni_rq->window_start > old_window_start) {
		unsigned long cap_orig = capacity_orig_of(cpu_of(rq));
		u64 busy_limit = (walt_ravg_window * sysctl_walt_busy_threshold) / 100;

		busy_limit = (busy_limit * cap_orig) >> SCHED_CAPACITY_SHIFT;
		if (uni_rq->prev_runnable_sum >= busy_limit) {
			if (uni_rq->is_busy == CPU_BUSY_CLR)
				uni_rq->is_busy = CPU_BUSY_PREPARE;
			else if (uni_rq->is_busy == CPU_BUSY_PREPARE)
				uni_rq->is_busy = CPU_BUSY_SET;
		} else if (uni_rq->is_busy != CPU_BUSY_CLR) {
			uni_rq->is_busy = CPU_BUSY_CLR;
		}
	}

	trace_walt_update_task_ravg(p, rq, uni_tsk, uni_rq, event, wallclock, irqtime);

	uni_tsk->mark_start = wallclock;
}

/*
 * static void reset_task_stats(struct task_struct *p)
 * {
 *         struct uni_task_struct *uni_tsk = (struct uni_task_struct *)p->android_vendor_data1;
 *
 *         memset(uni_tsk, 0, sizeof(struct uni_task_struct));
 * }
 */

static void walt_mark_task_starting(struct task_struct *p)
{
	u64 wallclock;
	struct rq *rq = task_rq(p);
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *)p->android_vendor_data1;
	struct uni_rq *uni_rq = (struct uni_rq *)rq->android_vendor_data1;

	if (unlikely(!uni_rq->window_start)) {
//		reset_task_stats(p);
		return;
	}

	wallclock = sched_ktime_clock();
	uni_tsk->mark_start = wallclock;
	uni_tsk->last_wakeup_ts = wallclock;
}

static void walt_set_window_start(struct rq *rq)
{
	struct uni_rq *uni_rq = (struct uni_rq *)rq->android_vendor_data1;
	struct uni_task_struct *curr_uni_tsk = (struct uni_task_struct *)rq->curr->android_vendor_data1;

	if (likely(uni_rq->window_start))
		return;

	if (cpu_of(rq) == sync_cpu) {
		uni_rq->window_start = 1;
	} else {
		struct rq *sync_rq = cpu_rq(sync_cpu);
		struct uni_rq *sync_uni_rq = (struct uni_rq *)sync_rq->android_vendor_data1;

		raw_spin_rq_unlock(rq);
		double_rq_lock(rq, sync_rq);
		uni_rq->window_start = sync_uni_rq->window_start;
		uni_rq->curr_runnable_sum = uni_rq->prev_runnable_sum = 0;
		raw_spin_rq_unlock(sync_rq);
	}

	curr_uni_tsk->mark_start = uni_rq->window_start;
}

static void walt_migrate_sync_cpu(int cpu)
{
	if (cpu == sync_cpu)
		sync_cpu = smp_processor_id();
}

static void walt_account_irqtime(int cpu, struct task_struct *curr, u64 delta)
{
	struct rq *rq = cpu_rq(cpu);
	struct uni_rq *uni_rq = (struct uni_rq *)rq->android_vendor_data1;
	unsigned long flags;
	u64 cur_jiffies_ts, nr_windows;

	raw_spin_rq_lock_irqsave(rq, flags);

	cur_jiffies_ts = get_jiffies_64();

	if (is_idle_task(curr))
		walt_update_task_ravg(curr, rq, IRQ_UPDATE, sched_ktime_clock(),
				 delta);

	nr_windows = cur_jiffies_ts - uni_rq->irqload_ts;

	if (nr_windows) {
		if (nr_windows < 10) {
			/* Decay CPU's irqload by 3/4 for each window. */
			uni_rq->avg_irqload *= (3 * nr_windows);
			uni_rq->avg_irqload = div64_u64(uni_rq->avg_irqload,
						    4 * nr_windows);
		} else {
			uni_rq->avg_irqload = 0;
		}
		uni_rq->avg_irqload += uni_rq->cur_irqload;
		uni_rq->cur_irqload = 0;
	}

	uni_rq->cur_irqload += delta;
	uni_rq->irqload_ts = cur_jiffies_ts;
	raw_spin_rq_unlock_irqrestore(rq, flags);
}

static void walt_fixup_busy_time(struct task_struct *p, int new_cpu)
{
	struct rq *src_rq = task_rq(p);
	struct rq *dest_rq = cpu_rq(new_cpu);
	struct uni_rq *src_uni_rq = (struct uni_rq *)src_rq->android_vendor_data1;
	struct uni_rq *dst_uni_rq = (struct uni_rq *)dest_rq->android_vendor_data1;
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *)p->android_vendor_data1;
	u64 wallclock;
	unsigned int p_state = READ_ONCE(p->__state);

	if (!p->on_rq && p_state != TASK_WAKING)
		return;

	if (exiting_task(p))
		return;

	if (p_state == TASK_WAKING)
		double_rq_lock(src_rq, dest_rq);

	lockdep_assert_rq_held(src_rq);
	lockdep_assert_rq_held(dest_rq);

	wallclock = sched_ktime_clock();

	walt_update_task_ravg(task_rq(p)->curr, task_rq(p),
			TASK_UPDATE, wallclock, 0);
	walt_update_task_ravg(dest_rq->curr, dest_rq,
			TASK_UPDATE, wallclock, 0);

	walt_update_task_ravg(p, task_rq(p), TASK_MIGRATE, wallclock, 0);

	/*
	 * When a task is migrating during the wakeup, adjust
	 * the task's contribution towards cumulative window
	 * demand.
	 */
	if (p_state == TASK_WAKING &&
	    uni_tsk->last_sleep_ts >= src_uni_rq->window_start) {
		fixup_cum_window_demand(src_rq, -(s64)uni_tsk->demand);
		fixup_cum_window_demand(dest_rq, uni_tsk->demand);
	}

	if (uni_tsk->curr_window) {
		src_uni_rq->curr_runnable_sum -= uni_tsk->curr_window;
		dst_uni_rq->curr_runnable_sum += uni_tsk->curr_window;
	}

	if (uni_tsk->prev_window) {
		src_uni_rq->prev_runnable_sum -= uni_tsk->prev_window;
		dst_uni_rq->prev_runnable_sum += uni_tsk->prev_window;
	}

	if ((s64)src_uni_rq->prev_runnable_sum < 0) {
		src_uni_rq->prev_runnable_sum = 0;
		WARN_ON(1);
	}
	if ((s64)src_uni_rq->curr_runnable_sum < 0) {
		src_uni_rq->curr_runnable_sum = 0;
		WARN_ON(1);
	}

	trace_walt_migration_update_sum(src_rq, src_uni_rq, p);
	trace_walt_migration_update_sum(dest_rq, dst_uni_rq, p);

	if (p_state == TASK_WAKING)
		double_rq_unlock(src_rq, dest_rq);
}

void walt_init_new_task_load(struct task_struct *p)
{
	int i;
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *)p->android_vendor_data1;
	struct uni_task_struct *cur_uni_tsk =
			(struct uni_task_struct *)current->android_vendor_data1;
	u32 init_load_windows =
			div64_u64((u64)sysctl_sched_walt_init_task_load_pct *
					(u64)walt_ravg_window, 100);
	u32 init_load_pct = cur_uni_tsk->init_load_pct;
	u32 tg_load_pct = tg_init_load_pct(p);

	uni_tsk->init_load_pct = 0;
	uni_tsk->mark_start = 0;
	uni_tsk->sum = 0;
	uni_tsk->sum_latest = 0;
	uni_tsk->curr_window = 0;
	uni_tsk->prev_window = 0;

	init_load_pct = init_load_pct > tg_load_pct ? init_load_pct : tg_load_pct;

	if (init_load_pct) {
		init_load_windows = div64_u64((u64)init_load_pct *
			  (u64)walt_ravg_window, 100);
	}

	if (unlikely(is_idle_task(p)))
		init_load_windows = 0;

	uni_tsk->demand = init_load_windows;
	uni_tsk->demand_scale = scale_demand(init_load_windows);
	for (i = 0; i < RAVG_HIST_SIZE_MAX; ++i)
		uni_tsk->sum_history[i] = init_load_windows;
}

static void note_task_waking(struct task_struct *p, u64 wallclock)
{
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *)p->android_vendor_data1;

	uni_tsk->last_wakeup_ts = wallclock;
}

static void android_rvh_build_perf_domains(void *data, bool *eas_check)
{
	if (unlikely(uni_sched_disabled))
		return;

	*eas_check = true;
}

static void android_rvh_sched_cpu_starting(void *data, int cpu)
{
	unsigned long flags;
	struct rq *rq = cpu_rq(cpu);

	if (unlikely(uni_sched_disabled))
		return;

	raw_spin_rq_lock_irqsave(rq, flags);
	walt_set_window_start(rq);
	raw_spin_rq_unlock_irqrestore(rq, flags);
}

static void android_rvh_sched_cpu_dying(void *data, int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	struct rq_flags rf;

	if (unlikely(uni_sched_disabled))
		return;

	rq_lock_irqsave(rq, &rf);
	walt_migrate_sync_cpu(cpu);
	rq_unlock_irqrestore(rq, &rf);
}

static void android_rvh_new_task_stats(void *data, struct task_struct *p)
{
	if (unlikely(uni_sched_disabled))
		return;

	walt_mark_task_starting(p);
}

static void android_rvh_set_task_cpu(void *data, struct task_struct *p, unsigned int new_cpu)
{
	if (unlikely(uni_sched_disabled))
		return;

	walt_fixup_busy_time(p, new_cpu);

}

static void android_rvh_try_to_wake_up(void *data, struct task_struct *p)
{
	struct rq *rq = cpu_rq(task_cpu(p));
	struct rq_flags rf;
	u64 wallclock;

	if (unlikely(uni_sched_disabled))
		return;

	rq_lock_irqsave(rq, &rf);
	wallclock = sched_ktime_clock();
	walt_update_task_ravg(rq->curr, rq, TASK_UPDATE, wallclock, 0);
	walt_update_task_ravg(p, rq, TASK_WAKE, wallclock, 0);
	note_task_waking(p, wallclock);
	rq_unlock_irqrestore(rq, &rf);
}

static void android_rvh_tick_entry(void *data, struct rq *rq)
{
	lockdep_assert_rq_held(rq);

	if (unlikely(uni_sched_disabled))
		return;

	walt_set_window_start(rq);
	walt_update_task_ravg(rq->curr, rq, TASK_UPDATE, sched_ktime_clock(), 0);

	walt_cpufreq_update_util(rq, 0);
}

static void android_rvh_account_irq_end(void *data, struct task_struct *curr, int cpu, s64 delta)
{
	if (unlikely(uni_sched_disabled) ||
				unlikely(!sysctl_walt_account_irq_time))
		return;

	walt_account_irqtime(cpu, curr, delta);
}

static void android_rvh_schedule(void *data, struct task_struct *prev,
				 struct task_struct *next, struct rq *rq)
{
	struct uni_task_struct *prev_uni_tsk = (struct uni_task_struct *)prev->android_vendor_data1;
	u64 wallclock = sched_ktime_clock();

	if (unlikely(uni_sched_disabled))
		return;

	if (likely(prev != next)) {
		if (!prev->on_rq)
			prev_uni_tsk->last_sleep_ts = wallclock;

		walt_update_task_ravg(prev, rq, PUT_PREV_TASK, wallclock, 0);
		walt_update_task_ravg(next, rq, PICK_NEXT_TASK, wallclock, 0);
	} else {
		walt_update_task_ravg(prev, rq, TASK_UPDATE, wallclock, 0);
	}
}

static void walt_effective_cpu_util(void *data, int cpu, unsigned long util_cfs,
				    unsigned long max, int type,
				    struct task_struct *p, unsigned long *new_util)
{
	u64 walt_cpu_util;
	struct rq *rq = cpu_rq(cpu);
	struct uni_rq *uni_rq = (struct uni_rq *) rq->android_vendor_data1;
	u64 prev_runnable_sum;

	if (unlikely(uni_sched_disabled))
		return;

	walt_cpu_util = uni_rq->cumulative_runnable_avg;
	walt_cpu_util <<= SCHED_CAPACITY_SHIFT;
	do_div(walt_cpu_util, walt_ravg_window);

	prev_runnable_sum = uni_rq->prev_runnable_sum;
	prev_runnable_sum <<= SCHED_CAPACITY_SHIFT;
	do_div(prev_runnable_sum, walt_ravg_window);

	walt_cpu_util = max(walt_cpu_util, prev_runnable_sum);

	if (type == FREQUENCY_UTIL) {
		walt_cpu_util = boosted_cpu_util(cpu, walt_cpu_util);
		walt_cpu_util = walt_uclamp_rq_util_with(rq, walt_cpu_util, p);
	}

	*new_util = min_t(unsigned long, walt_cpu_util, capacity_orig_of(cpu));
}

static void register_walt_vendor_hooks(void)
{
	register_trace_android_rvh_build_perf_domains(android_rvh_build_perf_domains, NULL);
	register_trace_android_rvh_sched_cpu_starting(android_rvh_sched_cpu_starting, NULL);
	register_trace_android_rvh_sched_cpu_dying(android_rvh_sched_cpu_dying, NULL);
	register_trace_android_rvh_new_task_stats(android_rvh_new_task_stats, NULL);
	register_trace_android_rvh_set_task_cpu(android_rvh_set_task_cpu, NULL);
	register_trace_android_rvh_try_to_wake_up(android_rvh_try_to_wake_up, NULL);
	register_trace_android_rvh_tick_entry(android_rvh_tick_entry, NULL);
	register_trace_android_rvh_account_irq_end(android_rvh_account_irq_end, NULL);
	register_trace_android_rvh_schedule(android_rvh_schedule, NULL);
	register_trace_android_rvh_effective_cpu_util(walt_effective_cpu_util, NULL);
}

void walt_init(void)
{
	register_syscore_ops(&walt_syscore_ops);
	register_walt_vendor_hooks();
}
