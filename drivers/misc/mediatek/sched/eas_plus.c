// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/module.h>
#include "eas_plus.h"
#include <sched_trace.h>
#include <linux/sort.h>
#include "../../../../drivers/thermal/mediatek/thermal_interface.h"

MODULE_LICENSE("GPL");

static struct perf_domain *find_pd(struct perf_domain *pd, int cpu)
{
	while (pd) {
		if (cpumask_test_cpu(cpu, perf_domain_span(pd))){
			return pd;
		}
		pd = pd->next;
	}

	return NULL;
}

void mtk_find_busiest_group(void *data, struct sched_group *busiest,
		struct rq *dst_rq, int *out_balance)
{
	int src_cpu = -1;

	if (busiest) {
			struct perf_domain *pd = NULL;
			int dst_cpu = dst_rq->cpu;

			pd = rcu_dereference(dst_rq->rd->pd);
			pd = find_pd(pd, dst_cpu);
			if (!pd)
				return;

			src_cpu = group_first_cpu(busiest);

			if (cpumask_test_cpu(src_cpu, perf_domain_span(pd)))
				*out_balance = 0;
	}
}

#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)

struct thermal_struct{
	int cpu_id;
	int headroom;
};

static int cmp(const void *a, const void *b)
{

	const struct thermal_struct *a1=a;
	const struct thermal_struct *b1=b;

	return b1->headroom - a1->headroom;
}

int sort_thermal_headroom(struct cpumask *cpus, int *cpu_order)
{
	int i, cpu, cnt=0;
	struct thermal_struct thermal_order[NR_CPUS];

	for_each_cpu_and(cpu, cpus, cpu_online_mask) {
		thermal_order[cnt].cpu_id = cpu;
		thermal_order[cnt].headroom = get_thermal_headroom(cpu);
		cnt++;
	}

	sort(thermal_order, cnt, sizeof(struct thermal_struct), cmp, NULL);

	for(i = 0; i < cnt; i++) {
		*cpu_order++ = thermal_order[i].cpu_id;
	}

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
void mtk_em_cpu_energy(void *data, struct em_perf_domain *pd,
		unsigned long max_util, unsigned long sum_util, unsigned long *energy)
{
	unsigned long freq, scale_cpu;
	struct em_perf_state *ps;
	int i, cpu, opp = -1;
	unsigned long dyn_pwr = 0, static_pwr = 0;

	if (!sum_util) {
		energy = 0;
		return;
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
	mtk_map_util_freq(NULL, max_util, ps->frequency, scale_cpu, &freq);
#else
	freq = map_util_freq(max_util, ps->frequency, scale_cpu);
#endif


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
	opp = pd->nr_perf_states - i -1;

	for_each_cpu_and(cpu, to_cpumask(pd->cpus), cpu_online_mask) {
		unsigned int temp;
		unsigned long cpu_static_pwr;

		temp = get_cpu_temp(cpu);
		temp /= 1000;

		cpu_static_pwr = mtk_get_leakage(cpu, opp, temp);
		static_pwr += cpu_static_pwr >> 10;

		trace_sched_leakage(cpu, opp, temp, cpu_static_pwr, static_pwr);
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

	dyn_pwr = (ps->cost * sum_util/ scale_cpu);
	*energy = dyn_pwr + static_pwr;

	trace_sched_em_cpu_energy(opp, freq, ps->cost, scale_cpu, dyn_pwr, static_pwr);
}

#define CSRAM_BASE 0x0011BC00
#define OFFS_THERMAL_LIMIT_S 0x1208
#define THERMAL_INFO_SIZE 200
static void __iomem *sram_base_addr;
int init_sram_info(void)
{
	sram_base_addr = ioremap(CSRAM_BASE + OFFS_THERMAL_LIMIT_S, THERMAL_INFO_SIZE);

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
	unsigned int freq_thermal, freq_max, freq_ceiling;
	unsigned long max_capacity, capacity;
	u32 opp_ceiling;
	struct cpufreq_policy *policy;

	this_cpu = cpu_of(rq);
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

	policy = cpufreq_cpu_get(this_cpu);
	if (!policy)
		freq_max = pd->table[pd->nr_perf_states-1].frequency;
	else {
		freq_max = policy->max;
		cpufreq_cpu_put(policy);
	}

	freq_ceiling = min(freq_thermal, freq_max);
	max_capacity = arch_scale_cpu_capacity(this_cpu);
	capacity = freq_ceiling * max_capacity;
	capacity /= pd->table[pd->nr_perf_states-1].frequency;
	arch_set_thermal_pressure(to_cpumask(pd->cpus), max_capacity - capacity);

	trace_sched_frequency_limits(this_cpu, freq_thermal, freq_max, freq_ceiling);
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


