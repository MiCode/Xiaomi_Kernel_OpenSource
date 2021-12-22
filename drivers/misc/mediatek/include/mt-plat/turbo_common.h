/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
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
void rwsem_list_add(struct task_struct *p,
		    struct list_head *entry,
		    struct list_head *head);
void rwsem_start_turbo_inherit(struct rw_semaphore *sem);
void rwsem_stop_turbo_inherit(struct rw_semaphore *sem);
void binder_stop_turbo_inherit(struct task_struct *p);
bool binder_start_turbo_inherit(struct task_struct *from,
				struct task_struct *to);
inline bool sub_feat_enable(int type);

#endif /* _TURBO_COMMON_H_ */
