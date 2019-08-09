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

extern int l_plus_cpu;

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

extern void unthrottle_offline_rt_rqs(struct rq *rq);
extern void mtk_update_new_capacity(struct energy_env *eenv);

#ifdef CONFIG_MTK_SCHED_TRACERS
#define LB_POLICY_SHIFT 16
#define LB_CPU_MASK ((1 << LB_POLICY_SHIFT) - 1)

#define LB_FORK			(0x1 << LB_POLICY_SHIFT)
#define LB_SMP			(0x2 << LB_POLICY_SHIFT)
#define LB_HMP			(0x4 << LB_POLICY_SHIFT)
#define LB_EAS			(0x8 << LB_POLICY_SHIFT)
#define LB_HINT                 (0x10 << LB_POLICY_SHIFT)

#else
#define LB_FORK			(0)
#define LB_SMP			(0)
#define LB_HMP			(0)
#define LB_EAS			(0)
#define LB_HINT                 (0)
#endif

#define MIGRATION_KICK 0
#define CPU_RESERVED 1
extern void task_rotate_work_init(void);
extern void migration_kick_cpus(void);
extern int got_migration_kick(void);
extern void clear_migration_kick(int cpu);
extern void task_check_for_rotation(struct rq *rq);
extern void set_sched_rotation_enable(bool enable);

static inline int is_reserved(int cpu)
{
	struct rq *rq = cpu_rq(cpu);

	return test_bit(CPU_RESERVED, &rq->rotat_flags);
}

static inline int mark_reserved(int cpu)
{
	struct rq *rq = cpu_rq(cpu);

	return test_and_set_bit(CPU_RESERVED, &rq->rotat_flags);
}

static inline void clear_reserved(int cpu)
{
	struct rq *rq = cpu_rq(cpu);

	clear_bit(CPU_RESERVED, &rq->rotat_flags);
}

static inline bool is_max_capacity_cpu(int cpu)
{
	return capacity_orig_of(cpu) == SCHED_CAPACITY_SCALE;
}

int select_task_prefer_cpu(struct task_struct *p, int new_cpu);
int task_prefer_little(struct task_struct *p);
int task_prefer_big(struct task_struct *p);
int task_prefer_fit(struct task_struct *p, int cpu);
int task_prefer_match(struct task_struct *p, int cpu);
int
task_prefer_match_on_cpu(struct task_struct *p, int src_cpu, int target_cpu);
#include "rt_enh.h"

extern unsigned int mt_cpufreq_get_cur_cci_freq_idx(void);

