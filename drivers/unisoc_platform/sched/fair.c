// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Unisoc (shanghai) Technologies Co., Ltd
 */

#include <linux/reciprocal_div.h>
#include <trace/hooks/binder.h>
#include <trace/hooks/sched.h>
#include "uni_sched.h"
#include "cpu_netlink.h"

#if IS_ENABLED(CONFIG_SCHED_WALT)

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


/**
 * is_idle_cpu - is a given CPU idle currently?
 * @cpu: the processor in question.
 *
 * Return: 1 if the CPU is currently idle. 0 otherwise.
 */
static int is_idle_cpu(int cpu)
{
	struct rq *rq = cpu_rq(cpu);

	if (rq->curr != rq->idle)
		return 0;

	if (rq->nr_running)
		return 0;

#ifdef CONFIG_SMP
	if (rq->ttwu_pending)
		return 0;
#endif

	return 1;
}

static bool cpu_overutilized(int cpu)
{
	struct rq *rq = cpu_rq(cpu);

	if (is_max_capacity_cpu(cpu)) {
		if (is_idle_cpu(cpu) || rq->nr_running <= 1)
			return false;
	}

	return walt_cpu_util(cpu) * sched_cap_margin_up[cpu] >
					capacity_orig_of(cpu) * 1024;
}

static unsigned long cpu_util_without(int cpu, struct task_struct *p)
{
	unsigned long util;

	/*
	 * WALT does not decay idle tasks in the same manner
	 * as PELT, so it makes little sense to subtract task
	 * utilization from cpu utilization. Instead just use
	 * cpu_util for this case.
	 */
	if (likely(READ_ONCE(p->__state) == TASK_WAKING))
		return walt_cpu_util(cpu);

	/* Task has no contribution or is new */
	if (cpu != task_cpu(p) || !READ_ONCE(p->se.avg.last_update_time))
		return walt_cpu_util(cpu);

	util = max_t(long, walt_cpu_util(cpu) - walt_task_util(p), 0);

	return min_t(unsigned long, util, capacity_orig_of(cpu));
}

static inline unsigned long capacity_of(int cpu)
{
	return cpu_rq(cpu)->cpu_capacity;
}

static inline int task_fits_capacity(struct task_struct *p, unsigned long capacity, int cpu)
{
	unsigned int margin;

	if (capacity_orig_of(task_cpu(p)) > capacity_orig_of(cpu))
		margin = sched_cap_margin_dn[cpu];
	else
		margin = sched_cap_margin_up[task_cpu(p)];

	return uclamp_task_util(p) * margin < capacity * 1024;
}

static inline int util_fits_capacity(unsigned long util, unsigned long capacity,
					int prev_cpu, int cpu)
{
	unsigned int margin;

	if (capacity_orig_of(prev_cpu) > capacity_orig_of(cpu))
		margin = sched_cap_margin_dn[cpu];
	else
		margin = sched_cap_margin_up[prev_cpu];

	return util * margin < capacity * 1024;
}

#if IS_ENABLED(CONFIG_UNISOC_ROTATION_TASK)
/* ========================= define data struct =========================== */
struct rotation_data {
	struct task_struct *rotation_thread;
	struct task_struct *src_task;
	struct task_struct *dst_task;
	int src_cpu;
	int dst_cpu;
};

static DEFINE_PER_CPU(struct rotation_data, rotation_datas);

#define ENABLE_DELAY_SEC	60
#define BIG_TASK_NUM		4
/* default enable rotation feature */
static bool rotation_enable;
#define threshold_time (sysctl_rotation_threshold_ms * 1000000)

/* after system start 30s, start rotation feature.*/
static struct timer_list rotation_timer;

/* core function */
static void check_for_task_rotation(struct rq *src_rq)
{
	int i, src_cpu = cpu_of(src_rq);
	struct rq *dst_rq;
	int deserved_cpu = nr_cpu_ids, dst_cpu = nr_cpu_ids;
	struct rotation_data *rd = NULL;
	u64 wc, wait, max_wait = 0;
	u64 run, max_run = 0;
	int big_task = 0;
	struct uni_task_struct *uni_tsk;

	if (!rotation_enable || !sysctl_rotation_enable)
		return;

	if (!is_min_capacity_cpu(src_cpu))
		return;

	for_each_possible_cpu(i) {
		struct rq *rq = cpu_rq(i);
		struct task_struct *curr_task = rq->curr;

		if (is_fair_task(curr_task) &&
		    !task_fits_capacity(curr_task, capacity_of(i), i))
			big_task += 1;
	}
	if (big_task < BIG_TASK_NUM)
		return;

	wc = sched_ktime_clock();
	for_each_possible_cpu(i) {
		struct rq *rq = cpu_rq(i);
		struct task_struct *curr_task = rq->curr;

		if (!is_min_capacity_cpu(i) || is_reserved(i))
			continue;

		if (!rq->misfit_task_load || !is_fair_task(curr_task) ||
		    task_fits_capacity(curr_task, capacity_of(i), i))
			continue;

		uni_tsk = (struct uni_task_struct *) curr_task->android_vendor_data1;
		wait = wc - uni_tsk->last_enqueue_ts;
		if (wait > max_wait) {
			max_wait = wait;
			deserved_cpu = i;
		}
	}

	if (deserved_cpu != src_cpu)
		return;

	for_each_possible_cpu(i) {
		struct rq *rq = cpu_rq(i);

		if (is_min_capacity_cpu(i) || is_reserved(i))
			continue;

		if (!is_fair_task(rq->curr))
			continue;

		if (rq->nr_running > 1)
			continue;

		uni_tsk = (struct uni_task_struct *) rq->curr->android_vendor_data1;
		run = wc - uni_tsk->last_enqueue_ts;

		if (run < threshold_time)
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
	if (is_fair_task(dst_rq->curr) &&
		!src_rq->active_balance && !dst_rq->active_balance &&
		cpumask_test_cpu(dst_cpu, src_rq->curr->cpus_ptr) &&
		cpumask_test_cpu(src_cpu, dst_rq->curr->cpus_ptr)) {

		get_task_struct(src_rq->curr);
		get_task_struct(dst_rq->curr);

		mark_reserved(src_cpu);
		mark_reserved(dst_cpu);

		rd = &per_cpu(rotation_datas, src_cpu);

		rd->src_task = src_rq->curr;
		rd->dst_task = dst_rq->curr;

		rd->src_cpu = src_cpu;
		rd->dst_cpu = dst_cpu;

		src_rq->active_balance = 1;
		dst_rq->active_balance = 1;
	}
	double_rq_unlock(src_rq, dst_rq);

	if (rd) {
		wake_up_process(rd->rotation_thread);
		trace_sched_task_rotation(rd->src_cpu, rd->dst_cpu,
				rd->src_task->pid, rd->dst_task->pid);
	}
}

static void do_rotation_task(struct rotation_data *rd)
{
	unsigned long flags;
	struct rq *src_rq = cpu_rq(rd->src_cpu), *dst_rq = cpu_rq(rd->dst_cpu);

	migrate_swap(rd->src_task, rd->dst_task, rd->dst_cpu, rd->src_cpu);

	put_task_struct(rd->src_task);
	put_task_struct(rd->dst_task);

	local_irq_save(flags);
	double_rq_lock(src_rq, dst_rq);
	dst_rq->active_balance = 0;
	src_rq->active_balance = 0;
	double_rq_unlock(src_rq, dst_rq);
	local_irq_restore(flags);

	clear_reserved(rd->src_cpu);
	clear_reserved(rd->dst_cpu);
}

static int __ref try_rotation_task(void *data)
{
	struct rotation_data *rd = data;

	do {
		do_rotation_task(rd);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	} while (!kthread_should_stop());

	return 0;
}

static void set_rotation_enable(struct timer_list *t)
{
	rotation_enable = true;
	pr_info("start rotation feature\n");
}

static void rotation_task_init(void)
{
	int ret = 0;
	int i;

	rotation_enable = false;

	for_each_possible_cpu(i) {
		struct rotation_data *rd = &per_cpu(rotation_datas, i);
		struct sched_param param = { .sched_priority = 49 };
		struct task_struct *thread;

		thread = kthread_create(try_rotation_task, (void *)rd,
					"rotation/%d", i);
		if (IS_ERR(thread))
			goto init_fail;

		ret = sched_setscheduler_nocheck(thread, SCHED_FIFO, &param);
		if (ret) {
			kthread_stop(thread);
			goto init_fail;
		}

		rd->rotation_thread = thread;
	}

	timer_setup(&rotation_timer, set_rotation_enable, 0);
	rotation_timer.expires = jiffies + ENABLE_DELAY_SEC * HZ;
	add_timer(&rotation_timer);
	return;

init_fail:
	for_each_possible_cpu(i)
		if ((&per_cpu(rotation_datas, i))->rotation_thread)
			kthread_stop((&per_cpu(rotation_datas, i))->rotation_thread);
}
#endif
/*
 * walt_compute_energy(): Estimates the energy that @pd would consume if @p was
 * migrated to @dst_cpu. compute_energy() predicts what will be the utilization
 * landscape of @pd's CPUs after the task migration, and uses the Energy Model
 * to compute what would be the energy if we decided to actually migrate that
 * task.
 */
static long walt_compute_energy(struct task_struct *p, int dst_cpu,
				struct perf_domain *pd, struct cpumask *pd_cpus,
				struct pd_cache *pdc)
{
	unsigned long max_util = 0, sum_util = 0, energy = 0;
	unsigned long cpu_cap;
	unsigned long uclamp_util = uclamp_task_util(p);
	int cpu;

	cpu_cap = arch_scale_cpu_capacity(cpumask_first(pd_cpus));
	cpu_cap -= arch_scale_thermal_pressure(cpumask_first(pd_cpus));

	/*
	 * The capacity state of CPUs of the current rd can be driven by CPUs
	 * of another rd if they belong to the same pd. So, account for the
	 * utilization of these CPUs too by masking pd with cpu_online_mask
	 * instead of the rd span.
	 *
	 * If an entire pd is outside of the current rd, it will not appear in
	 * its pd list and will not be accounted by compute_energy().
	 */
	for_each_cpu(cpu, pd_cpus) {
		unsigned long cpu_util;
		struct task_struct *tsk = NULL;

		cpu_util = pdc[cpu].wake_util;
		if (cpu == dst_cpu) {
			tsk = p;
			cpu_util += uclamp_util;
		}

		cpu_util = walt_uclamp_rq_util_with(cpu_rq(cpu), cpu_util, tsk);

		sum_util += min(cpu_util, cpu_cap);

		max_util = max(max_util, min(cpu_util, cpu_cap));
	}

	energy = em_cpu_energy(pd->em_pd, max_util, sum_util, cpu_cap);

	return energy;
}

static bool task_can_place_on_cpu(struct task_struct *p, int cpu)
{
	unsigned long capacity_orig = capacity_orig_of(cpu);
	unsigned long thermal_pressure = arch_scale_thermal_pressure(cpu);
	unsigned long max_capacity = max_possible_capacity;
	unsigned long capacity, cpu_util;
	unsigned int margin;

	if (capacity_orig == max_capacity && is_idle_cpu(cpu))
		return true;

	capacity = capacity_orig - thermal_pressure;

	cpu_util = cpu_util_without(cpu, p);
	cpu_util += uclamp_task_util(p);
	cpu_util = walt_uclamp_rq_util_with(cpu_rq(cpu), cpu_util, p);

	if (capacity_orig_of(task_cpu(p)) > capacity_orig)
		margin = sched_cap_margin_dn[cpu];
	else
		margin = sched_cap_margin_up[task_cpu(p)];

	return cpu_util * margin <  capacity * 1024;
}

static inline int select_cpu_when_overutiled(struct task_struct *p, int prev_cpu,
					     struct pd_cache *pdc)
{
	int cpu, best_active_cpu = -1, best_idle_cpu = -1, target = -1;
	int max_cap_idle = INT_MIN, max_spare = INT_MIN, least_running = INT_MAX;
	struct cpuidle_state *idle;
	unsigned int min_exit_lat = UINT_MAX;

	for_each_cpu(cpu, cpu_active_mask) {
		int spare_cap;
		int cpu_cap = pdc[cpu].cap;
		struct rq *rq = cpu_rq(cpu);
		unsigned int idle_exit_latency = UINT_MAX;

		if (cpu_halted(cpu))
			continue;

		if (!cpumask_test_cpu(cpu, p->cpus_ptr) || is_reserved(cpu))
			continue;

		if (is_idle_cpu(cpu)) {
			idle = idle_get_state(cpu_rq(cpu));
			if (idle)
				idle_exit_latency = idle->exit_latency;
			else
				idle_exit_latency = 0;

			if (cpu_cap > max_cap_idle) {
				best_idle_cpu = cpu;
				max_cap_idle = cpu_cap;
				min_exit_lat = idle_exit_latency;
			} else if (best_idle_cpu >= 0 &&
				   cpu_cap == max_cap_idle &&
				   (cpu == prev_cpu || (best_idle_cpu != prev_cpu &&
				   idle_exit_latency < min_exit_lat))) {
				best_idle_cpu = cpu;
				max_cap_idle = cpu_cap;
				min_exit_lat = idle_exit_latency;
			}

			continue;
		}

		if (rq->nr_running >= 8)
			continue;

		spare_cap = pdc[cpu].cap - pdc[cpu].wake_util;
		if (spare_cap > max_spare) {
			max_spare = spare_cap;
			best_active_cpu = cpu;
			least_running = rq->nr_running;
		} else if (spare_cap == max_spare &&
			   (rq->nr_running < least_running ||
			   (rq->nr_running == least_running && cpu == prev_cpu))) {
			max_spare = spare_cap;
			best_active_cpu = cpu;
			least_running = rq->nr_running;
		}
	}

	if (best_active_cpu == -1) {
		if (cpu_halted(prev_cpu)) {
			cpumask_t non_halted;
			/* choose the lowest-order, unhalted, allowed CPU */
			cpumask_andnot(&non_halted, p->cpus_ptr, cpu_halt_mask);
			target = cpumask_first(&non_halted);
			if (target < nr_cpu_ids)
				prev_cpu = target;
		}
		best_active_cpu = prev_cpu;
	}

	return best_idle_cpu >= 0 ? best_idle_cpu : best_active_cpu;
}

static inline int select_cpu_with_same_energy(int prev_cpu, int best_cpu,
					struct pd_cache *pdc, bool boosted)
{
	int prev_vip = num_vip_tasks_of_cpu(prev_cpu);
	int best_vip = num_vip_tasks_of_cpu(best_cpu);

	if (prev_vip > 0 && best_vip == 0)
		return best_cpu;
	/* the prev_cpu and the best_cpu belong to the same cluster */
	if (boosted && pdc[prev_cpu].cap_orig == pdc[best_cpu].cap_orig &&
	    pdc[best_cpu].wake_util < pdc[prev_cpu].wake_util)
		return best_cpu;

	/* prefer smaller cluster */
	if (!boosted && pdc[prev_cpu].cap_orig > pdc[best_cpu].cap_orig)
		return best_cpu;

	return prev_cpu;
}

static inline void
snapshot_pd_cache_of(struct pd_cache *pd_cache, int cpu, struct task_struct *p)
{
	pd_cache[cpu].wake_util = cpu_util_without(cpu, p);
	pd_cache[cpu].cap_orig = capacity_orig_of(cpu);
	pd_cache[cpu].thermal_pressure = arch_scale_thermal_pressure(cpu);
	pd_cache[cpu].cap = pd_cache[cpu].cap_orig - pd_cache[cpu].thermal_pressure;
	pd_cache[cpu].is_idle = is_idle_cpu(cpu);
}

static int select_best_performance_cpu_for_vip(struct task_struct *p, unsigned long uclamp_util)
{
	int cpu, max_spare_cap_cpu = -1;
	unsigned long util, spare_cap, cpu_cap, max_spare_cap = 0;
	unsigned long min_wake_util = ULONG_MAX;
	struct cpumask cpus;
	struct sched_cluster *cluster, *tmp;

	list_for_each_entry_safe_reverse(cluster, tmp, &cluster_head, list) {
		if (is_min_capacity_cluster(cluster))
			continue;

		cpumask_and(&cpus, &cluster->cpus, cpu_active_mask);

		if (cpumask_empty(&cpus))
			continue;

		for_each_cpu(cpu, &cpus) {
			unsigned long wake_util;

			if (!cpumask_test_cpu(cpu, p->cpus_ptr) || is_reserved(cpu))
				continue;

			if (walt_cpu_high_irqload(cpu))
				continue;

			if (num_vip_tasks_of_cpu(cpu))
				continue;

			if (is_idle_cpu(cpu) && is_max_capacity_cpu(cpu))
				return cpu;

			wake_util = cpu_util_without(cpu, p);
			util = uclamp_util + wake_util;
			cpu_cap = capacity_orig_of(cpu) - arch_scale_thermal_pressure(cpu);
			spare_cap = cpu_cap;
			lsub_positive(&spare_cap, util);

			if (max_spare_cap_cpu == -1 || spare_cap > max_spare_cap ||
			    (spare_cap == max_spare_cap && wake_util < min_wake_util)) {
				max_spare_cap_cpu = cpu;
				max_spare_cap = spare_cap;
				min_wake_util = wake_util;
			}
		}
	}

	return max_spare_cap_cpu;
}

static int walt_find_energy_efficient_cpu(struct task_struct *p, int prev_cpu, int sync)
{
	unsigned long prev_delta = ULONG_MAX, best_delta = ULONG_MAX;
	int cpu = smp_processor_id();
	struct root_domain *rd = cpu_rq(cpu)->rd;
	int max_spare_cap_cpu_ls = prev_cpu, best_idle_cpu = -1;
	int best_energy_cpu = -1, target = -1;
	int task_boost = task_group_boost(p);
	unsigned long max_spare_cap_ls = 0, target_cap = ULONG_MAX;
	unsigned long cpu_cap, util, uclamp_util, base_energy = 0;
	bool boosted, blocked, latency_sensitive = false;
	unsigned int min_exit_lat = UINT_MAX;
	struct cpuidle_state *idle;
	struct perf_domain *pd;
	struct pd_cache pdc[UNI_NR_CPUS];
	struct cpumask cpus;
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *) p->android_vendor_data1;

	rcu_read_lock();
	pd = rcu_dereference(rd->pd);
	if (!pd)
		goto unlock;

	if (sync && cpu_rq(cpu)->nr_running == 1 &&
	    cpumask_test_cpu(cpu, p->cpus_ptr) &&
	    is_min_capacity_cpu(cpu) &&
	    task_can_place_on_cpu(p, cpu) &&
	    !cpu_halted(cpu)) {
		rcu_read_unlock();
		return cpu;
	}

	uclamp_util = uclamp_task_util(p);
	latency_sensitive = uclamp_latency_sensitive(p);
	boosted = (task_boost > 0) || uclamp_boosted(p);
	blocked = (task_boost < 0) || uclamp_blocked(p);

	trace_sched_feec_task_info(p, prev_cpu, walt_task_util(p), task_boost,
				   uclamp_util, boosted, latency_sensitive, blocked);

	if (sched_custom_scene(SCENE_LAUNCH) && test_vip_task(p)) {
		if ((uni_tsk->vip_params & SCHED_BINDER_TYPE) && (uni_tsk->binder_from_cpu != -1)) {
			target = uni_tsk->binder_from_cpu;
			goto unlock;
		}

		if (is_max_capacity_cpu(prev_cpu) && is_idle_cpu(cpu)) {
			target = prev_cpu;
			goto unlock;
		}

		target = select_best_performance_cpu_for_vip(p, uclamp_util);
		if (target != -1)
			goto unlock;
	}

	for (; pd; pd = pd->next) {
		unsigned long cur_delta = ULONG_MAX, spare_cap, max_spare_cap = 0;
		bool compute_prev_delta = false;
		unsigned long base_energy_pd;
		int max_spare_cap_cpu = -1;

		cpumask_and(&cpus, perf_domain_span(pd), cpu_active_mask);
		cpumask_andnot(&cpus, &cpus, cpu_halt_mask);

		if (cpumask_empty(&cpus))
			continue;

		for_each_cpu(cpu, &cpus) {
			bool big_is_idle = false;
			unsigned int idle_exit_latency = UINT_MAX;

			snapshot_pd_cache_of(pdc, cpu, p);

			if (!cpumask_test_cpu(cpu, p->cpus_ptr) || is_reserved(cpu))
				continue;

			if (walt_cpu_high_irqload(cpu))
				continue;

			if (sched_custom_scene(SCENE_LAUNCH) && (!test_vip_task(p)) &&
				num_vip_tasks_of_cpu(cpu))
				continue;

			/* speed up goto big core */
			util = pdc[cpu].wake_util + uclamp_util;
			cpu_cap = pdc[cpu].cap;
			spare_cap = cpu_cap;
			lsub_positive(&spare_cap, util);

			if (pdc[cpu].is_idle) {
				idle = idle_get_state(cpu_rq(cpu));
				if (idle)
					idle_exit_latency = idle->exit_latency;
				else
					idle_exit_latency = 0;

				if (is_max_capacity_cpu(cpu))
					big_is_idle = true;
			}

			trace_sched_feec_rq_task_util(cpu, p, &pdc[cpu],
						      util, spare_cap, cpu_cap);

			if (!big_is_idle &&
			    !util_fits_capacity(util, cpu_cap, prev_cpu, cpu))
				continue;

			if (blocked && is_min_capacity_cpu(cpu)) {
				target = cpu;
				goto unlock;
			}

			if (!latency_sensitive && cpu == prev_cpu) {
				/* Always use prev_cpu as a candidate. */
				compute_prev_delta = true;
			} else if (spare_cap > max_spare_cap) {
				/*
				 * Find the CPU with the maximum spare capacity
				 * in the performance domain.
				 */
				max_spare_cap = spare_cap;
				max_spare_cap_cpu = cpu;
			} else if (spare_cap == 0 && big_is_idle &&
				   max_spare_cap == 0) {
				max_spare_cap = spare_cap;
				max_spare_cap_cpu = cpu;
			}

			if (!latency_sensitive)
				continue;

			if (pdc[cpu].is_idle) {
				/* prefer idle CPU with lower cap_orig */
				if (pdc[cpu].cap_orig > target_cap)
					continue;

				if (idle && idle->exit_latency > min_exit_lat &&
				    pdc[cpu].cap_orig == target_cap)
					continue;

				if (best_idle_cpu == prev_cpu)
					continue;

				min_exit_lat = idle_exit_latency;
				target_cap = pdc[cpu].cap_orig;
				best_idle_cpu = cpu;
			} else if (spare_cap > max_spare_cap_ls) {
				max_spare_cap_ls = spare_cap;
				max_spare_cap_cpu_ls = cpu;
			}
		}

		if (latency_sensitive ||
		   (max_spare_cap_cpu < 0 && !compute_prev_delta))
			continue;

		/* Compute the 'base' energy of the pd, without @p */
		base_energy_pd = walt_compute_energy(p, -1, pd, &cpus, pdc);
		base_energy += base_energy_pd;

		/* Evaluate the energy impact of using prev_cpu. */
		if (compute_prev_delta) {
			prev_delta = walt_compute_energy(p, prev_cpu, pd, &cpus, pdc);
			prev_delta -= base_energy_pd;
			if (prev_delta < best_delta) {
				best_delta = prev_delta;
				best_energy_cpu = prev_cpu;
			}
		}

		/* Evaluate the energy impact of using max_spare_cap_cpu. */
		if (max_spare_cap_cpu >= 0) {
			if (uclamp_util < sysctl_sched_task_util_prefer_little &&
			    is_min_capacity_cpu(max_spare_cap_cpu)) {
				target = max_spare_cap_cpu;
				goto unlock;
			}

			cur_delta = walt_compute_energy(p, max_spare_cap_cpu, pd, &cpus, pdc);
			cur_delta -= base_energy_pd;

			/* prefer small core when delta is equal, but it need
			 * satisfy the small core has the small cpu number.
			 */
			if (cur_delta <= best_delta) {
				best_delta = cur_delta;
				best_energy_cpu = max_spare_cap_cpu;
			}
		}
		trace_sched_energy_diff(base_energy_pd, base_energy, prev_delta,
					cur_delta, best_delta, prev_cpu,
					best_energy_cpu, max_spare_cap_cpu);
	}
	rcu_read_unlock();

	trace_sched_feec_candidates(prev_cpu, best_energy_cpu, base_energy, prev_delta,
				    best_delta, best_idle_cpu, max_spare_cap_cpu_ls);

	if (latency_sensitive)
		return best_idle_cpu >= 0 ? best_idle_cpu : max_spare_cap_cpu_ls;

	/* all cpus are overutiled */
	if (best_energy_cpu < 0)
		return select_cpu_when_overutiled(p, prev_cpu, pdc);
	/*
	 * Pick the best CPU if prev_cpu cannot be used, or if it saves at
	 * least 6% of the energy used by prev_cpu.
	 */
	if (prev_delta == ULONG_MAX || best_energy_cpu == prev_cpu)
		return best_energy_cpu;

	if ((prev_delta - best_delta) > ((prev_delta + base_energy) >> 4))
		return best_energy_cpu;

	return select_cpu_with_same_energy(prev_cpu, best_energy_cpu,
					   pdc, boosted);

unlock:
	rcu_read_unlock();

	return target;

}

static void record_wakee(struct task_struct *p)
{
	/*
	 * Only decay a single time; tasks that have less then 1 wakeup per
	 * jiffy will not have built up many flips.
	 */
	if (time_after(jiffies, current->wakee_flip_decay_ts + HZ)) {
		current->wakee_flips >>= 1;
		current->wakee_flip_decay_ts = jiffies;
	}

	if (current->last_wakee != p) {
		current->last_wakee = p;
		current->wakee_flips++;
	}
}

static void walt_select_task_rq_fair(void *data, struct task_struct *p, int prev_cpu,
					int sd_flag, int wake_flags, int *target_cpu)
{
	int sync;

	if (unlikely(uni_sched_disabled))
		return;

	/* just record the last cpu */
	p->recent_used_cpu = prev_cpu;

	if (wake_flags & WF_TTWU)
		record_wakee(p);

	sync = (wake_flags & WF_SYNC) && !(current->flags & PF_EXITING);

	*target_cpu = walt_find_energy_efficient_cpu(p, prev_cpu, sync);
}

/*
 * detach_task() -- detach the task for the migration specified in env
 */
static void walt_detach_task(struct task_struct *p, struct rq *src_rq,
						    struct rq *dst_rq)
{

	lockdep_assert_rq_held(src_rq);

	deactivate_task(src_rq, p, 0);
	double_lock_balance(src_rq, dst_rq);
	if (!(src_rq->clock_update_flags & RQCF_UPDATED))
		update_rq_clock(src_rq);
	set_task_cpu(p, dst_rq->cpu);
	double_unlock_balance(src_rq, dst_rq);
}

static void walt_attach_task(struct rq *rq, struct task_struct *p)
{
	lockdep_assert_rq_held(rq);

	BUG_ON(task_rq(p) != rq);
	activate_task(rq, p, 0);
	check_preempt_curr(rq, p, 0);
}

/*
 * attach_one_task() -- attaches the task returned from detach_one_task() to
 * its new rq.
 */
static void walt_attach_one_task(struct rq *rq, struct task_struct *p)
{
	struct rq_flags rf;

	rq_lock(rq, &rf);
	update_rq_clock(rq);
	walt_attach_task(rq, p);
	rq_unlock(rq, &rf);
}

static void walt_migrate_queued_task(void *data, struct rq *rq,
				     struct rq_flags *rf, struct task_struct *p,
				     int new_cpu, int *detached)
{
	if (unlikely(uni_sched_disabled))
		return;

	/*
	 * WALT expects both source and destination rqs to be
	 * held when set_task_cpu() is called on a queued task.
	 * so implementing this detach hook. unpin the lock
	 * before detaching and repin it later to make lockdep
	 * happy.
	 */
	BUG_ON(!rf);

	rq_unpin_lock(rq, rf);
	walt_detach_task(p, rq, cpu_rq(new_cpu));
	rq_repin_lock(rq, rf);

	*detached = 1;
}

static void walt_find_busiest_group(void *data, struct sched_group *busiest,
				    struct rq *dst_rq, int *out_balance)
{
	int busiest_cpu;

	if (unlikely(uni_sched_disabled))
		return;

	if (!busiest)
		return;

	/*there is only one cpu in group */
	busiest_cpu = group_first_cpu(busiest);

	/* it's not necessary to pull task when cpus belong to
	 * same cluster and the buiest_cpu's running is <=1;
	 */
	if (same_cluster(busiest_cpu, cpu_of(dst_rq)) &&
	    cpu_rq(busiest_cpu)->nr_running > 1)
		*out_balance = 0;

}

static void walt_nohz_balancer_kick(void *data, struct rq *rq,
					unsigned int *flags, int *done)
{
	if (unlikely(uni_sched_disabled))
		return;

	if (rq->nr_running >= 2 && (cpu_overutilized(rq->cpu) ||
		is_min_capacity_cpu(rq->cpu)))
		*flags = NOHZ_KICK_MASK;

	*done = 1;
}

static void walt_find_new_ilb(void *data, struct cpumask *nohz_idle_cpus_mask,
					  int *ilb)
{
	int cpu = smp_processor_id();
	cpumask_t idle_cpus, tmp_cpus;
	struct sched_cluster *cluster;
	unsigned long ref_cap = capacity_orig_of(cpu);
	unsigned long best_cap, best_cap_cpu = -1;
	int is_small_cpu;

	if (unlikely(uni_sched_disabled))
		return;

	cpumask_and(&idle_cpus, nohz_idle_cpus_mask,
			housekeeping_cpumask(HK_FLAG_MISC));

	if (cpumask_empty(&idle_cpus))
		return;

	is_small_cpu = is_min_capacity_cpu(cpu);
	best_cap = is_small_cpu ? ULONG_MAX : 0;

	for_each_sched_cluster(cluster) {
		int i;
		unsigned long cap;

		cpumask_and(&tmp_cpus, &idle_cpus, &cluster->cpus);

		/* This cluster did not have any idle CPUs */
		if (cpumask_empty(&tmp_cpus))
			continue;

		i = cpumask_first(&tmp_cpus);

		cap = capacity_orig_of(i);

		/* The first preference is for the same capacity CPU */
		if (cap == ref_cap) {
			*ilb = i;
			goto out;
		}

		/*
		 * When there are no idle CPUs in the same cluster, prefer cpu
		 * with best capacity:
		 * this_cpu is:
		 * small cpu : prefer middle cpu;
		 * middle cpu: prefer big cpu;
		 * big cpu   : prefer middle cpu;
		 */
		if (is_small_cpu) {
			if (cap < best_cap) {
				best_cap = cap;
				best_cap_cpu = i;
			}
		} else {
			if (cap > best_cap) {
				best_cap = cap;
				best_cap_cpu = i;
			}
		}

	}
out:
	*ilb = best_cap_cpu;

	trace_sched_find_new_ilb(cpu, ref_cap, best_cap_cpu, best_cap, *ilb);
}

static int walt_active_migration_cpu_stop(void *data)
{
	struct rq *busiest_rq = data;
	int busiest_cpu = cpu_of(busiest_rq);
	int target_cpu = busiest_rq->push_cpu;
	struct rq *target_rq = cpu_rq(target_cpu);
	struct uni_rq *busiest_uni_rq = (struct uni_rq *) busiest_rq->android_vendor_data1;
	struct task_struct *push_task;
	struct rq_flags rf;
	int push_task_detached = 0;

	rq_lock_irq(busiest_rq, &rf);
	push_task = busiest_uni_rq->push_task;

	if (!cpu_active(busiest_cpu) || !cpu_active(target_cpu) || !push_task)
		goto out_unlock;

	/* Make sure the requested CPU hasn't gone down in the meantime: */
	if (unlikely(busiest_cpu != smp_processor_id() ||
		     !busiest_rq->active_balance))
		goto out_unlock;

	/* Is there any task to move? */
	if (busiest_rq->nr_running <= 1)
		goto out_unlock;
	/*
	 * This condition is "impossible", if it occurs
	 * we need to fix it. Originally reported by
	 * Bjorn Helgaas on a 128-CPU setup.
	 */
	BUG_ON(busiest_rq == target_rq);

	if (task_on_rq_queued(push_task) &&
	    READ_ONCE(push_task->__state) == TASK_RUNNING &&
	    task_cpu(push_task) == busiest_cpu &&
	    cpu_active(target_cpu) &&
	    cpumask_test_cpu(target_cpu, push_task->cpus_ptr)) {
		update_rq_clock(busiest_rq);
		walt_detach_task(push_task, busiest_rq, target_rq);
		push_task_detached = 1;
	}

out_unlock:
	busiest_rq->active_balance = 0;
	clear_reserved(target_cpu);
	busiest_uni_rq->push_task = NULL;
	rq_unlock(busiest_rq, &rf);

	if (push_task_detached)
		walt_attach_one_task(target_rq, push_task);

	if (push_task)
		put_task_struct(push_task);

	local_irq_enable();

	return 0;
}

static DEFINE_RAW_SPINLOCK(migration_lock);
static void android_vh_scheduler_tick(void *unused, struct rq *rq)
{
	int prev_cpu = rq->cpu, new_cpu;
	struct task_struct *p = rq->curr;
	struct uni_rq *uni_rq = (struct uni_rq *) rq->android_vendor_data1;
	int ret;

	if (unlikely(uni_sched_disabled))
		return;

	if (!is_fair_task(p) || !rq->misfit_task_load ||
	    READ_ONCE(p->__state) != TASK_RUNNING || p->nr_cpus_allowed == 1)
		return;

	raw_spin_lock(&migration_lock);

	rcu_read_lock();
	new_cpu = walt_find_energy_efficient_cpu(p, prev_cpu, 0);
	rcu_read_unlock();

	if ((new_cpu != -1) &&
	    (capacity_orig_of(new_cpu) > capacity_orig_of(prev_cpu))) {
		/* Invoke active balance to force migrate currently running task */
		raw_spin_rq_lock(rq);

		if (rq->active_balance) {
			raw_spin_rq_unlock(rq);
			goto out_unlock;
		}

		rq->active_balance = 1;
		rq->push_cpu = new_cpu;
		get_task_struct(p);
		uni_rq->push_task = p;

		raw_spin_rq_unlock(rq);

		mark_reserved(new_cpu);

		raw_spin_unlock(&migration_lock);

		trace_sched_active_migration(p, prev_cpu, new_cpu);

		ret = stop_one_cpu_nowait(prev_cpu, walt_active_migration_cpu_stop,
					rq, &rq->active_balance_work);

		if (!ret)
			clear_reserved(new_cpu);

		return;
	} else {
		check_for_task_rotation(rq);
	}

out_unlock:
	raw_spin_unlock(&migration_lock);
}

static void walt_cpu_overutilzed(void *data, int cpu, int *overutilized)
{
	if (unlikely(uni_sched_disabled))
		return;

	*overutilized = cpu_overutilized(cpu);
}

static void android_rvh_update_misfit_status(void *data, struct task_struct *p,
					     struct rq *rq, bool *need_update)
{
	if (unlikely(uni_sched_disabled))
		return;

	*need_update = false;

	if (!p || p->nr_cpus_allowed == 1) {
		rq->misfit_task_load = 0;
		return;
	}

	if (is_max_capacity_cpu(cpu_of(rq)) ||
	    task_fits_capacity(p, capacity_orig_of(cpu_of(rq)), cpu_of(rq))) {
		rq->misfit_task_load = 0;
		return;
	}

	/*
	 * Make sure that misfit_task_load will not be null even if
	 * task_h_load() returns 0.
	 */
	rq->misfit_task_load = max_t(unsigned long, walt_task_util(p), 1);
}

#ifdef CONFIG_UNISOC_WORKAROUND_VRUNTIME_BIG_GAP

#define WMULT_CONST	(~0U)
#define WMULT_SHIFT	32

static void __update_inv_weight(struct load_weight *lw)
{
	unsigned long w;

	if (likely(lw->inv_weight))
		return;

	w = scale_load_down(lw->weight);

	if (BITS_PER_LONG > 32 && unlikely(w >= WMULT_CONST))
		lw->inv_weight = 1;
	else if (unlikely(!w))
		lw->inv_weight = WMULT_CONST;
	else
		lw->inv_weight = WMULT_CONST / w;
}

static u64 __calc_delta(u64 delta_exec, unsigned long weight, struct load_weight *lw)
{
	u64 fact = scale_load_down(weight);
	u32 fact_hi = (u32)(fact >> 32);
	int shift = WMULT_SHIFT;
	int fs;

	__update_inv_weight(lw);

	if (unlikely(fact_hi)) {
		fs = fls(fact_hi);
		shift -= fs;
		fact >>= fs;
	}

	fact = mul_u32_u32(fact, lw->inv_weight);

	fact_hi = (u32)(fact >> 32);
	if (fact_hi) {
		fs = fls(fact_hi);
		shift -= fs;
		fact >>= fs;
	}

	return mul_u64_u32_shr(delta_exec, fact, shift);
}
/*
 * delta /= w
 */
static inline u64 calc_delta_fair(u64 delta, struct sched_entity *se)
{
	if (unlikely(se->load.weight != NICE_0_LOAD))
		delta = __calc_delta(delta, NICE_0_LOAD, &se->load);

	return delta;
}

#define VRUNTIME_LIMIT (10 * NSEC_PER_SEC)
#endif

static void android_rvh_place_entity(void *data, struct cfs_rq *cfs_rq,
				struct sched_entity *se, int initial, u64 *vruntime)
{
	struct cfs_rq *se_cfs_rq;
	u64 sleep_time;

	se_cfs_rq = cfs_rq_of(se);

#ifdef CONFIG_UNISOC_WORKAROUND_VRUNTIME_BIG_GAP
	if (entity_is_task(se) && (((s64)(se->vruntime - *vruntime)) > VRUNTIME_LIMIT)) {
		u64 delta = VRUNTIME_LIMIT;
		struct sched_entity *curr = se_cfs_rq->curr;

		if (curr)
			delta = calc_delta_fair(VRUNTIME_LIMIT, curr);

		se->vruntime = *vruntime + delta;
		return;
	}
#endif
	if (se->exec_start == 0)
		return;

	sleep_time = rq_clock_task(rq_of(se_cfs_rq));

	/* Happen while migrating because of clock task divergence */
	if (sleep_time <= se->exec_start)
		return;

	sleep_time -= se->exec_start;
	if (sleep_time > 60LL * NSEC_PER_SEC)
		se->vruntime = *vruntime;
}
#endif

#ifdef CONFIG_UNISOC_SCHED_VIP_TASK
static int get_task_vip_level(struct task_struct *p)
{
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *) p->android_vendor_data1;

	if (!test_vip_task(p))
		return SCHED_NOT_VIP;

	if (uni_tsk->vip_params & SCHED_BINDER_TYPE)
		return SCHED_BINDER_VIP;

	if (uni_tsk->vip_params & SCHED_AUDIO_TYPE)
		return SCHED_AUDIO_VIP;

	if (uni_tsk->vip_params & SCHED_LL_CAMERA_TYPE) {
		if ((uni_tsk->vip_params & SCHED_ANIMATOR_TYPE) ||
			(uni_tsk->vip_params & SCHED_UI_THREAD_TYPE)) {
			return SCHED_CAMERA_UI_VIP;
		} else {
			return SCHED_CAMERA_VIP;
		}
	}

	if (uni_tsk->vip_params & SCHED_ANIMATOR_TYPE) {
		if (uni_tsk->vip_params & SCHED_UI_THREAD_TYPE)
			return SCHED_ANIMATOR_UI_VIP;
		else
			return SCHED_ANIMATOR_VIP;
	}

	if (uni_tsk->vip_params & SCHED_UI_THREAD_TYPE)
		return SCHED_UI_VIP;

	return SCHED_NOT_VIP;
}

static inline unsigned int cfs_vip_task_limit(struct task_struct *p)
{
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *) p->android_vendor_data1;

	/* audio tasks are high prio but have only single slice */
	if (uni_tsk->vip_level == SCHED_AUDIO_VIP)
		return SCHED_VIP_SLICE;

	if (sched_custom_scene(SCENE_LAUNCH))
		return SCHED_VIP_LAUNCH_LIMIT;

	return SCHED_VIP_LIMIT;
}

static void cfs_insert_vip_task(struct uni_rq *uni_rq, struct uni_task_struct *uni_tsk,
				bool at_front)
{
	struct list_head *pos;

	list_for_each(pos, &uni_rq->vip_tasks) {
		struct uni_task_struct *tmp_tsk = container_of(pos, struct uni_task_struct,
								vip_list);

		if (at_front) {
			if (uni_tsk->vip_level >= tmp_tsk->vip_level)
				break;
		} else {
			if (uni_tsk->vip_level > tmp_tsk->vip_level)
				break;
		}
	}

	list_add(&uni_tsk->vip_list, pos->prev);
	uni_rq->num_vip_tasks++;
}

static void cfs_deactivate_vip_task(struct rq *rq, struct task_struct *p)
{
	struct uni_rq *uni_rq = (struct uni_rq *) rq->android_vendor_data1;
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *) p->android_vendor_data1;

	list_del_init(&uni_tsk->vip_list);
	uni_tsk->vip_level = SCHED_NOT_VIP;
	uni_rq->num_vip_tasks--;
}

/*
 * MVP task runtime update happens here. Three possibilities:
 *
 * de-activated: The MVP consumed its runtime. Non MVP can preempt.
 * slice expired: MVP slice is expired and other MVP can preempt.
 * slice not expired: This MVP task can continue to run.
 */
static void cfs_account_vip_runtime(struct rq *rq, struct task_struct *curr)
{
	struct uni_rq *uni_rq = (struct uni_rq *) rq->android_vendor_data1;
	struct uni_task_struct *uni_curr = (struct uni_task_struct *) curr->android_vendor_data1;
	u64 slice;
	unsigned int limit;

	lockdep_assert_held(&rq->__lock);

	/*
	 * RQ clock update happens in tick path in the scheduler.
	 * Since we drop the lock in the scheduler before calling
	 * into vendor hook, it is possible that update flags are
	 * reset by another rq lock and unlock. Do the update here
	 * if required.
	 */
	if (!(rq->clock_update_flags & RQCF_UPDATED))
		update_rq_clock(rq);

	if (curr->se.sum_exec_runtime > uni_curr->sum_exec_snapshot_for_total)
		uni_curr->total_exec =
		curr->se.sum_exec_runtime - uni_curr->sum_exec_snapshot_for_total;
	else
		uni_curr->total_exec = 0;

	if (curr->se.sum_exec_runtime > uni_curr->sum_exec_snapshot_for_slice)
		slice = curr->se.sum_exec_runtime - uni_curr->sum_exec_snapshot_for_slice;
	else
		slice = 0;

	/* slice is not expired */
	if (slice < SCHED_VIP_SLICE)
		return;

	uni_curr->sum_exec_snapshot_for_slice = curr->se.sum_exec_runtime;
	/*
	 * slice is expired, check if we have to deactivate the
	 * VIP task, otherwise requeue the task in the list so
	 * that other VIP tasks gets a chance.
	 */

	limit = cfs_vip_task_limit(curr);
	if (uni_curr->total_exec > limit) {
		cfs_deactivate_vip_task(rq, curr);
		trace_cfs_deactivate_vip_task(curr, uni_curr, limit);
		return;
	}

	if (uni_rq->num_vip_tasks == 1)
		return;

	/* slice expired. re-queue the task */
	list_del(&uni_curr->vip_list);
	uni_rq->num_vip_tasks--;
	cfs_insert_vip_task(uni_rq, uni_curr, false);
}

void enqueue_cfs_vip_task(struct rq *rq, struct task_struct *p)
{
	struct uni_rq *uni_rq = (struct uni_rq *) rq->android_vendor_data1;
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *) p->android_vendor_data1;
	int vip_level = get_task_vip_level(p);

	if (vip_level == SCHED_NOT_VIP)
		return;

	uni_tsk->vip_level = vip_level;

	/*
	 * This can happen during migration or enq/deq for prio/class change.
	 * it was once vip but got demoted, it will not be vip until
	 * it goes to sleep again.
	 */
	if (uni_tsk->total_exec >= cfs_vip_task_limit(p))
		return;

	cfs_insert_vip_task(uni_rq, uni_tsk, task_running(rq, p));

	/*
	 * We inserted the task at the appropriate position. Take the
	 * task runtime snapshot. From now onwards we use this point as a
	 * baseline to enforce the slice and demotion.
	 */
	if (!uni_tsk->total_exec) {
		uni_tsk->sum_exec_snapshot_for_total = p->se.sum_exec_runtime;
		uni_tsk->sum_exec_snapshot_for_slice = p->se.sum_exec_runtime;
	}
}

void dequeue_cfs_vip_task(struct rq *rq, struct task_struct *p)
{
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *) p->android_vendor_data1;

	if (!list_empty(&uni_tsk->vip_list) && uni_tsk->vip_list.next)
		cfs_deactivate_vip_task(rq, p);

	/* The total exec time is reset only when sleep. */
	if (READ_ONCE(p->__state) != TASK_RUNNING)
		uni_tsk->total_exec = 0;
}

static void cfs_vip_scheduler_tick(void *data, struct rq *rq)
{
	struct uni_rq *uni_rq = (struct uni_rq *) rq->android_vendor_data1;
	struct task_struct *curr = rq->curr;
	struct uni_task_struct *uni_curr = (struct uni_task_struct *) curr->android_vendor_data1;

	if (unlikely(uni_sched_disabled))
		return;

	if (!is_fair_task(curr))
		return;

	raw_spin_rq_lock(rq);

	if (list_empty(&uni_curr->vip_list) || (uni_curr->vip_list.next == NULL))
		goto out;

	cfs_account_vip_runtime(rq, curr);
	/*
	 * If the current is not vip means, we have to re-schedule to
	 * see if we can run any other task including vip tasks.
	 */
	if ((uni_rq->vip_tasks.next != &uni_curr->vip_list) && rq->cfs.h_nr_running > 1)
		resched_curr(rq);

out:
	raw_spin_rq_unlock(rq);
}

/*
 * only compare p and curr, if the two tasks are both not vip task, decision to CFS.
 */
static void cfs_check_preempt_wakeup(void *data, struct rq *rq, struct task_struct *p,
					  bool *preempt, bool *ignore, int wake_flags,
					  struct sched_entity *se, struct sched_entity *pse,
					  int next_buddy_marked, unsigned int granularity)
{
	struct uni_rq *uni_rq = (struct uni_rq *) rq->android_vendor_data1;
	struct uni_task_struct *uni_p = (struct uni_task_struct *) p->android_vendor_data1;
	struct task_struct *curr = rq->curr;
	struct uni_task_struct *uni_curr =
				(struct uni_task_struct *)rq->curr->android_vendor_data1;
	bool resched = false;
	bool p_is_vip, curr_is_vip;

	if (unlikely(uni_sched_disabled))
		return;

	p_is_vip = test_vip_task(p) && (!list_empty(&uni_p->vip_list)) &&
					      uni_p->vip_list.next;
	curr_is_vip = test_vip_task(curr) && (!list_empty(&uni_curr->vip_list)) &&
						uni_curr->vip_list.next;

	/* current is not VIP. */
	if (!curr_is_vip) {
		if (p_is_vip)
			goto preempt;
		return; /* CFS decides preemption */
	}

	/* current is vip. update its runtime before deciding the preemption. */
	cfs_account_vip_runtime(rq, curr);
	resched = (uni_rq->vip_tasks.next != &uni_curr->vip_list);

	/*
	 * current is no longer eligible to run. It must have been
	 * picked (because of vip) ahead of other tasks in the CFS
	 * tree, so drive preemption to pick up the next task from
	 * the tree, which also includes picking up the first in
	 * the vip queue.
	 */
	if (resched)
		goto preempt;

	/* current is the first in the queue, so no preemption */
	*ignore = true;
	trace_cfs_vip_wakeup_nopreempt(curr, uni_curr, cfs_vip_task_limit(curr));
	return;
preempt:
	*preempt = true;
	trace_cfs_vip_wakeup_preempt(p, uni_p, cfs_vip_task_limit(p));
}

#ifdef CONFIG_FAIR_GROUP_SCHED
/* Walk up scheduling entities hierarchy */
#define for_each_sched_entity(se) \
		for (; se; se = se->parent)
#else /* !CONFIG_FAIR_GROUP_SCHED */
#define for_each_sched_entity(se) \
		for (; se; se = NULL)
#endif

extern void set_next_entity(struct cfs_rq *cfs_rq, struct sched_entity *se);
static void cfs_replace_next_task_fair(void *data, struct rq *rq, struct task_struct **p,
					struct sched_entity **se, bool *repick, bool simple,
					struct task_struct *prev)
{
	struct uni_rq *uni_rq = (struct uni_rq *) rq->android_vendor_data1;
	struct uni_task_struct *uni_tsk, *uni_tsk_tmp;
	struct task_struct *vip;
	struct cfs_rq *cfs_rq;

	if (unlikely(uni_sched_disabled))
		return;

	if ((*p) && (*p) != prev && ((*p)->on_cpu == 1 || (*p)->on_rq == 0 ||
				     (*p)->on_rq == TASK_ON_RQ_MIGRATING ||
				     task_cpu(*p) != cpu_of(rq))) {
		pr_err("picked %s(%d) on_cpu=%d on_rq=%d p->cpu=%d cpu_of(rq)=%d kthread=%d\n",
			(*p)->comm, (*p)->pid, (*p)->on_cpu,
			(*p)->on_rq, task_cpu(*p), cpu_of(rq), ((*p)->flags & PF_KTHREAD));
		BUG_ON(1);
	}
	/* We don't have vip tasks queued */
	if (list_empty(&uni_rq->vip_tasks) || (uni_rq->vip_tasks.next == NULL))
		return;

	list_for_each_entry_safe(uni_tsk, uni_tsk_tmp, &uni_rq->vip_tasks, vip_list) {
		/* Return the first task from vip queue */
		vip = unitsk_to_tsk(uni_tsk);

		if (unlikely(!test_vip_task(vip)))
			continue;

		*p = vip;
		*se = &vip->se;
		*repick = true;

		if (unlikely(simple)) {
			for_each_sched_entity((*se)) {
				/*
				 * TODO If CFS_BANDWIDTH is enabled, we might pick
				 * from a throttled cfs_rq
				 */
				cfs_rq = cfs_rq_of(*se);
				set_next_entity(cfs_rq, *se);
			}
		}

		if ((*p) && (*p) != prev && ((*p)->on_cpu == 1 || (*p)->on_rq == 0 ||
				     (*p)->on_rq == TASK_ON_RQ_MIGRATING ||
				     task_cpu(*p) != cpu_of(rq))) {
			pr_err("picked %s(%d) on_cpu=%d on_rq=%d p->cpu=%d cpu_of(rq)=%d kthread=%d\n",
			(*p)->comm, (*p)->pid, (*p)->on_cpu,
			(*p)->on_rq, task_cpu(*p), cpu_of(rq), ((*p)->flags & PF_KTHREAD));
			BUG_ON(1);
		}

		trace_cfs_vip_pick_next(vip, uni_tsk, cfs_vip_task_limit(vip));

		break;
	}
}

static void cfs_binder_vip_task_set(void *data, struct task_struct *task,
				bool sync, struct binder_proc *proc)
{
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *)task->android_vendor_data1;

	if (unlikely(uni_sched_disabled))
		return;

	if (is_fair_task(task) && test_vip_task(current)) {
		uni_tsk->vip_params |= SCHED_BINDER_TYPE;
		if (sched_custom_scene(SCENE_LAUNCH))
			uni_tsk->binder_from_cpu = smp_processor_id();
	} else if (uni_tsk->vip_params & SCHED_BINDER_TYPE) {
		uni_tsk->vip_params &= ~SCHED_BINDER_TYPE;
		uni_tsk->binder_from_cpu = -1;
	}
}

void check_parent_vip_status(struct task_struct *tsk)
{
	pid_t pid = tsk->pid, tgid = tsk->tgid;
	struct task_struct *tg_tsk;

	rcu_read_lock();

	if (uni_task_group_idx(tsk) != VIP_GROUP)
		goto unlock;

	if (pid != tgid) {
		tg_tsk = get_pid_task(find_vpid(tgid), PIDTYPE_PID);
		if (!tg_tsk)
			goto unlock;

		if (test_vip_task(tg_tsk) && uni_task_group_idx(tg_tsk) == VIP_GROUP) {
			rcu_read_unlock();
			if (cpu_notify_vip_fork_task(tgid, pid, tg_tsk->comm))
				pr_debug("cpu-netlink fail:vip-task:%s(%d) fork task(%d) to vip-group\n",
					  tg_tsk->comm, tgid, pid);
			rcu_read_lock();
		}
		put_task_struct(tg_tsk);
	}
unlock:
	rcu_read_unlock();
}

#else
static inline void cfs_check_preempt_wakeup(void *data, struct rq *rq, struct task_struct *p,
					  bool *preempt, bool *ignore, int wake_flags,
					  struct sched_entity *se, struct sched_entity *pse,
					  int next_buddy_marked, unsigned int granularity)
{ }
#endif

#define HEAVY_LOAD_SCALE       (80)
#define NS_TO_MS               1000000

static bool multi_thread_enable(void)
{
	return (sysctl_cpu_multi_thread_opt == 1) ? true : false;
}

static bool is_heavy_load_task(struct task_struct *p)
{
	int cpu;
	unsigned long thresh_load;
	struct reciprocal_value spc_rdiv = reciprocal_value(100);

	if (!sysctl_cpu_multi_thread_opt || !p)
		return false;

	for_each_cpu(cpu, cpu_active_mask) {
		struct rq *rq = cpu_rq(cpu);
		struct task_struct *p_curr = rq->curr;

		thresh_load = capacity_orig_of(cpu) * HEAVY_LOAD_SCALE;
		if (uclamp_task_util(p_curr) >= reciprocal_divide(thresh_load, spc_rdiv))
			continue;
		else
			return false;
	}
	return true;
}

static void check_preempt_tick_handler(void *data, struct task_struct *p,
				unsigned long *ideal_runtime, bool *skip_preempt,
				unsigned long delta_exec, struct cfs_rq *cfs_rq,
				struct sched_entity *curr, unsigned int granularity)
{
	if (unlikely(multi_thread_enable() && is_heavy_load_task(p)))
		*ideal_runtime = sysctl_multi_thread_heavy_load_runtime * NS_TO_MS;
}

static void sched_rebalance_domains_handler(void *data, struct rq *rq, int *continue_balancing)
{
	if (unlikely(multi_thread_enable() && is_heavy_load_task(rq->curr)))
		*continue_balancing = 0;
}

static void check_preempt_wakeup_handler(void *data, struct rq *rq, struct task_struct *p,
				bool *preempt, bool *ignore, int wake_flags,
				struct sched_entity *se, struct sched_entity *pse,
				int next_buddy_marked, unsigned int granularity)
{
	if (unlikely(multi_thread_enable() && is_heavy_load_task(rq->curr))) {
		*ignore = true;
		return;
	}

	cfs_check_preempt_wakeup(data, rq, p, preempt, ignore, wake_flags, se, pse,
				 next_buddy_marked, granularity);
}

#if IS_ENABLED(CONFIG_SCHED_WALT)
static inline unsigned long lb_cpu_util(int cpu)
{
	return walt_cpu_util(cpu);
}

static void uni_detach_task(struct task_struct *p, struct rq *src_rq, struct rq *dst_rq)
{
	walt_detach_task(p, src_rq, dst_rq);
}

static void uni_attach_task(struct rq *rq, struct task_struct *p)
{
	walt_attach_task(rq, p);
}
#else
static inline unsigned long lb_cpu_util(int cpu)
{
	struct cfs_rq *cfs_rq;
	unsigned int util;

	cfs_rq = &cpu_rq(cpu)->cfs;
	util = READ_ONCE(cfs_rq->avg.util_avg);

	if (sched_feat(UTIL_EST))
		util = max(util, READ_ONCE(cfs_rq->avg.util_est.enqueued));

	return min_t(unsigned long, util, capacity_orig_of(cpu));
}

static void uni_detach_task(struct task_struct *p, struct rq *src_rq, struct rq *dst_rq)
{
	lockdep_assert_rq_held(src_rq);

	deactivate_task(src_rq, p, 0);
	set_task_cpu(p, dst_rq->cpu);
}

static void uni_attach_task(struct rq *rq, struct task_struct *p)
{
	lockdep_assert_rq_held(rq);

	BUG_ON(task_rq(p) != rq);
	activate_task(rq, p, 0);
	check_preempt_curr(rq, p, 0);
}

#endif

static bool _can_migrate_task(struct task_struct *p, int dst_cpu)
{
	struct uni_rq *uni_rq = (struct uni_rq *) task_rq(p)->android_vendor_data1;

	if (cpu_halted(dst_cpu))
		return false;

	/* Don't detach task if it is under active migration */
	if (unlikely(uni_rq->push_task == p))
		return false;

	return true;
}

static int lb_pull_tasks(int dst_cpu, int src_cpu)
{
	struct rq *dst_rq = cpu_rq(dst_cpu);
	struct rq *src_rq = cpu_rq(src_cpu);
	unsigned long flags;
	struct task_struct *pulled_task = NULL, *p;
	bool to_lower;
	struct uni_rq *src_urq = (struct uni_rq *) src_rq->android_vendor_data1;
	struct uni_rq *dst_urq = (struct uni_rq *) cpu_rq(dst_cpu)->android_vendor_data1;
	struct task_struct *pull_me;
	int task_visited;

	if (unlikely(src_cpu == dst_cpu))
		return 0;

	to_lower = dst_urq->cluster->id < src_urq->cluster->id;

	raw_spin_lock_irqsave(&src_rq->__lock, flags);

	pull_me = NULL;
	task_visited = 0;
	list_for_each_entry_reverse(p, &src_rq->cfs_tasks, se.group_node) {
		if (!cpumask_test_cpu(dst_cpu, p->cpus_ptr))
			continue;

		if (task_running(src_rq, p))
			continue;

		if (!_can_migrate_task(p, dst_cpu))
			continue;

		/* do not pull vip-task when launch */
		if (sched_custom_scene(SCENE_LAUNCH) && test_vip_task(p) &&
		    num_vip_tasks_of_cpu(src_cpu) < 3)
			continue;

		if (pull_me == NULL) {
			pull_me = p;
		} else {
			if (to_lower) {
				if (task_util_est(p) < task_util_est(pull_me))
					pull_me = p;
			} else if (task_util_est(p) > task_util_est(pull_me)) {
				pull_me = p;
			}
		}

		task_visited++;
		if (task_visited > 5)
			break;
	}
	if (pull_me) {
		uni_detach_task(pull_me, src_rq, dst_rq);
		pulled_task = pull_me;
		goto unlock;
	}
unlock:
	/* lock must be dropped before waking the stopper */
	raw_spin_unlock_irqrestore(&src_rq->__lock, flags);

	if (!pulled_task)
		return 0;

	raw_spin_lock_irqsave(&dst_rq->__lock, flags);
	uni_attach_task(dst_rq, pulled_task);
	raw_spin_unlock_irqrestore(&dst_rq->__lock, flags);

	return 1; /* we pulled 1 task */
}
static int lb_find_busiest_from_similar_cap_cpu(int dst_cpu, const cpumask_t *src_mask,
						int *has_misfit, bool is_newidle)
{
	int i, busiest_cpu = -1;
	unsigned int nr_running, most_nr_running = 0;
	unsigned long util, busiest_util = 0;
	struct uni_rq *uni_rq;

	for_each_cpu(i, src_mask) {
		uni_rq = (struct uni_rq *) cpu_rq(i)->android_vendor_data1;
		nr_running = cpu_rq(i)->nr_running;

		if (nr_running < 2 || !cpu_rq(i)->cfs.h_nr_running)
			continue;

		if (sched_custom_scene(SCENE_LAUNCH)) {
			int vip_running = num_vip_tasks_of_cpu(i);

			if (nr_running == vip_running && vip_running < 3)
				continue;
		}

		util = lb_cpu_util(i);
		if (util < busiest_util)
			continue;

		if (util == busiest_util && nr_running < most_nr_running)
			continue;

		most_nr_running = nr_running;
		busiest_util = util;
		busiest_cpu = i;
	}

	return busiest_cpu;
}

static int lb_find_busiest_from_lower_cap_cpu(int dst_cpu, const cpumask_t *src_mask,
						int *has_misfit, bool is_newidle)
{
	return -1;
}

static int lb_find_busiest_from_higher_cap_cpu(int dst_cpu, const cpumask_t *src_mask,
						int *has_misfit, bool is_newidle)
{
	return -1;
}

static int lb_find_busiest_cpu(int dst_cpu, const cpumask_t *src_mask,
				int *has_misfit, bool is_newidle)
{
	int fsrc_cpu = cpumask_first(src_mask);
	int busiest_cpu = -1;
	struct uni_rq *fsrc_wrq = (struct uni_rq *) cpu_rq(fsrc_cpu)->android_vendor_data1;
	struct uni_rq *dst_wrq = (struct uni_rq *) cpu_rq(dst_cpu)->android_vendor_data1;

	if (dst_wrq->cluster->id == fsrc_wrq->cluster->id)
		busiest_cpu = lb_find_busiest_from_similar_cap_cpu(dst_cpu, src_mask,
								   has_misfit, is_newidle);
	else if (dst_wrq->cluster->id > fsrc_wrq->cluster->id)
		busiest_cpu = lb_find_busiest_from_lower_cap_cpu(dst_cpu, src_mask,
								 has_misfit, is_newidle);
	else
		busiest_cpu = lb_find_busiest_from_higher_cap_cpu(dst_cpu, src_mask,
								  has_misfit, is_newidle);

	return busiest_cpu;
}

/* similar to sysctl_sched_migration_cost */
#define NEWIDLE_BALANCE_THRESHOLD	500000
static void android_rvh_sched_newidle_balance(void *unused, struct rq *this_rq,
					      struct rq_flags *rf, int *pulled_task, int *done)
{
	int this_cpu = this_rq->cpu;
	bool enough_idle = (this_rq->avg_idle >= NEWIDLE_BALANCE_THRESHOLD);
	int has_misfit = 0;
	struct uni_rq *uni_this_rq = (struct uni_rq *) this_rq->android_vendor_data1;
	struct sched_cluster *this_cluster = uni_this_rq->cluster;
	int busy_cpu = -1;
	bool pull_success = false;

	if (unlikely(uni_sched_disabled))
		return;

	/*
	 * newly idle load balance is completely handled here, so
	 * set done to skip the load balance by the caller.
	 */
	*done = 1;
	*pulled_task = 0;

	/*
	 * This CPU is about to enter idle, so clear the
	 * misfit_task_load and mark the idle stamp.
	 */
	if (num_sched_clusters > 1)
		this_rq->misfit_task_load = 0;

	/*
	 * There is a task waiting to run. No need to search for one.
	 * Return 0; the task will be enqueued when switching to idle.
	 */
	if (this_rq->ttwu_pending)
		return;

	this_rq->idle_stamp = rq_clock(this_rq);

	if ((!cpu_active(this_cpu)) || cpu_halted(this_cpu) || is_reserved(this_cpu))
		return;

	if (!sysctl_force_newidle_balance) {
		*done = 0;
		return;
	}

	rq_unpin_lock(this_rq, rf);

	if (atomic_read(&this_rq->nr_iowait) && !enough_idle)
		goto repin;

	if (!READ_ONCE(this_rq->rd->overload))
		goto repin;

	raw_spin_rq_unlock(this_rq);

	busy_cpu = lb_find_busiest_cpu(this_cpu, &this_cluster->cpus, &has_misfit, true);
	if (busy_cpu == -1 && num_sched_clusters != 1)
		goto unlock;

	/* set true first */
	pull_success = true;

	/* sanity checks before attempting the pull */
	if (this_rq->nr_running > 0 || (busy_cpu == this_cpu) || busy_cpu == -1)
		goto unlock;

	*pulled_task = lb_pull_tasks(this_cpu, busy_cpu);
	if (*pulled_task == 0)
		pull_success = false;
unlock:
	raw_spin_rq_lock(this_rq);

	if (this_rq->cfs.h_nr_running && !*pulled_task)
		*pulled_task = 1;

	/* Is there a task of a high priority class? */
	if (this_rq->nr_running != this_rq->cfs.h_nr_running)
		*pulled_task = -1;

	/* reset the idle time stamp if we pulled any task */
	if (*pulled_task)
		this_rq->idle_stamp = 0;

	if ((!pull_success) && (*pulled_task == 0))
		*done = 0;

repin:
	rq_repin_lock(this_rq, rf);

	trace_sched_uni_newidle_balance(this_cpu, busy_cpu, *pulled_task, 0, enough_idle);
}

static void android_rvh_can_migrate_task(void *data, struct task_struct *p,
				  int dst_cpu, int *can_migrate)
{
	if (unlikely(uni_sched_disabled))
		return;

	*can_migrate = _can_migrate_task(p, dst_cpu);
}

void fair_init(void)
{
	register_trace_android_rvh_sched_rebalance_domains(sched_rebalance_domains_handler, NULL);
	register_trace_android_rvh_check_preempt_tick(check_preempt_tick_handler, NULL);
	register_trace_android_rvh_check_preempt_wakeup(check_preempt_wakeup_handler, NULL);
	register_trace_android_rvh_sched_newidle_balance(android_rvh_sched_newidle_balance, NULL);
	register_trace_android_rvh_can_migrate_task(android_rvh_can_migrate_task, NULL);
#ifdef CONFIG_UNISOC_SCHED_VIP_TASK
	register_trace_android_vh_scheduler_tick(cfs_vip_scheduler_tick, NULL);
	register_trace_android_rvh_replace_next_task_fair(cfs_replace_next_task_fair, NULL);
	register_trace_android_vh_binder_wakeup_ilocked(cfs_binder_vip_task_set, NULL);
#endif
#if IS_ENABLED(CONFIG_SCHED_WALT)
	register_trace_android_rvh_update_misfit_status(android_rvh_update_misfit_status, NULL);
	register_trace_android_rvh_cpu_overutilized(walt_cpu_overutilzed, NULL);
	register_trace_android_vh_scheduler_tick(android_vh_scheduler_tick, NULL);
	register_trace_android_rvh_migrate_queued_task(walt_migrate_queued_task, NULL);
	register_trace_android_rvh_find_new_ilb(walt_find_new_ilb, NULL);
	register_trace_android_rvh_sched_nohz_balancer_kick(walt_nohz_balancer_kick, NULL);
	register_trace_android_rvh_find_busiest_group(walt_find_busiest_group, NULL);
	register_trace_android_rvh_select_task_rq_fair(walt_select_task_rq_fair, NULL);
	register_trace_android_rvh_place_entity(android_rvh_place_entity, NULL);

	rotation_task_init();
#endif
}
