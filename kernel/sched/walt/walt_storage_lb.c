// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "walt.h"
#include "trace.h"

static bool lb_ignore_cpus(int cpu, cpumask_t *dst_cpu_mask_to_avoid)
{
	if (!cpu_active(cpu))
		return true;

	if (cpu_halted(cpu))
		return true;

	/* ignore high irq cpus */
	if (sched_cpu_high_irqload(cpu))
		return true;

	/* ignore cpus curerntly doing load balancing/migrations */
	if (is_reserved(cpu) || cpu_rq(cpu)->active_balance)
		return true;

	/* ignore cpus which are already selected for storage load balancing */
	if (cpumask_test_cpu(cpu, dst_cpu_mask_to_avoid))
		return true;


	return false;
}

/*
 * target cpu selection for pushing tasks from high irq cpus.
 * select any least loaded cpus valid for storage tasks placement.
 */
static int find_least_util_any_cpu(int src_cpu, cpumask_t *dst_cpu_mask_to_avoid,
					unsigned long src_util)
{
	int cpu, best_cpu = -1;
	unsigned long util = 0, min_util = ULONG_MAX;

	for_each_cpu(cpu, &storage_boost_cpus) {

		if (cpu == src_cpu)
			continue;

		if (lb_ignore_cpus(cpu, dst_cpu_mask_to_avoid))
			continue;

		util = walt_lb_cpu_util(cpu);
		if (util < min_util) {
			best_cpu = cpu;
			min_util = util;
		}
	}

	return best_cpu;
}

static bool move_task(int src_cpu, int dst_cpu, cpumask_t *dst_cpu_mask_to_avoid)
{
	struct rq *dst_rq = cpu_rq(dst_cpu);
	struct rq *src_rq = cpu_rq(src_cpu);
	struct walt_rq *src_wrq = &per_cpu(walt_rq, src_cpu);
	struct task_struct *p, *target_task;
	int ret, task_visited = 0;
	bool moved = false;
	unsigned long flags;
	unsigned long util, max_task_util = 0;

	raw_spin_lock_irqsave(&src_rq->__lock, flags);

	if (src_rq->active_balance) {
		raw_spin_unlock_irqrestore(&src_rq->__lock, flags);
		goto out;
	}

	list_for_each_entry_reverse(p, &src_rq->cfs_tasks, se.group_node) {

		if (!walt_fair_task(p))
			continue;

		if (!cpumask_test_cpu(dst_cpu, p->cpus_ptr))
			continue;

		task_visited++;

		/* storage task are generally heavy task > 700 util, ignore small tasks */
		util = task_util(p);

		if (util > max_task_util) {
			max_task_util = util;
			target_task = p;
		}

		if (task_visited > 10)
			break;
	}


	if (!target_task) {
		raw_spin_unlock_irqrestore(&src_rq->__lock, flags);
		goto out;
	}

	if (task_on_cpu(src_rq, target_task)) {
		get_task_struct(target_task);
		src_rq->active_balance = 1;
		src_rq->push_cpu = dst_cpu;
		src_wrq->push_task = target_task;
		mark_reserved(dst_cpu);
		raw_spin_unlock_irqrestore(&src_rq->__lock, flags);
		ret = stop_one_cpu_nowait(src_cpu,
				stop_walt_lb_active_migration,
				src_rq, &src_rq->active_balance_work);
		if (!ret) {
			clear_reserved(dst_cpu);
			goto out;
		} else {
			cpumask_set_cpu(dst_cpu, dst_cpu_mask_to_avoid);
			moved = true;
			wake_up_if_idle(dst_cpu);
		}
	} else {
		cpumask_set_cpu(dst_cpu, dst_cpu_mask_to_avoid);
		walt_detach_task(target_task, src_rq, dst_rq);
		raw_spin_unlock_irqrestore(&src_rq->__lock, flags);
		raw_spin_lock_irqsave(&dst_rq->__lock, flags);
		walt_attach_task(target_task, dst_rq);
		raw_spin_unlock_irqrestore(&dst_rq->__lock, flags);
		moved = true;
	}

out:
	return moved;
}

static bool migrate_high_irq_cpus(cpumask_t *dst_cpu_mask_to_avoid)
{
	bool done = false;
	int cpu, dst_cpu;
	unsigned long util;

	for_each_possible_cpu(cpu) {

		/* only select from storage high irq cpus */
		if (!cpumask_test_cpu(cpu, &walt_enforce_high_irq_cpu_mask))
			continue;

		/* skip cpus already doing load balancing */
		if (is_reserved(cpu) || cpu_rq(cpu)->active_balance)
			continue;

		util = walt_lb_cpu_util(cpu);

		dst_cpu = find_least_util_any_cpu(cpu, dst_cpu_mask_to_avoid, util);
		if (dst_cpu >= 0)
			done |= move_task(cpu, dst_cpu, dst_cpu_mask_to_avoid);
	}

	return done;
}

#define STORAGE_BALANCE_INTERVAL_NSEC	3000000ULL
bool move_storage_load(struct rq *rq)
{
	bool ret = false;
	cpumask_t dst_cpu_mask_to_avoid = CPU_MASK_NONE;
	static u64 next_balance_time_nsec;

	if (rq->clock < next_balance_time_nsec)
		return ret;

	next_balance_time_nsec = rq->clock + STORAGE_BALANCE_INTERVAL_NSEC;

	/* try to migrate task form high irq cpus */
	ret = migrate_high_irq_cpus(&dst_cpu_mask_to_avoid);
	return ret;
}
