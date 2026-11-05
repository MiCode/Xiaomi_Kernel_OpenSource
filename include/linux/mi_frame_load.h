// MIUI ADD: Performance_TurboSched
#ifndef _LINUX_MI_FRAME_LOAD_H
#define _LINUX_MI_FRAME_LOAD_H

#define TARGET_PERIOD_NS(x) (NSEC_PER_SEC / (x))

#define JOIN_EXPLICIT_CREATOR	(0x1)
#define JOIN_EXPLICIT		(0x2)
#define JOIN_AUTO		(0x4)
#define MASK_CLEAR_JOIN_TYPE	(JOIN_EXPLICIT_CREATOR | JOIN_EXPLICIT | JOIN_AUTO)

enum POINTED_TRACE_LOAD_TYPE {
	IOWAIT,
	MEMSTALL,
	POINTED_TRACE_LOAD_NUMS,
};

#define POINTED_TRACKED_IOWAIT	(1 << IOWAIT)
#define POINTED_MEMSTALL	(1 << MEMSTALL) //no hook in psi
#define CPU_NUMS		8


#define MAX_HIST_NUMS		5
struct mi_task_ravg {
	u64 running_exec;
	u64 runnable_exec;
	u64 sleep_exec;
	u64 io_exec;
	u64 memstall_exec;

	u32 demand;
	u32 demand_scaled;
	u32 coloc_demand;

	u64 running_hist[MAX_HIST_NUMS];
	u64 runnable_hist[MAX_HIST_NUMS];
	u64 sum_hist[MAX_HIST_NUMS];
	u64 sum_hist_util[MAX_HIST_NUMS];
	unsigned int flags;

};

#define GROUP_SYSTEM		0
#define GROUP_BACKGROUND	1
#define GROUP_DISPLAY		2
#define GROUP_KERNEL		3
#define GROUP_CREATE		4

enum MI_APP_TYPE {
	SYSTEM,
	BACKGROUND,
	FORE_POINT,
	FORE_NOPOINT,
	LATENCY_SENSITIVE,
	NEED_DECEL,
	NON_DECEL,
	DISPLAY,
	KERRNEL,
	APP_TYPE_NUMS,
};

#define KERNEL_GROUP	(1 << KERRNEL)
#define SYSTEM_DISPLAY	(1 << DISPLAY)
#define APP_SYSTEM (1 << SYSTEM)
#define APP_BACKGROUND	(1 << BACKGROUND)
#define APP_FOREPOINT	(1 << FORE_POINT)
#define APP_FORENOPOINT	(1 << FORE_NOPOINT)
#define PERIOD_GROUP    (1 << LATENCY_SENSITIVE)
#define APP_FORE	(APP_FOREPOINT | APP_FORENOPOINT)
#define GROUP_NEED_DECEL (1 << NEED_DECEL)
#define MASK_NON_DEL_TYPE (1 << NON_DECEL)

/*************/
#define LS_TYPE_MASK	(0x3 << (APP_TYPE_NUMS + 1))
#define LS_TYPE_RENDER	(0x1 << (APP_TYPE_NUMS + 1))
#define LS_TYPE_AUDIO	(0x2 << (APP_TYPE_NUMS + 1))

/*********/
#define CREATE_RUNNING_FLAG	0x1
#define CREATE_RUNNABLE_FLAG     0x2

struct group_frame_ravg {
	u64 running_sum_exec_runtime;
	u64 runnable_sum_exec_runtime;
	u64 sleep_sum_exec_runtime;
	u64 sum_exec_runtime;

	u64 real_runtime;
};

struct group_frame_status {
	u64 pred_demands_sum_scaled;
	u64 cumulative_runnable_avg_scaled;
};

struct mi_group {
	unsigned int id;

	int cal_cid;
	unsigned int group_type;
	raw_spinlock_t lock;
	raw_spinlock_t cpu_lock[CPU_NUMS];
	int nr_running;
	int cpu_nr_running[CPU_NUMS];
	refcount_t usage;
	struct list_head head;
	int cal_pid;
	unsigned int create_flag;
	u64 create_running_time;
	atomic_t caling_flag;
	unsigned int fps;

	u64 window_start;
	u64 mark_start;
	u64 window_period;
	u64 target_period;
	u64 last_update_time;

	u64 sum_freq_period[NUM_SCHED_CLUSTERS];
	u64 avg_freq_period[NUM_SCHED_CLUSTERS];
	u64 last_freq_change_time[NUM_SCHED_CLUSTERS];
	u64 cur_record_freq[NUM_SCHED_CLUSTERS];
	struct list_head freq_head;
	bool in_period_freq_list;


	struct group_frame_ravg cpu_prev_frame[CPU_NUMS];
	struct group_frame_ravg cpu_curr_frame[CPU_NUMS];
	struct group_frame_status stats;

	struct group_frame_ravg cluster_prev_frame[NUM_SCHED_CLUSTERS];
	struct group_frame_ravg cluster_curr_frame[NUM_SCHED_CLUSTERS];
	u64 cluster_mark_start[NUM_SCHED_CLUSTERS];
	atomic_t cluster_token_pid[NUM_SCHED_CLUSTERS];
	u8 wait_wake_flag[NUM_SCHED_CLUSTERS];


	struct group_frame_ravg prev_frame;
	struct group_frame_ravg curr_frame;
	unsigned int divisor;
	u64 pred_target_period;
	bool up_pred_util[NUM_SCHED_CLUSTERS];
	bool down_pred_util[NUM_SCHED_CLUSTERS];

	int prefer_cluster;
	struct list_head cluster_head;
	int group_priority;

	/****task*****/
	struct list_head task_queue;
};

struct mi_default_group {
	unsigned int id;
	unsigned int flag;

	struct mi_group * mi_grp;
};

struct ls_group {
	unsigned int id;
	refcount_t usage;

	unsigned int create_pid;
	unsigned int flag;
	struct mi_group *mi_grp;

	struct list_head list;
};


#endif
// END Performance_TurboSched