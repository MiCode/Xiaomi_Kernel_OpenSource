#ifndef _LINUX_PKG_STAT_H
#define _LINUX_PKG_STAT_H

#define HISTORY_ITMES		 4
#define HISTORY_WINDOWS          (HISTORY_ITMES + 2)
#define USER_PKG_MIN_UID         10000

enum cluster_type {
	LITTLE_CLUSTER = 0,
	MID_CLUSTER,
	BIG_CLUSTER,
	CLUSTER_TYPES,
};
#define MAX_CLUSTER		CLUSTER_TYPES
#define PKG_TASK_BUSY		1

struct package_runtime_info {
	u64 sup_cluster_runtime[HISTORY_WINDOWS];
	u64 big_cluster_runtime[HISTORY_WINDOWS];
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
	GAME_VIP_TASK,
	GAME_QRENDER_TASK,
	GAME_DQRENDER_TASK,
	GAME_SUPER_TASK,
	MI_VIP_TASK,
	GAME_TASK_LEVELS
};

#define MASK_STASK	(1 << GAME_SUPER_TASK)
#define MASK_RTASK	((1 << GAME_QRENDER_TASK) | (1 << GAME_DQRENDER_TASK))
#define MASK_VTASK	(MASK_STASK | MASK_RTASK | (1 << GAME_VIP_TASK))
#define MASK_ITASK	(MASK_VTASK | (1 << GAME_IP_TASK))
#define MASK_GTASK	(MASK_ITASK | (1 << GAME_NORMAL_TASK))
#define MASK_CLE_GTASK	(~MASK_GTASK)
#define MASK_MI_VTASK	(1 << MI_VIP_TASK)

struct gt_task {
	u32 migt_count;
	enum MIGT_TASK_TYPE flag;
	u32 wake_render;
	unsigned long boost_end;
	u64 run_times;
	u64 prev_sum;
	u32 max_exec;
	u64 fps_exec;
	u64 fps_mexec;

#ifdef VTASK_BOOST_DEBUG
	u32 boostat[NUM_MIGT_BUCKETS];
#endif
	u32 bucket[NUM_MIGT_BUCKETS];
};


static inline user_pkg(int uid)
{
	return uid > USER_PKG_MIN_UID;
}

//extern bool pkg_enable(void);
//extern void update_pkg_load(struct task_struct *tsk, int cpu, int flag,
//		u64 wallclock, u64 delta);

#endif /*_LINUX_PKG_STAT_H*/

