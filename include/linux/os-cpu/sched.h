/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022-2024, X-Ring technologies Inc., All rights reserved.
 */

#ifndef _LINUX_OS_CPU_SCHED_H
#define _LINUX_OS_CPU_SCHED_H

#include <linux/version.h>
#include <linux/sched/cputime.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/os-cpu/qos_inherit.h>
#include <linux/os-cpu/qos_sched.h>
#include <kernel/sched/sched.h>
#include <linux/sched/task.h>
#include <linux/sched/walt.h>
#if IS_ENABLED(CONFIG_MI_KERNEL_MONITOR)
#include <linux/mi_kernel_monitor.h>
#endif

enum {
	PINGPONG_THERM = 0,
};

#if IS_ENABLED(CONFIG_OS_CPU_SCHED_VIP)
struct vip_task {
	int prio;
	int orig_prio;
	struct list_head entry;
#if IS_ENABLED(CONFIG_OS_CPU_VIP_BANDWIDTH)
	u64 exec_start;
#endif
};
#endif


struct os_cpu_mutex {
	/*
	 * Number of vip thread in this mutex's
	 * waiter_list
	 */
	atomic_long_t cnt;
};

#define ITER_MAX_DEPTH	3

struct plat_task_struct {
#if IS_ENABLED(CONFIG_OS_CPU_DYNAMIC_TASK_SIZE)
	struct walt_task_struct wts;
#else
	atomic_t magic;
	struct task_struct *task;
#endif

#if IS_ENABLED(CONFIG_OS_CPU_QOS_INHERIT)
	/*
	 * This flag indicate this vip task is
	 * waiting a mutex or a rwsem
	 */
	u8 blocked;
	/*
	 * This indicate which type lock this task is blocked on,
	 * binder, mutex, rwsem, percpu-rwsem or futex
	 */
	u8 blocked_type : 4;
	u8 blocked_inited : 1;
	u8 iter_depth : 2;
	u8 reserved : 1;
	pid_t binder_client;
	u32 uaddr2_saved;
	void *blocked_on_queue;
	unsigned long iter_ptrs[ITER_MAX_DEPTH];
	pid_t blocked_owner;
	spinlock_t blocked_lock;
	struct qi_struct *qi;
#endif

#if IS_ENABLED(CONFIG_OS_CPU_QOS_SCHED)
	struct qs_struct qs;
#endif

#if IS_ENABLED(CONFIG_OS_CPU_SCHED_VIP)
	struct vip_task	vip;
#endif

#if IS_ENABLED(CONFIG_MI_KERNEL_MONITOR)
	struct mi_kernel_monitor monitor;
#endif
	/* Reserved for other modules */
};

#if IS_ENABLED(CONFIG_OS_CPU_DYNAMIC_TASK_SIZE)
static inline struct plat_task_struct *get_plat_task(struct task_struct *p)
{
	return (struct plat_task_struct *)android_task_vendor_data(p);
}

#define pts_to_ts(pts) ({ \
		void *__mptr = (void *)(pts); \
		(u64 *)pts == &vendor_data_pad[0] ? &init_task :\
		((struct task_struct *)(__mptr - sizeof(struct task_struct))); })
#else
/* The default value for magic is 0 */
#define TASK_MAGIC_ALLOC	0xacbdadbc
#define TASK_MAGIC_FREE		0xabcdabcd
#define TASK_MAGIC_INUSE 	0xadbcacbd

struct android_oem_data {
	atomic_t magic;
	struct plat_task_struct *pts;
};

#define set_plat_task_magic(p, v) \
	atomic_set(&((struct android_oem_data *)(p->android_oem_data1))->magic, v)
#define get_plat_task_magic(p) \
	atomic_read(&((struct android_oem_data *)(p->android_oem_data1))->magic)

#define set_plat_task_addr(p, v) \
	(((struct android_oem_data *)(p->android_oem_data1))->pts = (v))
#define get_plat_task_addr(p) \
	(((struct android_oem_data *)(p->android_oem_data1))->pts)

static inline struct plat_task_struct *get_plat_task(struct task_struct *p)
{
	if (get_plat_task_magic(p) == TASK_MAGIC_ALLOC)
		return get_plat_task_addr(p);
	return NULL;
}

#define pts_to_ts(pts) ({ \
		(struct task_struct *)(pts->task); })
#endif /* CONFIG_OS_CPU_DYNAMIC_TASK_SIZE */

#ifdef CONFIG_UCLAMP_TASK
static inline bool uclamp_boosted(struct task_struct *p)
{
	return uclamp_eff_value(p, UCLAMP_MIN) > 0;
}
#else
static inline bool uclamp_boosted(struct task_struct *p)
{
	return false;
}
#endif

#ifdef CONFIG_UCLAMP_TASK_GROUP
static inline bool uclamp_latency_sensitive(struct task_struct *p)
{
	struct cgroup_subsys_state *css = task_css(p, cpu_cgrp_id);
	struct task_group *tg;

	if (!css)
		return false;

	tg = container_of(css, struct task_group, css);
	return tg->latency_sensitive;
}
#else
static inline bool uclamp_latency_sensitive(struct task_struct *p)
{
	return false;
}
#endif

static inline struct os_cpu_mutex *mutex_to_oc(struct mutex *lock)
{
	return (struct os_cpu_mutex *)lock->android_oem_data1;
}

#endif /* _LINUX_OS_CPU_SCHED_H */
