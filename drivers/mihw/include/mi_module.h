// MIUI ADD: Performance_TurboSched
#ifndef MI_MODULE_H
#define MI_MODULE_H
#include <linux/mi_sched.h>
#include <linux/stop_machine.h>
#include "../../android/binder_internal.h"

typedef void (*mi_walt_get_indicies)(struct task_struct *, int *, int *, int, bool *, bool *);
#define MAX_TRACE_MIG 2
typedef void (*mi_update_global_period_f)(unsigned int);
typedef void (*mi_find_energy_efficient_cpu) (struct task_struct *, bool *);
typedef void (*update_task_load_hook)(struct task_struct *tsk, int cpu, int flag,
		u64 wallclock, u64 delta);
typedef void (*mi_enqueue_task_fair) (struct rq *, struct task_struct *, int);
typedef void (*mi_dequeue_task_fair) (struct rq *, struct task_struct *, int);
typedef void (*update_perflock_freq) (int cluster, int freq);
typedef int* (*get_perflock_last_freqs) (void);
typedef void (*oem_corectl_hook_f)(unsigned int cid, unsigned int val);
typedef void (*game_binder_f)(struct binder_transaction *, struct task_struct *);
typedef void (*game_enqueue_f)(struct task_struct *, struct rq *);
typedef bool (*task_detect_f)(struct task_struct *);
typedef int  (*mi_sched_detect_f)(void);
typedef unsigned int (*walt_window_update_f)(int);
typedef void (*mi_try_to_wake_up) (void *nouse, struct task_struct *task);
typedef bool (*mi_lb_detect_f) (struct task_struct *, int, int, int);
typedef void (*task_group_change_update_f) (void);
typedef void (*mi_update_fravg_hooks)(struct task_struct *, struct rq *, u32, u64, u64, int);
typedef void (*mi_update_group_nr_hooks)(struct task_struct *, int, int);
typedef void (*mi_enqueue_entity_f) (struct rq *, struct task_struct *);
typedef void (*mi_migrate_fload_f)(bool, struct task_struct *, int, bool, bool);
typedef void (*mi_run_frequpdate_f)(struct rq *, int);
typedef void (*mi_migarte_fix_fld_f)(struct rq *);

#define DEFINE_HOOK(name, return_type, ...) \
    typedef return_type (*oem_##name##_f)(__VA_ARGS__); \
    static oem_##name##_f game_##name##_hook = NULL; \
    void register_game_##name##_hook(oem_##name##_f f) { \
        if (likely(f)) { \
            game_##name##_hook = f; \
        } \
    } \
    EXPORT_SYMBOL_GPL(register_game_##name##_hook); \
    void unregister_game_##name##_hook(void) { \
        game_##name##_hook = NULL; \
    } \
    EXPORT_SYMBOL_GPL(unregister_game_##name##_hook)

#define EXTEND_HOOK(name, return_type, ...) \
    typedef return_type (*oem_##name##_f)(__VA_ARGS__); \
    extern void register_game_##name##_hook(oem_##name##_f f); \
    extern void unregister_game_##name##_hook(void)

enum FRAME_LOAD_TYPE {
	FREQ_CPU_LOAD,
	FREQ_GAME_LOAD,
	FREQ_STASK_LOAD,
	FREQ_VIP_LOAD,
	FREQ_RENDER_LOAD,
	FREQ_LOAD_ITEMS,
};

struct cpu_layered_task {
	unsigned int vip_nr_running;
	unsigned int sys_nr_running;

	u64 rt_cumulative_runnable_avg_scaled;
	u64 rt_pred_demands_sum_scaled;

	u64 vip_cumulative_runnable_avg_scaled;
	u64 vip_pred_demands_sum_scaled;

	u64 sys_cumulative_runnable_avg_scaled;
	u64 sys_pred_demands_sum_scaled;

	u64 raw_cumulative_runnable_avg_scaled;
	u64 raw_pred_demands_sum_scaled;
};

struct load_subs {
	u64 fs;
	u64 subs;
	u64 nt_subs;
};

struct mi_rq {
	u64	prev_frame_load[FREQ_LOAD_ITEMS];
	u64     cur_frame_load[FREQ_LOAD_ITEMS];
	u64     nt_prev_frame_load[FREQ_LOAD_ITEMS];
	u64     nt_cur_frame_load[FREQ_LOAD_ITEMS];
	u64	frame_start_group;
	u64	frame_start;
	u64	frame_load_last_update;
	u64	prev_frame_exec;
	u64	cur_frame_exec;
	u64	nt_prev_frame_exec;
	u64	nt_cur_frame_exec;
	u64	grp_prev_frame_exec;
	u64	grp_cur_frame_exec;
	struct load_subs ls[MAX_TRACE_MIG];
	struct rb_root_cached vip_tasks;
	struct cpu_layered_task layered;
	struct list_head vip_tasks_list;
	// for low latency task but not vip
	spinlock_t low_lt_lock;
	struct list_head low_lt_list;
};

enum MI_SCHED_MOD{
	GRP_FRAME,
	GT_TASK,
	METIS_TASK,
	MI_SCHED_TYPE,
};

enum BOOST_POLICY
{
	NO_MIGT = 0,
	MIGT_1_0,
	MIGT_2_0,
	MIGT_3_0,
};

enum LAYERED_LOAD_CMD {
	LAYERED_LOAD_DEC = -1,
	LAYERED_LOAD_UPDATE,
	LAYERED_LOAD_INC
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
	int order_index;
	int end_index;
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
	gt_tsk->lock_start_time = 0;
	gt_tsk->wake_count = 0;

	for (i = 0; i < NUM_MIGT_BUCKETS; i++) {
		gt_tsk->bucket[i] = 0;
#ifdef VTASK_BOOST_DEBUG
		gt_tsk->boostat[i] = 0;
#endif
	}
}

#endif
// END Performance_TurboSched
