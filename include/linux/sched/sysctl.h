/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SCHED_SYSCTL_H
#define _LINUX_SCHED_SYSCTL_H

#include <linux/types.h>

struct ctl_table;

#ifdef CONFIG_DETECT_HUNG_TASK
extern int	     sysctl_hung_task_check_count;
extern unsigned int  sysctl_hung_task_panic;
extern unsigned long sysctl_hung_task_timeout_secs;
extern unsigned long sysctl_hung_task_check_interval_secs;
extern int sysctl_hung_task_warnings;
extern int sysctl_hung_task_selective_monitoring;
extern int proc_dohung_task_timeout_secs(struct ctl_table *table, int write,
					 void __user *buffer,
					 size_t *lenp, loff_t *ppos);
#else
/* Avoid need for ifdefs elsewhere in the code */
enum { sysctl_hung_task_timeout_secs = 0 };
#endif

#define MAX_CLUSTERS 3
/* MAX_MARGIN_LEVELS should be one less than MAX_CLUSTERS */
#define MAX_MARGIN_LEVELS (MAX_CLUSTERS - 1)

extern unsigned int sysctl_sched_latency;
extern unsigned int sysctl_sched_min_granularity;
extern unsigned int sysctl_sched_sync_hint_enable;
extern unsigned int sysctl_sched_cstate_aware;
extern unsigned int sysctl_sched_wakeup_granularity;
extern unsigned int sysctl_sched_child_runs_first;
#ifdef CONFIG_SCHED_WALT
extern unsigned int sysctl_sched_capacity_margin_up[MAX_MARGIN_LEVELS];
extern unsigned int sysctl_sched_capacity_margin_down[MAX_MARGIN_LEVELS];
extern unsigned int sysctl_sched_user_hint;
extern const int sched_user_hint_max;
extern unsigned int sysctl_sched_cpu_high_irqload;
extern unsigned int sysctl_sched_boost;
extern unsigned int sysctl_sched_group_upmigrate_pct;
extern unsigned int sysctl_sched_group_downmigrate_pct;
extern unsigned int sysctl_sched_conservative_pl;
extern unsigned int sysctl_sched_many_wakeup_threshold;
extern unsigned int sysctl_sched_walt_rotate_big_tasks;
extern unsigned int sysctl_sched_min_task_util_for_boost;
extern unsigned int sysctl_sched_min_task_util_for_colocation;
extern unsigned int sysctl_sched_asym_cap_sibling_freq_match_pct;
extern unsigned int sysctl_sched_coloc_downmigrate_ns;
extern unsigned int sysctl_sched_task_unfilter_period;
extern unsigned int sysctl_sched_busy_hyst_enable_cpus;
extern unsigned int sysctl_sched_busy_hyst;
extern unsigned int sysctl_sched_coloc_busy_hyst_enable_cpus;
extern unsigned int sysctl_sched_coloc_busy_hyst;
extern unsigned int sysctl_sched_coloc_busy_hyst_max_ms;
extern unsigned int sysctl_sched_window_stats_policy;
extern unsigned int sysctl_sched_ravg_window_nr_ticks;
extern unsigned int sysctl_sched_dynamic_ravg_window_enable;

extern int
walt_proc_group_thresholds_handler(struct ctl_table *table, int write,
			 void __user *buffer, size_t *lenp,
			 loff_t *ppos);
extern int
walt_proc_user_hint_handler(struct ctl_table *table, int write,
			 void __user *buffer, size_t *lenp,
			 loff_t *ppos);

extern int
sched_ravg_window_handler(struct ctl_table *table, int write,
			 void __user *buffer, size_t *lenp,
			 loff_t *ppos);

extern int sched_updown_migrate_handler(struct ctl_table *table,
					int write, void __user *buffer,
					size_t *lenp, loff_t *ppos);
#endif

#if defined(CONFIG_PREEMPT_TRACER) || defined(CONFIG_DEBUG_PREEMPT)
extern unsigned int sysctl_preemptoff_tracing_threshold_ns;
#endif
#if defined(CONFIG_PREEMPTIRQ_EVENTS) && defined(CONFIG_IRQSOFF_TRACER)
extern unsigned int sysctl_irqsoff_tracing_threshold_ns;
#endif

enum sched_tunable_scaling {
	SCHED_TUNABLESCALING_NONE,
	SCHED_TUNABLESCALING_LOG,
	SCHED_TUNABLESCALING_LINEAR,
	SCHED_TUNABLESCALING_END,
};
extern enum sched_tunable_scaling sysctl_sched_tunable_scaling;

extern unsigned int sysctl_numa_balancing_scan_delay;
extern unsigned int sysctl_numa_balancing_scan_period_min;
extern unsigned int sysctl_numa_balancing_scan_period_max;
extern unsigned int sysctl_numa_balancing_scan_size;

#ifdef CONFIG_SCHED_DEBUG
extern __read_mostly unsigned int sysctl_sched_migration_cost;
extern __read_mostly unsigned int sysctl_sched_nr_migrate;

int sched_proc_update_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *length,
		loff_t *ppos);
#endif

extern int sched_boost_handler(struct ctl_table *table, int write,
			void __user *buffer, size_t *lenp, loff_t *ppos);
/*
 *  control realtime throttling:
 *
 *  /proc/sys/kernel/sched_rt_period_us
 *  /proc/sys/kernel/sched_rt_runtime_us
 */
extern unsigned int sysctl_sched_rt_period;
extern int sysctl_sched_rt_runtime;

#ifdef CONFIG_CFS_BANDWIDTH
extern unsigned int sysctl_sched_cfs_bandwidth_slice;
#endif

#ifdef CONFIG_SCHED_AUTOGROUP
extern unsigned int sysctl_sched_autogroup_enabled;
#endif

extern int sysctl_sched_rr_timeslice;
extern int sched_rr_timeslice;

extern int sched_rr_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp,
		loff_t *ppos);

extern int sched_rt_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp,
		loff_t *ppos);

extern int sysctl_numa_balancing(struct ctl_table *table, int write,
				 void __user *buffer, size_t *lenp,
				 loff_t *ppos);

extern int sysctl_schedstats(struct ctl_table *table, int write,
				 void __user *buffer, size_t *lenp,
				 loff_t *ppos);
#define LIB_PATH_LENGTH 512
extern char sched_lib_name[LIB_PATH_LENGTH];
extern unsigned int sched_lib_mask_force;
extern bool is_sched_lib_based_app(pid_t pid);

#endif /* _LINUX_SCHED_SYSCTL_H */
