/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _TURBO_COMMON_H_
#define _TURBO_COMMON_H_

enum {
	SUB_FEAT_LOCK		= 1U << 0,
	SUB_FEAT_BINDER		= 1U << 1,
	SUB_FEAT_SCHED		= 1U << 2,
	SUB_FEAT_FLAVOR_BIGCORE = 1U << 3,
};

extern void init_turbo_attr(struct task_struct *p);
extern bool is_turbo_task(struct task_struct *p);
extern bool do_task_turbo(struct task_struct *p);
extern int get_turbo_feats(void);
extern void cgroup_set_turbo_task(struct task_struct *p);
extern void sys_set_turbo_task(struct task_struct *p);

#endif /* _TURBO_COMMON_H_ */
