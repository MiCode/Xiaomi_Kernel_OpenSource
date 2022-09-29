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
#include <sched/sched.h>
#include <linux/energy_model.h>
#include "common.h"
#include "cpufreq.h"
#include "sugov_trace.h"
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
#include "mtk_energy_model/v2/energy_model.h"
#else
#include "mtk_energy_model/v1/energy_model.h"
#endif


DEFINE_PER_CPU(unsigned int, gear_id) = -1;
EXPORT_SYMBOL(gear_id);

#if IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO)
static void __iomem *sram_base_addr;
static struct pd_capacity_info *pd_capacity_tbl;
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
static void __iomem *sram_base_addr_freq_scaling;
static struct mtk_em_perf_domain *mtk_em_pd_ptr;
static bool freq_scaling_disabled = true;
#endif
static int pd_count;
static int entry_count;
static int busy_tick_boost_all;
static int sbb_active_ratio = 100;

enum {
	REG_FREQ_LUT_TABLE,
	REG_FREQ_ENABLE,
	REG_FREQ_PERF_STATE,
	REG_FREQ_HW_STATE,
	REG_EM_POWER_TBL,
	REG_FREQ_LATENCY,

	REG_ARRAY_SIZE,
};

struct cpufreq_mtk {
	struct cpufreq_frequency_table *table;
	void __iomem *reg_bases[REG_ARRAY_SIZE];
	int nr_opp;
	unsigned int last_index;
	cpumask_t related_cpus;
};

void set_sbb(int flag, int pid, bool set)
{
	struct task_struct *p;

	switch (flag) {
	case SBB_ALL:
		busy_tick_boost_all = set;
		break;
	case SBB_GROUP:
		rcu_read_lock();
		p = find_task_by_vpid(pid);
		if (p) {
			get_task_struct(p);
			p->sched_task_group->android_vendor_data1[TG_SBB_FLG] = set;
			put_task_struct(p);
		}
		rcu_read_unlock();
		break;
	case SBB_TASK:
		rcu_read_lock();
		p = find_task_by_vpid(pid);
		if (p) {
			get_task_struct(p);
			p->android_vendor_data1[T_SBB_FLG] = set;
			put_task_struct(p);
		}
		rcu_read_unlock();
	}
}
EXPORT_SYMBOL_GPL(set_sbb);

void set_sbb_active_ratio(int val)
{
	sbb_active_ratio = val;
}
EXPORT_SYMBOL_GPL(set_sbb_active_ratio);

int get_sbb_active_ratio(void)
{
	return sbb_active_ratio;
}
EXPORT_SYMBOL_GPL(get_sbb_active_ratio);

int is_busy_tick_boost_all(void)
{
	return busy_tick_boost_all;
}
EXPORT_SYMBOL_GPL(is_busy_tick_boost_all);

bool is_gearless_support(void)
{
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
	return !freq_scaling_disabled;
#else
	return false;
#endif
}
EXPORT_SYMBOL_GPL(is_gearless_support);

unsigned int get_nr_gears(void)
{
	return pd_count;
}
EXPORT_SYMBOL_GPL(get_nr_gears);

struct cpumask *get_gear_cpumask(unsigned int gear)
{
	struct pd_capacity_info *pd_info;

	pd_info = &pd_capacity_tbl[gear];
	return &pd_info->cpus;
}
EXPORT_SYMBOL_GPL(get_gear_cpumask);

static inline int map_freq_idx_by_tbl(struct pd_capacity_info *pd_info, unsigned long freq)
{
	int idx;

	freq = min_t(unsigned int, freq, pd_info->table[0].freq);
	if (freq <= pd_info->freq_min)
		return pd_info->nr_freq_opp_map - 1;

	idx = mul_u64_u32_shr((u32) pd_info->table[0].freq - freq, pd_info->inv_DFreq, 32);
	return idx;
}

static inline int map_util_idx_by_tbl(struct pd_capacity_info *pd_info, unsigned long util)
{
	int idx;

	util = clamp_val(util, pd_info->table[pd_info->nr_caps - 1].capacity,
		pd_info->table[0].capacity);
	idx = pd_info->table[0].capacity - util;
	return idx;
}

struct mtk_em_perf_state *pd_get_util_ps(int cpu, unsigned long util, int *opp)
{
	int i, idx;
	struct pd_capacity_info *pd_info;
	struct mtk_em_perf_state *ps;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	idx = map_util_idx_by_tbl(pd_info, util);
	idx = pd_info->util_opp_map[idx];
	ps = &pd_info->table[idx];
	*opp = idx;
	return ps;
}
EXPORT_SYMBOL_GPL(pd_get_util_ps);

unsigned long pd_get_util_opp(int cpu, unsigned long util)
{
	int i, idx;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	idx = map_util_idx_by_tbl(pd_info, util);
	return pd_info->util_opp_map[idx];
}
EXPORT_SYMBOL_GPL(pd_get_util_opp);

unsigned long pd_get_util_freq(int cpu, unsigned long util)
{
	int i, idx;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	idx = map_util_idx_by_tbl(pd_info, util);
	idx = pd_info->util_opp_map[idx];
	return max(pd_info->table[idx].freq, pd_info->freq_min);
}
EXPORT_SYMBOL_GPL(pd_get_util_freq);

unsigned long pd_get_util_pwr_eff(int cpu, unsigned long util)
{
	int i, idx;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	idx = map_util_idx_by_tbl(pd_info, util);
	idx = pd_info->util_opp_map[idx];
	return pd_info->table[idx].pwr_eff;
}
EXPORT_SYMBOL_GPL(pd_get_util_pwr_eff);

struct mtk_em_perf_state *pd_get_freq_ps(int cpu, unsigned long freq, int *opp)
{
	int i, idx;
	struct pd_capacity_info *pd_info;
	struct mtk_em_perf_state *ps;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	idx = map_freq_idx_by_tbl(pd_info, freq);
	idx = pd_info->freq_opp_map[idx];
	ps = &pd_info->table[idx];
	*opp = idx;
	return ps;
}
EXPORT_SYMBOL_GPL(pd_get_freq_ps);

unsigned long pd_get_freq_opp(int cpu, unsigned long freq)
{
	int i, idx;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	idx = map_freq_idx_by_tbl(pd_info, freq);
	return pd_info->freq_opp_map[idx];
}
EXPORT_SYMBOL_GPL(pd_get_freq_opp);

unsigned long pd_get_freq_opp_legacy(int cpu, unsigned long freq)
{
	int i, idx;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];

	if (freq <= pd_info->freq_min)
		return pd_info->freq_opp_map_legacy[pd_info->nr_freq_opp_map - 1];

	idx = map_freq_idx_by_tbl(pd_info, freq);
	if (freq > pd_get_opp_freq_legacy(cpu, pd_info->freq_opp_map_legacy[idx] + 1))
		return pd_info->freq_opp_map_legacy[idx];
	else
		return pd_info->freq_opp_map_legacy[idx] + 1;
}
EXPORT_SYMBOL_GPL(pd_get_freq_opp_legacy);

unsigned long pd_get_freq_util(int cpu, unsigned long freq)
{
	int i, idx;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	idx = map_freq_idx_by_tbl(pd_info, freq);
	idx = pd_info->freq_opp_map[idx];
	return pd_info->table[idx].capacity;
}
EXPORT_SYMBOL_GPL(pd_get_freq_util);

unsigned long pd_get_freq_pwr_eff(int cpu, unsigned long freq)
{
	int i, idx;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	idx = map_freq_idx_by_tbl(pd_info, freq);
	idx = pd_info->freq_opp_map[idx];
	return pd_info->table[idx].pwr_eff;
}
EXPORT_SYMBOL_GPL(pd_get_freq_pwr_eff);

unsigned long pd_get_opp_freq(int cpu, int opp)
{
	int i;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	opp = clamp_val(opp, 0, pd_info->nr_caps - 1);
	return max(pd_info->table[opp].freq, pd_info->freq_min);
}
EXPORT_SYMBOL_GPL(pd_get_opp_freq);

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

#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
unsigned long pd_get_opp_capacity_legacy(int cpu, int opp)
{
	int i;

	i = per_cpu(gear_id, cpu);
	opp = clamp_val(opp, 0,
		mtk_em_pd_ptr_public[i].nr_perf_states - 1);
	return mtk_em_pd_ptr_public[i].table[opp].capacity;
}
#else
unsigned long pd_get_opp_capacity_legacy(int cpu, int opp)
{
	int i;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	opp = clamp_val(opp, 0, pd_info->nr_caps - 1);
	return pd_info->table[opp].capacity;
}
#endif
EXPORT_SYMBOL_GPL(pd_get_opp_capacity_legacy);


#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
unsigned long pd_get_opp_freq_legacy(int cpu, int opp)
{
	int i;

	i = per_cpu(gear_id, cpu);
	opp = clamp_val(opp, 0,
		mtk_em_pd_ptr_public[i].nr_perf_states - 1);
	return mtk_em_pd_ptr_public[i].table[opp].freq;
}
#else
unsigned long pd_get_opp_freq_legacy(int cpu, int opp)
{
	int i;
	struct pd_capacity_info *pd_info;

	i = per_cpu(gear_id, cpu);
	pd_info = &pd_capacity_tbl[i];
	opp = clamp_val(opp, 0, pd_info->nr_caps - 1);
	return pd_info->table[opp].freq;
}
#endif
EXPORT_SYMBOL_GPL(pd_get_opp_freq_legacy);

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

unsigned int pd_get_opp_leakage(unsigned int cpu, unsigned int opp, unsigned int temperature)
{
	return mtk_get_leakage(cpu, opp, temperature);
}
EXPORT_SYMBOL_GPL(pd_get_opp_leakage);

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
		kfree(pd_capacity_tbl[i].freq_opp_map_legacy);
		pd_capacity_tbl[i].freq_opp_map_legacy = NULL;
	}
	kfree(pd_capacity_tbl);
	pd_capacity_tbl = NULL;
}

static int init_util_freq_opp_mapping_table(void)
{
	int i, j, k, nr_opp, next_k, opp;
	unsigned int min_gap;
	unsigned long min_cap, max_cap;
	unsigned long min_freq, max_freq, curr_freq;
	struct pd_capacity_info *pd_info;
	struct cpufreq_policy *policy;

	for (i = 0; i < pd_count; i++) {
		pd_info = &pd_capacity_tbl[i];
		nr_opp = pd_info->nr_caps;

		/* init util_opp_map */
		max_cap = pd_info->table[0].capacity;
		min_cap = pd_info->table[nr_opp - 1].capacity;
		pd_info->nr_util_opp_map = max_cap - min_cap + 1;
		pd_info->util_opp_map = kcalloc(pd_info->nr_util_opp_map, sizeof(int),
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
		min_gap = UINT_MAX;
		/* skip opp=0, nr_opp-1 because potential irregular freq delta*/
		for (j = 0; j < nr_opp - 2; j++)
			min_gap = min(min_gap, pd_info->table[j].freq - pd_info->table[j + 1].freq);
		pd_info->DFreq = min_gap;
		pd_info->inv_DFreq = (u32) DIV_ROUND_UP((u64) UINT_MAX, pd_info->DFreq);
		max_freq = pd_info->table[0].freq;
		min_freq = rounddown(pd_info->table[nr_opp - 1].freq, pd_info->DFreq);

		pd_info->nr_freq_opp_map = ((max_freq - min_freq) / pd_info->DFreq) + 1;

		pd_info->freq_opp_map = kcalloc(pd_info->nr_freq_opp_map, sizeof(int), GFP_KERNEL);
		if (!pd_info->freq_opp_map)
			goto nomem;

		pd_info->freq_opp_map_legacy = kcalloc(pd_info->nr_freq_opp_map, sizeof(int),
			GFP_KERNEL);
		if (!pd_info->freq_opp_map_legacy)
			goto nomem;

		opp = 0;
		curr_freq = pd_info->table[opp].freq;

		policy = cpufreq_cpu_get(cpumask_first(&pd_info->cpus));
		if (!policy)
			pr_info("%s: %d: policy NULL in pd=%d\n", __func__, __LINE__, i);

		for (j = 0; j < pd_info->nr_freq_opp_map - 1; j++) {
			if (curr_freq <= pd_info->table[opp + 1].freq)
				opp++;
			pd_info->freq_opp_map[j] = opp;

			if (policy)
				pd_info->freq_opp_map_legacy[j] =
				cpufreq_table_find_index_dl(policy, pd_info->table[opp].freq);

			curr_freq -= pd_info->DFreq;
		}
		/* fill last element with min_freq opp */
		pd_info->freq_opp_map[pd_info->nr_freq_opp_map - 1] = opp + 1;
		if (policy) {
			pd_info->freq_opp_map_legacy[pd_info->nr_freq_opp_map - 1] =
				cpufreq_table_find_index_dl(policy, pd_info->table[opp + 1].freq);
			cpufreq_cpu_put(policy);
		}
	}
	return 0;
nomem:
	pr_info("allocate util mapping table failed\n");
	free_capacity_table();
	return -ENOENT;
}

#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
static int init_capacity_table(void)
{
	int i, j;
	struct pd_capacity_info *pd_info;

	for (i = 0; i < pd_count; i++) {
		pd_info = &pd_capacity_tbl[i];
		if (!cpumask_equal(&pd_info->cpus, mtk_em_pd_ptr[i].cpumask)) {
			pr_info("cpumask mismatch, pd=%x, em=%x\n", pd_info->cpus,
				*mtk_em_pd_ptr[i].cpumask);
				return -1;
		}
		pd_info->table = mtk_em_pd_ptr[i].table;
		for_each_cpu(j, &pd_info->cpus) {
			per_cpu(gear_id, j) = i;
			if (per_cpu(cpu_scale, j) != pd_info->table[0].capacity) {
				pr_info("capacity err: cpu=%d, cpu_scale=%d, pd_info_cap=%d\n",
					j, per_cpu(cpu_scale, j), pd_info->table[0].capacity);
				per_cpu(cpu_scale, j) = pd_info->table[0].capacity;
			} else {
				pr_info("capacity match: cpu=%d, cpu_scale=%d, pd_info_cap=%d\n",
					j, per_cpu(cpu_scale, j), pd_info->table[0].capacity);
			}
		}
	}
	return 0;
}
#else
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
				pr_info("capacity err: cpu=%d, cpu_scale=%d, pd_info_cap=%d\n",
					j, per_cpu(cpu_scale, j), pd_info->table[0].capacity);
				per_cpu(cpu_scale, j) = pd_info->table[0].capacity;
			} else {
				pr_info("capacity match: cpu=%d, cpu_scale=%d, pd_info_cap=%d\n",
					j, per_cpu(cpu_scale, j), pd_info->table[0].capacity);
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
#endif

static int alloc_capacity_table(void)
{
	int cpu = 0;
	int cur_tbl = 0;
	int nr_caps;
	struct em_perf_domain *pd;

#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
	sram_base_addr_freq_scaling =
		ioremap(DVFS_TBL_BASE_PHYS + REG_FREQ_SCALING, LUT_ROW_SIZE);
	if (!sram_base_addr_freq_scaling) {
		pr_info("Remap sram_base_addr_freq_scaling failed!\n");
		return -EIO;
	}
	if (readl_relaxed(sram_base_addr_freq_scaling))
		freq_scaling_disabled = false;

	if (mtk_em_pd_ptr_private == NULL || mtk_em_pd_ptr_public == NULL) {
		pr_info("%s: NULL mtk_em_pd_ptr, private: %p, public: %p\n",
			__func__, mtk_em_pd_ptr_private, mtk_em_pd_ptr_public);
		return -EFAULT;
	}

	if (is_gearless_support())
		mtk_em_pd_ptr = mtk_em_pd_ptr_private;
	else
		mtk_em_pd_ptr = mtk_em_pd_ptr_public;
#endif
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

#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
		nr_caps = mtk_em_pd_ptr[cur_tbl].nr_perf_states;
#else
		nr_caps = pd->nr_perf_states;
		pd_capacity_tbl[cur_tbl].table = kcalloc(nr_caps, sizeof(struct mtk_em_perf_state),
							GFP_KERNEL);
		if (!pd_capacity_tbl[cur_tbl].table) {
			free_capacity_table();
			return -ENOMEM;
		}
#endif
		pd_capacity_tbl[cur_tbl].nr_caps = nr_caps;
		cpumask_copy(&pd_capacity_tbl[cur_tbl].cpus, to_cpumask(pd->cpus));

		pd_capacity_tbl[cur_tbl].freq_max =
			max(pd->table[pd->nr_perf_states - 1].frequency, pd->table[0].frequency);
		pd_capacity_tbl[cur_tbl].freq_min =
			min(pd->table[pd->nr_perf_states - 1].frequency, pd->table[0].frequency);
		pd_capacity_tbl[cur_tbl].util_opp_map = NULL;
		pd_capacity_tbl[cur_tbl].freq_opp_map = NULL;
		pd_capacity_tbl[cur_tbl].freq_opp_map_legacy = NULL;

		entry_count += nr_caps;

		cur_tbl++;
	}

	return 0;
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
	struct em_perf_domain *pd;

	for (i = 0; i < pd_count; i++) {
		pd_info = &pd_capacity_tbl[i];

		if (!pd_info->nr_caps)
			break;

		seq_printf(m, "pd table: %d\n", i);
		seq_printf(m, "cpus: %*pbl\n", cpumask_pr_args(&pd_info->cpus));
		pd = em_cpu_get(cpumask_first(&pd_info->cpus));
		if (!pd) {
			pr_info("sugov_ext err: pd null in cluster%d\n", i);
			continue;
		}
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
		seq_printf(m, "nr_caps: %d\n", pd->nr_perf_states);
		for (j = 0; j < pd->nr_perf_states; j++)
			seq_printf(m, "%3d: %4lu, %7lu\n", j,
				mtk_em_pd_ptr_public[i].table[j].capacity,
				mtk_em_pd_ptr_public[i].table[j].freq);
		if (is_gearless_support())
			seq_puts(m, "\n");
#else
		if (pd_info->nr_caps != pd->nr_perf_states) {
			pr_info("sugov_ext err: pd_info->nr_c. != pd->nr_perf_sta. in clus.=%d\n",
				i);
			continue;
		}
		seq_printf(m, "nr_caps: %d\n", pd_info->nr_caps);
		for (j = 0; j < pd_info->nr_caps; j++)
			seq_printf(m, "%d: %lu, %lu\n", j, pd_info->table[j].capacity,
				pd_info->table[j].freq);
#endif
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
static inline void mtk_arch_set_freq_scale_gearless(struct cpufreq_policy *policy,
		unsigned int *target_freq)
{
	int i;
	unsigned long cap, max_cap;
	struct cpufreq_mtk *c = policy->driver_data;

	if (c->last_index == policy->cached_resolved_idx) {
		cap = pd_get_freq_util(policy->cpu, *target_freq);
		max_cap = pd_get_freq_util(policy->cpu, policy->cpuinfo.max_freq);
		for_each_cpu(i, policy->related_cpus)
			per_cpu(arch_freq_scale, i) = SCHED_CAPACITY_SCALE * cap / max_cap;
	}
}

void mtk_cpufreq_fast_switch(void *data, struct cpufreq_policy *policy,
		unsigned int *target_freq, unsigned int old_target_freq)
{
#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEBUG)
	u64 ts[2];

	ts[0] = sched_clock();
#endif

	if (trace_sugov_ext_gear_state_enabled())
		trace_sugov_ext_gear_state(per_cpu(gear_id, policy->cpu),
			pd_get_freq_opp(policy->cpu, *target_freq));

	if (policy->cached_target_freq != *target_freq) {
		policy->cached_target_freq = *target_freq;
		policy->cached_resolved_idx = pd_get_freq_opp_legacy(policy->cpu,
			*target_freq);
	}

	if (is_gearless_support())
		mtk_arch_set_freq_scale_gearless(policy, target_freq);
#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEBUG)
	ts[1] = sched_clock();

	if ((ts[1] - ts[0] > 500000ULL) && in_hardirq()) {
		printk_deferred("%s duration %llu, ts[0]=%llu, ts[1]=%llu\n",
				__func__, ts[1] - ts[0], ts[0], ts[1]);

	}
#endif
}

void mtk_arch_set_freq_scale(void *data, const struct cpumask *cpus,
		unsigned long freq, unsigned long max, unsigned long *scale)
{
	int cpu = cpumask_first(cpus);
	unsigned long cap, max_cap;
	struct cpufreq_policy *policy;
#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEBUG)
	u64 ts[2];

	ts[0] = sched_clock();
#endif

	policy = cpufreq_cpu_get(cpu);
	if (policy) {
		freq = policy->cached_target_freq;
		cpufreq_cpu_put(policy);
	}
	cap = pd_get_freq_util(cpu, freq);
	max_cap = pd_get_freq_util(cpu, max);
	*scale = SCHED_CAPACITY_SCALE * cap / max_cap;
#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEBUG)
	ts[1] = sched_clock();

	if ((ts[1] - ts[0] > 500000ULL) && in_hardirq()) {
		printk_deferred("%s duration %llu, ts[0]=%llu, ts[1]=%llu\n",
				__func__, ts[1] - ts[0], ts[0], ts[1]);

	}
#endif
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
	int cpu;

	util = (util * util_scale) >> SCHED_CAPACITY_SHIFT;
	cpu = cpumask_first(cpumask);
	*next_freq = pd_get_util_freq(cpu, util);

	if (data != NULL) {
		struct sugov_policy *sg_policy = (struct sugov_policy *)data;
		struct cpufreq_policy *policy = sg_policy->policy;

		policy->cached_target_freq = *next_freq;
		policy->cached_resolved_idx = pd_get_freq_opp_legacy(cpu, *next_freq);
		sg_policy->cached_raw_freq = *next_freq;
	}
}
EXPORT_SYMBOL_GPL(mtk_map_util_freq);
#endif
#else

static int init_opp_cap_info(struct proc_dir_entry *dir) { return 0; }
#define clear_opp_cap_info()

#endif
