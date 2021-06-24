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
#include <trace/events/sched.h>
#include <trace/hooks/topology.h>
#include <trace/hooks/sched.h>
#include "sched_main.h"
#include "eas_plus.h"

#define CREATE_TRACE_POINTS
#include "sched_trace.h"

#if IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO)
static void __iomem *sram_base_addr;
static struct pd_capacity_info *pd_capacity_tbl;
static int pd_count;
static int entry_count;

int pd_freq_to_opp(int cpu, unsigned long freq)
{
	int idx;
	struct em_perf_domain *pd;

	pd = em_cpu_get(cpu);
	for (idx = 0; idx < pd->nr_perf_states; idx++) {
		if (pd->table[idx].frequency == freq)
			return idx;
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

		/* repeated last cap 0 between each cluster */
		end_cap = ioread16(base + offset);
		if (end_cap != cap)
			goto err;
		offset += CAPACITY_ENTRY_SIZE;
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
		pr_info("mtk_scheduler/pd_capacity_tbl entry create failed\n");

	return ret;
}

static void clear_opp_cap_info(void)
{
	free_capacity_table();
}

#if IS_ENABLED(CONFIG_NONLINEAR_FREQ_CTL)
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

void mtk_map_util_freq(void *data, unsigned long util, unsigned long freq,
				unsigned long cap, unsigned long *next_freq)
{
	int i, j;
	int cpu;
	int first_freq, last_freq;
	unsigned long cur_cap;
	struct pd_capacity_info *info;
	struct em_perf_domain *pd;
	unsigned long temp_util;

	temp_util = util;
	util = util + (util >> 2);

	for (i = 0; i < pd_count; i++) {
		info = &pd_capacity_tbl[i];
		if (cap != info->caps[0])
			continue;

		cpu = cpumask_first(&info->cpus);
		for (j = info->nr_caps - 1; j >= 0; j--) {
			cur_cap = info->caps[j];
			if (cur_cap >= util) {
				int opp;

				pd = em_cpu_get(cpu);
				first_freq = pd->table[0].frequency;
				last_freq = pd->table[pd->nr_perf_states - 1].frequency;

				if (first_freq > last_freq)
					opp = j;
				else
					opp = pd->nr_perf_states - j - 1;

				*next_freq = pd->table[opp].frequency;
				return;
			}
		}
	}
}
EXPORT_SYMBOL_GPL(mtk_map_util_freq);
#endif
#else

static int init_opp_cap_info(struct proc_dir_entry *dir) { return 0; }
#define clear_opp_cap_info()

#endif

static void sched_task_util_hook(void *data, struct sched_entity *se)
{
	if (trace_sched_task_util_enabled()) {
		struct task_struct *p;
		struct sched_avg *sa;

		if (!entity_is_task(se))
			return;

		p = container_of(se, struct task_struct, se);
		sa = &se->avg;

		trace_sched_task_util(p->pid, p->comm,
				sa->util_avg, sa->util_est.enqueued, sa->util_est.ewma);
	}
}

static void sched_task_uclamp_hook(void *data, struct sched_entity *se)
{
	if (trace_sched_task_uclamp_enabled()) {
		struct task_struct *p;
		struct sched_avg *sa;
		struct util_est ue;
		struct uclamp_se *uc_min_req, *uc_max_req;
		unsigned long util;

		if (!entity_is_task(se))
			return;

		p = container_of(se, struct task_struct, se);
		sa = &se->avg;
		ue = READ_ONCE(se->avg.util_est);
		util = max(ue.ewma, ue.enqueued);
		util = max(util, READ_ONCE(se->avg.util_avg));
		uc_min_req = &p->uclamp_req[UCLAMP_MIN];
		uc_max_req = &p->uclamp_req[UCLAMP_MAX];

		trace_sched_task_uclamp(p->pid, util,
				p->uclamp[UCLAMP_MIN].active,
				p->uclamp[UCLAMP_MIN].value, p->uclamp[UCLAMP_MAX].value,
				uc_min_req->user_defined, uc_min_req->value,
				uc_max_req->user_defined, uc_max_req->value);
	}
}

static int enqueue;
static int dequeue;
static void sched_queue_task_hook(void *data, struct rq *rq, struct task_struct *p, int flags)
{
	if (trace_sched_queue_task_enabled()) {
		int cpu = rq->cpu;
		unsigned long util = READ_ONCE(rq->cfs.avg.util_avg);

		util = max_t(unsigned long, util,
			     READ_ONCE(rq->cfs.avg.util_est.enqueued));

		trace_sched_queue_task(cpu, p->pid, *(int *)data, util,
				rq->uclamp[UCLAMP_MIN].value, rq->uclamp[UCLAMP_MAX].value,
				p->uclamp[UCLAMP_MIN].value, p->uclamp[UCLAMP_MAX].value);
	}
}

static void mtk_sched_trace_init(void)
{
	int ret = 0;

	enqueue = 1;
	dequeue = -1;

	ret = register_trace_android_rvh_enqueue_task(sched_queue_task_hook, &enqueue);
	if (ret)
		pr_info("register android_rvh_enqueue_task failed!\n");
	ret = register_trace_android_rvh_dequeue_task(sched_queue_task_hook, &dequeue);
	if (ret)
		pr_info("register android_rvh_dequeue_task failed!\n");

	ret = register_trace_pelt_se_tp(sched_task_util_hook, NULL);
	if (ret)
		pr_info("register sched_task_util_hook failed!\n");

	ret = register_trace_pelt_se_tp(sched_task_uclamp_hook, NULL);
	if (ret)
		pr_info("register sched_task_uclamp_hook failed!\n");
}

static void mtk_sched_trace_exit(void)
{
	unregister_trace_pelt_se_tp(sched_task_util_hook, NULL);
	unregister_trace_pelt_se_tp(sched_task_uclamp_hook, NULL);
}

static int __init mtk_scheduler_init(void)
{
	int ret = 0;
	struct proc_dir_entry *dir;

	dir = proc_mkdir("mtk_scheduler", NULL);
	if (!dir)
		return -ENOMEM;

	ret = init_sched_common_sysfs();
	if (ret)
		return ret;

	ret = init_opp_cap_info(dir);
	if (ret)
		return ret;
#if IS_ENABLED(CONFIG_NONLINEAR_FREQ_CTL)
	ret = register_trace_android_vh_arch_set_freq_scale(
			mtk_arch_set_freq_scale, NULL);
	if (ret)
		pr_info("register android_vh_arch_set_freq_scale failed\n");
#endif

#if IS_ENABLED(CONFIG_MTK_EAS)
	ret = init_sram_info();
	if (ret)
		return ret;

	ret = register_trace_android_rvh_find_busiest_group(
			mtk_find_busiest_group, NULL);
	if (ret)
		pr_info("register android_rvh_find_busiest_group failed\n");

	ret = register_trace_android_rvh_find_energy_efficient_cpu(
			mtk_find_energy_efficient_cpu, NULL);
	if (ret)
		pr_info("register android_rvh_find_energy_efficient_cpu failed\n");

	ret = register_trace_android_vh_em_cpu_energy(
			mtk_em_cpu_energy, NULL);
	if (ret)
		pr_info("register trace_android_vh_em_cpu_energy failed\n");

	ret = register_trace_android_rvh_tick_entry(
			mtk_tick_entry, NULL);
	if (ret)
		pr_info("register android_rvh_tick_entry failed\n");

	ret = register_trace_android_vh_set_wake_flags(
			mtk_set_wake_flags, NULL);
	if (ret)
		pr_info("register android_vh_set_wake_flags failed\n");

#if IS_ENABLED(CONFIG_MTK_NEWIDLE_BALANCE)
	ret = register_trace_android_rvh_sched_newidle_balance(
			mtk_sched_newidle_balance, NULL);
	if (ret)
		pr_info("register android_rvh_sched_newidle_balance failed\n");
#endif
#endif

	mtk_sched_trace_init();

	mtk_static_power_init();
	return ret;

}

static void __exit mtk_scheduler_exit(void)
{
	mtk_sched_trace_exit();
	clear_opp_cap_info();
	cleanup_sched_common_sysfs();
}

module_init(mtk_scheduler_init);
module_exit(mtk_scheduler_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek scheduler");
MODULE_AUTHOR("MediaTek Inc.");
