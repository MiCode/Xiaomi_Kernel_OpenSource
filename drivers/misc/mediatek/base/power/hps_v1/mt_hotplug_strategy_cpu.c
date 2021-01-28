// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

/**
 * @file    mt_hotplug_strategy_cpu.c
 * @brief   hotplug strategy(hps) - cpu
 */

#include <linux/kernel.h>
#include <linux/module.h>	/* MODULE_DESCRIPTION, MODULE_LICENSE */
#include <linux/init.h>		/* module_init, module_exit */
#include <linux/sched.h>	/* sched_get_* */
#include <linux/cpu.h>		/* cpu_up */
#include <linux/cpumask.h>
#include <linux/topology.h>

#include "mt_hotplug_strategy_internal.h"

#define HP_HAVE_MTK_TPLG		0
#define LOG_CPUMASK			0

#ifndef L_NUM_BASE_CUSTOM1
#define L_NUM_BASE_CUSTOM1		1
#endif
#ifndef L_NUM_BASE_CUSTOM2
#define L_NUM_BASE_CUSTOM2		1
#endif
#ifndef B_NUM_BASE_CUSTOM1
#define B_NUM_BASE_CUSTOM1		0
#endif
#ifndef B_NUM_BASE_CUSTOM2
#define B_NUM_BASE_CUSTOM2		0
#endif
#ifndef L_NUM_LIMIT_CUSTOM1
#define L_NUM_LIMIT_CUSTOM1		8
#endif
#ifndef L_NUM_LIMIT_CUSTOM2
#define L_NUM_LIMIT_CUSTOM2		8
#endif
#ifndef B_NUM_LIMIT_CUSTOM1
#define B_NUM_LIMIT_CUSTOM1		8
#endif
#ifndef B_NUM_LIMIT_CUSTOM2
#define B_NUM_LIMIT_CUSTOM2		8
#endif

/*
 * hps cpu interface - cpumask
 */
int hps_cpu_is_cpu_big(int cpu)
{
	if (!cpumask_empty(&hps_ctxt.big_cpumask)) {
		if (cpumask_test_cpu(cpu, &hps_ctxt.big_cpumask))
			return 1;
		else
			return 0;
	} else
		return 0;
}

int hps_cpu_is_cpu_little(int cpu)
{
	if (!cpumask_empty(&hps_ctxt.little_cpumask)) {
		if (cpumask_test_cpu(cpu, &hps_ctxt.little_cpumask))
			return 1;
		else
			return 0;
	} else
		return 0;
}

unsigned int num_online_little_cpus(void)
{
	struct cpumask dst_cpumask;

	cpumask_and(&dst_cpumask, &hps_ctxt.little_cpumask, cpu_online_mask);
	return cpumask_weight(&dst_cpumask);
}

unsigned int num_online_big_cpus(void)
{
	struct cpumask dst_cpumask;

	cpumask_and(&dst_cpumask, &hps_ctxt.big_cpumask, cpu_online_mask);
	return cpumask_weight(&dst_cpumask);
}

static unsigned int min_of_all(unsigned int *v[], size_t n)
{
	unsigned int m = *v[0];
	size_t i;

	for (i = 1; i < n; i++)
		m = min(m, *v[i]);

	return m;
}

static unsigned int max_of_all(unsigned int *v[], size_t n)
{
	unsigned int m = *v[0];
	size_t i;

	for (i = 1; i < n; i++)
		m = max(m, *v[i]);

	return m;
}

unsigned int num_limit_little_cpus(void)
{
	unsigned int *v[] = NUM_LIMIT_LITTLE_LIST;

	return min_of_all(v, ARRAY_SIZE(v));
}

unsigned int num_limit_big_cpus(void)
{
	unsigned int *v[] = NUM_LIMIT_BIG_LIST;

	return min_of_all(v, ARRAY_SIZE(v));
}

unsigned int num_base_little_cpus(void)
{
	unsigned int *v[] = NUM_BASE_LITTLE_LIST;

	return max_of_all(v, ARRAY_SIZE(v));
}

unsigned int num_base_big_cpus(void)
{
	unsigned int *v[] = NUM_BASE_BIG_LIST;

	return max_of_all(v, ARRAY_SIZE(v));
}

/*
 * hps cpu interface - scheduler
 */
unsigned int hps_cpu_get_percpu_load(int cpu)
{
#ifdef CONFIG_MTK_SCHED_RQAVG_US
	return sched_get_percpu_load(cpu, 1, 0);
#else
	return 100;
#endif
}

unsigned int hps_cpu_get_nr_heavy_task(void)
{
#ifdef CONFIG_MTK_SCHED_RQAVG_US
	return sched_get_nr_heavy_task();
#else
	return 0;
#endif
}

void hps_cpu_get_tlp(unsigned int *avg, unsigned int *iowait_avg)
{
#ifdef CONFIG_MTK_SCHED_RQAVG_KS
	sched_get_nr_running_avg((int *)avg, (int *)iowait_avg);
#else
	*avg = 0;
	*iowait_avg = 0;
#endif
}

void hps_cpu_get_big_little_cpumasks(
		struct cpumask *big, struct cpumask *little)
{
#if HP_HAVE_MTK_TPLG
	unsigned int cpu;

	cpumask_clear(big);
	cpumask_clear(little);

	for_each_possible_cpu(cpu) {
		if (arch_cpu_is_big(cpu))
			cpumask_set_cpu(cpu, big);
		else
			cpumask_set_cpu(cpu, little);
	}
#else
	unsigned int cpu;
	int id;
	int cluster_min = INT_MAX, cluster_max = INT_MIN;

	cpumask_clear(big);
	cpumask_clear(little);

	for_each_possible_cpu(cpu) {
		id = topology_physical_package_id(cpu);

		cluster_min = min(cluster_min, id);
		cluster_max = max(cluster_max, id);
	}

	for_each_possible_cpu(cpu) {
		id = topology_physical_package_id(cpu);

		if ((cluster_min < cluster_max) && (id == cluster_max))
			cpumask_set_cpu(cpu, big);
		else
			cpumask_set_cpu(cpu, little);
	}
#endif /* HP_HAVE_MTK_TPLG */
}

int hps_cpu_up(unsigned int cpu)
{
	int r;

	lock_device_hotplug();
	r = device_online(get_cpu_device(cpu));
	unlock_device_hotplug();

	return r;
}

int hps_cpu_down(unsigned int cpu)
{
	int r;

	lock_device_hotplug();
	r = device_offline(get_cpu_device(cpu));
	unlock_device_hotplug();

	return r;
}

/*
 * init
 */
int hps_cpu_init(void)
{
	int r = 0;
	int nl = 0, nb = 0;
#if LOG_CPUMASK
	char str1[32];
#endif

	log_info("%s\n", __func__);

	/* init cpu arch in hps_ctxt */
	/* init cpumask */
	cpumask_clear(&hps_ctxt.little_cpumask);
	cpumask_clear(&hps_ctxt.big_cpumask);

	hps_cpu_get_big_little_cpumasks(
		&hps_ctxt.big_cpumask, &hps_ctxt.little_cpumask);

#if LOG_CPUMASK
	cpulist_scnprintf(str1, sizeof(str1), &hps_ctxt.little_cpumask);
	log_info("hps_ctxt.little_cpumask: %s\n", str1);
	cpulist_scnprintf(str1, sizeof(str1), &hps_ctxt.big_cpumask);
	log_info("hps_ctxt.big_cpumask: %s\n", str1);
#endif

	if (cpumask_weight(&hps_ctxt.little_cpumask) == 0) {
		cpumask_copy(&hps_ctxt.little_cpumask, &hps_ctxt.big_cpumask);
		cpumask_clear(&hps_ctxt.big_cpumask);
	}

	nl = cpumask_weight(&hps_ctxt.little_cpumask);
	nb = cpumask_weight(&hps_ctxt.big_cpumask);

	/* verify arch is hmp or smp */
	if (!cpumask_empty(&hps_ctxt.little_cpumask) &&
		!cpumask_empty(&hps_ctxt.big_cpumask)) {
		unsigned int cpu;

		hps_ctxt.is_hmp = 1;
		hps_ctxt.little_cpu_id_min = num_possible_cpus();
		hps_ctxt.big_cpu_id_min = num_possible_cpus();

		for_each_cpu((cpu), &hps_ctxt.little_cpumask) {
			if (cpu < hps_ctxt.little_cpu_id_min)
				hps_ctxt.little_cpu_id_min = cpu;
			if (cpu > hps_ctxt.little_cpu_id_max)
				hps_ctxt.little_cpu_id_max = cpu;
		}

		for_each_cpu((cpu), &hps_ctxt.big_cpumask) {
			if (cpu < hps_ctxt.big_cpu_id_min)
				hps_ctxt.big_cpu_id_min = cpu;
			if (cpu > hps_ctxt.big_cpu_id_max)
				hps_ctxt.big_cpu_id_max = cpu;
		}
	} else {
		hps_ctxt.is_hmp = 0;
		hps_ctxt.little_cpu_id_min = 0;
		hps_ctxt.little_cpu_id_max = num_possible_little_cpus() - 1;
	}

	/* init bound in hps_ctxt */
	hps_ctxt.little_num_base_perf_serv = 1;
	hps_ctxt.little_num_base_custom1 = min(nl, L_NUM_BASE_CUSTOM1);
	hps_ctxt.little_num_base_custom2 = min(nl, L_NUM_BASE_CUSTOM2);
	hps_ctxt.little_num_limit_thermal = nl;
	hps_ctxt.little_num_limit_low_battery = nl;
	hps_ctxt.little_num_limit_ultra_power_saving = nl;
	hps_ctxt.little_num_limit_power_serv = nl;
	hps_ctxt.little_num_limit_custom1 = min(nl, L_NUM_LIMIT_CUSTOM1);
	hps_ctxt.little_num_limit_custom2 = min(nl, L_NUM_LIMIT_CUSTOM2);
	hps_ctxt.big_num_base_perf_serv = 0;
	hps_ctxt.big_num_base_custom1 = min(nb, B_NUM_BASE_CUSTOM1);
	hps_ctxt.big_num_base_custom2 = min(nb, B_NUM_BASE_CUSTOM2);
	hps_ctxt.big_num_limit_thermal = nb;
	hps_ctxt.big_num_limit_low_battery = nb;
	hps_ctxt.big_num_limit_ultra_power_saving = nb;
	hps_ctxt.big_num_limit_power_serv = nb;
	hps_ctxt.big_num_limit_custom1 = min(nb, B_NUM_LIMIT_CUSTOM1);
	hps_ctxt.big_num_limit_custom2 = min(nb, B_NUM_LIMIT_CUSTOM2);

	log_info(
		"%s: little_cpu_id_min: %u, little_cpu_id_max: %u, big_cpu_id_min: %u, big_cpu_id_max: %u\n",
		__func__,
		hps_ctxt.little_cpu_id_min, hps_ctxt.little_cpu_id_max,
		hps_ctxt.big_cpu_id_min, hps_ctxt.big_cpu_id_max);

	return r;
}

/*
 * deinit
 */
int hps_cpu_deinit(void)
{
	int r = 0;

	log_info("%s\n", __func__);

	return r;
}

