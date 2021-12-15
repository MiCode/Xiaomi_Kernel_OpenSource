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

#ifdef CONFIG_MTK_SCHED_EXTENSION

#define MIGR_IDLE_BALANCE      1
#define MIGR_IDLE_RUNNING      2
#define MIGR_ROTATION          3
DECLARE_PER_CPU(struct task_struct*, migrate_task);

struct rq *__migrate_task(struct rq *rq, struct rq_flags *rf,
				struct task_struct *p, int dest_cpu);
int active_load_balance_cpu_stop(void *data);
unsigned int aggressive_idle_pull(int this_cpu);
int migrate_running_task(int this_cpu, struct task_struct *p,
				struct rq *target);

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

#if defined(CONFIG_ENERGY_MODEL) && defined(CONFIG_CPU_FREQ_GOV_SCHEDUTIL)
unsigned int cpu_is_slowest(int cpu);
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
int cpu_prefer(struct task_struct *task);
#endif

#ifdef CONFIG_MTK_SCHED_BIG_TASK_MIGRATE
#include "../../drivers/misc/mediatek/include/mt-plat/eas_ctrl.h"
#define TASK_ROTATION_THRESHOLD_NS      6000000
#define HEAVY_TASK_NUM  4

struct task_rotate_work {
	struct work_struct w;
	struct task_struct *src_task;
	struct task_struct *dst_task;
	int src_cpu;
	int dst_cpu;
};

struct task_rotate_reset_uclamp_work {
	struct work_struct w;
};

DECLARE_PER_CPU(struct task_rotate_work, task_rotate_works);
extern bool big_task_rotation_enable;
extern void task_rotate_work_init(void);
extern void check_for_migration(struct task_struct *p);
extern int is_reserved(int cpu);
extern bool is_min_capacity_cpu(int cpu);
extern bool is_max_capacity_cpu(int cpu);
extern struct task_rotate_reset_uclamp_work task_rotate_reset_uclamp_works;
extern bool set_uclamp;
#endif

/**
 *for isolation
 */
extern struct cpumask cpu_all_masks;

#define tsk_cpus_allowed(tsk) (&(tsk)->cpus_allowed)

static inline struct cpumask *sched_group_cpus(struct sched_group *sg)
{
	return to_cpumask(sg->cpumask);
}

static inline struct cpumask *sched_group_mask(struct sched_group *sg)
{
	return to_cpumask(sg->sgc->cpumask);
}

void
iso_detach_one_task(struct task_struct *p, struct rq *rq,
				struct list_head *tasks);
void iso_attach_tasks(struct list_head *tasks, struct rq *rq);
void migrate_tasks(struct rq *dead_rq, struct rq_flags *rf,
			bool migrate_pinned_tasks);
void iso_init_sched_groups_capacity(int cpu, struct sched_domain *sd);
void iso_calc_load_migrate(struct rq *rq);
void nohz_balance_clear_nohz_mask(int cpu);
int __sched_deisolate_cpu_unlocked(int cpu);
int _sched_isolate_cpu(int cpu);
int _sched_deisolate_cpu(int cpu);
/*
 * for sched_boost
 */
#ifdef CONFIG_MTK_SCHED_CPU_PREFER
int task_cs_cpu_perfer(struct task_struct *task);
#endif
#endif /* CONFIG_MTK_SCHED_EXTENSION */

#endif

/* sched:  add for print aee log */
#if defined(CONFIG_SMP) && defined(CONFIG_MTK_SCHED_EXTENSION)
static inline int rq_cpu(const struct rq *rq) { return rq->cpu; }
#else
static inline int rq_cpu(const struct rq *rq) { return 0; }
#endif

#ifdef CONFIG_MTK_SCHED_EXTENSION
extern unsigned int capacity_margin;
static inline unsigned long map_util_freq_with_margin(
					unsigned long util,
					unsigned long freq,
					unsigned long cap)
{
	freq = freq * util / cap;
	freq = freq / SCHED_CAPACITY_SCALE * capacity_margin;
	return freq;
}
#endif
extern unsigned long capacity_spare_without(int cpu, struct task_struct *p);
