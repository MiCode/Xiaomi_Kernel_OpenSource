#ifndef MI_MODULE_H
#define MI_MODULE_H
#include <linux/mi_sched.h>
#include <linux/stop_machine.h>
#include "../../android/binder_internal.h"
typedef void (*mi_walt_get_indicies)(struct task_struct *, int *, int *, int, bool *);
typedef void (*mi_find_energy_efficient_cpu) (struct task_struct *, bool *);
typedef void (*update_task_load_hook)(struct task_struct *tsk, int cpu, int flag,
		u64 wallclock, u64 delta);
typedef void (*mi_enqueue_task_fair) (struct rq *, struct task_struct *);
typedef void (*mi_dequeue_task_fair) (struct rq *, struct task_struct *, int);
typedef void (*update_perflock_freq) (int cluster, int freq);
typedef int* (*get_perflock_last_freqs) (void);
typedef void (*oem_corectl_hook_f)(unsigned int cid, unsigned int val);
typedef void (*game_binder_f)(struct binder_transaction *, struct task_struct *);
typedef void (*mi_update_fravg_hooks)(struct task_struct *, struct rq *, u32, u64, u64, int);
typedef void (*game_wake_f)(struct task_struct *, int);
typedef void (*game_enqueue_f)(struct task_struct *, struct rq *);
typedef unsigned int (*walt_window_update_f)(int);
typedef void (*mi_try_to_wake_up) (void *nouse, struct task_struct *task);
typedef void (*mi_enqueue_entity_f) (struct rq *, struct task_struct *);

enum FRAME_LOAD_TYPE {
	FREQ_CPU_LOAD,
	FREQ_GAME_LOAD,
	FREQ_STASK_LOAD,
	FREQ_VIP_LOAD,
	FREQ_RENDER_LOAD,
	FREQ_RCAL_LOAD,
	FREQ_OTHER_VIP,
	FREQ_LOAD_ITEMS,
};

struct mi_rq {
	u64     prev_frame_load[FREQ_LOAD_ITEMS];
	u64     cur_frame_load[FREQ_LOAD_ITEMS];
	u64     nt_prev_frame_load[FREQ_LOAD_ITEMS];
	u64     nt_cur_frame_load[FREQ_LOAD_ITEMS];
	u64	prev_running_frame_load[FREQ_LOAD_ITEMS];
	u64	cur_running_frame_load[FREQ_LOAD_ITEMS];
	u64	frame_start;
	u64	frame_load_last_update;
	struct rb_root_cached vip_tasks;
	bool in_fas_balance_process;
	struct cpu_stop_work fas_balance_work;
};

enum MI_SCHED_MOD{
	GT_TASK,
	SCHED_TYPES,
};

enum MI_WALT_CFS_MOD{
	MIGT_TASK,
	METIS_TASK,
	WALT_CFS_TYPES,
};

enum BOOST_POLICY
{
	NO_MIGT = 0,
	MIGT_1_0,
	MIGT_2_0,
	MIGT_3_0,
};

struct enqueue_task_fair_hooks {
	mi_enqueue_task_fair f;
};

struct dequeue_task_fair_hooks {
	mi_dequeue_task_fair f;
};

struct find_energy_cpu_hooks {
	mi_find_energy_efficient_cpu f;
};

struct walt_get_indicies_hooks {
	mi_walt_get_indicies f;
};

struct sched_load_update_hooks {
	update_task_load_hook f;
};

struct mi_try_to_wake_up_hooks {
	mi_try_to_wake_up f;
};

struct  metis_choose_cpu_args {
    unsigned long min_task_util;
    cpumask_t *skip_cpumask;
    cpumask_t *start_clus_cpumask;
    cpumask_t *render_clus_cpumask;
    cpumask_t *cpu_halt_cpumask;
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
	gt_tsk->cur_window = 0;
	gt_tsk->prev_window = 0;
	gt_tsk->wake_ts = 0;
	gt_tsk->wake_count = 0;
	gt_tsk->lock_start_time = 0;

	for (i = 0; i < NUM_MIGT_BUCKETS; i++) {
		gt_tsk->bucket[i] = 0;
#ifdef VTASK_BOOST_DEBUG
		gt_tsk->boostat[i] = 0;
#endif
	}
}

#endif

