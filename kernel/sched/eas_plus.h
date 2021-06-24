/*
 * Copyright (C) 2018 MediaTek Inc.
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

/*
 * Add a system-wide over-utilization indicator which
 * is updated in load-balance.
 */
#include "../../drivers/misc/mediatek/base/power/include/mtk_upower.h"
#include "energy_plus.h"

extern int cpu_eff_tp;

inline bool system_overutilized(int cpu);

static inline unsigned long task_util(struct task_struct *p);
bool is_intra_domain(int prev, int target);
static int select_max_spare_capacity(struct task_struct *p, int target);
static int init_cpu_info(void);
static unsigned int aggressive_idle_pull(int this_cpu);
bool idle_lb_enhance(struct task_struct *p, int cpu);
static int
___select_idle_sibling(struct task_struct *p, int prev_cpu, int new_cpu);
static int __find_energy_efficient_cpu(struct sched_domain *sd,
		struct task_struct *p, int cpu, int prev_cpu, int sync);
extern int find_best_idle_cpu(struct task_struct *p, bool prefer_idle);

static int start_cpu(struct task_struct *p, bool prefer_idle,
				bool boosted);
static int
migrate_running_task(int this_cpu, struct task_struct *p, struct rq *target);

#ifdef CONFIG_UCLAMP_TASK
static __always_inline
unsigned long uclamp_rq_util_with(struct rq *rq, unsigned long util,
					struct task_struct *p);
#else

inline unsigned long uclamp_rq_util_with(struct rq *rq, unsigned long util,
					struct task_struct *p)
{
	return util;
}
#endif

#ifdef CONFIG_MTK_UNIFY_POWER
extern int
mtk_idle_power(int cpu_idx, int idle_state, int cpu, void *argu, int sd_level);

extern
int mtk_busy_power(int cpu_idx, int cpu, void *argu, int sd_level);

extern
const struct sched_group_energy * const cci_energy(void);
#endif

/*#define DEBUG_EENV_DECISIONS*/

#ifdef DEBUG_EENV_DECISIONS
/* max of 8 levels of sched groups traversed */
#define EAS_EENV_DEBUG_LEVELS 16

struct _eenv_debug {
	unsigned long cap;
	unsigned long norm_util;
	unsigned long cap_energy;
	unsigned long idle_energy;
	unsigned long this_energy;
	unsigned long this_busy_energy;
	unsigned long this_idle_energy;
	cpumask_t group_cpumask;
	unsigned long cpu_util[1];
};
#endif

struct eenv_cpu {
	/* CPU ID, must be in cpus_mask */
	int     cpu_id;

	/*
	 * Index (into sched_group_energy::cap_states) of the OPP the
	 * CPU needs to run at if the task is placed on it.
	 * This includes the both active and blocked load, due to
	 * other tasks on this CPU,  as well as the task's own
	 * utilization.
	 */
#ifndef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
	int     cap_idx;
	int     cap;
#else
	int     cap_idx[3];             /* [FIXME] cluster may > 3 */
	int     cap[3];
#endif

	/* Estimated system energy */
	unsigned long energy;

	/* Estimated energy variation wrt EAS_CPU_PRV */
	long nrg_delta;

#ifdef DEBUG_EENV_DECISIONS
	struct _eenv_debug *debug;
	int debug_idx;
#endif /* DEBUG_EENV_DECISIONS */
};

struct energy_env {
	/* Utilization to move */
	struct task_struct	*p;
	unsigned long		util_delta;
	unsigned long		util_delta_boosted;

	/* Mask of CPUs candidates to evaluate */
	cpumask_t		cpus_mask;

	/* CPU candidates to evaluate */
	struct eenv_cpu *cpu;
	int eenv_cpu_count;

#ifdef DEBUG_EENV_DECISIONS
	/* pointer to the memory block reserved
	 * for debug on this CPU - there will be
	 * sizeof(struct _eenv_debug) *
	 *  (EAS_CPU_CNT * EAS_EENV_DEBUG_LEVELS)
	 * bytes allocated here.
	 */
	struct _eenv_debug *debug;
#endif
	/*
	 * Index (into energy_env::cpu) of the morst energy efficient CPU for
	 * the specified energy_env::task
	 */
	int	next_idx;
	int	max_cpu_count;

	/* Support data */
	struct sched_group	*sg_top;
	struct sched_group	*sg_cap;
	struct sched_group	*sg;
};

void mtk_update_new_capacity(struct energy_env *eenv);

static void select_task_prefer_cpu_fair(struct task_struct *p, int *result);
inline int valid_cpu_prefer(int task_prefer);
inline int hinted_cpu_prefer(int task_prefer);
int cpu_prefer(struct task_struct *p);
extern unsigned int hmp_cpu_is_fastest(int cpu);

static int check_freq_turning(void);
struct rq *__migrate_task(struct rq *rq, struct rq_flags *rf,
				struct task_struct *p, int dest_cpu);

int sched_forked_ramup_factor(void);
