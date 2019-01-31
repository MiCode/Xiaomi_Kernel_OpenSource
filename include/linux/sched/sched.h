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

struct energy_env {
	struct sched_group	*sg_top;
	struct sched_group	*sg_cap;
	int			cap_idx;
	int			util_delta;
	int			src_cpu;
	int			dst_cpu;
	int			energy;
	int			payoff;
	struct task_struct	*task;
	struct {
		int before;
		int after;
		int delta;
		int diff;
	} nrg;
	struct {
		int before;
		int after;
		int delta;
	} cap;
#ifdef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
	int	opp_idx[3];	/* [FIXME] cluster may > 3 */
#endif
};

/* cpu_core_energy & cpu_cluster_energy both implmented in topology.c */
extern
const struct sched_group_energy * const cpu_core_energy(int cpu);

extern
const struct sched_group_energy * const cpu_cluster_energy(int cpu);

#ifdef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
extern inline
int mtk_idle_power(int idle_state, int cpu, void *argu, int sd_level);

extern inline
int mtk_busy_power(int cpu, void *argu, int sd_level);

extern int mtk_cluster_capacity_idx(int cid, struct energy_env *eenv);
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

#ifdef CONFIG_HMP_TRACER
/* Number of task statistic to force migration */
struct hmp_statisic {
	unsigned int nr_force_up;
	unsigned int nr_force_down;
};
#endif /* CONFIG_HMP_TRACER */

extern unsigned int hmp_cpu_is_slowest(int cpu);
#else
static inline unsigned int hmp_cpu_is_slowest(int cpu) { return false; }
#endif /* CONFIG_SCHED_HMP */

extern void
get_task_util(struct task_struct *p, unsigned long *util, unsigned long *boost);
#ifdef CONFIG_MTK_SCHED_INTEROP
extern bool is_rt_throttle(int cpu);
#endif

