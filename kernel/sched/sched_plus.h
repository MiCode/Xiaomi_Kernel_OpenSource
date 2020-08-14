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
extern unsigned int capacity_margin;
extern void unthrottle_offline_rt_rqs(struct rq *rq);
DECLARE_PER_CPU(struct hmp_domain *, hmp_cpu_domain);
#include "../../drivers/misc/mediatek/base/power/include/mtk_upower.h"
#include "../../drivers/misc/mediatek/include/mt-plat/eas_ctrl.h"
extern int l_plus_cpu;
extern unsigned long get_cpu_util(int cpu);
extern void init_sched_groups_capacity(int cpu, struct sched_domain *sd);
extern unsigned int capacity_margin;
#ifdef CONFIG_SMP
#ifdef CONFIG_ARM64
extern unsigned long arch_scale_get_max_freq(int cpu);
extern unsigned long arch_scale_get_min_freq(int cpu);
#else
static inline unsigned long arch_scale_get_max_freq(int cpu) { return 0; }
static inline unsigned long arch_scale_get_min_freq(int cpu) { return 0; }
#endif
#endif
#define SCHED_ENHANCED_ATTR 0x40000
int select_task_prefer_cpu(struct task_struct *p, int new_cpu);
int
sched_setattr_enhanced(struct task_struct *p, const struct sched_attr *attr);

int task_prefer_little(struct task_struct *p);
int task_prefer_big(struct task_struct *p);
int task_prefer_fit(struct task_struct *p, int cpu);
int task_prefer_match(struct task_struct *p, int cpu);
int
task_prefer_match_on_cpu(struct task_struct *p, int src_cpu, int target_cpu);
unsigned long cluster_max_capacity(void);
inline unsigned long task_uclamped_min_w_ceiling(struct task_struct *p);
inline unsigned int freq_util(unsigned long util);

#define LB_POLICY_SHIFT 16
#define LB_CPU_MASK ((1 << LB_POLICY_SHIFT) - 1)

#define LB_PREV          (0x0  << LB_POLICY_SHIFT)
#define LB_FORK          (0x1  << LB_POLICY_SHIFT)
#define LB_SMP           (0x2  << LB_POLICY_SHIFT)
#define LB_HMP           (0x4  << LB_POLICY_SHIFT)
#define LB_EAS           (0x8  << LB_POLICY_SHIFT)
#define LB_HINT         (0x10  << LB_POLICY_SHIFT)
#define LB_EAS_AFFINE   (0x18  << LB_POLICY_SHIFT)
#define LB_EAS_LB       (0x28  << LB_POLICY_SHIFT)
#define LB_THERMAL       (0x48  << LB_POLICY_SHIFT)

#define MIGR_LOAD_BALANCE      1
#define MIGR_UP_MIGRATE        2
#define MIGR_DOWN_MIGRATE      3
#define MIGR_IDLE_RUNNING      4
#define MIGR_ROTATION          5

#define TASK_ROTATION_THRESHOLD_NS      6000000
#define HEAVY_TASK_NUM  4

struct task_rotate_reset_uclamp_work {
	struct work_struct w;
};

extern struct task_rotate_reset_uclamp_work task_rotate_reset_uclamp_works;
extern bool set_uclamp;
extern void task_rotate_work_init(void);
extern void check_for_migration(struct rq *rq, struct task_struct *p);
extern void task_check_for_rotation(struct rq *rq);
extern void set_sched_rotation_enable(bool enable);

static inline int is_reserved(int cpu)
{
	struct rq *rq = cpu_rq(cpu);

	return (rq->active_balance != 0);
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

/*
 *for isolation interface
 */
#ifdef CONFIG_HOTPLUG_CPU
extern int sched_isolate_count(const cpumask_t *mask, bool include_offline);
extern int sched_isolate_cpu(int cpu);
extern int sched_deisolate_cpu(int cpu);
extern int sched_deisolate_cpu_unlocked(int cpu);
#else
static inline int sched_isolate_count(const cpumask_t *mask,
				      bool include_offline)
{
	cpumask_t count_mask;

	if (include_offline)
		cpumask_andnot(&count_mask, mask, cpu_online_mask);
	else
		return 0;

	return cpumask_weight(&count_mask);
}

static inline int sched_isolate_cpu(int cpu)
{
	return 0;
}

static inline int sched_deisolate_cpu(int cpu)
{
	return 0;
}

static inline int sched_deisolate_cpu_unlocked(int cpu)
{
	return 0;
}
#endif

enum iso_prio_t {ISO_CUSTOMIZE, ISO_TURBO, ISO_SCHED, ISO_UNSET};
extern int set_cpu_isolation(enum iso_prio_t prio, struct cpumask *cpumask_ptr);
extern int unset_cpu_isolation(enum iso_prio_t prio);
extern struct cpumask cpu_all_masks;
extern enum iso_prio_t iso_prio;
