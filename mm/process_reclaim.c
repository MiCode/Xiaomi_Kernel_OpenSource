/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/sort.h>
#include <linux/oom.h>
#include <linux/sched.h>
#include <linux/rcupdate.h>
#include <linux/notifier.h>
#include <linux/vmpressure.h>

#define CREATE_TRACE_POINTS
#include <trace/events/process_reclaim.h>

#define MAX_SWAP_TASKS SWAP_CLUSTER_MAX

static void swap_fn(struct work_struct *work);
DECLARE_WORK(swap_work, swap_fn);

/* User knob to enable/disable process reclaim feature */
static int enable_process_reclaim;
module_param_named(enable_process_reclaim, enable_process_reclaim, int,
	S_IRUGO | S_IWUSR);

/* The max number of pages tried to be reclaimed in a single run */
int per_swap_size = SWAP_CLUSTER_MAX * 32;
module_param_named(per_swap_size, per_swap_size, int, S_IRUGO | S_IWUSR);

int reclaim_avg_efficiency;
module_param_named(reclaim_avg_efficiency, reclaim_avg_efficiency,
			int, S_IRUGO);

/* The vmpressure region where process reclaim operates */
static unsigned long pressure_min = 50;
static unsigned long pressure_max = 90;
module_param_named(pressure_min, pressure_min, ulong, S_IRUGO | S_IWUSR);
module_param_named(pressure_max, pressure_max, ulong, S_IRUGO | S_IWUSR);

/*
 * Scheduling process reclaim workqueue unecessarily
 * when the reclaim efficiency is low does not make
 * sense. We try to detect a drop in efficiency and
 * disable reclaim for a time period. This period and the
 * period for which we monitor a drop in efficiency is
 * defined by swap_eff_win. swap_opt_eff is the optimal
 * efficincy used as theshold for this.
 */
static int swap_eff_win = 2;
module_param_named(swap_eff_win, swap_eff_win, int, S_IRUGO | S_IWUSR);

static int swap_opt_eff = 50;
module_param_named(swap_opt_eff, swap_opt_eff, int, S_IRUGO | S_IWUSR);

static atomic_t skip_reclaim = ATOMIC_INIT(0);
/* Not atomic since only a single instance of swap_fn run at a time */
static int monitor_eff;

struct selected_task {
	struct task_struct *p;
	int tasksize;
	short oom_score_adj;
};

int selected_cmp(const void *a, const void *b)
{
	const struct selected_task *x = a;
	const struct selected_task *y = b;
	int ret;

	ret = x->tasksize < y->tasksize ? -1 : 1;

	return ret;
}

static int test_task_flag(struct task_struct *p, int flag)
{
	struct task_struct *t = p;

	rcu_read_lock();
	for_each_thread(p, t) {
		task_lock(t);
		if (test_tsk_thread_flag(t, flag)) {
			task_unlock(t);
			rcu_read_unlock();
			return 1;
		}
		task_unlock(t);
	}
	rcu_read_unlock();

	return 0;
}

static void swap_fn(struct work_struct *work)
{
	struct task_struct *tsk;
	struct reclaim_param rp;

	/* Pick the best MAX_SWAP_TASKS tasks in terms of anon size */
	struct selected_task selected[MAX_SWAP_TASKS] = {{0, 0, 0},};
	int si = 0;
	int i;
	int tasksize;
	int total_sz = 0;
	short min_score_adj = 360;
	int total_scan = 0;
	int total_reclaimed = 0;
	int nr_to_reclaim;
	int efficiency;

	rcu_read_lock();
	for_each_process(tsk) {
		struct task_struct *p;
		short oom_score_adj;

		if (tsk->flags & PF_KTHREAD)
			continue;

		if (test_task_flag(tsk, TIF_MEMDIE))
			continue;

		p = find_lock_task_mm(tsk);
		if (!p)
			continue;

		oom_score_adj = p->signal->oom_score_adj;
		if (oom_score_adj < min_score_adj) {
			task_unlock(p);
			continue;
		}

		tasksize = get_mm_counter(p->mm, MM_ANONPAGES);
		task_unlock(p);

		if (tasksize <= 0)
			continue;

		if (si == MAX_SWAP_TASKS) {
			sort(&selected[0], MAX_SWAP_TASKS,
					sizeof(struct selected_task),
					&selected_cmp, NULL);
			if (tasksize < selected[0].tasksize)
				continue;
			selected[0].p = p;
			selected[0].oom_score_adj = oom_score_adj;
			selected[0].tasksize = tasksize;
		} else {
			selected[si].p = p;
			selected[si].oom_score_adj = oom_score_adj;
			selected[si].tasksize = tasksize;
			si++;
		}
	}

	for (i = 0; i < si; i++)
		total_sz += selected[i].tasksize;

	/* Skip reclaim if total size is too less */
	if (total_sz < SWAP_CLUSTER_MAX) {
		rcu_read_unlock();
		return;
	}

	for (i = 0; i < si; i++)
		get_task_struct(selected[i].p);

	rcu_read_unlock();

	while (si--) {
		nr_to_reclaim =
			(selected[si].tasksize * per_swap_size) / total_sz;
		/* scan atleast a page */
		if (!nr_to_reclaim)
			nr_to_reclaim = 1;

		rp = reclaim_task_anon(selected[si].p, nr_to_reclaim);

		trace_process_reclaim(selected[si].tasksize,
				selected[si].oom_score_adj, rp.nr_scanned,
				rp.nr_reclaimed, per_swap_size, total_sz,
				nr_to_reclaim);
		total_scan += rp.nr_scanned;
		total_reclaimed += rp.nr_reclaimed;
		put_task_struct(selected[si].p);
	}

	if (total_scan) {
		efficiency = (total_reclaimed * 100) / total_scan;

		if (efficiency < swap_opt_eff) {
			if (++monitor_eff == swap_eff_win) {
				atomic_set(&skip_reclaim, swap_eff_win);
				monitor_eff = 0;
			}
		} else {
			monitor_eff = 0;
		}

		reclaim_avg_efficiency =
			(efficiency + reclaim_avg_efficiency) / 2;
		trace_process_reclaim_eff(efficiency, reclaim_avg_efficiency);
	}
}

static int vmpressure_notifier(struct notifier_block *nb,
			unsigned long action, void *data)
{
	unsigned long pressure = action;

	if (!enable_process_reclaim)
		return 0;

	if (!current_is_kswapd())
		return 0;

	if (0 <= atomic_dec_if_positive(&skip_reclaim))
		return 0;

	if ((pressure >= pressure_min) && (pressure < pressure_max))
		if (!work_pending(&swap_work))
			schedule_work(&swap_work);
	return 0;
}

static struct notifier_block vmpr_nb = {
	.notifier_call = vmpressure_notifier,
};

static int __init process_reclaim_init(void)
{
	vmpressure_notifier_register(&vmpr_nb);
	return 0;
}

static void __exit process_reclaim_exit(void)
{
	vmpressure_notifier_unregister(&vmpr_nb);
}

module_init(process_reclaim_init);
module_exit(process_reclaim_exit);
