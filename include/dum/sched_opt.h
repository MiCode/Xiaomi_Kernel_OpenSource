#ifndef __SCHED_OPT_H__
#define __SCHED_OPT_H__

static inline void sched_boost_task_prio(const char *tag, struct task_struct *tsk) {}
static inline void sched_restore_task_prio(const char *tag, struct task_struct *tsk) {}

#endif
