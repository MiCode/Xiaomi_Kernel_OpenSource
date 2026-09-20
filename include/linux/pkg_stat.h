// MIUI ADD: Performance_TurboSched
#ifndef _LINUX_PKG_STAT_H
#define _LINUX_PKG_STAT_H

#define HISTORY_ITMES		 4
#define HISTORY_WINDOWS          (HISTORY_ITMES + 2)
#define USER_PKG_MIN_UID         10000

#if IS_ENABLED(CONFIG_MI_SCHED_4_CLUSTER)
#define NUM_SCHED_CLUSTERS      4
#elif IS_ENABLED(CONFIG_MI_SCHED_3_CLUSTER)
#define NUM_SCHED_CLUSTERS      3
#elif IS_ENABLED(CONFIG_MI_SCHED_2_CLUSTER)
#define NUM_SCHED_CLUSTERS      2
#else
#define NUM_SCHED_CLUSTERS      4
#endif

#if NUM_SCHED_CLUSTERS > 3
enum cluster_type {
        LITTLE_CLUSTER = 0,
        MID_CLUSTER_1,
        MID_CLUSTER_2,
        BIG_CLUSTER,
        CLUSTER_TYPES,
};
#elif NUM_SCHED_CLUSTERS > 2
enum cluster_type {
        LITTLE_CLUSTER = 0,
        MID_CLUSTER_1 = 1,
        MID_CLUSTER_2 = 1,
        BIG_CLUSTER,
        CLUSTER_TYPES,
};
#elif NUM_SCHED_CLUSTERS > 1
enum cluster_type {
        LITTLE_CLUSTER = 0,
        MID_CLUSTER_1 = 1,
        MID_CLUSTER_2 = 1,
        BIG_CLUSTER = 1,
        CLUSTER_TYPES,
};
#endif

// keep same with "include/linux/sched/walt.h"
#define MAX_CLUSTER             4
#define PKG_TASK_BUSY		1

struct package_runtime_info {
	u64 sup_cluster_runtime[HISTORY_WINDOWS];
	u64 big_cluster_2_runtime[HISTORY_WINDOWS];
	u64 big_cluster_1_runtime[HISTORY_WINDOWS];
	u64 little_cluster_runtime[HISTORY_WINDOWS];
};

#define NUM_MIGT_BUCKETS         10

enum RENDER_TYPE {
	RENDER_QUEUE_THREAD,
	RENDER_DEQUEUE_THREAD,
	RENDER_TYPES
};

enum MIGT_TASK_TYPE {
	MIGT_NORMAL_TASK,
	GAME_NORMAL_TASK,
	GAME_IP_TASK,
	GAME_IP_TASK_FUSER,
	GAME_VIP_TASK,
	GAME_VIP_TASK_FUSER,
	GAME_QRENDER_TASK,
	GAME_DQRENDER_TASK,
	GAME_SUPER_TASK,
	MI_VIP_TASK,
	GAME_CAL_TASK,
	GAME_CAL_AGING,
	GAME_WAIT_LOCK,
	GAME_LOCK_TASK,
	GAME_TASK_LEVELS
};

#define MASK_LOCK_TASK	(1 << GAME_LOCK_TASK)
#define MASK_WAIT_LOCK	(1 << GAME_WAIT_LOCK)
#define MASK_CAL	(1 << GAME_CAL_TASK)
#define MASK_AGING_CAL 	(1 << GAME_CAL_AGING)
#define MASK_STASK	(1 << GAME_SUPER_TASK)
#define MASK_RTASK	((1 << GAME_QRENDER_TASK) | (1 << GAME_DQRENDER_TASK))
#define MASK_VTASK	(MASK_STASK | MASK_RTASK | (1 << GAME_VIP_TASK))
#define MASK_ITASK	(MASK_VTASK | (1 << GAME_IP_TASK))
#define MASK_GTASK	(MASK_ITASK | (1 << GAME_NORMAL_TASK))
#define MASK_CLE_GTASK	(~MASK_GTASK)
#define MASK_MI_VTASK	(1 << MI_VIP_TASK)
#define MASK_FUSER_TASK	((1 << GAME_VIP_TASK_FUSER) | (1 << GAME_IP_TASK_FUSER))
#define MASK_FAST_TASK (MASK_STASK | MASK_RTASK | (1 << GAME_VIP_TASK_FUSER))

#define FRAME_STAT_CPUBUSY	1
#define FRAME_STAT_TASKBUSY	(1 << 1)
#define FRAME_STAT_WAITONIO	(1 << 2)

struct gt_task {
	u32 migt_count;
	enum MIGT_TASK_TYPE flag;
	u32 wake_render;
	unsigned long boost_end;
	u64 run_times;
	u64 prev_sum;
	u32 max_exec;
	u64 fps_exec_est;
	u64 fps_exec;
	u64 fps_mexec;
	u64 cur_window;
	u64 prev_window;
	u64 wake_ts;
	u64 lock_start_time;
	u32 wake_count;
	u64 prev_frame_load;
	u64 cur_frame_load;
	u64 nt_prev_frame_load;
	u64 nt_cur_frame_load;
#ifdef VTASK_BOOST_DEBUG
	u32 boostat[NUM_MIGT_BUCKETS];
#endif
	u32 bucket[NUM_MIGT_BUCKETS];
};

static inline int user_pkg(int uid)
{
	return uid > USER_PKG_MIN_UID;
}

//extern bool pkg_enable(void);
//extern void update_pkg_load(struct task_struct *tsk, int cpu, int flag,
//		u64 wallclock, u64 delta);
#endif /*_LINUX_PKG_STAT_H*/
// END Performance_TurboSched
