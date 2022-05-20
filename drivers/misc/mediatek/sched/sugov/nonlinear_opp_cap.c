// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/percpu.h>
#include <linux/sort.h>
#include <linux/log2.h>
#include <sched/sched.h>
#include <linux/energy_model.h>
#include "cpufreq.h"

DEFINE_PER_CPU(unsigned int, gear_id) = -1;
EXPORT_SYMBOL(gear_id);

#if IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO)
static void __iomem *sram_base_addr;
static struct pd_capacity_info *pd_capacity_tbl;
static int pd_count;
static int entry_count;

unsigned int get_nr_gears(void)
{
	return pd_count;
}
EXPORT_SYMBOL_GPL(get_nr_gears);

static inline int map_freq_opp_by_tbl(struct pd_capacity_info *pd_info, unsigned long freq)
{
	int idx;

	freq = clamp_val(freq, pd_info->table[pd_info->nr_caps - 1].freq, pd_info->table[0].freq);
	idx = (pd_info->table[0].freq - freq) >> pd_info->freq_opp_shift;
	return pd_info->freq_opp_map[idx];
}

static inline int map_util_opp_by_tbl(struct pd_capacity_info *pd_info, unsigned long util)
{
	int idx;

	util = clamp_val(util, pd_info->table[pd_info->nr_caps - 1].capacity,
		pd_info->table[0].capacity);
	idx = pd_info->table[0].capacity - util;
	return pd_info->util_opp_map[idx];
}

unsigned long pd_get_util_opp(int cpu, unsigned long util)
{
	int i;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	return map_util_opp_by_tbl(pd_info, util);
}
EXPORT_SYMBOL_GPL(pd_get_util_opp);

unsigned long pd_get_util_freq(int cpu, unsigned long util)
{
	int i, opp;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	opp = map_util_opp_by_tbl(pd_info, util);
	return pd_info->table[opp].freq;
}
EXPORT_SYMBOL_GPL(pd_get_util_freq);

unsigned long pd_get_util_pwr_eff(int cpu, unsigned long util)
{
	int i, opp;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	opp = map_util_opp_by_tbl(pd_info, util);
	return pd_info->table[opp].pwr_eff;
}
EXPORT_SYMBOL_GPL(pd_get_util_pwr_eff);

unsigned long pd_get_freq_opp(int cpu, unsigned long freq)
{
	int i;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	return map_freq_opp_by_tbl(pd_info, freq);
}
EXPORT_SYMBOL_GPL(pd_get_freq_opp);

unsigned long pd_get_freq_util(int cpu, unsigned long freq)
{
	int i, opp;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	opp = map_freq_opp_by_tbl(pd_info, freq);
	return pd_info->table[opp].capacity;
}
EXPORT_SYMBOL_GPL(pd_get_freq_util);

unsigned long pd_get_freq_pwr_eff(int cpu, unsigned long freq)
{
	int i, opp;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	opp = map_freq_opp_by_tbl(pd_info, freq);
	return pd_info->table[opp].pwr_eff;
}
EXPORT_SYMBOL_GPL(pd_get_freq_pwr_eff);

unsigned long pd_get_opp_capacity(int cpu, int opp)
{
	int i;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	opp = clamp_val(opp, 0, pd_info->nr_caps - 1);
	return pd_info->table[opp].capacity;
}
EXPORT_SYMBOL_GPL(pd_get_opp_capacity);

unsigned long pd_get_opp_pwr_eff(int cpu, int opp)
{
	int i;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	opp = clamp_val(opp, 0, pd_info->nr_caps - 1);
	return pd_info->table[opp].pwr_eff;
}
EXPORT_SYMBOL_GPL(pd_get_opp_pwr_eff);

unsigned int pd_get_cpu_opp(int cpu)
{
	int i;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	return pd_info->nr_caps;
}
EXPORT_SYMBOL_GPL(pd_get_cpu_opp);

static inline int cmpulong_dec(const void *a, const void *b)
{
	return -(*(unsigned long *)a - *(unsigned long *)b);
}

static void free_capacity_table(void)
{
	int i;

	if (!pd_capacity_tbl)
		return;

	for (i = 0; i < pd_count; i++) {
		kfree(pd_capacity_tbl[i].table);
		pd_capacity_tbl[i].table = NULL;
		kfree(pd_capacity_tbl[i].util_opp_map);
		pd_capacity_tbl[i].util_opp_map = NULL;
		kfree(pd_capacity_tbl[i].freq_opp_map);
		pd_capacity_tbl[i].freq_opp_map = NULL;
	}
	kfree(pd_capacity_tbl);
	pd_capacity_tbl = NULL;
}

static int init_util_freq_opp_mapping_table(void)
{
	int i, j, k, nr_opp, next_k;
	unsigned long min_gap;
	unsigned long min_cap, max_cap;
	unsigned long min_freq, max_freq;
	struct pd_capacity_info *pd_info;

	for (i = 0; i < pd_count; i++) {
		pd_info = &pd_capacity_tbl[i];
		nr_opp = pd_info->nr_caps;

		/* init util_opp_map */
		max_cap = pd_info->table[0].capacity;
		min_cap = pd_info->table[nr_opp - 1].capacity;
		pd_info->nr_util_opp_map = max_cap - min_cap;
		pd_info->util_opp_map = kcalloc(pd_info->nr_util_opp_map + 1, sizeof(int),
									GFP_KERNEL);
		if (!pd_info->util_opp_map)
			goto nomem;
		for (j = 0; j < nr_opp; j++) {
			k = max_cap - pd_info->table[j].capacity;
			next_k = max_cap - pd_info->table[min(nr_opp - 1, j + 1)].capacity;
			for (; k <= next_k; k++)
				pd_info->util_opp_map[k] = j;
		}

		/* init freq_opp_map */
		min_gap = ULONG_MAX;
		for (j = 0; j < nr_opp - 1; j++)
			min_gap = min(min_gap, pd_info->table[j].freq - pd_info->table[j + 1].freq);
		pd_info->freq_opp_shift = ilog2(min_gap);
		max_freq = pd_info->table[0].freq;
		min_freq = pd_info->table[nr_opp - 1].freq;
		pd_info->nr_freq_opp_map = (max_freq - min_freq)
			>> pd_info->freq_opp_shift;
		pd_info->freq_opp_map = kcalloc(pd_info->nr_freq_opp_map + 1, sizeof(int),
									GFP_KERNEL);
		if (!pd_info->freq_opp_map)
			goto nomem;
		for (j = 0; j < nr_opp; j++) {
			k = (max_freq - pd_info->table[j].freq) >> pd_info->freq_opp_shift;
			next_k = (max_freq - pd_info->table[min(nr_opp - 1, j + 1)].freq)
				>> pd_info->freq_opp_shift;
			for (; k <= next_k; k++)
				pd_info->freq_opp_map[k] = j;
		}
	}
	return 0;
nomem:
	pr_info("allocate util mapping table failed\n");
	free_capacity_table();
	return -ENOENT;
}

static int init_capacity_table(void)
{
	int i, j, cpu;
	void __iomem *base = sram_base_addr;
	int count = 0;
	unsigned long offset = 0;
	unsigned long cap;
	unsigned long end_cap;
	unsigned long *caps, *freqs, *powers;
	struct pd_capacity_info *pd_info;
	struct em_perf_domain *pd;

	for (i = 0; i < pd_count; i++) {
		pd_info = &pd_capacity_tbl[i];
		cpu = cpumask_first(&pd_info->cpus);
		pd = em_cpu_get(cpu);
		if (!pd)
			goto err;
		caps = kcalloc(pd_info->nr_caps, sizeof(unsigned long), GFP_KERNEL);
		freqs = kcalloc(pd_info->nr_caps, sizeof(unsigned long), GFP_KERNEL);
		powers = kcalloc(pd_info->nr_caps, sizeof(unsigned long), GFP_KERNEL);

		for (j = 0; j < pd_info->nr_caps; j++) {
			/* for init caps */
			cap = ioread16(base + offset);
			if (cap == 0)
				goto err;
			caps[j] = cap;

			/* for init freqs */
			freqs[j] = pd->table[j].frequency;

			/* for init pwr_eff */
			powers[j] = pd->table[j].power;

			count += 1;
			offset += CAPACITY_ENTRY_SIZE;
		}

		/* decreasing sorting */
		sort(caps, pd_info->nr_caps, sizeof(unsigned long), cmpulong_dec, NULL);
		sort(freqs, pd_info->nr_caps, sizeof(unsigned long), cmpulong_dec, NULL);
		sort(powers, pd_info->nr_caps, sizeof(unsigned long), cmpulong_dec, NULL);

		/* for init pwr_eff */
		for (j = 0; j < pd_info->nr_caps; j++) {
			pd_info->table[j].capacity = caps[j];
			pd_info->table[j].freq = freqs[j];
			pd_info->table[j].pwr_eff =
				em_scale_power(powers[j]) / pd_info->table[j].capacity;
		}
		kfree(caps);
		caps = NULL;
		kfree(freqs);
		freqs = NULL;
		kfree(powers);
		powers = NULL;

		/* repeated last cap 0 between each cluster */
		end_cap = ioread16(base + offset);
		if (end_cap != cap)
			goto err;
		offset += CAPACITY_ENTRY_SIZE;

		for_each_cpu(j, &pd_info->cpus) {
			per_cpu(gear_id, j) = i;
			if (per_cpu(cpu_scale, j) != pd_info->table[0].capacity) {
				pr_info("per_cpu(cpu_scale, %d)=%d, pd_info->table[0].cap=%d\n",
					j, per_cpu(cpu_scale, j), pd_info->table[0].capacity);
				per_cpu(cpu_scale, j) = pd_info->table[0].capacity;
			}
		}
	}

	if (entry_count != count)
		goto err;

	return 0;

err:
	pr_info("count %d does not match entry_count %d\n", count, entry_count);

	free_capacity_table();
	return -ENOENT;
}

static int alloc_capacity_table(void)
{
	int cpu = 0;
	int ret = 0;
	int cur_tbl = 0;
	int nr_caps;
	struct em_perf_domain *pd;

	for_each_possible_cpu(cpu) {
		pd = em_cpu_get(cpu);
		if (!pd) {
			pr_info("em_cpu_get return NULL for cpu#%d", cpu);
			continue;
		}
		if (cpu != cpumask_first(to_cpumask(pd->cpus)))
			continue;
		pd_count++;
	}

	pd_capacity_tbl = kcalloc(pd_count, sizeof(struct pd_capacity_info),
			GFP_KERNEL);
	if (!pd_capacity_tbl)
		return -ENOMEM;

	for_each_possible_cpu(cpu) {

		pd = em_cpu_get(cpu);
		if (!pd) {
			pr_info("em_cpu_get return NULL for cpu#%d", cpu);
			continue;
		}
		if (cpu != cpumask_first(to_cpumask(pd->cpus)))
			continue;

		WARN_ON(cur_tbl >= pd_count);

		nr_caps = pd->nr_perf_states;
		pd_capacity_tbl[cur_tbl].nr_caps = nr_caps;
		cpumask_copy(&pd_capacity_tbl[cur_tbl].cpus, to_cpumask(pd->cpus));

		pd_capacity_tbl[cur_tbl].table = kcalloc(nr_caps, sizeof(struct mtk_em_perf_state),
							GFP_KERNEL);
		if (!pd_capacity_tbl[cur_tbl].table)
			goto nomem;

		pd_capacity_tbl[cur_tbl].util_opp_map = NULL;
		pd_capacity_tbl[cur_tbl].freq_opp_map = NULL;

		entry_count += nr_caps;

		cur_tbl++;
	}

	return 0;

nomem:
	ret = -ENOMEM;
	free_capacity_table();

	return ret;
}

static int init_sram_mapping(void)
{
	sram_base_addr = ioremap(DVFS_TBL_BASE_PHYS + CAPACITY_TBL_OFFSET, CAPACITY_TBL_SIZE);

	if (!sram_base_addr) {
		pr_info("Remap capacity table failed!\n");

		return -EIO;
	}
	return 0;
}

static int pd_capacity_tbl_show(struct seq_file *m, void *v)
{
	int i, j;
	struct pd_capacity_info *pd_info;

	for (i = 0; i < pd_count; i++) {
		pd_info = &pd_capacity_tbl[i];

		if (!pd_info->nr_caps)
			break;

		seq_printf(m, "Pd table: %d\n", i);
		seq_printf(m, "nr_caps: %d\n", pd_info->nr_caps);
		seq_printf(m, "cpus: %*pbl\n", cpumask_pr_args(&pd_info->cpus));
		for (j = 0; j < pd_info->nr_caps; j++)
			seq_printf(m, "%d: %lu\n", j, pd_info->table[j].capacity);
	}

	return 0;
}

static int pd_capacity_tbl_open(struct inode *in, struct file *file)
{
	return single_open(file, pd_capacity_tbl_show, NULL);
}

static const struct proc_ops pd_capacity_tbl_ops = {
	.proc_open = pd_capacity_tbl_open,
	.proc_read = seq_read
};

int init_opp_cap_info(struct proc_dir_entry *dir)
{
	int ret;
	struct proc_dir_entry *entry;

	ret = init_sram_mapping();
	if (ret)
		return ret;

	ret = alloc_capacity_table();
	if (ret)
		return ret;

	ret = init_capacity_table();
	if (ret)
		return ret;

	ret = init_util_freq_opp_mapping_table();
	if (ret)
		return ret;

	entry = proc_create("pd_capacity_tbl", 0644, dir, &pd_capacity_tbl_ops);
	if (!entry)
		pr_info("mtk_scheduler/pd_capacity_tbl entry create failed\n");

	return ret;
}

void clear_opp_cap_info(void)
{
	free_capacity_table();
}

#if IS_ENABLED(CONFIG_NONLINEAR_FREQ_CTL)
void mtk_arch_set_freq_scale(void *data, const struct cpumask *cpus,
		unsigned long freq, unsigned long max, unsigned long *scale)
{
	int cpu = cpumask_first(cpus);
	unsigned long cap, max_cap;

	cap = pd_get_freq_util(cpu, freq);
	max_cap = pd_get_freq_util(cpu, max);
	*scale = SCHED_CAPACITY_SCALE * cap / max_cap;
}

unsigned int util_scale = 1280;
unsigned int sysctl_sched_capacity_margin_dvfs = 20;
/*
 * set sched capacity margin for DVFS, Default = 20
 */
int set_sched_capacity_margin_dvfs(unsigned int capacity_margin)
{
	if (capacity_margin < 0 || capacity_margin > 95)
		return -1;

	sysctl_sched_capacity_margin_dvfs = capacity_margin;
	util_scale = (SCHED_CAPACITY_SCALE * 100 / (100 - sysctl_sched_capacity_margin_dvfs));

	return 0;
}
EXPORT_SYMBOL_GPL(set_sched_capacity_margin_dvfs);

unsigned int get_sched_capacity_margin_dvfs(void)
{

	return sysctl_sched_capacity_margin_dvfs;
}
EXPORT_SYMBOL_GPL(get_sched_capacity_margin_dvfs);

void mtk_map_util_freq(void *data, unsigned long util, unsigned long freq,
				struct cpumask *cpumask, unsigned long *next_freq)
{
	int opp, cpu;

	util = (util * util_scale) >> SCHED_CAPACITY_SHIFT;
	cpu = cpumask_first(cpumask);
	opp = pd_get_util_opp(cpu, util);
	*next_freq = pd_get_util_freq(cpu, util);

	if (data != NULL) {
		struct sugov_policy *sg_policy = (struct sugov_policy *)data;
		struct cpufreq_policy *policy = sg_policy->policy;

		if (sg_policy->need_freq_update) {
			unsigned int idx_min, idx_max;
			unsigned int min_freq, max_freq;

			idx_min = cpufreq_frequency_table_target(policy, policy->min,
						CPUFREQ_RELATION_L);
			idx_max = cpufreq_frequency_table_target(policy, policy->max,
						CPUFREQ_RELATION_H);
			min_freq = policy->freq_table[idx_min].frequency;
			max_freq = policy->freq_table[idx_max].frequency;
			*next_freq = clamp_val(*next_freq, min_freq, max_freq);
			opp = clamp_val(opp, idx_max, idx_min);
		}
		policy->cached_target_freq = *next_freq;
		policy->cached_resolved_idx = opp;
		sg_policy->cached_raw_freq = *next_freq;
	}
}
EXPORT_SYMBOL_GPL(mtk_map_util_freq);
#endif
#else

static int init_opp_cap_info(struct proc_dir_entry *dir) { return 0; }
#define clear_opp_cap_info()

#endif
