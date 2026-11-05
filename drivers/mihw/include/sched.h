/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022-2024, X-Ring technologies Inc., All rights reserved.
 */

#ifndef _LINUX_XR_SCHED_H
#define _LINUX_XR_SCHED_H

#include <linux/version.h>
#include <linux/sched/cputime.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include "../../../kernel/sched/sched.h"
#include "qos_inherit.h"
#include "qos_sched.h"
#include <linux/sched/walt.h>

struct xr_task_struct {
#if IS_ENABLED(CONFIG_XRING_QOS_INHERIT)
	/*
	 * This flag indicate this vip task is
	 * waiting a mutex or a rwsem
	 */
	u8 blocked;
	struct qi_struct qi;
#endif // CONFIG_XRING_QOS_INHERIT

#if IS_ENABLED(CONFIG_XRING_QOS_SCHED)
	struct qs_struct qs;
#endif
	/* Reserved for other modules */
};

static inline struct mi_mutex *mutex_to_mi(struct mutex *lock)
{
	return (struct mi_mutex *)(lock->android_oem_data1);
}

static inline struct xr_task_struct *get_xr_task(struct task_struct *task)
{
	return (struct xr_task_struct *)(task->android_oem_data1[2]);
}

static inline void set_xr_task(struct task_struct *task,
			       struct xr_task_struct *xrts)
{
	task->android_oem_data1[2] = (unsigned long)xrts;
	BUILD_BUG_ON(sizeof(xrts) > sizeof(task->android_oem_data1[2]));
}

// qi_util.c
extern struct xr_task_struct *get_alloc_xr_task(struct task_struct *task);

#endif /* _LINUX_XR_SCHED_H */
