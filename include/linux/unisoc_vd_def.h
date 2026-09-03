/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 Unisoc Inc.
 */

#ifndef _UNISOC_VD_DEF_H
#define _UNISOC_VD_DEF_H

/*
 * unisoc task_struct vendordata
 * struct task_struct {
 *	......
 *	ANDROID_VENDOR_DATA_ARRAY(1, 64);
 *	......
 * };
 * if you want use task_struct's vendor_data, you can add params in the struct
 * just like follow:
 * 1. add params in struct:
 * struct uni_task_struct {
 *	int param1;
 *	int param2;
 * }
 * 2. when use the param:
 * struct uni_task_struct *uni_ts = (struct uni_task_struct *)task->android_vendor_data1;
 * temp1 = uni_ts->param1;
 * temp2 = uni_ts->param2;
 * ...
 */

struct uni_task_struct {
#if IS_ENABLED(CONFIG_SCHED_WALT)
	#define RAVG_HIST_SIZE_MAX  6
	/*
	 * 'mark_start' marks the beginning of an event (task waking up, task
	 * starting to execute, task being preempted) within a window
	 *
	 * 'sum' represents how runnable a task has been within current
	 * window. It incorporates both running time and wait time and is
	 * frequency scaled.
	 *
	 * 'sum_history' keeps track of history of 'sum' seen over previous
	 * RAVG_HIST_SIZE windows. Windows where task was entirely sleeping are
	 * ignored.
	 *
	 * 'demand' represents maximum sum seen over previous
	 * sysctl_sched_ravg_hist_size windows. 'demand' could drive frequency
	 * demand for tasks.
	 *
	 * 'curr_window' represents task's contribution to cpu busy time
	 * statistics (rq->curr_runnable_sum) in current window
	 *
	 * 'prev_window' represents task's contribution to cpu busy time
	 * statistics (rq->prev_runnable_sum) in previous window
	 */
	u64 mark_start;
	u32 sum, demand, sum_latest, demand_scale;
	u32 sum_history[RAVG_HIST_SIZE_MAX];
	u32 curr_window, prev_window;
	/*
	 * 'init_load_pct' represents the initial task load assigned to children
	 * of this task
	 */
	u32 init_load_pct;
#endif
	u64 last_sleep_ts;
	u64 last_enqueue_ts;
	u64 last_wakeup_ts;
	int uclamp_fork_reset;
#ifdef CONFIG_UNISOC_SCHED_VIP_TASK
	struct list_head	vip_list;
	u64			sum_exec_snapshot_for_slice;
	u64			sum_exec_snapshot_for_total;
	u64			total_exec;
	int			vip_level;
	u8			vip_params;
	int			binder_from_cpu;
#endif
	/*
	 * for debug iowait;
	 */
	u64 balance_dirty_start_ts;
	u64 iowait_start_ts;
#ifdef CONFIG_UNISOC_HUNG_TASK_ENH
	u8 hung_detect_status;
#endif

#ifdef CONFIG_UNISOC_SCHED_PAUSE_CPU
	struct task_struct	*percpu_tsk;
	struct list_head	percpu_kthread_node;
	cpumask_t		cpus_requested;
#endif

#ifdef CONFIG_UNISOC_BINDER_SCHED
	int unibinder_feature_flags;
#endif

#if IS_ENABLED(CONFIG_SPRD_DATA_COLLECTOR)
	u64 dr_thread_timestamp_start;
	u64 dr_thread_duration;
	u64 dr_thread_in_statistic;
#endif /* CONFIG_UNISOC_DATA_COLLECTOR */
};

struct sched_cluster {
	raw_spinlock_t		load_lock;
	struct list_head	list;
	struct cpumask		cpus;
	int			id;
	unsigned long		capacity;
};

/*
 *	struct rq {
 *	......
 *	ANDROID_VENDOR_DATA_ARRAY(1, 96);
 *	......
 *	}
 */
struct uni_rq {
	struct task_struct      *push_task;
	struct sched_cluster	*cluster;

	unsigned long sched_flag;
#if IS_ENABLED(CONFIG_SCHED_WALT)
	u64 cumulative_runnable_avg;
	u64 window_start;
	u64 curr_runnable_sum;
	u64 prev_runnable_sum;
	u64 cur_irqload;
	u64 avg_irqload;
	u64 irqload_ts;
	u64 cum_window_demand;
	enum {
		CPU_BUSY_CLR = 0,
		CPU_BUSY_PREPARE,
		CPU_BUSY_SET,
	} is_busy;
#endif

#ifdef CONFIG_UNISOC_SCHED_VIP_TASK
	struct list_head	vip_tasks;
	int			num_vip_tasks;
#endif

#ifdef CONFIG_UNISOC_SCHED_PAUSE_CPU
	u32 enqueue_counter;
#endif
};

#ifdef CONFIG_UCLAMP_TASK_GROUP
/*
 *	struct task_group {
 *	......
 *	ANDROID_VENDOR_DATA_ARRAY(1, 4);
 *	......
 *	}
 */
struct uni_task_group {
	/* CGroup index */
	int idx;
	/* Boost value for tasks in CGroup */
	int boost;

#if IS_ENABLED(CONFIG_SCHED_WALT)
	int account_wait_time;
	u32 init_task_load_pct;

	int prefer_active;
#endif
};
#endif

#if IS_ENABLED(CONFIG_UNISOC_SCHED)
extern unsigned int sched_get_cpu_util_pct(int cpu);
#else
static inline unsigned int sched_get_cpu_util_pct(int cpu)
{
	return 0;
}
#endif
#endif
