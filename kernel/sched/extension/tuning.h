/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/cgroup.h>
#include <linux/sched.h>
#ifdef CONFIG_UCLAMP_TASK
extern struct mutex uclamp_mutex;
struct task_struct *find_task_by_vpid(pid_t vnr);
void uclamp_group_get(struct task_struct *p,
			     struct cgroup_subsys_state *css,
			     struct uclamp_se *uc_se,
			     unsigned int clamp_id, unsigned int clamp_value);
#endif
