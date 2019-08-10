/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _EXTENSION_TUNING_H
#define _EXTENSION_TUNING_H

#ifdef CONFIG_MTK_SCHED_EXTENSION

#include <linux/types.h>
#include <linux/cgroup.h>
#include <linux/sched.h>
#include <trace/events/sched.h>
#include <linux/cpumask.h>
#include "sched.h"
#ifdef CONFIG_UCLAMP_TASK
extern struct mutex uclamp_mutex;
struct task_struct *find_task_by_vpid(pid_t vnr);
void uclamp_group_get(struct task_struct *p,
			     struct cgroup_subsys_state *css,
			     struct uclamp_se *uc_se,
			     unsigned int clamp_id, unsigned int clamp_value);
#if defined(CONFIG_UCLAMP_TASK_GROUP) && defined(CONFIG_SCHED_TUNE)
int schedtune_css_uclamp(int idx, unsigned int clamp_id,
		struct cgroup_subsys_state **css, struct uclamp_se **uc_se);
void cpu_util_update(struct cgroup_subsys_state *css,
		unsigned int clamp_id, unsigned int group_id,
		unsigned int value);
#endif
#endif

#ifdef CONFIG_MTK_SCHED_CPU_PREFER
extern int valid_cpu_prefer(int task_prefer);
#endif

#ifdef CONFIG_MTK_SCHED_BIG_TASK_MIGRATE
extern bool big_task_rotation_enable;
#endif

#endif /* CONFIG_MTK_SCHED_EXTENSION */

#endif
