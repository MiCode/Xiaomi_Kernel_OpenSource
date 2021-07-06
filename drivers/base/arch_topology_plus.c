//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "../../drivers/misc/mediatek/base/power/include/mtk_upower.h"
#include "../../drivers/misc/mediatek/include/mt-plat/mtk_cpufreq_common_api.h"

static inline unsigned long get_freq_capacity(int cpu, unsigned long freq)
{
	int i;
	int tmp;
	int idx = -1;
	struct upower_tbl *tbl;

	tbl = upower_get_core_tbl(cpu);
	for (i = 0; i < tbl->row_num ; i++) {
		tmp = mt_cpufreq_get_cpu_freq(cpu, i);
		if (unlikely(!tmp)) {
			WARN(1, "frequency not in opp table!\n");
			idx = tbl->row_num - 1;
			break;
		}
		if (tmp >= freq) {
			idx = i;
			break;
		}
	}

	if (idx < 0)
		idx = tbl->row_num - 1;

	return tbl->row[idx].cap;
}

static inline unsigned long get_freq_capacity_lt(int cpu, unsigned long freq)
{
	int i;
	int tmp;
	int idx = -1;
	struct upower_tbl *tbl;

	tbl = upower_get_core_tbl(cpu);
	for (i = tbl->row_num - 1; i >= 0; i--) {
		tmp = mt_cpufreq_get_cpu_freq(cpu, i);
		if (unlikely(!tmp)) {
			WARN(1, "frequency not in opp table!\n");
			idx = 0;
			break;
		}
		if (tmp <= freq) {
			idx = i;
			break;
		}
	}

	if (idx < 0)
		idx = 0;

	return tbl->row[idx].cap;
}

void arch_set_freq_scale(struct cpumask *cpus, unsigned long cur_freq,
			 unsigned long max_freq)
{
	unsigned long scale;
	int i;
	int cpu = cpumask_first(cpus);
	unsigned long cur_cap, max_cap;

	cur_cap = get_freq_capacity(cpu, cur_freq);
	max_cap = get_freq_capacity(cpu, max_freq);

	scale = (cur_cap << SCHED_CAPACITY_SHIFT) / max_cap;

	for_each_cpu(i, cpus) {
		per_cpu(freq_scale, i) = scale;
		per_cpu(max_cpu_freq, i) = max_freq;
	}
}

void arch_set_max_freq_scale(struct cpumask *cpus,
			     unsigned long policy_max_freq)
{
	unsigned long scale, max_freq;
	int cpu = cpumask_first(cpus);
	unsigned long policy_max_cap, max_cap;

	if (cpu > nr_cpu_ids)
		return;

	max_freq = per_cpu(max_cpu_freq, cpu);
	if (!max_freq)
		return;

	policy_max_cap = get_freq_capacity_lt(cpu, policy_max_freq);
	max_cap = get_freq_capacity_lt(cpu, max_freq);

	scale = (policy_max_cap << SCHED_CAPACITY_SHIFT) / max_cap;

	for_each_cpu(cpu, cpus)
		per_cpu(max_freq_scale, cpu) = scale;
}

void arch_set_min_freq_scale(struct cpumask *cpus,
			     unsigned long policy_min_freq)
{
	unsigned long scale, max_freq;
	int cpu = cpumask_first(cpus);
	unsigned long policy_min_cap, max_cap;

	if (cpu > nr_cpu_ids)
		return;

	max_freq = per_cpu(max_cpu_freq, cpu);
	if (!max_freq)
		return;

	policy_min_cap = get_freq_capacity(cpu, policy_min_freq);
	max_cap = get_freq_capacity(cpu, max_freq);

	scale = (policy_min_cap << SCHED_CAPACITY_SHIFT) / max_cap;

	for_each_cpu(cpu, cpus)
		per_cpu(min_freq_scale, cpu) = scale;
}
