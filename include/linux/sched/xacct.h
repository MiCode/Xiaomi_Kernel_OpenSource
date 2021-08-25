/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SCHED_XACCT_H
#define _LINUX_SCHED_XACCT_H

/*
 * Extended task accounting methods:
 */

#include <linux/sched.h>

#ifdef CONFIG_TASK_XACCT
#include <linux/mi_ioac.h>
static inline void add_rchar(struct task_struct *tsk, ssize_t amt)
{
	tsk->ioac.rchar += amt;
	add_last_ioac(tsk, RCHAR);
}

static inline void add_wchar(struct task_struct *tsk, ssize_t amt)
{
	tsk->ioac.wchar += amt;
	add_last_ioac(tsk, WCHAR);
}

static inline void inc_syscr(struct task_struct *tsk)
{
	tsk->ioac.syscr++;
	add_last_ioac(tsk, CR);
}

static inline void inc_syscw(struct task_struct *tsk)
{
	tsk->ioac.syscw++;
	add_last_ioac(tsk, CW);
}

static inline void inc_syscfs(struct task_struct *tsk)
{
	tsk->ioac.syscfs++;
	add_last_ioac(tsk, CS);
}
#else
static inline void add_rchar(struct task_struct *tsk, ssize_t amt)
{
}

static inline void add_wchar(struct task_struct *tsk, ssize_t amt)
{
}

static inline void inc_syscr(struct task_struct *tsk)
{
}

static inline void inc_syscw(struct task_struct *tsk)
{
}

static inline void inc_syscfs(struct task_struct *tsk)
{
}
#endif

#endif /* _LINUX_SCHED_XACCT_H */
