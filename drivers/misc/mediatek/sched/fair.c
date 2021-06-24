// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <trace/hooks/sched.h>
#include "eas_plus.h"
#include "../../../../kernel/sched/pelt.h"
#include "sched_trace.h"

MODULE_LICENSE("GPL");

#ifdef CONFIG_SMP
/*
 * The margin used when comparing utilization with CPU capacity.
 *
 * (default: ~20%)
 */
#define fits_capacity(cap, max)	((cap) * 1280 < (max) * 1024)

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

static unsigned long capacity_of(int cpu);
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
static inline unsigned long task_util(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.util_avg);
}

static inline unsigned long _task_util_est(struct task_struct *p)
{
	struct util_est ue = READ_ONCE(p->se.avg.util_est);

	return (max(ue.ewma, ue.enqueued) | UTIL_AVG_UNCHANGED);
}

static inline unsigned long task_util_est(struct task_struct *p)
{
	return max(task_util(p), _task_util_est(p));
}

#ifdef CONFIG_UCLAMP_TASK
static inline unsigned long uclamp_task_util(struct task_struct *p)
{
	return clamp(task_util_est(p),
		     uclamp_eff_value(p, UCLAMP_MIN),
		     uclamp_eff_value(p, UCLAMP_MAX));
}
#else
static inline unsigned long uclamp_task_util(struct task_struct *p)
{
	return task_util_est(p);
}
#endif

static inline int task_fits_capacity(struct task_struct *p, long capacity)
{
	return fits_capacity(uclamp_task_util(p), capacity);
}

static unsigned long capacity_of(int cpu)
{
	return cpu_rq(cpu)->cpu_capacity;

}

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
 * compute_energy(): Estimates the energy that @pd would consume if @p was
 * migrated to @dst_cpu. compute_energy() predicts what will be the utilization
 * landscape of @pd's CPUs after the task migration, and uses the Energy Model
 * to compute what would be the energy if we decided to actually migrate that
 * task.
 */
static long
compute_energy(struct task_struct *p, int dst_cpu, struct perf_domain *pd)
{
	struct cpumask *pd_mask = perf_domain_span(pd);
	unsigned long cpu_cap = arch_scale_cpu_capacity(cpumask_first(pd_mask));
	unsigned long max_util = 0, sum_util = 0;
	unsigned long energy = 0;
	int cpu;

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
		unsigned long cpu_util, util_cfs = cpu_util_next(cpu, p, dst_cpu);
		struct task_struct *tsk = cpu == dst_cpu ? p : NULL;

		/*
		 * Busy time computation: utilization clamping is not
		 * required since the ratio (sum_util / cpu_capacity)
		 * is already enough to scale the EM reported power
		 * consumption at the (eventually clamped) cpu_capacity.
		 */
		sum_util += schedutil_cpu_util(cpu, util_cfs, cpu_cap,
					       ENERGY_UTIL, NULL);

		/*
		 * Performance domain frequency: utilization clamping
		 * must be considered since it affects the selection
		 * of the performance domain frequency.
		 * NOTE: in case RT tasks are running, by default the
		 * FREQUENCY_UTIL's utilization can be max OPP.
		 */
		cpu_util = schedutil_cpu_util(cpu, util_cfs, cpu_cap,
					      FREQUENCY_UTIL, tsk);
		max_util = max(max_util, cpu_util);

		trace_sched_energy_util(dst_cpu, max_util, sum_util, cpu, util_cfs, cpu_util);
	}

	trace_android_vh_em_cpu_energy(pd->em_pd, max_util, sum_util, &energy);
	if (!energy)
		energy = em_cpu_energy(pd->em_pd, max_util, sum_util);

	trace_sched_compute_energy(dst_cpu, pd_mask, energy, max_util, sum_util);

	return energy;
}

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

void mtk_find_energy_efficient_cpu(void *data, struct task_struct *p, int prev_cpu, int sync, int *new_cpu)
{
	unsigned long prev_delta = ULONG_MAX, best_delta = ULONG_MAX, best_delta_active = ULONG_MAX;
	struct root_domain *rd = cpu_rq(smp_processor_id())->rd;
	int max_spare_cap_cpu_ls = prev_cpu, best_idle_cpu = -1;
	long sys_max_spare_cap = LONG_MIN;
	int sys_max_spare_cap_cpu = -1;
	unsigned long target_cap = 0;
	unsigned long cpu_cap, util, base_energy = 0;
	bool latency_sensitive = false;
	unsigned int min_exit_lat = UINT_MAX;
	int cpu, best_energy_cpu = prev_cpu;
	struct cpuidle_state *idle;
	struct perf_domain *pd;
	int select_reason = -1;


	rcu_read_lock();
	if (!uclamp_min_ls)
		latency_sensitive = uclamp_latency_sensitive(p);
	else
		latency_sensitive = p->uclamp_req[UCLAMP_MIN].value > 0 ? 1 : 0;

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

	if (!task_util_est(p)) {
		select_reason = LB_ZERO_UTIL;
		goto unlock;
	}

	for (; pd; pd = pd->next) {
		unsigned long cur_delta;
		long spare_cap, max_spare_cap = LONG_MIN;
		unsigned long base_energy_pd;
		unsigned long max_spare_cap_ls_idle = 0, max_spare_cap_ls_active = 0;
		int max_spare_cap_cpu = -1;
		int max_spare_cap_cpu_ls_idle = -1;
		int max_spare_cap_cpu_ls_active = -1;
#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
		int cpu_order[NR_CPUS], cnt, i;
#endif

		/* Compute the 'base' energy of the pd, without @p */
		base_energy_pd = compute_energy(p, -1, pd);
		base_energy += base_energy_pd;

#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
		cnt = sort_thermal_headroom(perf_domain_span(pd), cpu_order);

		for(i = 0; i < cnt; i++) {
			cpu = cpu_order[i];	
#else
		for_each_cpu_and(cpu, perf_domain_span(pd), cpu_online_mask) {
#endif

			if (!cpumask_test_cpu(cpu, p->cpus_ptr))
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
			 * Skip CPUs that cannot satisfy the capacity request.
			 * IOW, placing the task there would make the CPU
			 * overutilized. Take uclamp into account to see how
			 * much capacity we can get out of the CPU; this is
			 * aligned with schedutil_cpu_util().
			 */
			util = uclamp_rq_util_with(cpu_rq(cpu), util, p);
			if (!fits_capacity(util, cpu_cap))
				continue;

			/* Always use prev_cpu as a candidate. */
			if (!latency_sensitive && cpu == prev_cpu) {
				prev_delta = compute_energy(p, prev_cpu, pd);
				prev_delta -= base_energy_pd;
				best_delta = min(best_delta, prev_delta);
			}

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
			} else {
				if (spare_cap < max_spare_cap_ls_active)
					continue;
				max_spare_cap_ls_active = spare_cap;
				max_spare_cap_cpu_ls_active = cpu;
			}
		}

		/* Evaluate the energy impact of using this CPU. */
		if (!latency_sensitive && max_spare_cap_cpu >= 0 &&
						max_spare_cap_cpu != prev_cpu) {
			cur_delta = compute_energy(p, max_spare_cap_cpu, pd);
			cur_delta -= base_energy_pd;
			if (cur_delta < best_delta) {
				best_delta = cur_delta;
				best_energy_cpu = max_spare_cap_cpu;
			}
		}

		if (latency_sensitive) {
			if (max_spare_cap_cpu_ls_idle >= 0) {
				cur_delta = compute_energy(p, max_spare_cap_cpu_ls_idle, pd);
				cur_delta -= base_energy_pd;
				if (cur_delta < best_delta) {
					best_delta = cur_delta;
					best_idle_cpu = max_spare_cap_cpu_ls_idle;
				}
			}
			if (max_spare_cap_cpu_ls_active >= 0) {
				cur_delta = compute_energy(p, max_spare_cap_cpu_ls_active, pd);
				cur_delta -= base_energy_pd;
				if (cur_delta < best_delta_active) {
					best_delta_active = cur_delta;
					max_spare_cap_cpu_ls = max_spare_cap_cpu_ls_active;
				}
			}
		}
	}
unlock:
	rcu_read_unlock();

	if (latency_sensitive){
		*new_cpu = best_idle_cpu >= 0 ? best_idle_cpu : max_spare_cap_cpu_ls;
		select_reason = LB_LATENCY_SENSITIVE;
		goto done;
	}

	/*
	 * Pick the best CPU if prev_cpu cannot be used, or if it saves at
	 * least 6% of the energy used by prev_cpu.
	 */
	if (prev_delta == ULONG_MAX){
		/* All cpu failed on !fit_capacity, use sys_max_spare_cap_cpu */
		if (best_energy_cpu == prev_cpu)
			*new_cpu = sys_max_spare_cap_cpu;
		else
			*new_cpu = best_energy_cpu;
		select_reason = LB_NOT_PREV;
		goto done;

	}

	if (prev_delta > best_delta){
		*new_cpu = best_energy_cpu;
		select_reason = LB_BEST_ENERGY_CPU;
		goto done;
	}

	*new_cpu = prev_cpu;
	select_reason = LB_PREV;
	goto done;


fail:
	rcu_read_unlock();

	*new_cpu = -1;
done:
	trace_sched_find_energy_efficient_cpu(prev_delta, best_delta, best_energy_cpu,
			best_idle_cpu, max_spare_cap_cpu_ls, sys_max_spare_cap_cpu);
	trace_sched_select_task_rq(p, select_reason, prev_cpu, *new_cpu,
			task_util(p), task_util_est(p), uclamp_task_util(p),
			latency_sensitive , sync);

	return;
}

#endif

/* must hold runqueue lock for queue se is currently on */
static struct task_struct *detach_a_hint_task(struct rq *src_rq, int dst_cpu)
{
	struct task_struct *p;
	int dst_capacity;
	unsigned int util_min;
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

		util_min = uclamp_boosted(p);

		if (!uclamp_min_ls)
			latency_sensitive = uclamp_latency_sensitive(p);
		else
			latency_sensitive = p->uclamp_req[UCLAMP_MIN].value > 0 ? 1 : 0;

		if (latency_sensitive &&
			util_min <= dst_capacity &&
			src_rq->nr_running > 1) {

			/* detach_task */
			deactivate_task(src_rq, p, DEQUEUE_NOCLOCK);
			set_task_cpu(p, dst_cpu);
			/*
			 * Right now, this is only the second place where
			 * lb_gained[env->idle] is updated (other is detach_tasks)
			 * so we can safely collect stats here rather than
			 * inside detach_tasks().
			 */
			rcu_read_unlock();
			return p;
		}
	}
	rcu_read_unlock();
	return NULL;
}

void mtk_sched_newidle_balance(void *data, struct rq *this_rq, struct rq_flags *rf,
		int *pulled_task, int *done)
{
	int cpu;
	struct rq *src_rq;
	struct task_struct *p = NULL;
	struct rq_flags src_rf;
	int this_cpu = this_rq->cpu;

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

	update_rq_clock(this_rq);
	/*
	 * This is OK, because current is on_cpu, which avoids it being picked
	 * for load-balance and preemption/IRQs are still disabled avoiding
	 * further scheduler activity on it and we're being very careful to
	 * re-start the picking loop.
	 */
	rq_unpin_lock(this_rq, rf);
	raw_spin_unlock(&this_rq->lock);

	this_cpu = this_rq->cpu;
	for_each_cpu(cpu, cpu_online_mask) {
		if (cpu == this_cpu)
			continue;

		src_rq = cpu_rq(cpu);
		if (src_rq->nr_running <= 1)
			continue;

		rq_lock_irqsave(src_rq, &src_rf);
		p = detach_a_hint_task(src_rq, this_cpu);

		rq_unlock_irqrestore(src_rq, &src_rf);

		if (p) {
			trace_sched_force_migrate(p, this_cpu, MIGR_IDLE_BALANCE);
			attach_one_task(this_rq, p);
			break;
		}
	}

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

	rq_repin_lock(this_rq, rf);

	return;
}
