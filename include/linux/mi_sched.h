#ifndef _LINUX_MI_SCHED_H
#define _LINUX_MI_SCHED_H

#include <linux/sched.h>

inline bool set_mi_rq_balance_irq_task(struct task_struct *tsk)
{
	return false;
}

#endif