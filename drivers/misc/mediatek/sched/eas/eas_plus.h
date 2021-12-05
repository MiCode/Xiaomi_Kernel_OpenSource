/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <sched/pelt.h>

#ifndef _EAS_PLUS_H
#define _EAS_PLUS_H

#define MIGR_IDLE_BALANCE               1
#define MIGR_IDLE_PULL_MISFIT_RUNNING   2
#define MIGR_TICK_PULL_MISFIT_RUNNING   3

DECLARE_PER_CPU(unsigned long, max_freq_scale);
DECLARE_PER_CPU(unsigned long, min_freq);

#define LB_FAIL         (0x01)
#define LB_SYNC         (0x02)
#define LB_ZERO_UTIL    (0x04)
#define LB_PREV         (0x08)
#define LB_LATENCY_SENSITIVE_BEST_IDLE_CPU      (0x10)
#define LB_LATENCY_SENSITIVE_IDLE_MAX_SPARE_CPU (0x20)
#define LB_LATENCY_SENSITIVE_MAX_SPARE_CPU      (0x40)
#define LB_BEST_ENERGY_CPU      (0x100)
#define LB_MAX_SPARE_CPU        (0x200)
#define LB_IN_INTERRUPT		(0x400)
#define LB_RT_FAIL      (0x1000)
#define LB_RT_SYNC      (0x2000)
#define LB_RT_IDLE      (0x4000)
#define LB_RT_LOWEST_PRIO  (0x8000)

#ifdef CONFIG_SMP
/*
 * The margin used when comparing utilization with CPU capacity.
 *
 * (default: ~20%)
 */
#define fits_capacity(cap, max) ((cap) * 1280 < (max) * 1024)
unsigned long capacity_of(int cpu);
#endif

extern unsigned long cpu_util(int cpu);
extern int mtk_static_power_init(void);
extern int task_fits_capacity(struct task_struct *p, long capacity);

#if IS_ENABLED(CONFIG_MTK_EAS)
extern void mtk_find_busiest_group(void *data, struct sched_group *busiest,
		struct rq *dst_rq, int *out_balance);
extern void mtk_find_energy_efficient_cpu(void *data, struct task_struct *p,
		int prev_cpu, int sync, int *new_cpu);
extern void mtk_cpu_overutilized(void *data, int cpu, int *overutilized);
extern unsigned long mtk_em_cpu_energy(struct em_perf_domain *pd,
		unsigned long max_util, unsigned long sum_util, unsigned int *cpu_temp);
extern unsigned int mtk_get_leakage(unsigned int cpu, unsigned int opp, unsigned int temperature);
extern unsigned int new_idle_balance_interval_ns;
#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
extern int sort_thermal_headroom(struct cpumask *cpus, int *cpu_order);
extern unsigned int thermal_headroom_interval_tick;
#endif

extern void mtk_freq_limit_notifier_register(void);
extern int init_sram_info(void);
extern void mtk_tick_entry(void *data, struct rq *rq);
extern void mtk_set_wake_flags(void *data, int *wake_flags, unsigned int *mode);
extern void mtk_update_cpu_capacity(void *data, int cpu, unsigned long *capacity);

#if IS_ENABLED(CONFIG_MTK_NEWIDLE_BALANCE)
extern void mtk_sched_newidle_balance(void *data, struct rq *this_rq,
		struct rq_flags *rf, int *pulled_task, int *done);
#endif
#endif

extern int migrate_running_task(int this_cpu, struct task_struct *p, struct rq *target,
		int reason);
extern void hook_scheduler_tick(void *data, struct rq *rq);
#if IS_ENABLED(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)
extern void task_check_for_rotation(struct rq *src_rq);
extern void rotat_after_enqueue_task(void *data, struct rq *rq, struct task_struct *p);
extern void rotat_task_stats(void *data, struct task_struct *p);
extern void rotat_task_newtask(void __always_unused *data, struct task_struct *p,
				unsigned long clone_flags);
#endif
extern bool check_freq_update_for_time(struct update_util_data *hook, u64 time);
extern void mtk_hook_after_enqueue_task(void *data, struct rq *rq,
				struct task_struct *p);
extern void mtk_select_task_rq_rt(void *data, struct task_struct *p, int cpu, int sd_flag,
				int flags, int *target_cpu);
extern int mtk_sched_asym_cpucapacity;
#endif

#ifdef CONFIG_SMP
static inline unsigned long task_util(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.util_avg);
}

static inline unsigned long _task_util_est(struct task_struct *p)
{
	struct util_est ue = READ_ONCE(p->se.avg.util_est);

	return max(ue.ewma, (ue.enqueued & ~UTIL_AVG_UNCHANGED));
}

static inline unsigned long task_util_est(struct task_struct *p)
{
	return max(task_util(p), _task_util_est(p));
}

#ifdef CONFIG_UCLAMP_TASK
static inline unsigned long uclamp_task_util(struct task_struct *p)
{
	return clamp(task_util_est(p),
			uclamp_eff_value(p, UCLAMP_MIN),
			uclamp_eff_value(p, UCLAMP_MAX));
}
#else
static inline unsigned long uclamp_task_util(struct task_struct *p)
{
	return task_util_est(p);
}
#endif
#endif

