// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/energy_model.h>
#include <trace/hooks/topology.h>
#include "scheduler.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yun Hsiang");

#if defined(CONFIG_MTK_OPP_CAP_INFO)
static void __iomem *sram_base_addr;
static struct pd_capacity_info *pd_capacity_tbl;
static int pd_count;
static int entry_count;

/*
 * Calculate the opp for power table
 * The power table opp is descending order.
 * So the returned opp need to be transferred due to pd's frequency order.
 * order 0 is descending, order 1 is ascending.
 */
int pd_freq_to_opp(int cpu, unsigned long freq)
{
	int idx;
	int order;
	unsigned long first_freq, last_freq;
	struct em_perf_domain *pd;

	pd = em_cpu_get(cpu);

	if (!pd)
		return -1;

	first_freq = pd->table[0].frequency;
	last_freq = pd->table[pd->nr_cap_states - 1].frequency;

	if (first_freq > last_freq)
		order = 0;
	else
		order = 1;


	for (idx = 0; idx < pd->nr_cap_states; idx++) {
		if (pd->table[idx].frequency == freq) {
			if (order == 0)
				return idx;
			else
				return pd->nr_cap_states - idx - 1;
		}
	}

	return -1;
}
EXPORT_SYMBOL_GPL(pd_freq_to_opp);

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

	for (i = 0; i < pd_count; i++)
		kfree(pd_capacity_tbl[i].caps);
	kfree(pd_capacity_tbl);
	pd_capacity_tbl = NULL;
}

static int init_capacity_table(void)
{
	int i, j;
	void __iomem *base = sram_base_addr;
	int count = 0;
	unsigned long offset = 0;
	unsigned long cap;
	unsigned long end_cap;
	struct pd_capacity_info *pd_info;

	for (i = 0; i < pd_count; i++) {
		pd_info = &pd_capacity_tbl[i];
		for (j = 0; j < pd_info->nr_caps; j++) {
			cap = ioread16(base + offset);

			if (cap == 0)
				goto err;

			pd_info->caps[j] = cap;

			count += 1;
			offset += CAPACITY_ENTRY_SIZE;
		}

		/* repeated last cap and 2 bytes of 0 between each cluster */
		end_cap = ioread16(base + offset);
		if (end_cap != cap)
			goto err;
		offset += CAPACITY_ENTRY_SIZE * 2;
	}

	if (entry_count != count)
		goto err;

	return 0;
err:
	pr_err("count %d does not match entry_count %d\n", count, entry_count);

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
		if (!pd)
			continue;
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
	sram_base_addr = ioremap(DVFS_TBL_BASE_PHYS + CAPACITY_TBL_OFFSET, CAPACITY_TBL_SIZE);

	if (!sram_base_addr) {
		pr_err("Remap capacity table failed!\n");

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
		seq_printf(m, "nr_caps: %d\n", pd_info->nr_caps);
		seq_printf(m, "cpus: %*pbl\n", cpumask_pr_args(&pd_info->cpus));
		for (j = 0; j < pd_info->nr_caps; j++)
			seq_printf(m, "%d: %lu\n", j, pd_info->caps[j]);
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

static int init_opp_cap_info(struct proc_dir_entry *dir)
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

	entry = proc_create("pd_capacity_tbl", 0644, dir, &pd_capacity_tbl_ops);
	if (!entry)
		pr_warn("mtk_scheduler/pd_capacity_tbl entry create failed\n");

	return ret;
}

static void clear_opp_cap_info(void)
{
	free_capacity_table();
}

#if defined(CONFIG_NONLINEAR_FREQ_CTL)
static void mtk_arch_set_freq_scale(void *data, const struct cpumask *cpus,
		unsigned long freq, unsigned long max, unsigned long *scale)
{
	int cpu = cpumask_first(cpus);
	int opp;
	unsigned long cap, max_cap;

	opp = pd_freq_to_opp(cpu, freq);

	if (opp < 0)
		return;

	cap = pd_get_opp_capacity(cpu, opp);
	max_cap = pd_get_opp_capacity(cpu, 0);

	*scale = SCHED_CAPACITY_SCALE * cap / max_cap;
}
#endif
#else

static int init_opp_cap_info(struct proc_dir_entry *dir) { return 0; }
#define clear_opp_cap_info()

#endif

static int __init mtk_scheduler_init(void)
{
	int ret = 0;
	struct proc_dir_entry *dir;

	dir = proc_mkdir("mtk_scheduler", NULL);
	if (!dir)
		return -ENOMEM;

	ret = init_opp_cap_info(dir);
	if (ret)
		return ret;
#if defined(CONFIG_NONLINEAR_FREQ_CTL)
	ret = register_trace_android_vh_arch_set_freq_scale(
			mtk_arch_set_freq_scale, NULL);
	if (ret)
		pr_info("register android_vh_arch_set_freq_scale failed\n");
#endif
	return ret;

}

static void __exit mtk_scheduler_exit(void)
{
	clear_opp_cap_info();
}

module_init(mtk_scheduler_init);
module_exit(mtk_scheduler_exit);
