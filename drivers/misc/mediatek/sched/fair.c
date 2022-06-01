// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/sched.h>
#include <trace/hooks/sched.h>
#include <sched/sched.h>
#include "eas/eas_plus.h"
#include "common.h"
#include <linux/stop_machine.h>
#include <linux/kthread.h>
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
#include <thermal_interface.h>
#endif

#define CREATE_TRACE_POINTS
#include "sched_trace.h"

MODULE_LICENSE("GPL");

#ifdef CONFIG_SMP
#ifdef CONFIG_FAIR_GROUP_SCHED
static inline struct task_struct *task_of(struct sched_entity *se)
{
	SCHED_WARN_ON(!entity_is_task(se));
	return container_of(se, struct task_struct, se);
}
#else
static inline struct task_struct *task_of(struct sched_entity *se)
{
	return container_of(se, struct task_struct, se);
}
#endif
#endif

/*
 * Unsigned subtract and clamp on underflow.
 *
 * Explicitly do a load-store to ensure the intermediate value never hits
 * memory. This allows lockless observations without ever seeing the negative
 * values.
 */
#define sub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	typeof(*ptr) val = (_val);				\
	typeof(*ptr) res, var = READ_ONCE(*ptr);		\
	res = var - val;					\
	if (res > var)						\
		res = 0;					\
	WRITE_ONCE(*ptr, res);					\
} while (0)

/*
 * Remove and clamp on negative, from a local variable.
 *
 * A variant of sub_positive(), which does not use explicit load-store
 * and is thus optimized for local variable updates.
 */
#define lsub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	*ptr -= min_t(typeof(*ptr), *ptr, _val);		\
} while (0)

#ifdef CONFIG_SMP
int task_fits_capacity(struct task_struct *p, long capacity)
{
	return fits_capacity(uclamp_task_util(p), capacity);
}

unsigned long capacity_of(int cpu)
{
	return cpu_rq(cpu)->cpu_capacity;

}

unsigned long cpu_util(int cpu)
{
	struct cfs_rq *cfs_rq;
	unsigned int util;

	cfs_rq = &cpu_rq(cpu)->cfs;
	util = READ_ONCE(cfs_rq->avg.util_avg);

	if (sched_feat(UTIL_EST))
		util = max(util, READ_ONCE(cfs_rq->avg.util_est.enqueued));

	return min_t(unsigned long, util, capacity_orig_of(cpu));
}

#if IS_ENABLED(CONFIG_MTK_EAS)
/*
 * Predicts what cpu_util(@cpu) would return if @p was migrated (and enqueued)
 * to @dst_cpu.
 */
static unsigned long cpu_util_next(int cpu, struct task_struct *p, int dst_cpu)
{
	struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;
	unsigned long util_est, util = READ_ONCE(cfs_rq->avg.util_avg);

	/*
	 * If @p migrates from @cpu to another, remove its contribution. Or,
	 * if @p migrates from another CPU to @cpu, add its contribution. In
	 * the other cases, @cpu is not impacted by the migration, so the
	 * util_avg should already be correct.
	 */
	if (task_cpu(p) == cpu && dst_cpu != cpu)
		sub_positive(&util, task_util(p));
	else if (task_cpu(p) != cpu && dst_cpu == cpu)
		util += task_util(p);

	if (sched_feat(UTIL_EST)) {
		util_est = READ_ONCE(cfs_rq->avg.util_est.enqueued);

		/*
		 * During wake-up, the task isn't enqueued yet and doesn't
		 * appear in the cfs_rq->avg.util_est.enqueued of any rq,
		 * so just add it (if needed) to "simulate" what will be
		 * cpu_util() after the task has been enqueued.
		 */
		if (dst_cpu == cpu)
			util_est += _task_util_est(p);

		util = max(util, util_est);
	}

	return min(util, capacity_orig_of(cpu));
}

/*
 * Predicts what cpu_util(@cpu) would return if @p was migrated (and enqueued)
 * to @dst_cpu.
 * input:
 * util_freq = READ_ONCE(cfs_rq->avg.util_avg);
 *
 * if (sched_feat(UTIL_EST)) {
 *	util_est = READ_ONCE(cfs_rq->avg.util_est.enqueued);
 * }
 */
static unsigned long mtk_cpu_util_next(int cpu, struct task_struct *p, int dst_cpu,
					unsigned long util_freq, unsigned long util_est,
					unsigned long *util_energy)
{
	*util_energy = util_freq;
	/*
	 * If @p migrates from @cpu to another, remove its contribution. Or,
	 * if @p migrates from another CPU to @cpu, add its contribution. In
	 * the other cases, @cpu is not impacted by the migration, so the
	 * util_avg should already be correct.
	 */
	if (task_cpu(p) == cpu && dst_cpu != cpu)
		sub_positive(&util_freq, task_util(p));
	else if (task_cpu(p) != cpu && dst_cpu == cpu)
		util_freq += task_util(p);

	if (task_cpu(p) == cpu)
		sub_positive(util_energy, task_util(p));

	if (sched_feat(UTIL_EST)) {
		*util_energy = max(*util_energy, util_est);

		/*
		 * During wake-up, the task isn't enqueued yet and doesn't
		 * appear in the cfs_rq->avg.util_est.enqueued of any rq,
		 * so just add it (if needed) to "simulate" what will be
		 * cpu_util() after the task has been enqueued.
		 */
		if (dst_cpu == cpu)
			util_est += _task_util_est(p);

		util_freq = max(util_freq, util_est);
	}

	if (dst_cpu == cpu) {
		unsigned long task_util_energy;

		if (sched_feat(UTIL_EST))
			task_util_energy = task_util_est(p);
		else
			task_util_energy = task_util(p);

		*util_energy += task_util_energy;
	}

	*util_energy =  min(*util_energy, capacity_orig_of(cpu));

	return min(util_freq, capacity_orig_of(cpu));
}

/*
 * compute_energy(): Estimates the energy that @pd would consume if @p was
 * migrated to @dst_cpu. compute_energy() predicts what will be the utilization
 * landscape of @pd's CPUs after the task migration, and uses the Energy Model
 * to compute what would be the energy if we decided to actually migrate that
 * task.
 * return the delta energy of put task p in dst_cpu
 */
static unsigned long
mtk_compute_energy(struct task_struct *p, int dst_cpu, struct perf_domain *pd)
{
	struct cpumask *pd_mask = perf_domain_span(pd);
	unsigned long cpu_cap = arch_scale_cpu_capacity(cpumask_first(pd_mask));
	unsigned long max_util_base = 0, max_util_cur = 0;
	unsigned long cpu_energy_util, sum_util_base = 0, sum_util_cur = 0;
	unsigned long util_cfs_base, util_cfs_cur, util_cfs_energy_base, util_cfs_energy_cur;
	unsigned long energy_base = 0, energy_cur = 0, energy_delta = 0;
	int cpu;
	int cpu_temp[NR_CPUS];

	/*
	 * The capacity state of CPUs of the current rd can be driven by CPUs
	 * of another rd if they belong to the same pd. So, account for the
	 * utilization of these CPUs too by masking pd with cpu_online_mask
	 * instead of the rd span.
	 *
	 * If an entire pd is outside of the current rd, it will not appear in
	 * its pd list and will not be accounted by compute_energy().
	 */
	for_each_cpu_and(cpu, pd_mask, cpu_online_mask) {
		unsigned long cpu_util_base, cpu_util_cur;
		struct task_struct *tsk = cpu == dst_cpu ? p : NULL;
		struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;
		unsigned long util_est = 0, util_freq = READ_ONCE(cfs_rq->avg.util_avg);

		if (sched_feat(UTIL_EST))
			util_est = READ_ONCE(cfs_rq->avg.util_est.enqueued);

		util_cfs_base = mtk_cpu_util_next(cpu, p, -1, util_freq, util_est,
							&util_cfs_energy_base);
		/*
		 * Busy time computation: utilization clamping is not
		 * required since the ratio (sum_util / cpu_capacity)
		 * is already enough to scale the EM reported power
		 * consumption at the (eventually clamped) cpu_capacity.
		 */
#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
		cpu_energy_util = mtk_cpu_util(cpu, util_cfs_energy_base, cpu_cap,
					       ENERGY_UTIL, NULL);
#else
		cpu_energy_util = schedutil_cpu_util(cpu, util_cfs_energy_base, cpu_cap,
					       ENERGY_UTIL, NULL);
#endif
		sum_util_base += cpu_energy_util;

		/*
		 * Performance domain frequency: utilization clamping
		 * must be considered since it affects the selection
		 * of the performance domain frequency.
		 * NOTE: in case RT tasks are running, by default the
		 * FREQUENCY_UTIL's utilization can be max OPP.
		 */
#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
		cpu_util_base = mtk_cpu_util(cpu, util_cfs_base, cpu_cap,
					      FREQUENCY_UTIL, NULL);
#else
		cpu_util_base = schedutil_cpu_util(cpu, util_cfs_base, cpu_cap,
					      FREQUENCY_UTIL, NULL);
#endif

		if (cpu == dst_cpu) {
			util_cfs_cur = mtk_cpu_util_next(cpu, p, dst_cpu, util_freq, util_est,
						&util_cfs_energy_cur);
			/*
			 * Busy time computation: utilization clamping is not
			 * required since the ratio (sum_util / cpu_capacity)
			 * is already enough to scale the EM reported power
			 * consumption at the (eventually clamped) cpu_capacity.
			 */
#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
			sum_util_cur += mtk_cpu_util(cpu, util_cfs_energy_cur, cpu_cap,
					       ENERGY_UTIL, NULL);
#else
			sum_util_cur += schedutil_cpu_util(cpu, util_cfs_energy_cur, cpu_cap,
					       ENERGY_UTIL, NULL);
#endif
			/*
			 * Performance domain frequency: utilization clamping
			 * must be considered since it affects the selection
			 * of the performance domain frequency.
			 * NOTE: in case RT tasks are running, by default the
			 * FREQUENCY_UTIL's utilization can be max OPP.
			 */
#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
			cpu_util_cur = mtk_cpu_util(cpu, util_cfs_cur, cpu_cap,
					      FREQUENCY_UTIL, tsk);
#else
			cpu_util_cur = schedutil_cpu_util(cpu, util_cfs_cur, cpu_cap,
					      FREQUENCY_UTIL, tsk);
#endif
		} else {

			util_cfs_energy_cur = util_cfs_energy_base;
			util_cfs_cur = util_cfs_base;
			sum_util_cur += cpu_energy_util;
			cpu_util_cur = cpu_util_base;
		}

		max_util_base = max(max_util_base, cpu_util_base);
		max_util_cur = max(max_util_cur, cpu_util_cur);

		trace_sched_energy_util(-1, max_util_base, sum_util_base, cpu, util_cfs_base,
				util_cfs_energy_base, cpu_util_base);
		trace_sched_energy_util(dst_cpu, max_util_cur, sum_util_cur, cpu, util_cfs_cur,
				util_cfs_energy_cur, cpu_util_cur);

		/* get temperature for each cpu*/
		cpu_temp[cpu] = get_cpu_temp(cpu);
		cpu_temp[cpu] /= 1000;
	}

	energy_base = mtk_em_cpu_energy(pd->em_pd, max_util_base, sum_util_base, cpu_temp);
	energy_cur = mtk_em_cpu_energy(pd->em_pd, max_util_cur, sum_util_cur, cpu_temp);
	energy_delta = energy_cur - energy_base;

	trace_sched_compute_energy(-1, pd_mask, energy_base, max_util_base, sum_util_base);
	trace_sched_compute_energy(dst_cpu, pd_mask, energy_cur, max_util_cur, sum_util_cur);

	return energy_delta;
}
#endif

static unsigned int uclamp_min_ls;
void set_uclamp_min_ls(unsigned int val)
{
	uclamp_min_ls = val;
}
EXPORT_SYMBOL_GPL(set_uclamp_min_ls);

unsigned int get_uclamp_min_ls(void)
{
	return uclamp_min_ls;
}
EXPORT_SYMBOL_GPL(get_uclamp_min_ls);

/*
 * attach_task() -- attach the task detached by detach_task() to its new rq.
 */
static void attach_task(struct rq *rq, struct task_struct *p)
{
	lockdep_assert_held(&rq->lock);

	BUG_ON(task_rq(p) != rq);
	activate_task(rq, p, ENQUEUE_NOCLOCK);
	check_preempt_curr(rq, p, 0);
}

/*
 * attach_one_task() -- attaches the task returned from detach_one_task() to
 * its new rq.
 */
static void attach_one_task(struct rq *rq, struct task_struct *p)
{
	struct rq_flags rf;

	rq_lock(rq, &rf);
	update_rq_clock(rq);
	attach_task(rq, p);
	rq_unlock(rq, &rf);
}

#if IS_ENABLED(CONFIG_MTK_EAS)
int mtk_find_idle_cpu(struct task_struct *p)
{
	int target_cpu = -1, cpu;
	unsigned long task_util = uclamp_task_util(p);

	for_each_cpu_and(cpu, p->cpus_ptr,
			cpu_active_mask) {
		if (idle_cpu(cpu) && fits_capacity(task_util, capacity_of(cpu))) {
			target_cpu = cpu;
			break;
		}

	}

	return target_cpu;
}

void mtk_find_energy_efficient_cpu(void *data, struct task_struct *p, int prev_cpu, int sync, int *new_cpu)
{
	unsigned long best_delta = ULONG_MAX;
	struct root_domain *rd = cpu_rq(smp_processor_id())->rd;
	int best_idle_cpu = -1;
	long sys_max_spare_cap = LONG_MIN, idle_max_spare_cap = LONG_MIN;
	int sys_max_spare_cap_cpu = -1;
	int idle_max_spare_cap_cpu = -1;
	unsigned long target_cap = 0;
	unsigned long cpu_cap, util;
	bool latency_sensitive = false;
	unsigned int min_exit_lat = UINT_MAX;
	int cpu, best_energy_cpu = -1;
	struct cpuidle_state *idle;
	struct perf_domain *pd;
	int select_reason = -1;

	rcu_read_lock();
	if (!uclamp_min_ls)
		latency_sensitive = uclamp_latency_sensitive(p);
	else {
		latency_sensitive = (p->uclamp_req[UCLAMP_MIN].value > 0 ? 1 : 0) ||
					uclamp_latency_sensitive(p);
	}

	pd = rcu_dereference(rd->pd);
	if (!pd || READ_ONCE(rd->overutilized)) {
		select_reason = LB_FAIL;
		goto fail;
	}

	cpu = smp_processor_id();
	if (sync && cpu_rq(cpu)->nr_running == 1 &&
	    cpumask_test_cpu(cpu, p->cpus_ptr) &&
	    task_fits_capacity(p, capacity_of(cpu))) {
		rcu_read_unlock();
		*new_cpu = cpu;
		select_reason = LB_SYNC;
		goto done;
	}

	if (in_interrupt()) {
		*new_cpu = mtk_find_idle_cpu(p);
		rcu_read_unlock();
		select_reason = LB_IN_INTERRUPT;
		goto done;
	}

	if (!task_util_est(p)) {
		select_reason = LB_ZERO_UTIL;
		goto unlock;
	}

	for (; pd; pd = pd->next) {
		unsigned long cur_delta;
		long spare_cap, max_spare_cap = LONG_MIN;
		unsigned long max_spare_cap_ls_idle = 0;
		int max_spare_cap_cpu = -1;
		int max_spare_cap_cpu_ls_idle = -1;
#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
		int cpu_order[NR_CPUS]  ____cacheline_aligned, cnt, i;
#endif

#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
		cnt = sort_thermal_headroom(perf_domain_span(pd), cpu_order);

		for(i = 0; i < cnt; i++) {
			cpu = cpu_order[i];
#else
		for_each_cpu_and(cpu, perf_domain_span(pd), cpu_active_mask) {
#endif

			if (!cpumask_test_cpu(cpu, p->cpus_ptr))
				continue;

			if (cpu_rq(cpu)->rt.rt_nr_running >= 1)
				continue;

			util = cpu_util_next(cpu, p, cpu);
			cpu_cap = capacity_of(cpu);
			spare_cap = cpu_cap;
			lsub_positive(&spare_cap, util);

			if (spare_cap > sys_max_spare_cap) {
				sys_max_spare_cap = spare_cap;
				sys_max_spare_cap_cpu = cpu;
			}

			/*
			 * if there is no best idle cpu, then select max spare cap
			 * and idle cpu for latency_sensitive task to avoid runnable.
			 * Because this is just a backup option, we do not take care
			 * of exit latency.
			 */
			if (latency_sensitive && idle_cpu(cpu) &&
					spare_cap > idle_max_spare_cap) {
				idle_max_spare_cap = spare_cap;
				idle_max_spare_cap_cpu = cpu;
			}

			/*
			 * Skip CPUs that cannot satisfy the capacity request.
			 * IOW, placing the task there would make the CPU
			 * overutilized. Take uclamp into account to see how
			 * much capacity we can get out of the CPU; this is
			 * aligned with schedutil_cpu_util().
			 */
			util = mtk_uclamp_rq_util_with(cpu_rq(cpu), util, p);
			if (!fits_capacity(util, cpu_cap))
				continue;

			/*
			 * Find the CPU with the maximum spare capacity in
			 * the performance domain
			 */
			if (spare_cap > max_spare_cap) {
				max_spare_cap = spare_cap;
				max_spare_cap_cpu = cpu;
			}

			if (!latency_sensitive)
				continue;

			if (idle_cpu(cpu)) {
				cpu_cap = capacity_orig_of(cpu);
				idle = idle_get_state(cpu_rq(cpu));
#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
				if (idle && idle->exit_latency >= min_exit_lat &&
						cpu_cap == target_cap)
					continue;
#else
				if (idle && idle->exit_latency > min_exit_lat &&
						cpu_cap == target_cap)
					continue;
#endif

				if (spare_cap < max_spare_cap_ls_idle)
					continue;

				if (idle)
					min_exit_lat = idle->exit_latency;

				max_spare_cap_ls_idle = spare_cap;
				target_cap = cpu_cap;
				max_spare_cap_cpu_ls_idle = cpu;
			}
		}

		/* Evaluate the energy impact of using this CPU. */
		if (!latency_sensitive && max_spare_cap_cpu >= 0) {
			cur_delta = mtk_compute_energy(p, max_spare_cap_cpu, pd);
			if (cur_delta <= best_delta) {
				best_delta = cur_delta;
				best_energy_cpu = max_spare_cap_cpu;
			}
		}

		if (latency_sensitive) {
			if (max_spare_cap_cpu_ls_idle >= 0) {
				cur_delta = mtk_compute_energy(p, max_spare_cap_cpu_ls_idle, pd);
				if (cur_delta <= best_delta) {
					best_delta = cur_delta;
					best_idle_cpu = max_spare_cap_cpu_ls_idle;
				}
			}
		}
	}
unlock:
	rcu_read_unlock();

	if (latency_sensitive){
		if (best_idle_cpu >= 0) {
			*new_cpu = best_idle_cpu;
			select_reason = LB_LATENCY_SENSITIVE_BEST_IDLE_CPU;
		} else if (idle_max_spare_cap_cpu >= 0) {
			*new_cpu = idle_max_spare_cap_cpu;
			select_reason = LB_LATENCY_SENSITIVE_IDLE_MAX_SPARE_CPU;
		} else {
			*new_cpu = sys_max_spare_cap_cpu;
			select_reason = LB_LATENCY_SENSITIVE_MAX_SPARE_CPU;
		}
		goto done;
	}

	/* All cpu failed on !fit_capacity, use sys_max_spare_cap_cpu */
	if (best_energy_cpu != -1) {
		*new_cpu = best_energy_cpu;
		select_reason = LB_BEST_ENERGY_CPU;
		goto done;
	} else {
		*new_cpu = sys_max_spare_cap_cpu;
		select_reason = LB_MAX_SPARE_CPU;
		goto done;
	}

	*new_cpu = prev_cpu;
	select_reason = LB_PREV;
	goto done;


fail:
	rcu_read_unlock();

	*new_cpu = -1;
done:
	trace_sched_find_energy_efficient_cpu(best_delta, best_energy_cpu,
			best_idle_cpu, idle_max_spare_cap_cpu, sys_max_spare_cap_cpu);
	trace_sched_select_task_rq(p, select_reason, prev_cpu, *new_cpu,
			latency_sensitive, sync);

	return;
}
#endif

#endif

#if IS_ENABLED(CONFIG_MTK_EAS)
/* must hold runqueue lock for queue se is currently on */
static struct task_struct *detach_a_hint_task(struct rq *src_rq, int dst_cpu)
{
	struct task_struct *p, *best_task = NULL, *backup = NULL;
	int dst_capacity;
	unsigned int task_util;
	bool latency_sensitive = false;

	lockdep_assert_held(&src_rq->lock);

	rcu_read_lock();
	dst_capacity = capacity_orig_of(dst_cpu);
	list_for_each_entry_reverse(p,
			&src_rq->cfs_tasks, se.group_node) {

		if (!cpumask_test_cpu(dst_cpu, p->cpus_ptr))
			continue;

		if (task_running(src_rq, p))
			continue;

		task_util = uclamp_task_util(p);

		if (!uclamp_min_ls)
			latency_sensitive = uclamp_latency_sensitive(p);
		else {
			latency_sensitive = (p->uclamp_req[UCLAMP_MIN].value > 0 ? 1 : 0) ||
					uclamp_latency_sensitive(p);
		}

		if (latency_sensitive &&
			task_util <= dst_capacity) {
			best_task = p;
			break;
		} else if (latency_sensitive && !backup) {
			backup = p;
		}
	}
	p = best_task ? best_task : backup;
	if (p) {
		/* detach_task */
		deactivate_task(src_rq, p, DEQUEUE_NOCLOCK);
		set_task_cpu(p, dst_cpu);
	}
	rcu_read_unlock();
	return p;
}
#endif

inline bool is_task_latency_sensitive(struct task_struct *p)
{
	bool latency_sensitive = false;

	rcu_read_lock();
	if (!uclamp_min_ls)
		latency_sensitive = uclamp_latency_sensitive(p);
	else {
		latency_sensitive = (p->uclamp_req[UCLAMP_MIN].value > 0 ? 1 : 0) ||
					uclamp_latency_sensitive(p);
	}
	rcu_read_unlock();

	return latency_sensitive;
}

static int mtk_active_load_balance_cpu_stop(void *data)
{

	struct task_struct *target_task = data;
	int busiest_cpu = smp_processor_id();
	struct rq *busiest_rq = cpu_rq(busiest_cpu);
	int target_cpu = busiest_rq->push_cpu;
	struct rq *target_rq = cpu_rq(target_cpu);
	struct rq_flags rf;
	int deactivated = 0;

	local_irq_disable();
	raw_spin_lock(&target_task->pi_lock);
	rq_lock(busiest_rq, &rf);

	if (task_cpu(target_task) != busiest_cpu ||
		(!cpumask_test_cpu(target_cpu, target_task->cpus_ptr)) ||
		task_running(busiest_rq, target_task) ||
		target_rq == busiest_rq)
		goto out_unlock;

	if (!task_on_rq_queued(target_task))
		goto out_unlock;

	if (!cpu_active(busiest_cpu) || !cpu_active(target_cpu))
		goto out_unlock;
	/* Make sure the requested CPU hasn't gone down in the meantime: */
	if (unlikely(!busiest_rq->active_balance))
		goto out_unlock;

	/* Is there any task to move? */
	if (busiest_rq->nr_running <= 1)
		goto out_unlock;

	spin_lock(&per_cpu(cpufreq_idle_cpu_lock, target_cpu));
	if (!per_cpu(cpufreq_idle_cpu, target_cpu) &&
		is_task_latency_sensitive(target_task)) {
		spin_unlock(&per_cpu(cpufreq_idle_cpu_lock, target_cpu));
		goto out_unlock;
	}

	spin_unlock(&per_cpu(cpufreq_idle_cpu_lock, target_cpu));
	update_rq_clock(busiest_rq);
	deactivate_task(busiest_rq, target_task, DEQUEUE_NOCLOCK);
	set_task_cpu(target_task, target_cpu);
	deactivated = 1;
out_unlock:
	busiest_rq->active_balance = 0;
	rq_unlock(busiest_rq, &rf);

	if (deactivated)
		attach_one_task(target_rq, target_task);

	raw_spin_unlock(&target_task->pi_lock);
	put_task_struct(target_task);

	local_irq_enable();
	return 0;
}

int migrate_running_task(int this_cpu, struct task_struct *p, struct rq *target, int reason)
{
	int active_balance = false;
	unsigned long flags;

	raw_spin_lock_irqsave(&target->lock, flags);
	if (!target->active_balance &&
		(task_rq(p) == target) && p->state != TASK_DEAD) {
		target->active_balance = 1;
		target->push_cpu = this_cpu;
		active_balance = true;
		get_task_struct(p);
	}
	raw_spin_unlock_irqrestore(&target->lock, flags);
	if (active_balance) {
		trace_sched_force_migrate(p, this_cpu, reason);
		stop_one_cpu_nowait(cpu_of(target),
				mtk_active_load_balance_cpu_stop,
				p, &target->active_balance_work);
	}

	return active_balance;
}

#if IS_ENABLED(CONFIG_MTK_EAS)
static DEFINE_PER_CPU(u64, next_update_new_balance_time_ns);
void mtk_sched_newidle_balance(void *data, struct rq *this_rq, struct rq_flags *rf,
		int *pulled_task, int *done)
{
	int cpu;
	struct rq *src_rq, *misfit_task_rq = NULL;
	struct task_struct *p = NULL, *best_running_task = NULL;
	struct rq_flags src_rf;
	int this_cpu = this_rq->cpu;
	unsigned long misfit_load = 0;
	u64 now_ns;

	/*
	 * We must set idle_stamp _before_ calling idle_balance(), such that we
	 * measure the duration of idle_balance() as idle time.
	 */
	this_rq->idle_stamp = rq_clock(this_rq);

	/*
	 * Do not pull tasks towards !active CPUs...
	 */
	if (!cpu_active(this_cpu))
		return;

	now_ns = ktime_get_real_ns();

	if (now_ns < per_cpu(next_update_new_balance_time_ns, this_cpu))
		return;

	per_cpu(next_update_new_balance_time_ns, this_cpu) =
		now_ns + new_idle_balance_interval_ns;

	trace_sched_next_new_balance(now_ns, per_cpu(next_update_new_balance_time_ns, this_cpu));

	/*
	 * This is OK, because current is on_cpu, which avoids it being picked
	 * for load-balance and preemption/IRQs are still disabled avoiding
	 * further scheduler activity on it and we're being very careful to
	 * re-start the picking loop.
	 */
	rq_unpin_lock(this_rq, rf);
	raw_spin_unlock(&this_rq->lock);

	this_cpu = this_rq->cpu;
	for_each_cpu(cpu, cpu_active_mask) {
		if (cpu == this_cpu)
			continue;

		src_rq = cpu_rq(cpu);
		rq_lock_irqsave(src_rq, &src_rf);
		update_rq_clock(src_rq);
		if (src_rq->active_balance) {
			rq_unlock_irqrestore(src_rq, &src_rf);
			continue;
		}
		if (src_rq->misfit_task_load > misfit_load &&
			capacity_orig_of(this_cpu) > capacity_orig_of(cpu)) {
			p = src_rq->curr;
			if (p && p->policy == SCHED_NORMAL &&
				cpumask_test_cpu(this_cpu, p->cpus_ptr)) {
				misfit_task_rq = src_rq;
				misfit_load = src_rq->misfit_task_load;
				if (best_running_task)
					put_task_struct(best_running_task);
				best_running_task = p;
				get_task_struct(best_running_task);
			}
			p = NULL;
		}

		if (src_rq->nr_running <= 1) {
			rq_unlock_irqrestore(src_rq, &src_rf);
			continue;
		}

		p = detach_a_hint_task(src_rq, this_cpu);

		rq_unlock_irqrestore(src_rq, &src_rf);

		if (p) {
			trace_sched_force_migrate(p, this_cpu, MIGR_IDLE_BALANCE);
			attach_one_task(this_rq, p);
			break;
		}
	}

	/*
	 * If p is null meaning that we have not pull a runnable task, we try to
	 * pull a latency sensitive running task.
	 */
	if (!p && misfit_task_rq)
		*done = migrate_running_task(this_cpu, best_running_task,
					misfit_task_rq, MIGR_IDLE_PULL_MISFIT_RUNNING);
	if (best_running_task)
		put_task_struct(best_running_task);
	raw_spin_lock(&this_rq->lock);
	/*
	 * While browsing the domains, we released the rq lock, a task could
	 * have been enqueued in the meantime. Since we're not going idle,
	 * pretend we pulled a task.
	 */
	if (this_rq->cfs.h_nr_running && !*pulled_task)
		*pulled_task = 1;

	/* Is there a task of a high priority class? */
	if (this_rq->nr_running != this_rq->cfs.h_nr_running)
		*pulled_task = -1;

	if (*pulled_task)
		this_rq->idle_stamp = 0;

	if (*pulled_task != 0)
		*done = 1;

	rq_repin_lock(this_rq, rf);

	return;
}
#endif
