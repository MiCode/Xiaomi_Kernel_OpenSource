#ifndef _SCHED_SYSCTL_H
#define _SCHED_SYSCTL_H

#ifdef CONFIG_DETECT_HUNG_TASK
extern int	     sysctl_hung_task_check_count;
extern unsigned int  sysctl_hung_task_panic;
extern unsigned long sysctl_hung_task_timeout_secs;
extern int sysctl_hung_task_warnings;
extern int proc_dohung_task_timeout_secs(struct ctl_table *table, int write,
					 void __user *buffer,
					 size_t *lenp, loff_t *ppos);
#else
/* Avoid need for ifdefs elsewhere in the code */
enum { sysctl_hung_task_timeout_secs = 0 };
#endif

/*
 * Default maximum number of active map areas, this limits the number of vmas
 * per mm struct. Users can overwrite this number by sysctl but there is a
 * problem.
 *
 * When a program's coredump is generated as ELF format, a section is created
 * per a vma. In ELF, the number of sections is represented in unsigned short.
 * This means the number of sections should be smaller than 65535 at coredump.
 * Because the kernel adds some informative sections to a image of program at
 * generating coredump, we need some margin. The number of extra sections is
 * 1-3 now and depends on arch. We use "5" as safe margin, here.
 *
 * ELF extended numbering allows more than 65535 sections, so 16-bit bound is
 * not a hard limit any more. Although some userspace tools can be surprised by
 * that.
 */
#define MAPCOUNT_ELF_CORE_MARGIN	(5)
#define DEFAULT_MAX_MAP_COUNT	(USHRT_MAX - MAPCOUNT_ELF_CORE_MARGIN)

extern int sysctl_max_map_count;

extern unsigned int sysctl_sched_latency;
extern unsigned int sysctl_sched_min_granularity;
extern unsigned int sysctl_sched_wakeup_granularity;
extern unsigned int sysctl_sched_child_runs_first;
extern unsigned int sysctl_sched_wake_to_idle;
extern unsigned int sysctl_sched_wakeup_load_threshold;
extern unsigned int sysctl_sched_window_stats_policy;
extern unsigned int sysctl_sched_ravg_hist_size;
extern unsigned int sysctl_sched_cpu_high_irqload;
extern unsigned int sysctl_sched_heavy_task_pct;

#if defined(CONFIG_SCHED_FREQ_INPUT) || defined(CONFIG_SCHED_HMP)
extern unsigned int sysctl_sched_init_task_load_pct;
#endif

#ifdef CONFIG_SCHED_FREQ_INPUT
extern int sysctl_sched_freq_inc_notify;
extern int sysctl_sched_freq_dec_notify;
#endif

#ifdef CONFIG_SCHED_HMP
extern unsigned int sysctl_sched_spill_nr_run;
extern unsigned int sysctl_sched_spill_load_pct;
extern unsigned int sysctl_sched_upmigrate_pct;
extern unsigned int sysctl_sched_downmigrate_pct;
extern int sysctl_sched_upmigrate_min_nice;
extern unsigned int sysctl_sched_boost;
extern unsigned int sysctl_early_detection_duration;
extern unsigned int sysctl_sched_small_wakee_task_load_pct;
extern unsigned int sysctl_sched_big_waker_task_load_pct;
extern unsigned int sysctl_sched_prefer_sync_wakee_to_waker;

#ifdef CONFIG_SCHED_QHMP
extern unsigned int sysctl_sched_min_runtime;
extern unsigned int sysctl_sched_small_task_pct;
extern unsigned int sysctl_sched_restrict_tasks_spread;
extern unsigned int sysctl_sched_account_wait_time;
extern unsigned int sysctl_sched_freq_account_wait_time;
extern unsigned int sysctl_sched_enable_power_aware;
extern unsigned int sysctl_sched_migration_fixup;
#else
extern unsigned int sysctl_sched_select_prev_cpu_us;
extern unsigned int sysctl_sched_enable_colocation;
extern unsigned int sysctl_sched_restrict_cluster_spill;
extern unsigned int sysctl_sched_enable_thread_grouping;
#if defined(CONFIG_SCHED_FREQ_INPUT)
extern unsigned int sysctl_sched_new_task_windows;
extern unsigned int sysctl_sched_pred_alert_freq;
extern unsigned int sysctl_sched_freq_aggregate;
#endif
#endif

#else /* CONFIG_SCHED_HMP */

#define sysctl_sched_enable_hmp_task_placement 0

#endif /* CONFIG_SCHED_HMP */

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
extern unsigned int sysctl_sched_migration_cost;
extern unsigned int sysctl_sched_nr_migrate;
extern unsigned int sysctl_sched_time_avg;
extern unsigned int sysctl_timer_migration;
extern unsigned int sysctl_sched_shares_window;

int sched_proc_update_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *length,
		loff_t *ppos);
#endif

extern int sched_migrate_notify_proc_handler(struct ctl_table *table,
		int write, void __user *buffer, size_t *lenp, loff_t *ppos);

extern int sched_hmp_proc_update_handler(struct ctl_table *table,
		int write, void __user *buffer, size_t *lenp, loff_t *ppos);

extern int sched_boost_handler(struct ctl_table *table, int write,
			void __user *buffer, size_t *lenp, loff_t *ppos);

extern int sched_window_update_handler(struct ctl_table *table,
		 int write, void __user *buffer, size_t *lenp, loff_t *ppos);

#ifdef CONFIG_SCHED_DEBUG
static inline unsigned int get_sysctl_timer_migration(void)
{
	return sysctl_timer_migration;
}
#else
static inline unsigned int get_sysctl_timer_migration(void)
{
	return 1;
}
#endif

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

#ifdef CONFIG_SCHEDSTATS
extern unsigned int sysctl_sched_latency_panic_threshold;
extern unsigned int sysctl_sched_latency_warn_threshold;

extern int sched_max_latency_sysctl(struct ctl_table *table, int write,
				    void __user *buffer, size_t *lenp,
				    loff_t *ppos);
#endif

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

#endif /* _SCHED_SYSCTL_H */
