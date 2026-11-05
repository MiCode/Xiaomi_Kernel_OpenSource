/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2024-2024 XRing Technologies Co., Ltd.
 */
#ifndef __QIS_QOS_H__
#define __QIS_QOS_H__

#include <linux/notifier.h>
#include <linux/sched.h>
#include <linux/types.h>

/* qos feature types */
enum {
	QOS_BINDER,
	QOS_MUTEX,
	QOS_RWSEM,
	QOS_PERCPU_RWSEM,
	QOS_FUTEX,
	QOS_TYPE_NUM,
	QOS_TYPE_FIRST = QOS_BINDER,
};

/* qos feature policy enum */
enum {
	QI_ENABLE,
	QI_BINDER,
	QI_MUTEX_JUMP,
	QI_MUTEX_INHERIT,
	QI_RWSEM_JUMP,
	QI_RWSEM_INHERIT,
	QI_PERCPU_RWSEM_JUMP,
	QI_PERCPU_RWSEM_INHERIT,
	QI_FUTEX_INHERIT,
	QI_LOCK_OPT,
	QI_MAX,
	QI_FEATURE_FIRST = QI_BINDER,
};

/* qos level types */
enum {
	QOS_LOW,
	QOS_NORMAL,
	QOS_HIGH,
	QOS_CRITICAL,
	QOS_NUM,
	QOS_FIRST = QOS_LOW,
};

/* set or inherit qos event types */
enum qievent_notif_type {
	QIEVENT_QOS_SET = 1,
	QIEVENT_QOS_UNSET,
	QIEVENT_QOS_INHERIT,
	QIEVENT_QOS_UNINHERIT,
	QIEVENT_QOS_CORRECT,
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

struct qievent_struct {
	struct task_struct *from;
	struct task_struct *to;
	union qos_union from_qos;
	union qos_union to_qos;
};

/**
 * lock_or_binder saves the lock or binder pointer that blocked qos task.
 */
struct inherit_list {
	struct list_head list;
	spinlock_t lock;
	void *lock_or_binder;
};

/**
 * qi_struct is the key structure for each task to inherit QoS tags,
 * where the inherit array stores data of each inheritance event.
 */
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
	void *mutex_or_rwsem;
};

/*
* 0-31bits of binder_tranaction's vendor_data used by
* qos_binder in qi_binder.c.
* 32-59bits of binder_tranaction's vendor_data used by
* binder_prio in binder_prio.c.
* 60-63bits of binder_tranaction's vendor_data used by
* walt_task_struct->boost.
*/
struct xr_binder_transaction {
	pid_t pid;
	u32 binder_prio : 28;
	u32 walt_boost : 4;
};

/*
* get futex owner tid from uaddr2
*/
#define FUTEX_ID_BITS 16
#define FUTEX_PID_BITS 16
#define FUTEX_PID_MASK (((1 << FUTEX_PID_BITS) - 1) << FUTEX_ID_BITS)
#define FUTEX_PID_OFFSET FUTEX_ID_BITS
#define FUTEX_PID_VALUE(v) (((v) & FUTEX_PID_MASK) >> FUTEX_PID_OFFSET)

struct xr_rwsem_struct {
	u64 writing_finish_flag : 1;
	u64 reserve : 63;
};

int qi_intf_set_qos(void __user *uarg);
int qi_intf_get_qos_by_tid(void __user *uarg);
int qi_intf_get_qos_by_pid(void __user *uarg);
void qi_intf_schedinfo_update(struct qievent_struct *data);
void qi_intf_schedinfo_reset(struct qievent_struct *data);
int register_qievent_notifier(struct notifier_block *nb);
int unregister_qievent_notifier(struct notifier_block *nb);
int get_proc_static_qos_lvl(struct task_struct *task);
u32 get_qos_lvl(struct task_struct *task);

#if IS_ENABLED(CONFIG_XRING_QOS)
bool is_qos_task(struct task_struct *task);
bool is_qi_feat_enabled(u8 type);
#else
static inline bool is_qos_task(struct task_struct *task)
{
	return false;
}

static inline bool is_qi_feat_enabled(u8 unused)
{
	return false;
}
#endif // CONFIG_XRING_QOS

#endif
