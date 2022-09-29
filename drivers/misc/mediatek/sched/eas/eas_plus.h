/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _EAS_PLUS_H
#define _EAS_PLUS_H
#include <linux/ioctl.h>

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
#define LB_IRQ_BEST_IDLE    (0x410)
#define LB_IRQ_SYS_MAX_SPARE   (0x420)
#define LB_IRQ_MAX_SPARE   (0x440)
#define LB_IRQ_BACKUP_CURR         (0x480)
#define LB_IRQ_BACKUP_PREV         (0x481)
#define LB_IRQ_BACKUP_ALLOWED      (0x482)
#define LB_RT_FAIL         (0x1000)
#define LB_RT_FAIL_PD      (0x1001)
#define LB_RT_FAIL_CPU     (0x1002)
#define LB_RT_SYNC      (0x2000)
#define LB_RT_IDLE      (0x4000)
#define LB_RT_LOWEST_PRIO         (0x8000)
#define LB_RT_LOWEST_PRIO_NORMAL  (0x8001)
#define LB_RT_LOWEST_PRIO_RT      (0x8002)
#define LB_RT_SOURCE_CPU       (0x10000)
#define LB_RT_FAIL_SYNC        (0x20000)
#define LB_RT_FAIL_RANDOM      (0x40000)
#define LB_RT_NO_LOWEST_RQ     (0x80000)

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
extern int task_fits_capacity(struct task_struct *p, long capacity);

#if IS_ENABLED(CONFIG_MTK_EAS)
extern void mtk_find_busiest_group(void *data, struct sched_group *busiest,
		struct rq *dst_rq, int *out_balance);
extern void mtk_find_energy_efficient_cpu(void *data, struct task_struct *p,
		int prev_cpu, int sync, int *new_cpu);
extern void mtk_cpu_overutilized(void *data, int cpu, int *overutilized);
extern unsigned long mtk_em_cpu_energy(struct em_perf_domain *pd,
		unsigned long max_util, unsigned long sum_util,
		unsigned long allowed_cpu_cap, unsigned int *cpu_temp);
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
extern void mtk_pelt_rt_tp(void *data, struct rq *rq);
extern void mtk_sched_switch(void *data, struct task_struct *prev,
		struct task_struct *next, struct rq *rq);

extern void set_wake_sync(unsigned int sync);
extern unsigned int get_wake_sync(void);
extern void set_uclamp_min_ls(unsigned int val);
extern unsigned int get_uclamp_min_ls(void);
extern void set_newly_idle_balance_interval_us(unsigned int interval_us);
extern unsigned int get_newly_idle_balance_interval_us(void);
extern void set_get_thermal_headroom_interval_tick(unsigned int tick);
extern unsigned int get_thermal_headroom_interval_tick(void);

extern void init_system_cpumask(void);
extern void set_system_cpumask_int(unsigned int val);
extern struct cpumask *get_system_cpumask(void);

extern void get_most_powerful_pd_and_util_Th(void);

#define EAS_SYNC_SET                            _IOW('g', 1,  unsigned int)
#define EAS_SYNC_GET                            _IOW('g', 2,  unsigned int)
#define EAS_PERTASK_LS_SET                      _IOW('g', 3,  unsigned int)
#define EAS_PERTASK_LS_GET                      _IOR('g', 4,  unsigned int)
#define EAS_ACTIVE_MASK_GET                     _IOR('g', 5,  unsigned int)
#define EAS_NEWLY_IDLE_BALANCE_INTERVAL_SET	_IOW('g', 6,  unsigned int)
#define EAS_NEWLY_IDLE_BALANCE_INTERVAL_GET	_IOR('g', 7,  unsigned int)
#define EAS_GET_THERMAL_HEADROOM_INTERVAL_SET	_IOW('g', 8,  unsigned int)
#define EAS_GET_THERMAL_HEADROOM_INTERVAL_GET	_IOR('g', 9,  unsigned int)
#define EAS_SET_SYSTEM_MASK			_IOW('g', 10,  unsigned int)
#define EAS_GET_SYSTEM_MASK			_IOW('g', 11,  unsigned int)
#define EAS_SBB_ALL_SET				_IOW('g', 12,  unsigned int)
#define EAS_SBB_ALL_UNSET			_IOW('g', 13,  unsigned int)
#define EAS_SBB_GROUP_SET			_IOW('g', 14,  unsigned int)
#define EAS_SBB_GROUP_UNSET			_IOW('g', 15,  unsigned int)
#define EAS_SBB_TASK_SET			_IOW('g', 16,  unsigned int)
#define EAS_SBB_TASK_UNSET			_IOW('g', 17,  unsigned int)
#define EAS_SBB_ACTIVE_RATIO		_IOW('g', 18,  unsigned int)

#if IS_ENABLED(CONFIG_MTK_NEWIDLE_BALANCE)
extern void mtk_sched_newidle_balance(void *data, struct rq *this_rq,
		struct rq_flags *rf, int *pulled_task, int *done);
#endif

extern unsigned long calc_pwr(int cpu, unsigned long task_util);
extern unsigned long calc_pwr_eff(int cpu, unsigned long cpu_util);
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
				struct task_struct *p, int flags);
extern void mtk_select_task_rq_rt(void *data, struct task_struct *p, int cpu, int sd_flag,
				int flags, int *target_cpu);
extern int mtk_sched_asym_cpucapacity;

extern void mtk_find_lowest_rq(void *data, struct task_struct *p, struct cpumask *lowest_mask,
				int ret, int *lowest_cpu);

extern struct cpumask __cpu_pause_mask;
#define cpu_pause_mask ((struct cpumask *)&__cpu_pause_mask)

#if IS_ENABLED(CONFIG_MTK_CORE_PAUSE)
#define cpu_paused(cpu) cpumask_test_cpu((cpu), cpu_pause_mask)

extern void sched_pause_init(void);
#else
#define cpu_paused(cpu) 0
#endif

#endif
