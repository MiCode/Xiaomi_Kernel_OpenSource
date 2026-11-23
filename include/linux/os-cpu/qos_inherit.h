/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2025-2025 OS_CPU Technologies Co., Ltd.
 */
#ifndef __QIS_QOS_H__
#define __QIS_QOS_H__

#include <linux/notifier.h>
#include <linux/sched.h>
#include <linux/types.h>

enum {
	QOS_BINDER,
	QOS_MUTEX,
	QOS_RWSEM,
	QOS_PERCPU_RWSEM,
	QOS_FUTEX,
#if IS_ENABLED(CONFIG_OS_CPU_QIS_V2)
	QOS_FUSE,
	QOS_RCU,
#endif
	QOS_TYPE_NUM,
	QOS_TYPE_FIRST = QOS_BINDER,
};

#if IS_ENABLED(CONFIG_OS_CPU_QIS_V1)
enum {
	QOS_LOW,
	QOS_NORMAL,
	QOS_HIGH,
	QOS_CRITICAL,
	QOS_NUM,
	QOS_FIRST = QOS_LOW,
};

union qos_union {
	u32 qos;
	struct {
		u32 qos_lvl : 4;
		u32 sched_opt : 1;
		u32 vip_sched_qos : 3;
		u32 reserved : 24;
	} bits;
};
#elif IS_ENABLED(CONFIG_OS_CPU_QIS_V2)
enum {
	QOS_NONE,
	QOS_LOW,
	QOS_NORMAL,
	QOS_HIGH_SYS,
	QOS_HIGH_PERC,
	QOS_HIGH_VISIBLE,
	QOS_CRITICAL_DRAW_NON_FOCUS,
	QOS_CRITICAL_INPUT,
	QOS_CRITICAL_ANIM,
	QOS_CRITICAL_DRAW_FOCUS,
	QOS_CRITICAL_COMP,
	QOS_NUM,
	QOS_FIRST = QOS_LOW,
	QOS_CRITICAL_FIRST = QOS_CRITICAL_DRAW_NON_FOCUS,
	QOS_CRITICAL = QOS_CRITICAL_FIRST,
	QOS_HIGH_FIRST = QOS_HIGH_SYS,
};

enum {
	GPU_BLOCK_START,
	GPU_BLOCK_END,
	QOS_EVENT_NUM,
};

enum {
	NON_SCHED,
	RT_SCHED,
	VIP_SCHED,
	SCHED_NUM,
};

enum {
	/* bit0~bit3 for qos level */
	QOS_BIT_OFFSET = 0,
	/* bit4~bit5 for sched type, 0b01 is rt sched, 0b10 is vip sched */
	SCHED_BIT_OFFSET = 4,
	/* bit6~bit31 reserved */
	MAX_BIT = 31,
};

#define QOS_LVL_MASK	0xf
#define SCHED_RT_MASK	0x1
#define SCHED_VIP_MASK	0x2
#define SCHED_MASK	0x3
#define SCHED_MASK_FULL	(0x3 << SCHED_BIT_OFFSET)

#define GET_QOS_LVL(qos)	((qos) & QOS_LVL_MASK)
#define GET_SCHED(qos)		(((qos) >> SCHED_BIT_OFFSET) & SCHED_MASK)
#define GET_RT_SCHED(qos)	(((qos) >> SCHED_BIT_OFFSET) & SCHED_RT_MASK)
#define JOINT_QOS(orig, next)	(((orig) & SCHED_MASK_FULL) | next)

struct gpu_blocked_info {
	pid_t pid;
	u64 blocked_ns;
};

struct inherit_node {
	struct list_head list;
	void *lock_or_binder;
	int prio;
	u32 qos;
};
#endif

enum qievent_notif_type {
	QIEVENT_QOS_SET = 1,
	QIEVENT_QOS_RESET,
	QIEVENT_QOS_UNSET,
	QIEVENT_QOS_INHERIT,
	QIEVENT_QOS_UNINHERIT,
	QIEVENT_QOS_CORRECT,
	QIEVENT_QOS_TASK_EXIT,
	QIEVENT_QOS_EVENT,
};

struct qievent_struct {
	struct task_struct *from;
	struct task_struct *to;
#if IS_ENABLED(CONFIG_OS_CPU_QIS_V1)
	union qos_union from_qos;
	union qos_union to_qos;
#elif IS_ENABLED(CONFIG_OS_CPU_QIS_V2)
	int prio;
	int vip_prio;
	u32 qos;
	u32 eff_qos;
	u32 event;
#endif
};

struct inherit_list {
	struct list_head list;
#if IS_ENABLED(CONFIG_OS_CPU_QIS_V1)
	spinlock_t lock;
	void *lock_or_binder;
#endif
};

struct qi_struct {
	spinlock_t lock;
	atomic_t flag;
	atomic_t static_qos;
	atomic_t dyn_qos;
	atomic_t proc_static_qos;
	pid_t futex_owner;
	unsigned long timeout;
	struct inherit_list inherit[QOS_TYPE_NUM];
	u64 opt_spin_start_time;
#if IS_ENABLED(CONFIG_OS_CPU_QIS_V2)
	int static_prio;
	int dyn_prio;
	int dyn_vip_prio;
	u8 blocked_on_gpu;
	u8 async_binder_todo;
#endif
};

struct os_cpu_binder_transaction {
	pid_t pid;
	u32 reserved;
};

struct os_cpu_rwsem_struct {
	u64 writing_finish_flag : 1;
	u64 reserve : 63;
};

enum QosLevel {
	/*
	 * QosLevel: [bit 7:4] sched_type | [bit 3:0] qos_level
	 * RT_CRITICAL  = rt  << 4 | level = 0001 1001 = 25
	 * VIP_CRITICAL = vip << 4 | level = 0010 0110 = 38
	 */
	RT_CRITICAL  = 25,
	VIP_CRITICAL = 38,
};

#if IS_ENABLED(CONFIG_OS_CPU_QIS_V1) && IS_ENABLED(CONFIG_OS_CPU_QOS_INHERIT)
int qi_intf_set_qos(void __user *uarg);
int qi_intf_get_qos_by_tid(void __user *uarg);
int qi_intf_get_qos_by_pid(void __user *uarg);
void qi_intf_schedinfo_update(struct qievent_struct *data);
void qi_intf_schedinfo_reset(struct qievent_struct *data);
#elif IS_ENABLED(CONFIG_OS_CPU_QIS_V2) && IS_ENABLED(CONFIG_OS_CPU_QOS_INHERIT)
void update_static_prio(struct task_struct *task, int prio);
int get_task_prio(struct task_struct *task);
u32 get_static_qos_lvl(struct task_struct *task);
bool is_gpu_blocked_task(struct task_struct *task);
struct task_struct *gpu_blocked_longest(void);
int acuqire_gpu_blockeds(struct gpu_blocked_info *array, u32 len);
#elif IS_ENABLED(CONFIG_OS_CPU_QIS_V1)
static inline int qi_intf_set_qos(void __user *uarg) { return 0; }
static inline int qi_intf_get_qos_by_tid(void __user *uarg) { return 0; }
static inline int qi_intf_get_qos_by_pid(void __user *uarg) { return 0; }
static inline void qi_intf_schedinfo_update(struct qievent_struct *data) {}
static inline void qi_intf_schedinfo_reset(struct qievent_struct *data) {}
#elif IS_ENABLED(CONFIG_OS_CPU_QIS_V2)
static inline void update_static_prio(struct task_struct *task, int prio) {}
static inline int get_task_prio(struct task_struct *task) { return 0; }
static inline u32 get_static_qos_lvl(struct task_struct *task) { return 0; }
static inline bool is_gpu_blocked_task(struct task_struct *task) { return false; }
static inline struct task_struct *gpu_blocked_longest(void) { return NULL; }
static inline int acuqire_gpu_blockeds(struct gpu_blocked_info *array, u32 len) { return 0; }
#endif

#if IS_ENABLED(CONFIG_OS_CPU_QOS_INHERIT)
int register_qievent_notifier(struct notifier_block *nb);
int unregister_qievent_notifier(struct notifier_block *nb);
bool is_qos_task(struct task_struct *task);
int get_proc_static_qos_lvl(struct task_struct *task);
u32 get_qos_lvl(struct task_struct *task);
int kern_qi_set_qos(struct task_struct *p, u32 qos_value);
u32 get_task_qos_lvl(struct task_struct *task);
#else
static inline int register_qievent_notifier(struct notifier_block *nb) { return 0; }
static inline int unregister_qievent_notifier(struct notifier_block *nb) { return 0; }
static inline bool is_qos_task(struct task_struct *task) { return false; }
static inline int get_proc_static_qos_lvl(struct task_struct *task) { return 0; }
static inline u32 get_qos_lvl(struct task_struct *task) { return 0; }
static inline int kern_qi_set_qos(struct task_struct *p, u32 qos_value) { return 0; }
static inline u32 get_task_qos_lvl(struct task_struct *task) { return 0; }
#endif // CONFIG_OS_CPU_QOS_INHERIT

#endif