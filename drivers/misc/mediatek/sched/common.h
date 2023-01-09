/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef _SCHED_COMMON_H
#define _SCHED_COMMON_H

/* Task Vendor Data */
#define T_SBB_FLG 5
#define T_TASK_IDLE_PREFER_FLAG 7

/* Task Group Vendor Data */
#define TG_SBB_FLG 0

/* Run Queue Vendor Data */
#define RQ_SBB_ACTIVE 0
#define RQ_SBB_IDLE_TIME 1
#define RQ_SBB_WALL_TIME 2
#define RQ_SBB_BOOST_FACTOR 3
#define RQ_SBB_TICK_START 4
#define RQ_SBB_CPU_UTILIZE 15

struct util_rq {
	unsigned long util_cfs;
	unsigned long dl_util;
	unsigned long irq_util;
	unsigned long rt_util;
	unsigned long bw_dl_util;
	bool base;
};

#if IS_ENABLED(CONFIG_NONLINEAR_FREQ_CTL)
extern void mtk_map_util_freq(void *data, unsigned long util, unsigned long freq,
			struct cpumask *cpumask, unsigned long *next_freq);
#else
#define mtk_map_util_freq(data, util, freq, cap, next_freq)
#endif /* CONFIG_NONLINEAR_FREQ_CTL */

#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
DECLARE_PER_CPU(int, cpufreq_idle_cpu);
DECLARE_PER_CPU(spinlock_t, cpufreq_idle_cpu_lock);
unsigned long mtk_cpu_util(int cpu, struct util_rq *util_rq,
				unsigned long max, enum cpu_util_type type,
				struct task_struct *p,
				unsigned long min_cap, unsigned long max_cap);
int dequeue_idle_cpu(int cpu);
#endif
__always_inline
unsigned long mtk_uclamp_rq_util_with(struct rq *rq, unsigned long util,
				  struct task_struct *p,
				  unsigned long min_cap, unsigned long max_cap);

#if IS_ENABLED(CONFIG_RT_GROUP_SCHED)
static inline int rt_rq_throttled(struct rt_rq *rt_rq)
{
	return rt_rq->rt_throttled && !rt_rq->rt_nr_boosted;
}
#else /* !CONFIG_RT_GROUP_SCHED */
static inline int rt_rq_throttled(struct rt_rq *rt_rq)
{
	return rt_rq->rt_throttled;
}
#endif

extern int set_target_margin(int gearid, int margin);
extern int set_turn_point_freq(int gearid, unsigned long freq);

#if IS_ENABLED(CONFIG_MTK_SCHEDULER)
extern bool sysctl_util_est;
#endif

static inline bool is_util_est_enable(void)
{
#if IS_ENABLED(CONFIG_MTK_SCHEDULER)
	return sysctl_util_est;
#else
	return true;
#endif
}

static inline unsigned long mtk_cpu_util_cfs(struct rq *rq)
{
	unsigned long util = READ_ONCE(rq->cfs.avg.util_avg);

	if (sched_feat(UTIL_EST) && is_util_est_enable()) {
		util = max_t(unsigned long, util,
			     READ_ONCE(rq->cfs.avg.util_est.enqueued));
	}

	return util;
}

#endif /* _SCHED_COMMON_H */
