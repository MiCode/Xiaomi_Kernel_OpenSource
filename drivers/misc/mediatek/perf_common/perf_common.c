// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/spinlock.h>
#include <linux/time.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/energy_model.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <trace/hooks/sched.h>

#include <perf_tracker_internal.h>

#define TAG	"mtk_perf_common"

static u64 checked_timestamp;
static bool long_trace_check_flag;
static DEFINE_SPINLOCK(check_lock);
static int perf_common_init;
static atomic_t perf_in_progress;

void __iomem *csram_base;
struct em_perf_domain *em_pd;

static int first_cpu_in_cluster[MAX_CLUSTER_NR] = {0};
struct ppm_data cluster_ppm_info[MAX_CLUSTER_NR];

int cluster_nr = -1;

static int init_cpu_cluster_info(void)
{
	int ret = 0, cluster_num = 0, i;

	/* initial array of first cpu id */
	for (i = 0; i < MAX_CLUSTER_NR; i++)
		first_cpu_in_cluster[i] = -1;

	/* get first cpu id of cluster and initial cluster number */
	for (i = 0; i < nr_cpu_ids; i++) {
		int cpu;

		em_pd = em_cpu_get(i);
		if (!em_pd) {
			pr_info("%s: no EM found for CPU%d\n", TAG, i);
			return -EINVAL;
		}

		cpu = cpumask_first(to_cpumask(em_pd->cpus));
		if (i == cpu) {
			first_cpu_in_cluster[cluster_num] = i;
			cluster_num++;
		}
	}

	cluster_nr = cluster_num;
	return ret;
}

static inline int get_opp_count(struct cpufreq_policy *policy)
{
	int opp_nr;
	struct cpufreq_frequency_table *freq_pos;

	cpufreq_for_each_entry_idx(freq_pos, policy->freq_table, opp_nr);
	return opp_nr;
}

static int init_cpufreq_table(void)
{
	struct cpufreq_policy *policy;
	int ret = 0, i;

	for (i = 0; i < cluster_nr; i++) {
		int opp_ids, opp_nr;

		policy = cpufreq_cpu_get(first_cpu_in_cluster[i]);
		if (!policy) {
			pr_info("%s: policy %d is null", TAG, i);
			continue;
		}
		/* get CPU OPP number */
		opp_nr = get_opp_count(policy);

		cluster_ppm_info[i].dvfs_tbl =
			kcalloc(opp_nr,
				sizeof(*cluster_ppm_info[i].dvfs_tbl),
				GFP_KERNEL);
		if (!cluster_ppm_info[i].dvfs_tbl) {
			ret = -ENOMEM;
			pr_info("%s: Failed to allocate dvfs table for cid %d", TAG, i);
			cpufreq_cpu_put(policy);
			goto alloc_table_oom;
		}

		cluster_ppm_info[i].opp_nr = opp_nr;
		for (opp_ids = 0; opp_ids < opp_nr; opp_ids++)
			cluster_ppm_info[i].dvfs_tbl[opp_ids] =
				policy->freq_table[opp_ids];

		cluster_ppm_info[i].init = 1;
		cpufreq_cpu_put(policy);
	}
	return ret;

alloc_table_oom:
	/* release dvfs table of previous cluster */
	while (--i >= 0) {
		kfree(cluster_ppm_info[i].dvfs_tbl);
		cluster_ppm_info[i].init = 0;
	}
	return ret;
}

void exit_cpufreq_table(void)
{
	int i;

	for (i = 0; i < cluster_nr; i++) {
		kfree(cluster_ppm_info[i].dvfs_tbl);
		cluster_ppm_info[i].init = 0;
	}
}

static inline bool perf_do_check(u64 wallclock)
{
	bool do_check = false;
	unsigned long flags;

	/* check interval */
	spin_lock_irqsave(&check_lock, flags);
	if ((s64)(wallclock - checked_timestamp)
			>= (s64)(2 * NSEC_PER_MSEC)) {
		checked_timestamp = wallclock;
		long_trace_check_flag = !long_trace_check_flag;
		do_check = true;
	}
	spin_unlock_irqrestore(&check_lock, flags);

	return do_check;
}

bool hit_long_check(void)
{
	bool do_check = false;
	unsigned long flags;

	spin_lock_irqsave(&check_lock, flags);
	if (long_trace_check_flag)
		do_check = true;
	spin_unlock_irqrestore(&check_lock, flags);
	return do_check;
}

static void perf_common(void *data, struct rq *rq)
{
	u64 wallclock;

	wallclock = ktime_get_ns();
	if (!perf_do_check(wallclock))
		return;

	if (unlikely(!perf_common_init))
		return;

	atomic_inc(&perf_in_progress);
	perf_tracker(wallclock, hit_long_check());
	atomic_dec(&perf_in_progress);
}

static struct attribute *perf_attrs[] = {
#if IS_ENABLED(CONFIG_MTK_PERF_TRACKER)
	&perf_tracker_enable_attr.attr,

	&perf_fuel_gauge_enable_attr.attr,
	&perf_fuel_gauge_period_attr.attr,
#if IS_ENABLED(CONFIG_MTK_CHARGER)
	&perf_charger_enable_attr.attr,
	&perf_charger_period_attr.attr,
#endif
#if IS_ENABLED(CONFIG_MTK_GPU_SWPM_SUPPORT)
	&perf_gpu_pmu_enable_attr.attr,
	&perf_gpu_pmu_period_attr.attr,
#endif

#endif

	NULL,
};

static struct attribute_group perf_attr_group = {
	.attrs = perf_attrs,
};

static struct kobject *kobj;
static int init_perf_common_sysfs(void)
{
	int ret = 0;

	kobj = kobject_create_and_add("perf", &cpu_subsys.dev_root->kobj);
	if (!kobj)
		return -ENOMEM;

	ret = sysfs_create_group(kobj, &perf_attr_group);
	if (ret)
		goto error;
	kobject_uevent(kobj, KOBJ_ADD);
	return 0;

error:
	kobject_put(kobj);
	kobj = NULL;
	return ret;
}

static void cleanup_perf_common_sysfs(void)
{
	if (kobj) {
		sysfs_remove_group(kobj, &perf_attr_group);
		kobject_put(kobj);
		kobj = NULL;
	}
}

static int __init init_perf_common(void)
{
	int ret = 0;
	struct device_node *dn = NULL;
	struct platform_device *pdev = NULL;
	struct resource *csram_res = NULL;

	ret = init_perf_common_sysfs();
	if (ret)
		goto out;

	ret = init_cpu_cluster_info();
	if (ret)
		goto out;

	ret = init_cpufreq_table();
	if (ret)
		goto out;

#if IS_ENABLED(CONFIG_MTK_PERF_TRACKER)
	init_perf_freq_tracker();
#endif

	/* register tracepoint of scheduler_tick */
	ret = register_trace_android_vh_scheduler_tick(perf_common, NULL);
	if (ret) {
		pr_info("%s: register hooks failed, returned %d\n", TAG, ret);
		goto register_failed;
	}
	perf_common_init = 1;
	atomic_set(&perf_in_progress, 0);

	/* get cpufreq driver base address */
	dn = of_find_node_by_name(NULL, "cpuhvfs");
	if (!dn) {
		ret = -ENOMEM;
		pr_info("%s: find cpuhvfs node failed\n", TAG);
		goto get_base_failed;
	}

	pdev = of_find_device_by_node(dn);
	of_node_put(dn);
	if (!pdev) {
		ret = -ENODEV;
		pr_info("%s: cpuhvfs is not ready\n", TAG);
		goto get_base_failed;
	}

	csram_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!csram_res) {
		ret = -ENODEV;
		pr_info("%s: cpuhvfs resource is not found\n", TAG);
		goto get_base_failed;
	}

	csram_base = ioremap(csram_res->start, resource_size(csram_res));
	if (IS_ERR_OR_NULL((void *)csram_base)) {
		ret = -ENOMEM;
		pr_info("%s: find csram base failed\n", TAG);
		goto get_base_failed;
	}
	return ret;

get_base_failed:
#if IS_ENABLED(CONFIG_MEDIATEK_CPU_DVFS)
	return 0;
#endif
	exit_cpufreq_table();
register_failed:
	unregister_trace_android_vh_scheduler_tick(perf_common, NULL);
out:
	cleanup_perf_common_sysfs();
	return ret;
}

static void __exit exit_perf_common(void)
{
	while (atomic_read(&perf_in_progress) > 0)
		udelay(30);
	unregister_trace_android_vh_scheduler_tick(perf_common, NULL);
	exit_cpufreq_table();
	cleanup_perf_common_sysfs();
#if IS_ENABLED(CONFIG_MTK_PERF_TRACKER)
	exit_perf_freq_tracker();
#endif
}

module_init(init_perf_common);
module_exit(exit_perf_common);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("MediaTek performance tracker");
