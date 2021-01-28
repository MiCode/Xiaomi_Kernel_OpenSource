// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#define pr_fmt(fmt) "cpuhp: " fmt

#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/topology.h>
#include <linux/suspend.h>
#include <linux/of.h>

#include "mtk_cpuhp_private.h"

static int is_multi_cluster(void)
{
	struct device_node *cn, *map;

	cn = of_find_node_by_path("/cpus");
	if (!cn) {
		pr_debug("No CPU information found in DT\n");
		return 0;
	}

	map = of_get_child_by_name(cn, "virtual-cpu-map");
	if (!map) {
		map = of_get_child_by_name(cn, "cpu-map");
		if (!map)
			return 0;

		return arch_get_nr_clusters() > 1 ? 1 : 0;
	}

	return 0;
}

static int get_cpu_topology(int cpu, int *isalone)
{
	int cluster;
	struct cpumask cpumask_this_cluster;

	cluster = arch_get_cluster_id(cpu);

	/*
	 * test this being hotplugged up/down CPU if the first/last core in
	 * this cluster. It would affect each platform's buck control policy.
	 */
	if (is_multi_cluster()) {
		arch_get_cluster_cpus(&cpumask_this_cluster, cluster);
		cpumask_and(&cpumask_this_cluster,
			    &cpumask_this_cluster, cpu_online_mask);

		pr_debug("cluster=%d, cpumask_weight(&cpumask_this_cluster)=%d\n",
			 cluster, cpumask_weight(&cpumask_this_cluster));

		if (cpumask_weight(&cpumask_this_cluster))
			*isalone = 0;
		else
			*isalone = 1;
	} else {
		*isalone = 0;
	}

	return cluster;
}

static int cpuhp_callback(struct notifier_block *nb,
			  unsigned long action, void *hcpu)
{
	int cluster;
	int isalone;

	ulong cpu = (ulong)hcpu;
	int rc = 0;

	/* do platform-depend hotplug implementation */
	switch (action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
		cluster = get_cpu_topology(cpu, &isalone);

		pr_debug("cluster=%d, cpu=%d, isalone=%d\n",
			 cluster, (int)cpu, isalone);

		rc = cpuhp_platform_cpuon(cluster, cpu, isalone, action);
		break;
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		cluster = get_cpu_topology(cpu, &isalone);

		pr_debug("cluster=%d, cpu=%d, isalone=%d\n",
			 cluster, (int)cpu, isalone);

		rc = cpuhp_platform_cpuoff(cluster, cpu, isalone, action);
		break;
	}

	return notifier_from_errno(rc);
}

#ifdef CONFIG_PM_SLEEP
static int cpuhp_pm_callback(struct notifier_block *nb,
			     unsigned long action, void *ptr)
{
	switch (action) {
	case PM_SUSPEND_PREPARE:
	case PM_HIBERNATION_PREPARE:
		break;

	case PM_POST_SUSPEND:
	case PM_POST_HIBERNATION:
		break;

	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}
#endif

static int __init cpuhp_init(void)
{
	int rc;

	pr_debug("%s+\n", __func__);

	hotcpu_notifier(cpuhp_callback, 0);
	pm_notifier(cpuhp_pm_callback, 0);
	ppm_notifier();
	rc = cpuhp_platform_init();

	pr_debug("%s-\n", __func__);

	return rc;
}
late_initcall(cpuhp_init);
