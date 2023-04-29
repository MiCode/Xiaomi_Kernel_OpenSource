#ifndef MI_MODULE_H
#define MI_MODULE_H

#include <linux/mi_sched.h>

typedef void (*mi_walt_get_indicies)(struct task_struct *, int *, int *, int, bool *);
typedef void (*mi_find_energy_efficient_cpu) (struct task_struct *, bool *);

typedef void (*update_task_load_hook)(struct task_struct *tsk, int cpu, int flag,
		u64 wallclock, u64 delta);
typedef void (*mi_enqueue_task_fair) (struct rq *, struct task_struct *);
typedef void (*mi_dequeue_task_fair) (struct rq *, struct task_struct *);

enum MI_SCHED_MOD{
	GT_TASK,
	SCHED_TYPES,
};

struct sched_load_update_hooks {
	update_task_load_hook f;
};

static inline void init_gt_task(struct mi_task_struct *tsk)
{
	int i;
	struct gt_task *gt_tsk = &tsk->migt;

	gt_tsk->migt_count  = 0;
	gt_tsk->prev_sum    = 0;
	gt_tsk->max_exec    = 0;
	gt_tsk->fps_exec    = 0;
	gt_tsk->fps_mexec   = 0;
	gt_tsk->flag        = MIGT_NORMAL_TASK;
	gt_tsk->run_times   = 0;
	gt_tsk->wake_render = 0;
	gt_tsk->boost_end   = 0;

	for (i = 0; i < NUM_MIGT_BUCKETS; i++) {
		gt_tsk->bucket[i] = 0;
#ifdef VTASK_BOOST_DEBUG
		gt_tsk->boostat[i] = 0;
#endif
	}
}

#endif
