#ifndef _PRIO_TRACER_H
#define _PRIO_TRACER_H

#include <linux/sched.h>

#define PTS_DEFAULT_PRIO (-101)

#define PTS_USER 0
#define PTS_KRNL 1
#define PTS_BNDR 2

extern void create_prio_tracer(pid_t tid);
extern void delete_prio_tracer(pid_t tid);

extern void update_prio_tracer(pid_t tid, int prio, int policy, int kernel);

extern void set_user_nice_syscall(struct task_struct *p, long nice);
extern void set_user_nice_binder(struct task_struct *p, long nice);
extern int sched_setscheduler_syscall(struct task_struct *, int, const struct sched_param *);
extern int sched_setscheduler_nocheck_binder(struct task_struct *, int, const struct sched_param *);
#endif
