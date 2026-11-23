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

/*
* sm8850 mqos_task_struct pointer saved in android_vendor_data1[0] of task_struct
*/
struct mqos_task_struct {
#if IS_ENABLED(CONFIG_MQOS_INHERIT)
	/*
	 * This flag indicate this vip task is
	 * waiting a mutex or a rwsem
	 */
	u8 blocked;
	struct qi_struct qi;
#endif
#if IS_ENABLED(CONFIG_MQOS_SCHED)
	struct qs_struct qs;
#endif
	/* Reserved for other modules */
};

#define xrts_to_ts(xrts) ({ \
		void *__mptr = (void *)(xrts); \
		((struct task_struct *)(__mptr - \
			offsetof(struct task_struct, android_vendor_data1))); })

static inline struct mi_mutex *mutex_to_mi(struct mutex *lock)
{
	return (struct mi_mutex *)(lock->android_oem_data1);
}

static inline struct mqos_task_struct *get_mqos_task(struct task_struct *task)
{
	return (struct mqos_task_struct *)(task->android_vendor_data1[0]);
}

// qi_utils.c
extern struct mqos_task_struct *get_alloc_mqos_task(struct task_struct *task);
// qi.c
extern void qos_set_mi_vip_task(struct task_struct *tsk, unsigned long mask);
extern void qos_restore_mi_vip_task(struct task_struct *tsk, unsigned long mask);
extern bool qos_is_mi_vip_task(struct task_struct *tsk, unsigned long mask);

typedef bool (*qos_cold_start_task)(struct task_struct * task);
extern void register_qos_cold_start_task(qos_cold_start_task func);

#endif /* _LINUX_XR_SCHED_H */
