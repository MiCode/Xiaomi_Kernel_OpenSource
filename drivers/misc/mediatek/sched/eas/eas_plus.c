// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/module.h>
#include <sched/sched.h>
#include "common.h"
#include "eas_plus.h"
#include "eas_trace.h"
#include <linux/sort.h>
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
#include <thermal_interface.h>
#endif

MODULE_LICENSE("GPL");

#define IB_ASYM_MISFIT		(0x02)
#define IB_SAME_CLUSTER		(0x01)
#define IB_OVERUTILIZATION	(0x04)

static struct perf_domain *find_pd(struct perf_domain *pd, int cpu)
{
	while (pd) {
		if (cpumask_test_cpu(cpu, perf_domain_span(pd)))
			return pd;

		pd = pd->next;
	}

	return NULL;
}

static inline bool check_faster_idle_balance(struct sched_group *busiest, struct rq *dst_rq)
{

	int src_cpu = group_first_cpu(busiest);
	int dst_cpu = cpu_of(dst_rq);
	int cpu;

	if (capacity_orig_of(dst_cpu) <= capacity_orig_of(src_cpu))
		return false;

	for_each_cpu(cpu, sched_group_span(busiest)) {
		if (cpu_rq(cpu)->misfit_task_load)
			return true;
	}

	return false;
}

static inline bool check_has_overutilize_cpu(struct cpumask *grp)
{

	int cpu;

	for_each_cpu(cpu, grp) {
		if (cpu_rq(cpu)->nr_running >= 2 &&
			!fits_capacity(cpu_util(cpu), capacity_of(cpu)))
			return true;
	}
	return false;
}

void mtk_find_busiest_group(void *data, struct sched_group *busiest,
		struct rq *dst_rq, int *out_balance)
{
	int src_cpu = -1;

	if (busiest) {
		struct perf_domain *pd = NULL;
		int dst_cpu = dst_rq->cpu;
		int fbg_reason = 0;

		pd = rcu_dereference(dst_rq->rd->pd);
		pd = find_pd(pd, dst_cpu);
		if (!pd)
			return;

		src_cpu = group_first_cpu(busiest);

		/*
		 *  1.same cluster
		 *  2.not same cluster but dst_cpu has a higher capacity and
		 *    busiest group has misfit task. The purpose of this condition
		 *    is trying to let misfit task goto hiehger cpu.
		 */
		if (cpumask_test_cpu(src_cpu, perf_domain_span(pd))) {
			*out_balance = 0;
			fbg_reason |= IB_SAME_CLUSTER;
		} else if (check_faster_idle_balance(busiest, dst_rq)) {
			*out_balance = 0;
			fbg_reason |= IB_ASYM_MISFIT;
		} else if (check_has_overutilize_cpu(sched_group_span(busiest))) {
			*out_balance = 0;
			fbg_reason |= IB_OVERUTILIZATION;
		}

		trace_sched_find_busiest_group(src_cpu, dst_cpu, *out_balance, fbg_reason);
	}
}

void mtk_cpu_overutilized(void *data, int cpu, int *overutilized)
{
	struct perf_domain *pd = NULL;
	struct rq *rq = cpu_rq(cpu);
	unsigned long sum_util = 0, sum_cap = 0;
	int i = 0;

	rcu_read_lock();
	pd = rcu_dereference(rq->rd->pd);
	pd = find_pd(pd, cpu);
	if (!pd) {
		rcu_read_unlock();
		return;
	}

	if (cpumask_weight(perf_domain_span(pd)) == 1 &&
		capacity_orig_of(cpu) == SCHED_CAPACITY_SCALE) {
		*overutilized = 0;
		rcu_read_unlock();
		return;
	}

	for_each_cpu(i, perf_domain_span(pd)) {
		sum_util += cpu_util(i);
		sum_cap += capacity_of(i);
	}


	*overutilized = !fits_capacity(sum_util, sum_cap);
	trace_sched_cpu_overutilized(cpu, perf_domain_span(pd), sum_util, sum_cap, *overutilized);

	rcu_read_unlock();
}

#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
int __read_mostly thermal_headroom[NR_CPUS]  ____cacheline_aligned;
unsigned long next_update_thermal;
static DEFINE_SPINLOCK(thermal_headroom_lock);
static void update_thermal_headroom(int this_cpu)
{
	int cpu;

	if (spin_trylock(&thermal_headroom_lock)) {
		if (time_before(jiffies, next_update_thermal)) {
			spin_unlock(&thermal_headroom_lock);
			return;
		}

		next_update_thermal = jiffies + thermal_headroom_interval_tick;
		for_each_cpu(cpu, cpu_possible_mask) {
			thermal_headroom[cpu] = get_thermal_headroom(cpu);
		}
		spin_unlock(&thermal_headroom_lock);
	}

	trace_sched_next_update_thermal_headroom(jiffies, next_update_thermal);
}

int sort_thermal_headroom(struct cpumask *cpus, int *cpu_order)
{
	int i, j, cpu, cnt = 0;
	int headroom_order[NR_CPUS] ____cacheline_aligned;

	if (cpumask_weight(cpus) == 1) {
		cpu = cpumask_first(cpus);
		*cpu_order = cpu;

		return 1;
	}

	spin_lock(&thermal_headroom_lock);
	for_each_cpu_and(cpu, cpus, cpu_active_mask) {
		int headroom;

		headroom = thermal_headroom[cpu];

		for (i = 0; i < cnt; i++) {
			if (headroom > headroom_order[i])
				break;
		}

		for (j = cnt; j >= i; j--) {
			headroom_order[j+1] = headroom_order[j];
			cpu_order[j+1] = cpu_order[j];
		}

		headroom_order[i] = headroom;
		cpu_order[i] = cpu;
		cnt++;
	}
	spin_unlock(&thermal_headroom_lock);

	return cnt;
}

#endif

/**
 * em_cpu_energy() - Estimates the energy consumed by the CPUs of a
		performance domain
 * @pd		: performance domain for which energy has to be estimated
 * @max_util	: highest utilization among CPUs of the domain
 * @sum_util	: sum of the utilization of all CPUs in the domain
 *
 * This function must be used only for CPU devices. There is no validation,
 * i.e. if the EM is a CPU type and has cpumask allocated. It is called from
 * the scheduler code quite frequently and that is why there is not checks.
 *
 * Return: the sum of the energy consumed by the CPUs of the domain assuming
 * a capacity state satisfying the max utilization of the domain.
 */
unsigned long mtk_em_cpu_energy(struct em_perf_domain *pd,
		unsigned long max_util, unsigned long sum_util, unsigned int *cpu_temp)
{
	unsigned long freq, scale_cpu;
	struct em_perf_state *ps;
	int i, cpu, opp = -1;
	unsigned long dyn_pwr = 0, static_pwr = 0;
	unsigned long energy;

	if (!sum_util) {
		return 0;
	}

	/*
	 * In order to predict the performance state, map the utilization of
	 * the most utilized CPU of the performance domain to a requested
	 * frequency, like schedutil.
	 */
	cpu = cpumask_first(to_cpumask(pd->cpus));
	scale_cpu = arch_scale_cpu_capacity(cpu);
	ps = &pd->table[pd->nr_perf_states - 1];
#if IS_ENABLED(CONFIG_NONLINEAR_FREQ_CTL)
	mtk_map_util_freq(NULL, max_util, ps->frequency, to_cpumask(pd->cpus), &freq);
#else
	freq = map_util_freq(max_util, ps->frequency, scale_cpu);
#endif
	freq = max(freq, per_cpu(min_freq, cpu));

	/*
	 * Find the lowest performance state of the Energy Model above the
	 * requested frequency.
	 */
	for (i = 0; i < pd->nr_perf_states; i++) {
		ps = &pd->table[i];
		if (ps->frequency >= freq)
			break;
	}

#if IS_ENABLED(CONFIG_MTK_LEAKAGE_AWARE_TEMP)
	i = min(i, pd->nr_perf_states - 1);
	opp = pd->nr_perf_states - i - 1;

	for_each_cpu_and(cpu, to_cpumask(pd->cpus), cpu_online_mask) {
		unsigned int cpu_static_pwr;

		cpu_static_pwr = mtk_get_leakage(cpu, opp, cpu_temp[cpu]);
		static_pwr += cpu_static_pwr;

		trace_sched_leakage(cpu, opp, cpu_temp[cpu], cpu_static_pwr, static_pwr);
	}
#endif

	/*
	 * The capacity of a CPU in the domain at the performance state (ps)
	 * can be computed as:
	 *
	 *             ps->freq * scale_cpu
	 *   ps->cap = --------------------                          (1)
	 *                 cpu_max_freq
	 *
	 * So, ignoring the costs of idle states (which are not available in
	 * the EM), the energy consumed by this CPU at that performance state
	 * is estimated as:
	 *
	 *             ps->power * cpu_util
	 *   cpu_nrg = --------------------                          (2)
	 *                   ps->cap
	 *
	 * since 'cpu_util / ps->cap' represents its percentage of busy time.
	 *
	 *   NOTE: Although the result of this computation actually is in
	 *         units of power, it can be manipulated as an energy value
	 *         over a scheduling period, since it is assumed to be
	 *         constant during that interval.
	 *
	 * By injecting (1) in (2), 'cpu_nrg' can be re-expressed as a product
	 * of two terms:
	 *
	 *             ps->power * cpu_max_freq   cpu_util
	 *   cpu_nrg = ------------------------ * ---------          (3)
	 *                    ps->freq            scale_cpu
	 *
	 * The first term is static, and is stored in the em_perf_state struct
	 * as 'ps->cost'.
	 *
	 * Since all CPUs of the domain have the same micro-architecture, they
	 * share the same 'ps->cost', and the same CPU capacity. Hence, the
	 * total energy of the domain (which is the simple sum of the energy of
	 * all of its CPUs) can be factorized as:
	 *
	 *            ps->cost * \Sum cpu_util
	 *   pd_nrg = ------------------------                       (4)
	 *                  scale_cpu
	 */

	dyn_pwr = (ps->cost * sum_util / scale_cpu);
	energy = dyn_pwr + static_pwr;

	trace_sched_em_cpu_energy(opp, freq, ps->cost, scale_cpu, dyn_pwr, static_pwr);

	return energy;
}

#define CSRAM_BASE 0x0011BC00
#define OFFS_THERMAL_LIMIT_S 0x1208
#define THERMAL_INFO_SIZE 200

static void __iomem *sram_base_addr;
int init_sram_info(void)
{
	sram_base_addr =
		ioremap(CSRAM_BASE + OFFS_THERMAL_LIMIT_S, THERMAL_INFO_SIZE);

	if (!sram_base_addr) {
		pr_info("Remap thermal info failed\n");

		return -EIO;
	}

	return 0;
}

void mtk_tick_entry(void *data, struct rq *rq)
{
	void __iomem *base = sram_base_addr;
	struct em_perf_domain *pd;
	int this_cpu, gear_id, opp_idx, offset;
	unsigned int freq_thermal;
	unsigned long max_capacity, capacity;
	u32 opp_ceiling;

	this_cpu = cpu_of(rq);
#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
	update_thermal_headroom(this_cpu);
#endif

	pd = em_cpu_get(this_cpu);
	if (!pd)
		return;

	if (this_cpu != cpumask_first(to_cpumask(pd->cpus)))
		return;

	gear_id = topology_physical_package_id(this_cpu);
	offset = gear_id << 2;

	opp_ceiling = ioread32(base + offset);
	opp_idx = pd->nr_perf_states - opp_ceiling - 1;
	freq_thermal = pd->table[opp_idx].frequency;

	max_capacity = arch_scale_cpu_capacity(this_cpu);
	capacity = freq_thermal * max_capacity;
	capacity /= pd->table[pd->nr_perf_states-1].frequency;
	arch_set_thermal_pressure(to_cpumask(pd->cpus), max_capacity - capacity);

	trace_sched_frequency_limits(this_cpu, freq_thermal);
}

/*
 * Enable/Disable honoring sync flag in energy-aware wakeups
 */
unsigned int sched_sync_hint_enable = 1;
void set_wake_sync(unsigned int sync)
{
	sched_sync_hint_enable = sync;
}
EXPORT_SYMBOL_GPL(set_wake_sync);

unsigned int get_wake_sync(void)
{
	return sched_sync_hint_enable;
}
EXPORT_SYMBOL_GPL(get_wake_sync);

void mtk_set_wake_flags(void *data, int *wake_flags, unsigned int *mode)
{
	if (!sched_sync_hint_enable)
		*wake_flags &= ~WF_SYNC;
}

unsigned int new_idle_balance_interval_ns  =  1000000;
unsigned int thermal_headroom_interval_tick =  1;

void set_newly_idle_balance_interval_us(unsigned int interval_us)
{
	new_idle_balance_interval_ns = interval_us * 1000;

	trace_sched_newly_idle_balance_interval(interval_us);
}
EXPORT_SYMBOL_GPL(set_newly_idle_balance_interval_us);

unsigned int get_newly_idle_balance_interval_us(void)
{
	return new_idle_balance_interval_ns / 1000;
}
EXPORT_SYMBOL_GPL(get_newly_idle_balance_interval_us);

void set_get_thermal_headroom_interval_tick(unsigned int tick)
{
	thermal_headroom_interval_tick = tick;

	trace_sched_headroom_interval_tick(tick);
}
EXPORT_SYMBOL_GPL(set_get_thermal_headroom_interval_tick);

unsigned int get_thermal_headroom_interval_tick(void)
{
	return thermal_headroom_interval_tick;
}
EXPORT_SYMBOL_GPL(get_thermal_headroom_interval_tick);

static DEFINE_RAW_SPINLOCK(migration_lock);

int select_idle_cpu_from_domains(struct task_struct *p,
					struct perf_domain **prefer_pds, int len)
{
	int i = 0;
	struct perf_domain *pd;
	int cpu, best_cpu = -1;

	for (; i < len; i++) {
		pd = prefer_pds[i];
		for_each_cpu_and(cpu, perf_domain_span(pd),
						cpu_active_mask) {
			if (!cpumask_test_cpu(cpu, p->cpus_ptr))
				continue;
			if (idle_cpu(cpu)) {
				best_cpu = cpu;
				break;
			}
		}
		if (best_cpu != -1)
			break;
	}

	return best_cpu;
}

int select_bigger_idle_cpu(struct task_struct *p)
{
	struct root_domain *rd = cpu_rq(smp_processor_id())->rd;
	struct perf_domain *pd, *prefer_pds[NR_CPUS];
	int cpu = task_cpu(p), bigger_idle_cpu = -1;
	int i = 0;
	long max_capacity = capacity_orig_of(cpu);
	long capacity;

	rcu_read_lock();
	pd = rcu_dereference(rd->pd);

	for (; pd; pd = pd->next) {
		capacity = capacity_orig_of(cpumask_first(perf_domain_span(pd)));
		if (capacity > max_capacity &&
			cpumask_intersects(p->cpus_ptr, perf_domain_span(pd))) {
			prefer_pds[i++] = pd;
		}
	}

	if (i != 0)
		bigger_idle_cpu = select_idle_cpu_from_domains(p, prefer_pds, i);

	rcu_read_unlock();
	return bigger_idle_cpu;
}

void check_for_migration(struct task_struct *p)
{
	int new_cpu = -1, better_idle_cpu = -1;
	int cpu = task_cpu(p);
	struct rq *rq = cpu_rq(cpu);

	if (rq->misfit_task_load) {
		struct em_perf_domain *pd;
		struct cpufreq_policy *policy;
		int opp_curr = 0, thre = 0, thre_idx = 0;

		if (rq->curr->state != TASK_RUNNING ||
			rq->curr->nr_cpus_allowed == 1)
			return;

		pd = em_cpu_get(cpu);
		if (!pd)
			return;
		thre_idx = (pd->nr_perf_states >> 3) - 1;
		if (thre_idx >= 0)
			thre = pd->table[thre_idx].frequency;

		policy = cpufreq_cpu_get(cpu);
		if (policy) {
			opp_curr = policy->cur;
			cpufreq_cpu_put(policy);
		}

		if (opp_curr <= thre)
			return;

		raw_spin_lock(&migration_lock);
		rcu_read_lock();
		new_cpu = p->sched_class->select_task_rq(p, cpu, SD_BALANCE_WAKE, 0);
		rcu_read_unlock();

		if ((new_cpu < 0) ||
			(capacity_orig_of(new_cpu) <= capacity_orig_of(cpu)))
			better_idle_cpu = select_bigger_idle_cpu(p);
		if (better_idle_cpu >= 0)
			new_cpu = better_idle_cpu;

		if (new_cpu < 0) {
			raw_spin_unlock(&migration_lock);
			return;
		}
		if ((better_idle_cpu >= 0) ||
			(capacity_orig_of(new_cpu) > capacity_orig_of(cpu))) {
			raw_spin_unlock(&migration_lock);
			migrate_running_task(new_cpu, p, rq, MIGR_TICK_PULL_MISFIT_RUNNING);
		} else {
#if IS_ENABLED(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)
			int thre_rot = 0, thre_rot_idx = 0;

			thre_rot_idx = (pd->nr_perf_states >> 1) - 1;
			if (thre_rot_idx >= 0)
				thre_rot = pd->table[thre_rot_idx].frequency;

			if (opp_curr > thre_rot)
				task_check_for_rotation(rq);

#endif
			raw_spin_unlock(&migration_lock);
		}
	}
}

void hook_scheduler_tick(void *data, struct rq *rq)
{
	if (rq->curr->policy == SCHED_NORMAL)
		check_for_migration(rq->curr);
}

void mtk_hook_after_enqueue_task(void *data, struct rq *rq,
				struct task_struct *p)
{
	struct update_util_data *fdata;
	bool should_update = false;

#if IS_ENABLED(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)
	rotat_after_enqueue_task(data, rq, p);
#endif

#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
	if (rq->nr_running != 1)
		return;

	fdata = rcu_dereference_sched(*per_cpu_ptr(&cpufreq_update_util_data,
							  cpu_of(rq)));

	if (fdata) {
		should_update = !check_freq_update_for_time(fdata, rq_clock(rq));
		if (should_update)
			fdata->func(fdata, rq_clock(rq), 0);
	}
#endif
}

#if IS_ENABLED(CONFIG_UCLAMP_TASK)
/*
 * Verify the fitness of task @p to run on @cpu taking into account the uclamp
 * settings.
 *
 * This check is only important for heterogeneous systems where uclamp_min value
 * is higher than the capacity of a @cpu. For non-heterogeneous system this
 * function will always return true.
 *
 * The function will return true if the capacity of the @cpu is >= the
 * uclamp_min and false otherwise.
 *
 * Note that uclamp_min will be clamped to uclamp_max if uclamp_min
 * > uclamp_max.
 */
static inline bool rt_task_fits_capacity(struct task_struct *p, int cpu)
{
	unsigned int min_cap;
	unsigned int max_cap;
	unsigned int cpu_cap;

	/* Only heterogeneous systems can benefit from this check */
	if (!likely(mtk_sched_asym_cpucapacity))
		return true;

	min_cap = uclamp_eff_value(p, UCLAMP_MIN);
	max_cap = uclamp_eff_value(p, UCLAMP_MAX);

	cpu_cap = capacity_orig_of(cpu);

	return cpu_cap >= min(min_cap, max_cap);
}
#else
static inline bool rt_task_fits_capacity(struct task_struct *p, int cpu)
{
	return true;
}
#endif

#if IS_ENABLED(CONFIG_SMP)
static inline bool should_honor_rt_sync(struct rq *rq, struct task_struct *p,
					bool sync)
{
	/*
	 * If the waker is CFS, then an RT sync wakeup would preempt the waker
	 * and force it to run for a likely small time after the RT wakee is
	 * done. So, only honor RT sync wakeups from RT wakers.
	 */
	return sync && task_has_rt_policy(rq->curr) &&
		p->prio <= rq->rt.highest_prio.next &&
		rq->rt.rt_nr_running <= 2;
}
#else
static inline bool should_honor_rt_sync(struct rq *rq, struct task_struct *p,
					bool sync)
{
	return 0;
}
#endif

void mtk_select_task_rq_rt(void *data, struct task_struct *p, int source_cpu,
				int sd_flag, int flags, int *target_cpu)
{
	struct task_struct *curr;
	struct rq *rq;
	struct rq *this_cpu_rq;
	int lowest_cpu = -1;
	int lowest_prio = 0;
	int cpu;
	int select_reason = -1;
	bool sync = !!(flags & WF_SYNC);
	int this_cpu;

	*target_cpu = -1;
	/* For anything but wake ups, just return the task_cpu */
	if (sd_flag != SD_BALANCE_WAKE && sd_flag != SD_BALANCE_FORK) {
		select_reason = LB_RT_FAIL;
		goto out;
	}

	rcu_read_lock();
	this_cpu = smp_processor_id();
	this_cpu_rq = cpu_rq(this_cpu);
	/*
	 * Respect the sync flag as long as the task can run on this CPU.
	 */
	if (should_honor_rt_sync(this_cpu_rq, p, sync) &&
			cpumask_test_cpu(this_cpu, p->cpus_ptr)) {
		*target_cpu = this_cpu;
		select_reason = LB_RT_SYNC;
		goto out_unlock;
	}

	for_each_cpu_and(cpu, p->cpus_ptr,
			cpu_active_mask) {
		if (idle_cpu(cpu) && rt_task_fits_capacity(p, cpu)) {
			*target_cpu = cpu;
			select_reason = LB_RT_IDLE;
			break;
		}
		rq = cpu_rq(cpu);
		curr = rq->curr;
		if (curr && (curr->policy == SCHED_NORMAL)
				&& (curr->prio > lowest_prio)
				&& (!task_may_not_preempt(curr, cpu))
				&& (rt_task_fits_capacity(p, cpu))) {
			lowest_prio = curr->prio;
			lowest_cpu = cpu;
		}
	}

	if (-1 == *target_cpu) {
		*target_cpu =  lowest_cpu;
		select_reason = LB_RT_LOWEST_PRIO;
	}

out_unlock:
	rcu_read_unlock();
out:

	trace_sched_select_task_rq_rt(p, select_reason, *target_cpu, sd_flag, sync);
}

