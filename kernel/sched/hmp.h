/*
 * Copyright (C) 2016 MediaTek Inc.
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

/* Heterogenous multi processor common utils */
LIST_HEAD(hmp_domains);

#ifdef CONFIG_SCHED_HMP

/* CPU cluster statistics for task migration control */
#define HMP_GB (0x1000)
#define HMP_SELECT_RQ (0x2000)
#define HMP_LB (0x4000)
#define HMP_MAX_LOAD (NICE_0_LOAD - 1)

#define __LOAD_AVG_MAX 47742 /* FIXME, maximum possible load avg */

#ifdef CONFIG_SCHED_HMP_PRIO_FILTER
unsigned int hmp_up_prio = NICE_TO_PRIO(CONFIG_SCHED_HMP_PRIO_FILTER_VAL);
#define task_low_priority(prio) ((prio >= hmp_up_prio)?1:0)
#define cfs_nr_dequeuing_low_prio(cpu) \
	cpu_rq(cpu)->cfs.avg.nr_dequeuing_low_prio
#define cfs_reset_nr_dequeuing_low_prio(cpu) \
	(cfs_nr_dequeuing_low_prio(cpu) = 0)
#else
#define task_low_priority(prio) (0)
#define cfs_reset_nr_dequeuing_low_prio(cpu)
#endif

/* Schedule entity */
#define se_load(se) se->avg.loadwop_avg

/* #define se_contrib(se) se->avg.load_avg_contrib */

/* CPU related : load information */
#define cfs_pending_load(cpu) cpu_rq(cpu)->cfs.avg.pending_load
#define cfs_load(cpu) cpu_rq(cpu)->cfs.avg.loadwop_avg
#define cfs_contrib(cpu) cpu_rq(cpu)->cfs.avg.loadwop_avg

/* CPU related : the number of tasks */
#define cfs_nr_normal_prio(cpu) cpu_rq(cpu)->cfs.avg.nr_normal_prio
#define cfs_nr_pending(cpu) cpu_rq(cpu)->cfs.avg.nr_pending
#define cfs_length(cpu) cpu_rq(cpu)->cfs.h_nr_running
#define rq_length(cpu) (cpu_rq(cpu)->nr_running + cfs_nr_pending(cpu))


inline int hmp_fork_balance(struct task_struct *p, int prev_cpu);
static int hmp_select_task_rq_fair(int sd_flag, struct task_struct *p,
		int prev_cpu, int new_cpu);
static unsigned int hmp_idle_pull(int this_cpu);
static void hmp_force_up_migration(int this_cpu);
static void hmp_online_cpu(int cpu);
static void hmp_offline_cpu(int cpu);

static void __init hmp_cpu_mask_setup(void);
static inline void
hmp_enqueue_entity_load_avg(struct cfs_rq *cfs_rq, struct sched_entity *se);
static inline void
hmp_dequeue_entity_load_avg(struct cfs_rq *cfs_rq, struct sched_entity *se);
static inline void
hmp_update_cfs_rq_load_avg(struct cfs_rq *cfs_rq, struct sched_avg *sa);
static inline void
hmp_update_load_avg(unsigned int decayed, unsigned long weight,
		unsigned int scaled_delta_w, struct sched_avg *sa, u64 periods,
		u32 contrib, u64 scaled_delta, struct cfs_rq *cfs_rq);

#else
#define se_load(se) 0

static inline int hmp_fork_balance(struct task_struct *p, int prev_cpu)
{
	return prev_cpu;
}
static void hmp_force_up_migration(int this_cpu) {}
static int hmp_select_task_rq_fair(int sd_flag, struct task_struct *p,
		int prev_cpu, int new_cpu) { return new_cpu; }
static void hmp_online_cpu(int cpu) {}
static void hmp_offline_cpu(int cpu) {}
static int hmp_idle_pull(int this_cpu) { return 0; }
static inline void
hmp_update_cfs_rq_load_avg(struct cfs_rq *cfs_rq, struct sched_avg *sa) {}
static void __init hmp_cpu_mask_setup(void) {}

#endif /* CONFIG_SCHED_HMP */
