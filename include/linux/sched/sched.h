/*
 * Copyright (C) 2017 MediaTek Inc.
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
#ifdef CONFIG_MTK_UNIFY_POWER
#include "../../drivers/misc/mediatek/base/power/include/mtk_upower.h"
#endif

#ifdef CONFIG_MTK_SCHED_TRACE
#ifdef CONFIG_MTK_SCHED_DEBUG
#define mt_sched_printf(event, x...) \
do { \
	char strings[128] = "";  \
	snprintf(strings, sizeof(strings)-1, x); \
	pr_debug(x); \
	trace_##event(strings); \
} while (0)
#else
#define mt_sched_printf(event, x...) \
do { \
	char strings[128] = "";  \
	snprintf(strings, sizeof(strings)-1, x); \
	trace_##event(strings); \
} while (0)

#endif
#else
#define mt_sched_printf(event, x...) do {} while (0)
#endif
/*
 * CPU candidates.
 *
 * These are labels to reference CPU candidates for an energy_diff.
 * Currently we support only two possible candidates: the task's previous CPU
 * and another candiate CPU.
 * More advanced/aggressive EAS selection policies can consider more
 * candidates.
 */
#define EAS_CPU_PRV	0
#define EAS_CPU_NXT	1
#define EAS_CPU_BKP	2
#define EAS_CPU_CNT	3
/*
 * energy_diff - supports the computation of the estimated energy impact in
 * moving a "task"'s "util_delta" between different CPU candidates.
 */
struct energy_env {
	/* Utilization to move */
	struct task_struct	*p;
	int			util_delta;

	/* Mask of CPUs candidates to evaluate */
	cpumask_t		cpus_mask;

	/* CPU candidates to evaluate */
	struct {

		/* CPU ID, must be in cpus_mask */
		int	cpu_id;

		/*
		 * Index (into sched_group_energy::cap_states) of the OPP the
		 * CPU needs to run at if the task is placed on it.
		 * This includes the both active and blocked load, due to
		 * other tasks on this CPU,  as well as the task's own
		 * utilization.
		 */
#ifndef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
		int	cap_idx;
		int	cap;
#else
		int     cap_idx[3];             /* [FIXME] cluster may > 3 */
		int     cap[3];
#endif

		/* Estimated system energy */
		unsigned int energy;

		/* Estimated energy variation wrt EAS_CPU_PRV */
		int	nrg_delta;

	} cpu[EAS_CPU_CNT];

	/*
	 * Index (into energy_env::cpu) of the morst energy efficient CPU for
	 * the specified energy_env::task
	 */
	int			next_idx;

	/* Support data */
	struct sched_group	*sg_top;
	struct sched_group	*sg_cap;
	struct sched_group	*sg;

};

/* cpu_core_energy & cpu_cluster_energy both implmented in topology.c */
extern
const struct sched_group_energy * const cpu_core_energy(int cpu);

extern
const struct sched_group_energy * const cpu_cluster_energy(int cpu);

#ifdef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
extern const struct sched_group_energy * const cci_energy(void);
extern int
mtk_idle_power(int cpu_idx, int idle_state, int cpu, void *argu, int sd_level);

extern
int mtk_busy_power(int cpu_idx, int cpu, void *argu, int sd_level);

extern void
mtk_cluster_capacity_idx(int cid, struct energy_env *eenv, int cpu_idx);
#endif

extern void __init arch_init_hmp_domains(void);
struct hmp_domain {
	struct cpumask cpus;
	struct cpumask possible_cpus;
	struct list_head hmp_domains;
};
extern struct list_head hmp_domains;

#define for_each_hmp_domain(hmpd) for_each_hmp_domain_B_first(hmpd)
#define for_each_hmp_domain_B_first(hmpd) \
		list_for_each_entry(hmpd, &hmp_domains, hmp_domains)

#define for_each_hmp_domain_L_first(hmpd) \
		list_for_each_entry_reverse(hmpd, &hmp_domains, hmp_domains)

#ifdef CONFIG_SCHED_HMP
struct clb_stats {
	int ncpu;                  /* The number of CPU */
	int ntask;                 /* The number of tasks */
	int load_avg;              /* Arithmetic average of task load ratio */
	int cpu_capacity;          /* Current CPU capacity */
	int cpu_power;             /* Max CPU capacity */
	int acap;                  /* Available CPU capacity */
	int scaled_acap;           /* Scaled available CPU capacity */
	int scaled_atask;          /* Scaled available task */
	int threshold;             /* Dynamic threshold */
#ifdef CONFIG_SCHED_HMP_PRIO_FILTER
	int nr_normal_prio_task;   /* The number of normal-prio tasks */
	int nr_dequeuing_low_prio; /* The number of dequeuing low-prio tasks */
#endif
};


extern struct cpumask hmp_fast_cpu_mask;
extern struct cpumask hmp_slow_cpu_mask;

#define hmp_cpu_domain(cpu)     (per_cpu(hmp_cpu_domain, (cpu)))

/* Number of task statistic to force migration */
struct hmp_statisic {
	unsigned int nr_force_up;
	unsigned int nr_force_down;
};

extern unsigned int hmp_cpu_is_slowest(int cpu);
#else
static inline unsigned int hmp_cpu_is_slowest(int cpu) { return false; }
#endif /* CONFIG_SCHED_HMP */

extern void
get_task_util(struct task_struct *p, unsigned long *util, unsigned long *boost);
#ifdef CONFIG_MTK_SCHED_INTEROP
extern bool is_rt_throttle(int cpu);
#endif

extern void set_sched_rotation_enable(bool enable);

#ifdef CONFIG_CGROUP_SCHEDTUNE
extern int set_stune_task_threshold(int threshold);
#endif
