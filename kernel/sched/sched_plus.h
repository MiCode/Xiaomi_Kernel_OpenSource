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

#ifdef CONFIG_SMP
extern unsigned long arch_scale_get_max_freq(int cpu);
extern unsigned long arch_scale_get_min_freq(int cpu);
#endif

extern int stune_task_threshold;
#ifdef CONFIG_SCHED_TUNE
extern bool global_negative_flag;

void show_ste_info(void);
void show_pwr_info(void);
extern int sys_boosted;
#endif

#ifdef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
extern int l_plus_cpu;
#endif

enum fbq_type { regular, remote, all };

enum group_type {
	group_other = 0,
	group_misfit_task,
	group_imbalanced,
	group_overloaded,
};

struct lb_env {
	struct sched_domain	*sd;

	struct rq		*src_rq;
	int			src_cpu;

	int			dst_cpu;
	struct rq		*dst_rq;

	struct cpumask		*dst_grpmask;
	int			new_dst_cpu;
	enum cpu_idle_type	idle;
	long			imbalance;
	unsigned int		src_grp_nr_running;
	/* The set of CPUs under consideration for load-balancing */
	struct cpumask		*cpus;

	unsigned int		flags;

	unsigned int		loop;
	unsigned int		loop_break;
	unsigned int		loop_max;

	enum fbq_type		fbq_type;
	enum group_type		busiest_group_type;
	struct list_head	tasks;
};

extern bool sched_boost(void);
extern void unthrottle_offline_rt_rqs(struct rq *rq);

#ifdef CONFIG_MTK_SCHED_TRACERS
#define LB_POLICY_SHIFT 16
#define LB_CPU_MASK ((1 << LB_POLICY_SHIFT) - 1)

#define LB_FORK			(0x1 << LB_POLICY_SHIFT)
#define LB_SMP			(0x2 << LB_POLICY_SHIFT)
#define LB_HMP			(0x4 << LB_POLICY_SHIFT)
#define LB_EAS			(0x8 << LB_POLICY_SHIFT)

#else
#define LB_FORK			(0)
#define LB_SMP			(0)
#define LB_HMP			(0)
#define LB_EAS			(0)
#endif



#include "rt_enh.h"
