/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _EXTENSION_EAS_PLUS_H
#define _EXTENSION_EAS_PLUS_H

#include <linux/math64.h>
#include <linux/kobject.h>
#include <linux/cpumask.h>
#include <linux/list_sort.h>

#define LB_POLICY_SHIFT 16
#define LB_CPU_MASK ((1 << LB_POLICY_SHIFT) - 1)

#define LB_PREV          (0x0  << LB_POLICY_SHIFT)
#define LB_EAS           (0x1  << LB_POLICY_SHIFT)
#define LB_WAKE_AFFINE   (0x2  << LB_POLICY_SHIFT)
#define LB_IDLEST        (0x4  << LB_POLICY_SHIFT)
#define LB_IDLE_SIBLING  (0x8  << LB_POLICY_SHIFT)

#if defined(CONFIG_ENERGY_MODEL) && defined(CONFIG_CPU_FREQ_GOV_SCHEDUTIL)
struct perf_order_domain {
	struct cpumask cpus;
	struct cpumask possible_cpus;
	struct list_head perf_order_domains;
};

extern void init_perf_order_domains(struct perf_domain *pd);
extern struct list_head perf_order_domains;
#define perf_order_cpu_domain(cpu) (per_cpu(perf_order_cpu_domain, (cpu)))
#define for_each_perf_domain(pod) for_each_perf_domain_descending(pod)
#define for_each_perf_domain_descending(pod) \
		list_for_each_entry(pod, &perf_order_domains, \
					perf_order_domains)

#define for_each_perf_domain_ascending(pod) \
		list_for_each_entry_reverse(pod, &perf_order_domains, \
						perf_order_domains)
bool pod_is_ready(void);
#endif

#ifdef CONFIG_MTK_SCHED_INTEROP
extern bool is_rt_throttle(int cpu);
#endif

bool is_intra_domain(int prev, int target);

#endif
