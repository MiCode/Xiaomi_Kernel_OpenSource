// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/timekeeping.h>
#include <linux/energy_model.h>
#include <trace/hooks/topology.h>
#include <trace/hooks/sched.h>
#include <sched/sched.h>
#include "sched_sys_common.h"
#include "eas_plus.h"
#include "eas_trace.h"

DEFINE_PER_CPU(struct task_rotate_work, task_rotate_works);
bool big_task_rotation_enable = true;
int sched_min_cap_orig_cpu = -1;
#define TASK_ROTATION_THRESHOLD_NS 6000000
#define HEAVY_TASK_NUM 4

unsigned int capacity_margin = 1280;

struct task_rotate_work {
	struct work_struct w;
	struct task_struct *src_task;
	struct task_struct *dst_task;
	int src_cpu;
	int dst_cpu;
};

int is_reserved(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	int reserved = 0;
	struct rq_flags rf;

	rq_lock(rq, &rf);
	reserved = rq->active_balance;
	rq_unlock(rq, &rf);

	return reserved;
}

bool is_min_capacity_cpu(int cpu)
{
	if (capacity_orig_of(cpu) == capacity_orig_of(sched_min_cap_orig_cpu))
		return true;

	return false;
}

bool is_max_capacity_cpu(int cpu)
{
	return capacity_orig_of(cpu) == SCHED_CAPACITY_SCALE;
}

static void task_rotate_work_func(struct work_struct *work)
{
	struct task_rotate_work *wr = container_of(work,
				struct task_rotate_work, w);

	int ret = -1;
	struct rq *src_rq, *dst_rq;

	ret = migrate_swap(wr->src_task, wr->dst_task,
			task_cpu(wr->dst_task), task_cpu(wr->src_task));

	if (ret == 0) {
		trace_sched_big_task_rotation(wr->src_cpu, wr->dst_cpu,
						wr->src_task->pid,
						wr->dst_task->pid,
						true);
	}

	put_task_struct(wr->src_task);
	put_task_struct(wr->dst_task);

	src_rq = cpu_rq(wr->src_cpu);
	dst_rq = cpu_rq(wr->dst_cpu);

	local_irq_disable();
	double_rq_lock(src_rq, dst_rq);
	src_rq->active_balance = 0;
	dst_rq->active_balance = 0;
	double_rq_unlock(src_rq, dst_rq);
	local_irq_enable();
}

void task_rotate_work_init(void)
{
	int i;

	for_each_possible_cpu(i) {
		struct task_rotate_work *wr = &per_cpu(task_rotate_works, i);

		INIT_WORK(&wr->w, task_rotate_work_func);
	}
}

void task_rotate_init(void)
{
	int i, min_cap_orig_cpu = -1;
	unsigned long min_orig_cap = ULONG_MAX;

	/* find min_cap cpu */
	for_each_possible_cpu(i) {
		if (capacity_orig_of(i) >= min_orig_cap)
			continue;
		min_orig_cap = capacity_orig_of(i);
		min_cap_orig_cpu = i;
	}

	if (min_cap_orig_cpu >= 0) {
		sched_min_cap_orig_cpu = min_cap_orig_cpu;
		pr_info("scheduler: min_cap_orig_cpu = %d\n",
			sched_min_cap_orig_cpu);
	} else
		pr_info("scheduler: can not find min_cap_orig_cpu\n");

	/* init rotate work */
	task_rotate_work_init();
}

void task_check_for_rotation(struct rq *src_rq)
{
	u64 wc, wait, max_wait = 0, run, max_run = 0;
	int deserved_cpu = nr_cpu_ids, dst_cpu = nr_cpu_ids;
	int i, src_cpu = cpu_of(src_rq);
	struct rq *dst_rq;
	struct task_rotate_work *wr = NULL;
	int heavy_task = 0;
	int force = 0;

	if (!big_task_rotation_enable)
		return;

	if (is_max_capacity_cpu(src_cpu))
		return;

	for_each_possible_cpu(i) {
		struct rq *rq = cpu_rq(i);
		struct task_struct *curr_task = rq->curr;

		if (curr_task &&
			!task_fits_capacity(curr_task, cpu_rq(i)->cpu_capacity))
			heavy_task += 1;

		if (heavy_task >= HEAVY_TASK_NUM)
			break;
	}

	if (heavy_task < HEAVY_TASK_NUM)
		return;

	wc = ktime_get_raw_ns();
	for_each_possible_cpu(i) {
		struct rq *rq = cpu_rq(i);

		if (!is_min_capacity_cpu(i))
			continue;

		if (!rq->misfit_task_load ||
			(rq->curr->policy != SCHED_NORMAL))
			continue;

		if (is_reserved(i))
			continue;

		wait = wc - rq->curr->android_vendor_data1[3];

		if (wait > max_wait) {
			max_wait = wait;
			deserved_cpu = i;
		}
	}

	if (deserved_cpu != src_cpu)
		return;

	for_each_possible_cpu(i) {
		struct rq *rq = cpu_rq(i);

		if (capacity_orig_of(i) <= capacity_orig_of(src_cpu))
			continue;

		if (rq->curr->policy != SCHED_NORMAL)
			continue;

		if (rq->nr_running > 1)
			continue;

		if (is_reserved(i))
			continue;

		run = wc - rq->curr->android_vendor_data1[3];

		if (run < TASK_ROTATION_THRESHOLD_NS)
			continue;

		if (run > max_run) {
			max_run = run;
			dst_cpu = i;
		}
	}

	if (dst_cpu == nr_cpu_ids)
		return;

	dst_rq = cpu_rq(dst_cpu);

	double_rq_lock(src_rq, dst_rq);
	if (dst_rq->curr->policy == SCHED_NORMAL) {
		if (!cpumask_test_cpu(dst_cpu,
					src_rq->curr->cpus_ptr) ||
			!cpumask_test_cpu(src_cpu,
					dst_rq->curr->cpus_ptr)) {
			double_rq_unlock(src_rq, dst_rq);
			return;
		}

		if (!src_rq->active_balance && !dst_rq->active_balance) {
			src_rq->active_balance = 1;
			dst_rq->active_balance = 1;

			get_task_struct(src_rq->curr);
			get_task_struct(dst_rq->curr);

			wr = &per_cpu(task_rotate_works, src_cpu);

			wr->src_task = src_rq->curr;
			wr->dst_task = dst_rq->curr;

			wr->src_cpu = src_rq->cpu;
			wr->dst_cpu = dst_rq->cpu;
			force = 1;
		}
	}
	double_rq_unlock(src_rq, dst_rq);

	if (force) {
		queue_work_on(src_cpu, system_highpri_wq, &wr->w);
		trace_sched_big_task_rotation(wr->src_cpu, wr->dst_cpu,
					wr->src_task->pid, wr->dst_task->pid,
					false);
	}
}

void set_big_task_rotation(bool enable)
{
	big_task_rotation_enable = enable;
}
EXPORT_SYMBOL_GPL(set_big_task_rotation);

void rotat_after_enqueue_task(void __always_unused *data, struct rq *rq,
				struct task_struct *p)
{
	p->android_vendor_data1[3] = ktime_get_raw_ns();
}

void rotat_task_stats(void __always_unused *data,
				struct task_struct *p)
{
	p->android_vendor_data1[3] = ktime_get_raw_ns();
}

void rotat_task_newtask(void __always_unused *data,
				struct task_struct *p, unsigned long clone_flags)
{
	p->android_vendor_data1[3] = 0;
}
