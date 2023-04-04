// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/percpu.h>
#include <sched/sched.h>
#include "cpufreq.h"
#include "mtk_unified_power.h"
#include "common.h"

#if IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO)
static void __iomem *sram_base_addr;
static struct pd_capacity_info *pd_capacity_tbl;
static int pd_count;
static int entry_count;
static struct eas_info eas_node;
static bool initialized;

unsigned long pd_get_opp_capacity(int cpu, int opp)
{
	int i;
	struct pd_capacity_info *pd_info;

	for (i = 0; i < pd_count; i++) {
		pd_info = &pd_capacity_tbl[i];

		if (!cpumask_test_cpu(cpu, &pd_info->cpus))
			continue;

		/* Return max capacity if opp is not valid */
		if (opp < 0 || opp >= pd_info->nr_caps)
			return pd_info->caps[0];

		return pd_info->caps[opp];
	}

	/* Should NOT reach here */
	return 0;
}
EXPORT_SYMBOL_GPL(pd_get_opp_capacity);

static void free_capacity_table(void)
{
	int i;

	if (!pd_capacity_tbl)
		return;

	for (i = 0; i < pd_count; i++) {
		kfree(pd_capacity_tbl[i].caps);
		kfree(pd_capacity_tbl[i].util_opp);
		kfree(pd_capacity_tbl[i].util_freq);
	}
	kfree(pd_capacity_tbl);
	pd_capacity_tbl = NULL;
}

static int init_capacity_table(void)
{
	int i, j, cpu;
	void __iomem *base = sram_base_addr;
	int count = 0;
	unsigned long offset = 0;
	unsigned long cap;
	unsigned long end_cap;
	long next_cap, k;
	struct pd_capacity_info *pd_info;
	struct em_perf_domain *pd;
	struct upower_tbl *tbl = NULL;

	for (i = 0; i < pd_count; i++) {
		pd_info = &pd_capacity_tbl[i];
		cpu = cpumask_first(&pd_info->cpus);
		pd = em_cpu_get(cpu);
		if (!pd)
			goto err;
		if (!eas_node.available) {
#if IS_ENABLED(CONFIG_MTK_UNIFIED_POWER)
			tbl = upower_get_core_tbl(cpu);
#endif
			if (!tbl)
				goto err;
		}
		for (j = 0; j < pd_info->nr_caps; j++) {
			if (eas_node.available) {
				cap = ioread16(base + offset);
				next_cap = ioread16(base + offset + CAPACITY_ENTRY_SIZE);
			} else {
				cap = tbl->row[pd_info->nr_caps - j - 1].cap;
				if (j == pd_info->nr_caps - 1)
					next_cap = -1;
				else
					next_cap = tbl->row[pd_info->nr_caps - j - 2].cap;
			}
			if (cap == 0 || next_cap == 0)
				goto err;

			pd_info->caps[j] = cap;

			if (!pd_info->util_opp) {
				pd_info->util_opp = kcalloc(cap + 1, sizeof(unsigned int),
										GFP_KERNEL);
				if (!pd_info->util_opp)
					goto nomem;
			}

			if (!pd_info->util_freq) {
				pd_info->util_freq = kcalloc(cap + 1, sizeof(unsigned int),
										GFP_KERNEL);
				if (!pd_info->util_freq)
					goto nomem;
			}

			if (j == pd_info->nr_caps - 1)
				next_cap = -1;

			for (k = cap; k > next_cap; k--) {
				pd_info->util_opp[k] = j;
				pd_info->util_freq[k] =
					pd->table[pd->nr_perf_states - j - 1].frequency;
			}

			count += 1;
			if (eas_node.available)
				offset += CAPACITY_ENTRY_SIZE;
		}

		/* repeated last cap 0 between each cluster */
		if (eas_node.available) {
			end_cap = ioread16(base + offset);
			if (end_cap != cap)
				goto err;
			offset += CAPACITY_ENTRY_SIZE;
		}

		for_each_cpu(j, &pd_info->cpus) {
			if (per_cpu(cpu_scale, j) != pd_info->caps[0]) {
				pr_info("capacity err: cpu=%d, cpu_scale=%d, pd_info_cap=%d\n",
					j, per_cpu(cpu_scale, j), pd_info->caps[0]);
				per_cpu(cpu_scale, j) = pd_info->caps[0];
			} else {
				pr_info("capacity match: cpu=%d, cpu_scale=%d, pd_info_cap=%d\n",
					j, per_cpu(cpu_scale, j), pd_info->caps[0]);
			}
		}
	}

	if (entry_count != count)
		goto err;

	return 0;

nomem:
	pr_info("allocate util mapping table failed\n");
err:
	pr_info("count %d does not match entry_count %d\n", count, entry_count);

	free_capacity_table();
	return -ENOENT;
}

static int alloc_capacity_table(void)
{
	int i;
	int ret = 0;
	int cur_tbl = 0;

	pd_capacity_tbl = kcalloc(MAX_PD_COUNT, sizeof(struct pd_capacity_info),
			GFP_KERNEL);
	if (!pd_capacity_tbl)
		return -ENOMEM;

	for (i = 0; i < nr_cpu_ids; i++) {
		int nr_caps;
		struct em_perf_domain *pd;

		pd = em_cpu_get(i);
		if (!pd) {
			pr_info("em_cpu_get return NULL for cpu#%d", i);
			continue;
		}
		if (i != cpumask_first(to_cpumask(pd->cpus)))
			continue;

		WARN_ON(cur_tbl >= MAX_PD_COUNT);

		nr_caps = pd->nr_perf_states;
		pd_capacity_tbl[cur_tbl].nr_caps = nr_caps;
		cpumask_copy(&pd_capacity_tbl[cur_tbl].cpus, to_cpumask(pd->cpus));
		pd_capacity_tbl[cur_tbl].caps = kcalloc(nr_caps, sizeof(unsigned long),
							GFP_KERNEL);
		if (!pd_capacity_tbl[cur_tbl].caps)
			goto nomem;

		pd_capacity_tbl[cur_tbl].util_opp = NULL;
		pd_capacity_tbl[cur_tbl].util_freq = NULL;

		entry_count += nr_caps;

		cur_tbl++;
	}

	pd_count = cur_tbl;

	return 0;

nomem:
	ret = -ENOMEM;
	free_capacity_table();

	return ret;
}

static int init_sram_mapping(void)
{
	sram_base_addr = ioremap(eas_node.csram_base + eas_node.offs_cap, CAPACITY_TBL_SIZE);

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

	for (i = 0; i < MAX_PD_COUNT; i++) {
		pd_info = &pd_capacity_tbl[i];

		if (!pd_info->nr_caps)
			break;

		seq_printf(m, "Pd table: %d\n", i);
		seq_printf(m, "cpus: %*pbl\n", cpumask_pr_args(&pd_info->cpus));
		seq_printf(m, "nr_caps: %d\n", pd_info->nr_caps);
		for (j = 0; j < pd_info->nr_caps; j++)
			seq_printf(m, "%d: %lu, %lu\n", j, pd_info->caps[j],
					pd_info->util_freq[pd_info->caps[j]]);
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

#if IS_ENABLED(CONFIG_NONLINEAR_FREQ_CTL)
void mtk_arch_set_freq_scale(void *data, const struct cpumask *cpus,
		unsigned long freq, unsigned long max, unsigned long *scale)
{
	int cpu = cpumask_first(cpus);
	int opp;
	unsigned long cap, max_cap;
	struct cpufreq_policy *policy;

	if (unlikely(!initialized))
		return;

	policy = cpufreq_cpu_get(cpu);
	if (!policy)
		return;

	opp = cpufreq_frequency_table_get_index(policy, freq);
	cpufreq_cpu_put(policy);
	if (opp < 0)
		return;

	cap = pd_get_opp_capacity(cpu, opp);
	max_cap = pd_get_opp_capacity(cpu, 0);
	if (max_cap == 0)
		return;

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

static int capacity_margin_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%u\n", get_sched_capacity_margin_dvfs());
	return 0;
}

static int capacity_margin_open(struct inode *in, struct file *file)
{
	return single_open(file, capacity_margin_show, NULL);
}

static ssize_t capacity_margin_write(struct file *filp, const char *ubuf,
	size_t count, loff_t *data)
{
	char buf[16] = {0};
	int ret;
	unsigned int input = sysctl_sched_capacity_margin_dvfs;

	if (!count)
		return count;
	if (count + 1 > 16)
		return -ENOMEM;
	ret = copy_from_user(buf, ubuf, count);
	if (ret)
		return -EFAULT;
	buf[count] = '\0';
	ret = kstrtouint(buf, 10, &input);
	if (ret)
		return -EFAULT;
	ret = set_sched_capacity_margin_dvfs(input);
	if (ret)
		return -EFAULT;
	return count;
}

static const struct proc_ops capacity_margin_ops = {
	.proc_open = capacity_margin_open,
	.proc_read = seq_read,
	.proc_write = capacity_margin_write
};

void mtk_map_util_freq(void *data, unsigned long util, unsigned long freq,
				struct cpumask *cpumask, unsigned long *next_freq)
{
	int i, j, cap;
	struct pd_capacity_info *info;
	unsigned long temp_util;

	if (unlikely(!initialized))
		return;

	temp_util = util;
	util = (util * util_scale) >> SCHED_CAPACITY_SHIFT;

	for (i = 0; i < pd_count; i++) {
		info = &pd_capacity_tbl[i];
		if (!cpumask_equal(cpumask, &info->cpus))
			continue;
		cap = info->caps[0];
		util = min(util, info->caps[0]);
		j = info->util_opp[util];
		*next_freq = info->util_freq[util];
		break;
	}

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
			j = clamp_val(j, idx_max, idx_min);
		}
		policy->cached_target_freq = *next_freq;
		policy->cached_resolved_idx = j;
		if (cap != 0)
			sg_policy->cached_raw_freq = map_util_freq(temp_util, freq, cap);
	}
}
EXPORT_SYMBOL_GPL(mtk_map_util_freq);
#endif

int init_opp_cap_info(struct proc_dir_entry *dir)
{
	int ret;
	struct proc_dir_entry *entry;

	parse_eas_data(&eas_node);

	if (eas_node.available) {
		ret = init_sram_mapping();
		if (ret)
			return ret;
	}

	ret = alloc_capacity_table();
	if (ret)
		return ret;

	ret = init_capacity_table();
	if (ret)
		return ret;
	initialized = true;
	entry = proc_create("pd_capacity_tbl", 0644, dir, &pd_capacity_tbl_ops);
	if (!entry)
		pr_info("mtk_scheduler/pd_capacity_tbl entry create failed\n");
#if IS_ENABLED(CONFIG_NONLINEAR_FREQ_CTL)
	entry = proc_create("capacity_margin", 0644, dir, &capacity_margin_ops);
	if (!entry)
		pr_info("mtk_scheduler/capacity_margin entry create failed\n");
#endif
	return ret;
}

void clear_opp_cap_info(void)
{
	free_capacity_table();
}

#else

static int init_opp_cap_info(struct proc_dir_entry *dir) { return 0; }
#define clear_opp_cap_info()

#endif
