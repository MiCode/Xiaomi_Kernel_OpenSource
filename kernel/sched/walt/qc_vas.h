/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */

#include "../sched.h"
#include "../../../fs/proc/internal.h"

#include "walt.h"
#include "trace.h"

#ifdef CONFIG_SCHED_WALT
#ifdef CONFIG_HZ_300
/*
 * Tick interval becomes to 3333333 due to
 * rounding error when HZ=300.
 */
#define DEFAULT_SCHED_RAVG_WINDOW (3333333 * 5)
#else
/* Min window size (in ns) = 16ms */
#define DEFAULT_SCHED_RAVG_WINDOW 16000000
#endif

/* Max window size (in ns) = 1s */
#define MAX_SCHED_RAVG_WINDOW 1000000000

#define NR_WINDOWS_PER_SEC (NSEC_PER_SEC / DEFAULT_SCHED_RAVG_WINDOW)

extern int num_sched_clusters;

extern unsigned int walt_big_tasks(int cpu);
extern void reset_task_stats(struct task_struct *p);
extern void walt_rotate_work_init(void);
extern void walt_rotation_checkpoint(int nr_big);
extern void walt_fill_ta_data(struct core_ctl_notif_data *data);
extern int sched_set_group_id(struct task_struct *p, unsigned int group_id);
extern unsigned int sched_get_group_id(struct task_struct *p);
extern int sched_set_init_task_load(struct task_struct *p, int init_load_pct);
extern u32 sched_get_init_task_load(struct task_struct *p);
extern void core_ctl_check(u64 wallclock);
extern int sched_set_boost(int enable);
extern int sched_isolate_count(const cpumask_t *mask, bool include_offline);

extern struct list_head cluster_head;
#define for_each_sched_cluster(cluster) \
	list_for_each_entry_rcu(cluster, &cluster_head, list)

static inline u32 cpu_cycles_to_freq(u64 cycles, u64 period)
{
	return div64_u64(cycles, period);
}

static inline unsigned int sched_cpu_legacy_freq(int cpu)
{
	unsigned long curr_cap = arch_scale_freq_capacity(cpu);

	return (curr_cap * (u64) cpu_rq(cpu)->wrq.cluster->max_possible_freq) >>
		SCHED_CAPACITY_SHIFT;
}

extern __read_mostly bool sched_freq_aggr_en;
static inline void walt_enable_frequency_aggregation(bool enable)
{
	sched_freq_aggr_en = enable;
}

#ifndef CONFIG_IRQ_TIME_ACCOUNTING
static inline u64 irq_time_read(int cpu) { return 0; }
#endif

#else
static inline unsigned int walt_big_tasks(int cpu)
{
	return 0;
}

static inline int sched_set_boost(int enable)
{
	return -EINVAL;
}
#endif
