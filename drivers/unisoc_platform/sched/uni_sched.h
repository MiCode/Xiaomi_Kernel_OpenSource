// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Unisoc (shanghai) Technologies Co., Ltd.
 */
#ifndef _UNI_SCHED_H
#define _UNI_SCHED_H

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/unisoc_vd_def.h>

#include "trace.h"
#include "../../../kernel/sched/sched.h"

#define UNI_NR_CPUS	8
#define MAX_CLUSTERS	3

#define SCENE_LAUNCH 1

extern bool uni_sched_disabled;

static inline struct task_group *css_tg(struct cgroup_subsys_state *css)
{
	return css ? container_of(css, struct task_group, css) : NULL;
}

static inline struct uni_task_group *get_uni_task_group(struct task_struct *p)
{
	return (struct uni_task_group *) (p->sched_task_group->android_vendor_data1);
}


enum cgroup_name {
	ROOT_GROUP = 0,
	TOP_APP,
#ifdef CONFIG_UNISOC_SCHED_VIP_TASK
	VIP_GROUP,
#endif
	FOREGROUND,
	CAMERA_DAEMON,
	BACKGROUND,
	SYSTEM_GROUP,
	OTHER_GROUPS,
};

struct pd_cache {
	unsigned long wake_util;
	unsigned long cap_orig;
	unsigned long cap;
	unsigned long thermal_pressure;
	unsigned long base_energy;
	bool is_idle;
};

#ifdef CONFIG_UNISOC_SCHED_VIP_TASK

#define SCHED_UI_THREAD_TYPE	0x01
#define SCHED_ANIMATOR_TYPE	0x02
#define SCHED_LL_CAMERA_TYPE	0x04
#define SCHED_AUDIO_TYPE	0x08
#define SCHED_BINDER_TYPE	0x10

#define SCHED_VIP_TASK_TYPE	(SCHED_UI_THREAD_TYPE | SCHED_ANIMATOR_TYPE | \
				 SCHED_LL_CAMERA_TYPE | SCHED_AUDIO_TYPE | SCHED_BINDER_TYPE)

#define SCHED_VIP_SLICE		4000000U
#define SCHED_VIP_LIMIT		(4 * SCHED_VIP_SLICE)
#define SCHED_VIP_LAUNCH_LIMIT	(20 * SCHED_VIP_SLICE)

#define SCHED_NOT_VIP		0
#define SCHED_UI_VIP		1
#define SCHED_ANIMATOR_VIP	2
#define SCHED_ANIMATOR_UI_VIP	3
#define SCHED_CAMERA_VIP	4
#define SCHED_CAMERA_UI_VIP	5
#define SCHED_AUDIO_VIP		6
#define SCHED_BINDER_VIP	7

extern void enqueue_cfs_vip_task(struct rq *rq, struct task_struct *p);
extern void dequeue_cfs_vip_task(struct rq *rq, struct task_struct *p);
#ifdef CONFIG_UNISOC_CPU_NETLINK
extern void check_parent_vip_status(struct task_struct *p);
#else
static inline void check_parent_vip_status(struct task_struct *p) {}
#endif

static inline bool test_vip_task(struct task_struct *tsk)
{
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *) tsk->android_vendor_data1;

	return (uni_tsk->vip_params & SCHED_VIP_TASK_TYPE);
}
static inline int num_vip_tasks_of_cpu(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	struct uni_rq *uni_rq = (struct uni_rq *) rq->android_vendor_data1;

	return uni_rq->num_vip_tasks;
}

#else
static inline void enqueue_cfs_vip_task(struct rq *rq, struct task_struct *p) {}
static inline void dequeue_cfs_vip_task(struct rq *rq, struct task_struct *p) {}
static inline void check_parent_vip_status(struct task_struct *p) {}
#define test_vip_task(task) false
static inline int num_vip_tasks_of_cpu(int cpu)
{
	return 0;
}

#endif

#define unitsk_to_tsk(unitsk) ({ \
		void *__mptr = (void *)(unitsk); \
		((struct task_struct *)(__mptr - \
		offsetof(struct task_struct, android_vendor_data1))); })

#if IS_ENABLED(CONFIG_SCHED_WALT)
extern __read_mostly unsigned int walt_ravg_window;
extern unsigned int sysctl_sched_walt_cross_window_util;
extern unsigned int sysctl_walt_io_is_busy;
extern unsigned int sysctl_walt_busy_threshold;
extern unsigned int sysctl_sched_walt_init_task_load_pct;
extern unsigned int sysctl_sched_walt_cpu_high_irqload;
extern unsigned int sysctl_walt_account_irq_time;
extern unsigned int sysctl_walt_boost_irqload;

#define scale_demand(d) ((d) / (walt_ravg_window >> SCHED_CAPACITY_SHIFT))

extern void walt_init(void);
extern u64 sched_ktime_clock(void);
extern void walt_inc_cumulative_runnable_avg(struct rq *rq, struct task_struct *p);
extern void walt_dec_cumulative_runnable_avg(struct rq *rq, struct task_struct *p);
extern void walt_init_new_task_load(struct task_struct *p);
extern unsigned long walt_cpu_util_freq(int cpu);
extern void reset_rt_task_arrival_time(int cpu);

#define WALT_HIGH_IRQ_TIMEOUT 3
static inline int walt_cpu_high_irqload(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	struct uni_rq *uni_rq = (struct uni_rq *)rq->android_vendor_data1;
	s64 delta = get_jiffies_64() - uni_rq->irqload_ts;
	u64 irq_load = 0;

	/*
	 * Current context can be preempted by irq and rq->irqload_ts can be
	 * updated by irq context so that delta can be negative.
	 * But this is okay and we can safely return as this means there
	 * was recent irq occurrence.
	 */

	if (delta < WALT_HIGH_IRQ_TIMEOUT)
		irq_load = uni_rq->avg_irqload;

	return irq_load >= sysctl_sched_walt_cpu_high_irqload;
}

static inline unsigned long walt_task_util(struct task_struct *p)
{
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *) p->android_vendor_data1;

	return uni_tsk->demand_scale;
}

static inline unsigned long walt_cpu_util(int cpu)
{
	struct uni_rq *uni_rq = (struct uni_rq *) cpu_rq(cpu)->android_vendor_data1;
	u64 cpu_util = uni_rq->cumulative_runnable_avg;

	cpu_util <<= SCHED_CAPACITY_SHIFT;
	do_div(cpu_util, walt_ravg_window);

	return min_t(unsigned long, cpu_util, capacity_orig_of(cpu));
}

static inline unsigned long task_util_est(struct task_struct *p)
{
	return walt_task_util(p);
}

static inline unsigned int tg_init_load_pct(struct task_struct *p)
{
#ifdef CONFIG_UNISOC_GROUP_CTL
	struct uni_task_group *wtg = get_uni_task_group(p);

	return wtg->init_task_load_pct;
#else
	return 0;
#endif
}

static inline unsigned int tg_account_wait_time(struct task_struct *p)
{
#ifdef CONFIG_UNISOC_GROUP_CTL
	struct uni_task_group *wtg = get_uni_task_group(p);

	return wtg->account_wait_time;
#else
	return 0;
#endif
}

#else
static inline void walt_init(void) {}
static inline u64 sched_ktime_clock(void)
{
	return sched_clock();
}
static inline void walt_inc_cumulative_runnable_avg(struct rq *rq, struct task_struct *p) {}
static inline void walt_dec_cumulative_runnable_avg(struct rq *rq, struct task_struct *p) {}
static inline void walt_init_new_task_load(struct task_struct *p) {}
static inline void reset_rt_task_arrival_time(int cpu) {}

static inline unsigned long task_util(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.util_avg);
}

static inline unsigned long _task_util_est(struct task_struct *p)
{
	struct util_est ue = READ_ONCE(p->se.avg.util_est);

	return max(ue.ewma, (ue.enqueued & ~UTIL_AVG_UNCHANGED));
}

static inline unsigned long task_util_est(struct task_struct *p)
{
	return max(task_util(p), _task_util_est(p));
}
#endif

extern struct ctl_table sched_base_table[];
extern unsigned int sysctl_sched_uclamp_threshold;
extern unsigned int sysctl_sched_task_util_prefer_little;
extern unsigned int sysctl_sched_long_running_rt_task_ms;
extern unsigned int sysctl_sched_rt_task_timeout_panic;
#if IS_ENABLED(CONFIG_UCLAMP_MIN_TO_BOOST)
extern unsigned int sysctl_sched_uclamp_min_to_boost;
#endif
extern unsigned int sysctl_cpu_multi_thread_opt;
extern unsigned int sysctl_multi_thread_heavy_load_runtime;
extern unsigned int sysctl_force_newidle_balance;
extern unsigned int sysctl_sched_custom_scene;
#ifdef CONFIG_UNISOC_INPUT_BOOST
extern unsigned int sysctl_input_boost_ms;
extern unsigned int sysctl_input_boost_freq[8];
extern unsigned int sysctl_input_boost_enable;

extern unsigned int sysctl_powerkey_boost_ms;
extern unsigned int sysctl_powerkey_boost_freq[8];
extern unsigned int sysctl_powerkey_boost_enable;
#endif

extern unsigned int min_max_possible_capacity;
extern unsigned int max_possible_capacity;
extern unsigned int sched_cap_margin_up[UNI_NR_CPUS];
extern unsigned int sched_cap_margin_dn[UNI_NR_CPUS];

extern struct list_head cluster_head;
extern int num_sched_clusters;
#define for_each_sched_cluster(cluster) \
	list_for_each_entry_rcu(cluster, &cluster_head, list)

static inline int cluster_first_cpu(struct sched_cluster *cluster)
{
	return cpumask_first(&cluster->cpus);
}

static inline bool is_max_capacity_cpu(int cpu)
{
	return arch_scale_cpu_capacity(cpu) == max_possible_capacity;
}

static inline bool is_min_capacity_cpu(int cpu)
{
	return arch_scale_cpu_capacity(cpu) == min_max_possible_capacity;
}

static inline bool is_min_capacity_cluster(struct sched_cluster *cluster)
{
	return is_min_capacity_cpu(cluster_first_cpu(cluster));
}

static inline int same_cluster(int src_cpu, int dst_cpu)
{
	struct uni_rq *src_uni_rq = (struct uni_rq *) cpu_rq(src_cpu)->android_vendor_data1;
	struct uni_rq *dest_uni_rq = (struct uni_rq *) cpu_rq(dst_cpu)->android_vendor_data1;

	return src_uni_rq->cluster == dest_uni_rq->cluster;
}

static inline bool sched_custom_scene(int scene)
{
	return sysctl_sched_custom_scene == scene;
}

extern void rt_init(void);
extern void fair_init(void);
extern int proc_cpuload_init(void);
extern int sched_long_running_rt_task_ms_handler(struct ctl_table *table, int write,
	void __user *buffer, size_t *lenp, loff_t *ppos);
extern int init_multithread_opt(void);

enum pause_reason {
	PAUSE_CORE_CTL	= 0x01,
	PAUSE_THERMAL	= 0x02,
	PAUSE_HYP	= 0x04,
};

extern struct cpumask __cpu_halt_mask;
#define cpu_halt_mask ((struct cpumask *)&__cpu_halt_mask)

#ifdef CONFIG_UNISOC_SCHED_PAUSE_CPU
#define cpu_halted(cpu) ((cpu < nr_cpu_ids) && cpumask_test_cpu((cpu), cpu_halt_mask))
extern int pause_cpus(struct cpumask *cpus, enum pause_reason reason);
extern int resume_cpus(struct cpumask *cpus, enum pause_reason reason);
extern void core_pause_init(void);
extern int core_ctl_init(void);
extern bool halt_check_last(int cpu);
extern int is_cpu_paused(int cpu);
#else
#define cpu_halted(cpu) 0
static inline void core_pause_init(void)
{
}

static inline int core_ctl_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_UNISOC_INPUT_BOOST
extern int input_boost_init(void);
extern int powerkey_boost_init(void);
#else
static inline int input_boost_init(void)
{
	return 0;
}

static inline int powerkey_boost_init(void)
{
	return 0;
}
#endif

#ifdef CONFIG_UNISOC_HUNG_TASK_ENH
extern int hung_task_enh_init(void);
#else
static inline int hung_task_enh_init(void)
{
	return 0;
}
#endif
/*
 * The policy of a RT boosted task (via PI mutex) still is a fair task,
 * so use prio check as well. The prio check alone is not sufficient
 * since idle task's prio is also 120.
 */
static inline bool is_fair_task(struct task_struct *p)
{
	return p->prio >= MAX_RT_PRIO && !is_idle_task(p);
}


static inline int task_group_boost(struct task_struct *p)
{
#ifdef CONFIG_UNISOC_GROUP_BOOST
	struct uni_task_group *wtg = get_uni_task_group(p);

	return wtg->boost;
#else
	return 0;
#endif
}

static inline int uni_task_group_idx(struct task_struct *p)
{
#ifdef CONFIG_UNISOC_GROUP_CTL
	struct uni_task_group *tg = get_uni_task_group(p);

	return tg->idx;
#else
	return 0;
#endif
}

#ifdef CONFIG_UNISOC_GROUP_CTL

extern struct ctl_table boost_table[];
extern void init_group_control(void);

#ifdef CONFIG_UNISOC_GROUP_BOOST

extern unsigned int sysctl_sched_spc_threshold;
extern void group_boost_enqueue(struct rq *rq, struct task_struct *p);
extern void group_boost_dequeue(struct rq *rq, struct task_struct *p);
extern int cpu_group_boost(int cpu);
extern unsigned long boosted_cpu_util(int cpu, unsigned long util);

static inline int group_boost_margin(long util, int boost)
{
	int margin;

	/*
	 * Signal proportional compensation (SPC)
	 *
	 * The Boost (B) value is used to compute a Margin (M) which is
	 * proportional to the complement of the original Signal (S):
	 *   M = B * (SCHED_CAPACITY_SCALE - S)
	 * The obtained M could be used by the caller to "boost" S.
	 */
	if (boost >= 0) {
		if (util < sysctl_sched_spc_threshold)
			margin = util * boost / 100;
		else
			margin  = (SCHED_CAPACITY_SCALE - util) * boost / 100;
	} else
		margin = util * boost / 100;

	return margin;
}

static inline unsigned long boosted_task_util(struct task_struct *task)
{
#if IS_ENABLED(CONFIG_SCHED_WALT)
	int util = walt_task_util(task);
#else
	int util = task_util_est(task);
#endif
	int boost = task_group_boost(task);
	int margin = group_boost_margin(util, boost);

	trace_sched_boost_task(task, util, boost, margin);

	return util + margin;
}
#else
static inline void group_boost_enqueue(struct rq *rq, struct task_struct *p) { };
static inline void group_boost_dequeue(struct rq *rq, struct task_struct *p) { };
static inline int cpu_group_boost(int cpu)
{
	return 0;
}
static inline unsigned long boosted_cpu_util(int cpu, unsigned long util)
{
	return util;
}
static inline unsigned long boosted_task_util(struct task_struct *task)
{
#if IS_ENABLED(CONFIG_SCHED_WALT)
	return walt_task_util(task);
#else
	return task_util_est(task);
#endif
}
#endif
#else
static inline void update_task_group(struct cgroup_subsys_state *css) {}
static inline void init_group_control(void) {}
static inline void group_boost_enqueue(struct rq *rq, struct task_struct *p) { };
static inline void group_boost_dequeue(struct rq *rq, struct task_struct *p) { };
static inline int cpu_group_boost(int cpu)
{
	return 0;
}
static inline unsigned long boosted_cpu_util(int cpu, unsigned long util)
{
	return util;
}
static inline unsigned long boosted_task_util(struct task_struct *task)
{
#if IS_ENABLED(CONFIG_SCHED_WALT)
	return walt_task_util(task);
#else
	return task_util_est(task);
#endif
}
#endif


#ifdef CONFIG_UCLAMP_TASK
#if IS_ENABLED(CONFIG_UCLAMP_MIN_TO_BOOST)
static inline unsigned long uclamp_transform_boost(unsigned long util,
						unsigned long uclamp_min,
						unsigned long uclamp_max)
{
	unsigned long margin, boost;

	if (unlikely(uclamp_min > uclamp_max))
		return util;

	if (util >= uclamp_max)
		return uclamp_max;

	boost = util < sysctl_sched_uclamp_threshold ? util : (uclamp_max - util);

	margin = DIV_ROUND_CLOSEST_ULL(uclamp_min * boost, SCHED_CAPACITY_SCALE);

	return util + margin;
}
#endif

static inline unsigned long uclamp_task_util(struct task_struct *p)
{
	unsigned long min_util = uclamp_eff_value(p, UCLAMP_MIN);
	unsigned long max_util = uclamp_eff_value(p, UCLAMP_MAX);
	unsigned long util;

	util = boosted_task_util(p);

#ifdef CONFIG_UCLAMP_MIN_TO_BOOST
	if (sysctl_sched_uclamp_min_to_boost)
		util = uclamp_transform_boost(util, min_util, max_util);
	else if (util >= sysctl_sched_uclamp_threshold)
		util = clamp(util, min_util, max_util);
#else
	if (util >= sysctl_sched_uclamp_threshold)
		util = clamp(util, min_util, max_util);

#endif

	return util;
}

static __always_inline
unsigned long walt_uclamp_rq_util_with(struct rq *rq, unsigned long util,
				  struct task_struct *p)
{
	unsigned long min_util = 0;
	unsigned long max_util = 0;

	if (!static_branch_likely(&sched_uclamp_used))
		return util;

	if (p) {
		min_util = uclamp_eff_value(p, UCLAMP_MIN);
		max_util = uclamp_eff_value(p, UCLAMP_MAX);

		/*
		 * Ignore last runnable task's max clamp, as this task will
		 * reset it. Similarly, no need to read the rq's min clamp.
		 */
		if (rq->uclamp_flags & UCLAMP_FLAG_IDLE)
			goto out;
	}

	min_util = max_t(unsigned long, min_util, READ_ONCE(rq->uclamp[UCLAMP_MIN].value));
	max_util = max_t(unsigned long, max_util, READ_ONCE(rq->uclamp[UCLAMP_MAX].value));
out:
	/*
	 * Since CPU's {min,max}_util clamps are MAX aggregated considering
	 * RUNNABLE tasks with _different_ clamps, we can end up with an
	 * inversion. Fix it now when the clamps are applied.
	 */
	if (unlikely(min_util >= max_util))
		return min_util;

#if IS_ENABLED(CONFIG_UCLAMP_MIN_TO_BOOST)
	if (sysctl_sched_uclamp_min_to_boost)
		util = uclamp_transform_boost(util, min_util, max_util);
	else if (util >= sysctl_sched_uclamp_threshold)
		util = clamp(util, min_util, max_util);
#else
	if (util >= sysctl_sched_uclamp_threshold)
		util = clamp(util, min_util, max_util);
#endif

	return util;
}

static inline bool uclamp_blocked(struct task_struct *p)
{
	return uclamp_eff_value(p, UCLAMP_MAX) < SCHED_CAPACITY_SCALE;
}

#else
static inline unsigned long uclamp_task_util(struct task_struct *p)
{
	return boosted_task_util(p);
}

static inline
unsigned long walt_uclamp_rq_util_with(struct rq *rq, unsigned long util,
				  struct task_struct *p)
{
	return util;
}

static inline bool uclamp_blocked(struct task_struct *p)
{
	return false;
}
#endif

#if IS_ENABLED(CONFIG_SCHED_WALT)
struct walt_update_util_data {
	void (*func)(struct walt_update_util_data *data, u64 time, unsigned int flags);
};

void walt_cpufreq_add_update_util_hook(int cpu, struct walt_update_util_data *data,
			void (*func)(struct walt_update_util_data *data, u64 time,
				unsigned int flags));
void walt_cpufreq_remove_update_util_hook(int cpu);
bool walt_cpufreq_this_cpu_can_update(struct cpufreq_policy *policy);

DECLARE_PER_CPU(struct walt_update_util_data __rcu *, walt_cpufreq_update_util_data);
static inline void walt_cpufreq_update_util(struct rq *rq, unsigned int flags)
{
	struct walt_update_util_data *data;

	data = rcu_dereference_sched(*per_cpu_ptr(&walt_cpufreq_update_util_data,
						  cpu_of(rq)));
	if (data)
		data->func(data, rq_clock(rq), flags);
}
#else
static inline void walt_cpufreq_update_util(struct rq *rq, unsigned int flags) {}
#endif

#if IS_ENABLED(CONFIG_UNISOC_SCHEDUTIL_GOV)
extern int uscfreq_gov_register(void);
#else
static inline int uscfreq_gov_register(void)
{
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_UNISOC_ROTATION_TASK)
#define CPU_RESERVED	1
extern unsigned int sysctl_rotation_enable;
extern unsigned int sysctl_rotation_threshold_ms;

static inline bool is_reserved(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	struct uni_rq *uni_rq = (struct uni_rq *) rq->android_vendor_data1;

	return test_bit(CPU_RESERVED, &uni_rq->sched_flag);
}

static inline void mark_reserved(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	struct uni_rq *uni_rq = (struct uni_rq *) rq->android_vendor_data1;

	test_and_set_bit(CPU_RESERVED, &uni_rq->sched_flag);
}

static inline void clear_reserved(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	struct uni_rq *uni_rq = (struct uni_rq *) rq->android_vendor_data1;

	return clear_bit(CPU_RESERVED, &uni_rq->sched_flag);
}
#else
static inline bool is_reserved(int cpu)
{
	return false;
}
static inline void mark_reserved(int cpu) { }
static inline void clear_reserved(int cpu) { }
static inline void rotation_task_init(void) { }
static inline void check_for_task_rotation(struct rq *src_rq) { }
#endif
#endif /* _UNI_SCHED_H */
