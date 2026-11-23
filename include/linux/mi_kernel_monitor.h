#ifndef __MONITOR_TASK_H__
#define __MONITOR_TASK_H__

#if IS_ENABLED(CONFIG_MI_KERNEL_MONITOR)

struct locking_info {
	u64 waittime_stamp;
	u64 holdtime_stamp;
};
struct mi_kernel_monitor {
	struct locking_info lkinfo;
	u64 cma_alloc_start_tm;
	u64 direct_reclaim_start_tm;
	u64 sleep_tm;
	u64 sched_switch_tm;
	u64 sched_reserved;
	union {
		u64 throttle_reclaim_start_tm;
		u64 direct_compact_start_tm;
	};
	u64 direct_reclaim_window_start_tm;
	u64 direct_reclaim_accumulate_tm;
	u64 mem_failure_prop_expire_time;
	u16 direct_reclaim_accumulate_cnt;
	union {
		u8 reclaim_order;
		u8 compact_order;
	};
	u8 mem_failure_prop_depth;
	bool io_waited;
};

#define KERNEL_MONITOR_RWSEM_OEM_DATA(sem) \
	(((struct rw_semaphore *)(sem))->android_oem_data1[1])

#if IS_ENABLED(CONFIG_OS_CPU_PLAT_COMMON)
extern struct mi_kernel_monitor *plat_get_mi_kernel_monitor(struct task_struct *p);
static inline struct mi_kernel_monitor *get_mi_kernel_monitor(struct task_struct *p) {
	return plat_get_mi_kernel_monitor(p);
}
#elif IS_ENABLED(CONFIG_MI_SCHED)
extern struct mi_kernel_monitor *metis_get_mi_kernel_monitor(struct task_struct *p);
static inline struct mi_kernel_monitor *get_mi_kernel_monitor(struct task_struct *p) {
	return metis_get_mi_kernel_monitor(p);
}
#else
static inline struct mi_kernel_monitor *get_mi_kernel_monitor(struct task_struct *p) { return NULL; }
#endif

#endif /*CONFIG_MI_KERNEL_MONITOR*/

#endif  /* endif */
