/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/**
 * @file    mt_hotplug_strategy_internal.h
 * @brief   hotplug strategy(hps) - internal header file
 */

#ifndef __MT_HOTPLUG_STRATEGY_INTERNAL_H__
#define __MT_HOTPLUG_STRATEGY_INTERNAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/platform_device.h>	/* struct platform_driver */
#include <linux/kthread.h>		/* struct task_struct */

#include <mt_hotplug_strategy_platform.h>	/* platform defines */

/*
 * LOG
 */
#define hps_err(fmt, args...)		pr_notice("[HPS] " fmt, ##args)
#define hps_warn(fmt, args...)		pr_notice("[HPS] " fmt, ##args)

#if EN_LOG_NOTICE
#define hps_notice(fmt, args...)	pr_notice("[HPS] " fmt, ##args)
#else
#define hps_notice(fmt, args...)	pr_debug("[HPS] " fmt, ##args)
#endif

#if EN_LOG_INFO
#define hps_info(fmt, args...)		pr_info("[HPS] " fmt, ##args)
#else
#define hps_info(fmt, args...)		pr_debug("[HPS] " fmt, ##args)
#endif

#if EN_LOG_DEBUG
#define hps_debug(fmt, args...)		pr_info("[HPS] " fmt, ##args)
#else
#define hps_debug(fmt, args...)		pr_debug("[HPS] " fmt, ##args)
#endif

enum hps_init_state_e {
	INIT_STATE_NOT_READY = 0,
	INIT_STATE_DONE
};

#if !defined(HPS_PERIODICAL_BY_WAIT_QUEUE) && !defined(HPS_PERIODICAL_BY_TIMER)
#define HPS_PERIODICAL_BY_WAIT_QUEUE	0
#define HPS_PERIODICAL_BY_TIMER		1
#endif

enum hps_ctxt_action_e {
	ACTION_NONE = 0,
	ACTION_BASE_LITTLE,		/* bit  1, 0x0002 */
	ACTION_BASE_BIG,		/* bit  2, 0x0004 */
	ACTION_LIMIT_LITTLE,		/* bit  3, 0x0008 */
	ACTION_LIMIT_BIG,		/* bit  4, 0x0010 */
	ACTION_RUSH_BOOST_LITTLE,	/* bit  5, 0x0020 */
	ACTION_RUSH_BOOST_BIG,		/* bit  6, 0x0040 */
	ACTION_UP_LITTLE,		/* bit  7, 0x0080 */
	ACTION_UP_BIG,			/* bit  8, 0x0100 */
	ACTION_DOWN_LITTLE,		/* bit  9, 0x0200 */
	ACTION_DOWN_BIG,		/* bit 10, 0x0400 */
	ACTION_BIG_TO_LITTLE,		/* bit 11, 0x0800 */
	ACTION_INPUT,			/* bit 12, 0x1000 */
	ACTION_COUNT
};

struct hps_ctxt_struct {
	/* state */
	unsigned int init_state;

	/* enabled */
	unsigned int enabled;
	unsigned int log_mask;

	/* core */
	struct mutex lock;		/* Synchronizes accesses */
	struct task_struct *tsk_struct_ptr;

#if HPS_PERIODICAL_BY_WAIT_QUEUE
	wait_queue_head_t wait_queue;
#elif HPS_PERIODICAL_BY_TIMER
	struct timer_list hps_tmr_dfr;
	struct timer_list hps_tmr;
	struct timer_list *active_hps_tmr;
#endif

	/* cpu arch */
	unsigned int is_hmp;
	struct cpumask little_cpumask;
	struct cpumask big_cpumask;
	unsigned int little_cpu_id_min;
	unsigned int little_cpu_id_max;
	unsigned int big_cpu_id_min;
	unsigned int big_cpu_id_max;

	/* algo config */
	unsigned int up_threshold;
	unsigned int up_times;
	unsigned int down_threshold;
	unsigned int down_times;
	unsigned int rush_boost_enabled;
	unsigned int rush_boost_threshold;
	unsigned int rush_boost_times;
	unsigned int quick_landing_enabled;
	unsigned int tlp_times;

	/* algo bound */
	unsigned int little_num_base_perf_serv;
	unsigned int little_num_base_custom1;
	unsigned int little_num_base_custom2;
	unsigned int little_num_limit_custom1;
	unsigned int little_num_limit_custom2;
	unsigned int little_num_limit_thermal;
	unsigned int little_num_limit_low_battery;
	unsigned int little_num_limit_ultra_power_saving;
	unsigned int little_num_limit_power_serv;
	unsigned int big_num_base_perf_serv;
	unsigned int big_num_base_custom1;
	unsigned int big_num_base_custom2;
	unsigned int big_num_limit_custom1;
	unsigned int big_num_limit_custom2;
	unsigned int big_num_limit_thermal;
	unsigned int big_num_limit_low_battery;
	unsigned int big_num_limit_ultra_power_saving;
	unsigned int big_num_limit_power_serv;

	/* algo statistics */
	unsigned int cur_loads;
	unsigned int cur_tlp;
	unsigned int cur_iowait;
	unsigned int cur_nr_heavy_task;
	unsigned int up_loads_sum;
	unsigned int up_loads_count;
	unsigned int up_loads_history[MAX_CPU_UP_TIMES];
	unsigned int up_loads_history_index;
	unsigned int down_loads_sum;
	unsigned int down_loads_count;
	unsigned int down_loads_history[MAX_CPU_DOWN_TIMES];
	unsigned int down_loads_history_index;
	unsigned int rush_count;
	unsigned int tlp_sum;
	unsigned int tlp_count;
	unsigned int tlp_history[MAX_TLP_TIMES];
	unsigned int tlp_history_index;
	unsigned int tlp_avg;

	/* algo action */
	unsigned int action;
	atomic_t is_ondemand;
};

#define NUM_LIMIT_LITTLE_LIST	{				\
		&hps_ctxt.little_num_limit_thermal,		\
		&hps_ctxt.little_num_limit_low_battery,		\
		&hps_ctxt.little_num_limit_ultra_power_saving,	\
		&hps_ctxt.little_num_limit_power_serv,		\
		&hps_ctxt.little_num_limit_custom1,		\
		&hps_ctxt.little_num_limit_custom2		\
	}

#define NUM_LIMIT_BIG_LIST	{				\
		&hps_ctxt.big_num_limit_thermal,		\
		&hps_ctxt.big_num_limit_low_battery,		\
		&hps_ctxt.big_num_limit_ultra_power_saving,	\
		&hps_ctxt.big_num_limit_power_serv,		\
		&hps_ctxt.big_num_limit_custom1,		\
		&hps_ctxt.big_num_limit_custom2			\
	}

#define NUM_BASE_LITTLE_LIST	{				\
		&hps_ctxt.little_num_base_perf_serv,		\
		&hps_ctxt.little_num_base_custom1,		\
		&hps_ctxt.little_num_base_custom2		\
	}

#define NUM_BASE_BIG_LIST	{				\
		&hps_ctxt.big_num_base_perf_serv,		\
		&hps_ctxt.big_num_base_custom1,			\
		&hps_ctxt.big_num_base_custom2			\
	}

struct hps_cpu_ctxt_struct {
	unsigned int load;
};

extern struct hps_ctxt_struct hps_ctxt;
DECLARE_PER_CPU(struct hps_cpu_ctxt_struct, hps_percpu_ctxt);

#define HPS_LOG_INFO	BIT(0)
#define HPS_LOG_ACT	BIT(1)
#define HPS_LOG_ALGO	BIT(2)
#define HPS_LOG_ALGO2	BIT(3)
#define HPS_LOG_TMR	BIT(4)

#define log_is_en(mask)		(hps_ctxt.log_mask & mask)
#define log_if(mask, fmt, args...) \
	do { if (log_is_en(mask)) hps_notice(fmt, ##args); } while (0)
#define log_info(fmt, args...)	log_if(HPS_LOG_INFO, fmt, ##args)
#define log_act(fmt, args...)	log_if(HPS_LOG_ACT, fmt, ##args)
#define log_alog(fmt, args...)	log_if(HPS_LOG_ALGO, fmt, ##args)
#define log_alog2(fmt, args...)	log_if(HPS_LOG_ALGO2, fmt, ##args)
#define log_tmr(fmt, args...)	log_if(HPS_LOG_TMR, fmt, ##args)

/*
 * mt_hotplug_strategy_main.c
 */
extern void hps_ctxt_reset_stas_nolock(void);
extern void hps_ctxt_reset_stas(void);
extern void hps_ctxt_print_basic(int toUart);
extern void hps_ctxt_print_algo_config(int toUart);
extern void hps_ctxt_print_algo_bound(int toUart);
extern void hps_ctxt_print_algo_stats_cur(int toUart);
extern void hps_ctxt_print_algo_stats_up(int toUart);
extern void hps_ctxt_print_algo_stats_down(int toUart);
extern void hps_ctxt_print_algo_stats_tlp(int toUart);

/*
 * mt_hotplug_strategy_core.c
 */
extern int hps_core_init(void);
extern int hps_core_deinit(void);
extern int hps_task_start(void);
extern void hps_task_stop(void);
extern void hps_task_wakeup_nolock(void);
extern void hps_task_wakeup(void);

/*
 * mt_hotplug_strategy_algo.c
 */
extern void hps_algo_hmp(void);
extern void hps_algo_smp(void);

/*
 * mt_hotplug_strategy_procfs.c
 */
extern int hps_procfs_init(void);

/*
 * mt_hotplug_strategy_cpu.c
 */
#define num_possible_little_cpus()	cpumask_weight(&hps_ctxt.little_cpumask)
#define num_possible_big_cpus()		cpumask_weight(&hps_ctxt.big_cpumask)

extern unsigned int num_limit_little_cpus(void);
extern unsigned int num_limit_big_cpus(void);
extern unsigned int num_base_little_cpus(void);
extern unsigned int num_base_big_cpus(void);

extern int hps_cpu_up(unsigned int cpu);
extern int hps_cpu_down(unsigned int cpu);

extern int hps_cpu_init(void);
extern int hps_cpu_deinit(void);
extern unsigned int num_online_little_cpus(void);
extern unsigned int num_online_big_cpus(void);
extern int hps_cpu_is_cpu_big(int cpu);
extern int hps_cpu_is_cpu_little(int cpu);
extern unsigned int hps_cpu_get_percpu_load(int cpu);
extern unsigned int hps_cpu_get_nr_heavy_task(void);
extern void hps_cpu_get_tlp(unsigned int *avg, unsigned int *iowait_avg);
extern int hps_get_num_possible_cpus(
		unsigned int *little_cpu_ptr,
		unsigned int *big_cpu_ptr);
extern int hps_get_num_online_cpus(
		unsigned int *little_cpu_ptr,
		unsigned int *big_cpu_ptr);

/* TODO: remove it, use linux/sched.h instead */
extern unsigned int sched_get_percpu_load(
			int cpu, bool reset, bool use_maxfreq);
/* definition in mediatek/kernel/kernel/sched/rq_stats.c */
extern unsigned int sched_get_nr_heavy_task(void);
extern void sched_get_nr_running_avg(int *avg, int *iowait_avg);

#ifdef __cplusplus
}
#endif

#endif /* __MT_HOTPLUG_STRATEGY_INTERNAL_H__ */

