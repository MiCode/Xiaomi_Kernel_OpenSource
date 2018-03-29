/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __MT_HOTPLUG_STRATEGY_INTERNAL_H__
#define __MT_HOTPLUG_STRATEGY_INTERNAL_H__

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/sched/rt.h>

/* CONFIG - compile time */
#define HPS_TASK_PRIORITY		(MAX_RT_PRIO - 3)
#define HPS_TIMER_INTERVAL_MS		100

#define HPS_PERIODICAL_BY_WAIT_QUEUE        (1)
#define HPS_PERIODICAL_BY_TIMER             (2)
#define HPS_PERIODICAL_BY_HR_TIMER          (3)

#define MAX_CPU_UP_TIMES		10
#define MAX_CPU_DOWN_TIMES		100
#define MAX_TLP_TIMES			10
/* cpu capability of big / little = 1.7, aka 170, 170 - 100 = 70 */
#define CPU_DMIPS_BIG_LITTLE_DIFF	70

/* CONFIG - runtime (execute time interval : 100 ms */
#define DEF_CPU_UP_THRESHOLD		95
#define DEF_CPU_UP_TIMES		2
#define DEF_CPU_DOWN_THRESHOLD		85
#define DEF_CPU_DOWN_TIMES		8
#define DEF_TLP_TIMES			1

#define EN_CPU_INPUT_BOOST		1
#define DEF_CPU_INPUT_BOOST_CPU_NUM	2

#define EN_CPU_RUSH_BOOST		1
#define DEF_CPU_RUSH_BOOST_THRESHOLD	98
#define DEF_CPU_RUSH_BOOST_TIMES	1

#define EN_HPS_LOG			1
#define EN_ISR_LOG			0

/*
 * EARLY SYSPEND CONFIG - runtime
 * Execute time interval : 400 ms
 */
#define DEF_ES_CPU_UP_THRESHOLD		95
#define DEF_ES_CPU_UP_TIMES		1
#define DEF_ES_CPU_DOWN_THRESHOLD	85
#define DEF_ES_CPU_DOWN_TIMES		2

/*
 * LOG
 */
#define hps_emerg(fmt, args...)		pr_emerg("[HPS] " fmt, ##args)
#define hps_alert(fmt, args...)		pr_alert("[HPS] " fmt, ##args)
#define hps_crit(fmt, args...)		pr_crit("[HPS] " fmt, ##args)
#define hps_error(fmt, args...)		pr_err("[HPS] " fmt, ##args)
#define hps_warn(fmt, args...)		pr_warn("[HPS] " fmt, ##args)
#define hps_notice(fmt, args...)	pr_notice("[HPS] " fmt, ##args)
#define hps_info(fmt, args...)		pr_info("[HPS] " fmt, ##args)
#define hps_debug(fmt, args...)		pr_devel("[HPS] " fmt, ##args)

#define hps_read(addr)			__raw_readl(IOMEM(addr))
#define hps_write(addr, val)		mt_reg_sync_writel(val, addr)

enum hps_init_state_e {
	INIT_STATE_NOT_READY = 0,
	INIT_STATE_DONE
};

enum hps_ctxt_state_e {
	STATE_LATE_RESUME = 0,
	STATE_EARLY_SUSPEND,
	STATE_SUSPEND,
	STATE_COUNT
};

enum hps_ctxt_action_e {
	ACTION_NONE = 0,
	ACTION_BASE_LITTLE,	/* bit  1, 0x0002 */
	ACTION_BASE_BIG,	/* bit  2, 0x0004 */
	ACTION_LIMIT_LITTLE,	/* bit  3, 0x0008 */
	ACTION_LIMIT_BIG,	/* bit  4, 0x0010 */
	ACTION_RUSH_BOOST_LITTLE,	/* bit  5, 0x0020 */
	ACTION_RUSH_BOOST_BIG,	/* bit  6, 0x0040 */
	ACTION_UP_LITTLE,	/* bit  7, 0x0080 */
	ACTION_UP_BIG,		/* bit  8, 0x0100 */
	ACTION_DOWN_LITTLE,	/* bit  9, 0x0200 */
	ACTION_DOWN_BIG,	/* bit 10, 0x0400 */
	ACTION_BIG_TO_LITTLE,	/* bit 11, 0x0800 */
	ACTION_INPUT,		/* bit 12, 0x1000 */
	ACTION_COUNT
};

struct hps_ctxt_struct {
	/* state */
	unsigned int init_state;
	unsigned int state;

	/* enabled */
	unsigned int enabled;
	/* disable hotplug strategy in suspend flow */
	unsigned int suspend_enabled;	/* default: 1 */
	unsigned int cur_dump_enabled;
	unsigned int stats_dump_enabled;

	/* core */
	/* Synchronizes accesses */
	struct mutex lock;
	struct task_struct *tsk_struct_ptr;
	wait_queue_head_t wait_queue;
	struct timer_list tmr_list;
	unsigned int periodical_by;
	struct hrtimer hr_timer;
	struct platform_driver pdrv;

	/* backup */
	unsigned int enabled_backup;
	unsigned int rush_boost_enabled_backup;

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
	unsigned int input_boost_enabled;
	unsigned int input_boost_cpu_num;
	unsigned int rush_boost_enabled;
	unsigned int rush_boost_threshold;
	unsigned int rush_boost_times;
	unsigned int tlp_times;

	/* algo bound */
	unsigned int little_num_base_perf_serv;
	unsigned int little_num_limit_thermal;
	unsigned int little_num_limit_low_battery;
	unsigned int little_num_limit_ultra_power_saving;
	unsigned int little_num_limit_power_serv;
	unsigned int big_num_base_perf_serv;
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
	unsigned long action;
	atomic_t is_ondemand;

	/* misc */
	unsigned int test0;
	unsigned int test1;
};

struct hps_cpu_ctxt_struct {
	unsigned int load;
};

extern struct hps_ctxt_struct hps_ctxt;
DECLARE_PER_CPU(struct hps_cpu_ctxt_struct, hps_percpu_ctxt);

/* mt_hotplug_strategy_main.c */
extern void hps_ctxt_reset_stas_nolock(void);
extern void hps_ctxt_reset_stas(void);
extern void hps_ctxt_print_basic(int toUart);
extern void hps_ctxt_print_algo_config(int toUart);
extern void hps_ctxt_print_algo_bound(int toUart);
extern void hps_ctxt_print_algo_stats_cur(int toUart);
extern void hps_ctxt_print_algo_stats_up(int toUart);
extern void hps_ctxt_print_algo_stats_down(int toUart);
extern void hps_ctxt_print_algo_stats_tlp(int toUart);
extern int hps_restart_timer(void);
extern int hps_del_timer(void);
extern int hps_core_deinit(void);

/* mt_hotplug_strategy_core.c */
extern int hps_core_init(void);
extern int hps_core_deinit(void);
extern int hps_task_start(void);
extern void hps_task_stop(void);
extern void hps_task_wakeup_nolock(void);
extern void hps_task_wakeup(void);

/* mt_hotplug_strategy_algo.c */
extern void hps_algo_hmp(void);
extern void hps_algo_smp(void);

/* mt_hotplug_strategy_procfs.c */
extern int hps_procfs_init(void);

/* mt_hotplug_strategy_cpu.c */
#define num_possible_little_cpus()	\
	cpumask_weight(&hps_ctxt.little_cpumask)
#define num_possible_big_cpus()		\
	cpumask_weight(&hps_ctxt.big_cpumask)

extern int hps_cpu_init(void);
extern int hps_cpu_deinit(void);
extern unsigned int num_online_little_cpus(void);
extern unsigned int num_online_big_cpus(void);
extern int hps_cpu_is_cpu_big(int cpu);
extern int hps_cpu_is_cpu_little(int cpu);
extern unsigned int hps_cpu_get_percpu_load(int cpu);
extern unsigned int hps_cpu_get_nr_heavy_task(void);
extern void hps_cpu_get_tlp(unsigned int *avg, unsigned int *iowait_avg);

extern struct cpumask cpu_domain_big_mask;
extern struct cpumask cpu_domain_little_mask;
extern void sched_get_nr_running_avg(int *avg, int *iowait_avg);

extern unsigned int sched_get_percpu_load(int cpu, bool reset, bool use_maxfreq);
extern unsigned int sched_get_nr_heavy_task(void);

#endif
