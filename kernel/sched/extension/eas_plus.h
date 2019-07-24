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
#ifdef CONFIG_MTK_SCHED_CPU_PREFER
#define LB_CPU_PREFER   (0x10  << LB_POLICY_SHIFT)
#endif

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

#ifdef CONFIG_MTK_SCHED_LB_ENHANCEMENT
bool is_intra_domain(int prev, int target);
#endif

#ifdef CONFIG_MTK_IDLE_BALANCE_ENHANCEMENT
#define MIGR_IDLE_BALANCE      1
#define MIGR_IDLE_RUNNING      2

struct rq *__migrate_task(struct rq *rq, struct rq_flags *rf,
					struct task_struct *p, int dest_cpu);
int active_load_balance_cpu_stop(void *data);
unsigned int aggressive_idle_pull(int this_cpu);
#endif


#ifdef CONFIG_MTK_SCHED_CPU_PREFER
#define SCHED_PREFER_NONE   0
#define SCHED_PREFER_BIG    1
#define SCHED_PREFER_LITTLE 2
#define SCHED_PREFER_END    3

int task_prefer_fit(struct task_struct *p, int cpu);
int select_task_prefer_cpu(struct task_struct *p, int new_cpu);
void select_task_prefer_cpu_fair(struct task_struct *p, int *result);
#endif

#endif
