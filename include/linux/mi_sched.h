
// MIUI ADD: Performance_TurboSched
#ifndef _LINUX_MI_SCHED_H
#define _LINUX_MI_SCHED_H

#define MI_TASK_MAGIC_NUM	0xabcdabcdacbdadbc
#define MI_USER_MAGIC_NUM	0xdcba1234abcd4321
#define MAGIC_INUSE_NUM 	0xacbdacbd
#define LOCK_MAX_DEPTH		10
#define MI_TASK_ON_PKG_TREE	0xbadc
//#ifdef CONFIG_CFS_BANDWIDTH_MI
#define MI_TASK_BANDWIDTH_DATA_MAGIC_WRAPPER 0xdcbadcbadcbadcba
//#endif

#include <linux/pkg_stat.h>
#ifdef CONFIG_MI_GROUP_ASSIST
#include <linux/mi_frame_load.h>
#endif

extern int num_sched_clusters;

enum MI_TASK_TYPE {
	MI_MINOR_TASK,
	MI_REBIND_TASK,
	MI_LAT_TASK,
	MI_VVIP_TASK,
	MI_NEW_VTASK,
	MI_AGING_VTASK,
	MI_UI_TASK,
	MI_UI_PIPELINE,
	MI_REN_TASK,
	MI_DYNAMIC_VTASK,
	MI_IN_RTBOOST,
	MI_LOCK_TASK,
	MI_BINDER_TASK,
	MI_RT_BINDER_FREE,
	MI_REQ_ASYNC_BINDER_TASK,
	MI_ART_MUTEX_TASK,
	MI_LOW_LT_TASK,
	MI_LINK_BOOST_TASK,
	MI_CLUSTER_BOOST,
	MI_FBOOST,
	MI_EBOOST,
	MI_SF_TASK,
	MI_HWC_TASK,
	MI_DIRECT_SET,
	MI_BAL_IRQ_TASK,
	MI_MMLOCK_BOOST_TASK,
	MI_FREEDOM_TASK,
	MI_GRP_LINK,
	MI_PERSIST_VTASK,
	MI_TASK_TYPES
};

enum MI_TASK_PRIO {
	MI_UI_PRIO = 0,
	MI_NORMAL_PRIO,
};

enum MI_WAKEUP_TYPE {
	MI_AWAKENED_BY_NORM	=	0,
	MI_AWAKENED_BY_WAKE,
	MI_AWAKENED_BY_UI,

	MI_WAKEUP_MARK,
	MI_WAKEUP_CRIT_MARK,	// critical

	MI_WAKEUP_TASK,
	MI_WAKEUP_CRIT_TASK,	// critical
	MI_WAKEUP_IND_TASK,	// individual

	MI_WAKEUP_VIP_TASK,
};

#define MINOR_TASK		(1 << MI_MINOR_TASK)
#define MASK_REBIND_TASK	(1 << MI_REBIND_TASK)
#define FREEDOM_TASK		(1 << MI_FREEDOM_TASK)

enum MI_USER_FLAG {
	MI_NORMAL_USER,
	MI_32BIT_APP,
	MI_APP_TYPES,
};

#define MASK_MI_GRP_LINK		(1 << MI_GRP_LINK)
#define MASK_MI_WAKEUP_CRIT_TASK	(1 << MI_WAKEUP_CRIT_TASK)
#define MASK_MI_WAKEUP_TASK		(1 << MI_WAKEUP_TASK)
#define MASK_MI_WAKEUP_MARK		(1 << MI_WAKEUP_MARK)
#define MASK_MI_BAL_IRQ_TASK		(1 << MI_BAL_IRQ_TASK)
#define MASK_MI_DIRECT_SET		(1 << MI_DIRECT_SET)
#define MASK_MI_CLBOOST			(1 << MI_CLUSTER_BOOST)
#define MASK_MI_FBOOST         		(1 << MI_FBOOST)
#define MASK_MI_EBOOST          	(1 << MI_EBOOST)
#define MASK_MI_NORUSER			(1 << MI_NORMAL_USER)
#define MASK_LAT_TASK			(1 << MI_LAT_TASK)
#define MASK_MI_VVTASK			(1 << MI_VVIP_TASK)
#define MASK_MI_NEW_VTASK		(1 << MI_NEW_VTASK)
#define MASK_MI_AGING_VTASK		(1 << MI_AGING_VTASK)
#define MASK_MI_UI_TASK			(1 << MI_UI_TASK)
#define MASK_MI_UI_PIPELINE		(1 << MI_UI_PIPELINE)
#define MASK_MI_REN_TASK		(1 << MI_REN_TASK)
#define MASK_MI_SF_TASK 		(1 << MI_SF_TASK)
#define MASK_MI_HWC_TASK		(1 << MI_HWC_TASK)
#define MASK_MI_DYN_VTASK		(1 << MI_DYNAMIC_VTASK)
#define MASK_IN_RTBOOST			(1 << MI_IN_RTBOOST)
#define MASK_MI_LOCK_TASK		(1 << MI_LOCK_TASK)
#define MASK_MI_BINDER_TASK		(1 << MI_BINDER_TASK)
#define MASK_MI_RT_BINDER_FREE		(1 << MI_RT_BINDER_FREE)
#define MASK_MI_REQ_ASYNC_BINDER_TASK	(1 << MI_REQ_ASYNC_BINDER_TASK)
#define MASK_MI_LOW_LT			(1 << MI_LOW_LT_TASK)
#define MASK_MI_ART_MUTEX_TASK		(1 << MI_ART_MUTEX_TASK)
#define MASK_MI_LINK_BOOST_TASK		(1 << MI_LINK_BOOST_TASK)
#define MASK_MI_MMLOCK_BOOST_TASK	(1 << MI_MMLOCK_BOOST_TASK)
#define MASK_MI_PERSIST_VTASK		(1 << MI_PERSIST_VTASK)
#define MASK_MI_LINK_VIPTASK		(MASK_MI_LOCK_TASK | MASK_MI_BINDER_TASK | MASK_MI_ART_MUTEX_TASK \
					| MASK_MI_LINK_BOOST_TASK | MASK_MI_MMLOCK_BOOST_TASK | MASK_MI_RT_BINDER_FREE)
#define MASK_MI_VIPTASK			(MASK_MI_VVTASK | MASK_MI_LINK_VIPTASK | MASK_MI_PERSIST_VTASK)

// wakeup source
#define MASK_MI_AWAKENED_BY_NORM (1 << MI_AWAKENED_BY_NORM)
#define MASK_MI_AWAKENED_BY_WAKE (1 << MI_AWAKENED_BY_WAKE)
#define MASK_MI_AWAKENED_BY_UI	 (1 << MI_AWAKENED_BY_UI)
#define MASK_MI_AWAKENED_BY_ALL	 (MASK_MI_AWAKENED_BY_NORM | MASK_MI_AWAKENED_BY_WAKE | MASK_MI_AWAKENED_BY_UI)
// wakeup mark
#define MASK_MI_WAKEUP_MARK	 (1 << MI_WAKEUP_MARK)
#define MASK_MI_WAKEUP_CRIT_MARK (1 << MI_WAKEUP_CRIT_MARK)
// wakeup task
#define MASK_MI_WAKEUP_CRIT_TASK (1 << MI_WAKEUP_CRIT_TASK)
#define MASK_MI_WAKEUP_IND_TASK	 (1 << MI_WAKEUP_IND_TASK)
#define MASK_MI_WAKEUP_TASK	 (1 << MI_WAKEUP_TASK)
#define MASK_MI_WAKEUP_VIP_TASK  (1 << MI_WAKEUP_VIP_TASK)

#define MASK_MI_WAKEUP_ALL_TASK	 (MASK_MI_WAKEUP_CRIT_TASK | MASK_MI_WAKEUP_IND_TASK | MASK_MI_WAKEUP_TASK)
#define MASK_MI_WAKEUP_LINK_TASK (MASK_MI_WAKEUP_CRIT_TASK | MASK_MI_WAKEUP_TASK)

// task running state
enum MI_TASK_STATE {
	MI_TASK_RUNNING		=	0,
	BLOCKED_ON_FUTEX,
	BOOST_FUTEX,
	TASK_IN_CS,
	TASK_IN_PRECS,
	TASK_IN_D,
	BLOCKED_ON_RWSEM,
	BLOCKED_ON_PRWSEM,
	BLOCKED_ON_MUTEX,
	VIP_BLOCKED_ON_MUTEX,
	BLOCKED_ON_RTMUTEX,
	MMAPLK_ACQ,
	MI_TASK_STATES
};

#define MASK_BLOCKED_ON_FUTEX		(1 << BLOCKED_ON_FUTEX)
#define MASK_BOOST_FUTEX		(1 << BOOST_FUTEX)
#define MASK_MI_CSTASK			(1 << TASK_IN_CS)
#define MASK_MI_PRECSTASK		(1 << TASK_IN_PRECS)
#define MASK_MI_DTASK			(1 << TASK_IN_D)

#define MASK_BLOCKED_ON_RWSEM		(1 << BLOCKED_ON_RWSEM)
#define MASK_BLOCKED_ON_PRWSEM		(1 << BLOCKED_ON_PRWSEM)
#define MASK_BLOCKED_ON_MUTEX		(1 << BLOCKED_ON_MUTEX)
#define MASK_VIP_BLOCKED_ON_MUTEX	(1 << VIP_BLOCKED_ON_MUTEX)
#define MASK_BLOCKED_ON_RTMUTEX		(1 << BLOCKED_ON_RTMUTEX)

#define MASK_IN_ACQ_MMAPLK		(1 << MMAPLK_ACQ)

struct mi_user_struct;

//#ifdef CONFIG_CFS_BANDWIDTH_MI
struct mi_task_bandwidth_data_wrapper{
	unsigned long magic_num;
	struct list_head        throttled_list;
	int list_flag;
	struct delayed_work      wakeup_work;
	struct task_struct *owner;
};
//#endif

struct locking_info {
	u64 waittime_stamp;
	u64 holdtime_stamp;
};

enum MI_LOCK_TYPE {
	MI_LOCK_MUTEX		=	0,
	MI_LOCK_RWSEM,
	MI_LOCK_RTMUTEX,
	MI_LOCK_PCPURW,
	MI_LOCK_FUTEX,
	MI_LOCK_MAX_TYPE,
};
struct lock_hold_info {
	unsigned long start_time[LOCK_MAX_DEPTH];
	int depth;
};

struct mi_task_struct {
	unsigned long magic_num;
//#ifdef CONFIG_CFS_BANDWIDTH_MI
	struct mi_task_bandwidth_data_wrapper bd_data;
//#endif
	bool in_user_list;
	bool in_work_wq;
	bool enqueue_from_wakeup;
	int owner_uid;
	enum MI_TASK_TYPE flag;
	enum MI_WAKEUP_TYPE wakeup;
	cpumask_t cpus_allowed;
	cpumask_t rebind_cpumask;
	struct task_struct *owner;
	struct mi_user_struct *mi_user;
	struct list_head list;
	struct list_head delay_list;
	struct package_runtime_info pkg;
	struct gt_task migt;
	struct rb_node run_node;
	unsigned int on_pkg_tree;
	unsigned long touch_jif;
	u64 enqueue_tm;
	unsigned long enqueue_jif;  /* enqueue time in jiffies */
	u64 double_cyc_sum;
	raw_spinlock_t link_lock;
	unsigned long boost_end_jif;
	unsigned int link_depth;
	int old_cid;
	struct list_head vtask_node;
	int link_owner_pid;
	enum MI_TASK_STATE task_state;

	u64 cs_or_spin_time;
	struct list_head lt_list;
	struct rw_semaphore	*rwsem;
	struct mutex		*mutex;
#ifdef CONFIG_MI_GROUP_ASSIST
	struct mi_task_ravg load;
	struct ls_group *ls_grp;
	struct mi_group *mi_grp;
	struct list_head group_list;
	unsigned int group_flag;
	u64 grp_prev_window_cpus[CPU_NUMS];
	u64 grp_curr_window_cpus[CPU_NUMS];
	u64 grp_curr_window_cpu;
	u64 grp_prev_window_cpu;
	u64 prev_window_cpus[CPU_NUMS];
	u64 curr_window_cpus[CPU_NUMS];
	u64 curr_window_cpu;
	u64 prev_window_cpu;
	bool idle_flag;
	u64 window_start;
	u8 enqueue_after_migration;
#endif
	struct lock_hold_info lock_hold[MI_LOCK_MAX_TYPE];
};

struct mi_user_struct {
	enum MI_USER_FLAG flag;
	unsigned long magic_num;
	rwlock_t lock;
	struct list_head list;
	struct package_runtime_info pkg;
	struct user_struct *user;

#ifdef CONFIG_MI_GROUP_ASSIST
	struct mi_default_group *default_group;
#endif
};

/**
 * The mi_mutex is stored in struct mutex->android_oem_data1, u64 android_oem_data1[2].
 */
struct mi_mutex {
	/*
	* Number of metis vip thread in this mutex's waiter_list
	*/
	int metis_cnt;
	/*
	* Number of xring vip thread in this mutex's waiter_list
	*/
	atomic_t xr_cnt;
	/*
	* VIP task protection period
	*/
	unsigned long long block_vip_jifs;
};

struct mi_task_struct *get_mi_task_struct(struct task_struct *tsk);
struct mi_user_struct *get_mi_user_struct(struct user_struct *tsk);
extern bool set_mi_rq_balance_irq_task(struct task_struct *tsk);
#define MI_TASK_UID(mi_task)    (mi_task->owner_uid)

extern void mi_interface_set_dynamic_vip_task(unsigned int boost_msec);
extern void mi_interface_restore_dynamic_vip_task(void);

#endif /*_LINUX_MI_SCHED_H*/
// END Performance_TurboSched
