// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/spinlock.h>
#include <linux/time.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/energy_model.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <trace/hooks/sched.h>

#include <perf_tracker_internal.h>

static u64 checked_timestamp;
static bool long_trace_check_flag;
static DEFINE_SPINLOCK(check_lock);
static int perf_common_init;
static atomic_t perf_in_progress;

void __iomem *csram_base;
struct ppm_data ppm_main_info;
struct em_perf_domain *em_pd;

#define MAX_CLUSTER_NR	3
static int first_cpu_in_cluster[MAX_CLUSTER_NR] = {0};
// module_param_array(first_cpu_in_cluster, int, NULL, 0600);

int cluster_nr = -1;
// module_param(cluster_nr, int, 0600);

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
			pr_info("%s: no EM found for CPU%d\n", __func__, i);
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

	ppm_main_info.cluster_info = kcalloc(cluster_nr,
			sizeof(*ppm_main_info.cluster_info), GFP_KERNEL);
	if (!ppm_main_info.cluster_info) {
		ret = -ENOMEM;
		pr_info("%s: fail to allocate memory for cluster info", __func__);
		goto out_cluster_oom;
	}

	for (i = 0; i < cluster_nr; i++) {
		int opp_ids, opp_nr;

		policy = cpufreq_cpu_get(first_cpu_in_cluster[i]);
		if (!policy) {
			pr_info("%s: policy is null", __func__);
			return -EINVAL;
		}
		/* get CPU OPP number */
		opp_nr = get_opp_count(policy);

		ppm_main_info.cluster_info[i].dvfs_tbl =
			kcalloc(opp_nr,
			       sizeof(*ppm_main_info.cluster_info[i].dvfs_tbl),
			       GFP_KERNEL);
		if (!ppm_main_info.cluster_info[i].dvfs_tbl) {
			ret = -ENOMEM;
			pr_info("Failed to allocate memory for dvfs table");
			goto out_tbl_oom;
		}

		for (opp_ids = 0; opp_ids < opp_nr; opp_ids++)
			ppm_main_info.cluster_info[i].dvfs_tbl[opp_ids] =
				policy->freq_table[opp_ids];
	}
	return ret;

out_tbl_oom:
	while (--i >= 0)
		kfree(ppm_main_info.cluster_info[i].dvfs_tbl);
	kfree(ppm_main_info.cluster_info);
out_cluster_oom:
	return ret;
}

void exit_cpufreq_table(void)
{
	int i;

	for (i = 0; i < cluster_nr; i++)
		kfree(ppm_main_info.cluster_info[i].dvfs_tbl);
	kfree(ppm_main_info.cluster_info);
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

	ret = init_perf_common_sysfs();
	if (ret)
		goto out;

	ret = init_cpu_cluster_info();
	if (ret)
		goto out;

	ret = init_cpufreq_table();
	if (ret)
		goto out;

	/* get cpufreq driver base address */
	dn = of_find_compatible_node(NULL, NULL, "mediatek,mt6873-mcupm-dvfs");
	if (!dn) {
		pr_info("find mcupm-dvfsp node failed\n");
		goto get_base_failed;
	}

	csram_base = of_iomap(dn, 0);
	of_node_put(dn);
	if (IS_ERR_OR_NULL((void *)csram_base)) {
		pr_info("find mcupm-dvfsp node failed\n");
		goto get_base_failed;
	}

	/* register tracepoint of scheduler_tick */
	ret = register_trace_android_vh_scheduler_tick(perf_common, NULL);
	if (ret) {
		pr_debug("perf_comm: register hooks failed, returned %d\n", ret);
		goto register_failed;
	}
	perf_common_init = 1;
	atomic_set(&perf_in_progress, 0);
	return ret;

register_failed:
	cleanup_perf_common_sysfs();
get_base_failed:
	exit_cpufreq_table();
out:
	return ret;
}

static void __exit exit_perf_common(void)
{
	while (atomic_read(&perf_in_progress) > 0)
		udelay(30);
	unregister_trace_android_vh_scheduler_tick(perf_common, NULL);
	exit_cpufreq_table();
	cleanup_perf_common_sysfs();
}

module_init(init_perf_common);
module_exit(exit_perf_common);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("MediaTek performance tracker");
