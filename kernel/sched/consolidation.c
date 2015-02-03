/*
 * CPU ConCurrency (CC) is measures the CPU load by averaging
 * the number of running tasks. Using CC, the scheduler can
 * evaluate the load of CPUs to improve load balance for power
 * efficiency without sacrificing performance.
 *
 * Copyright (C) 2013 Intel, Inc.,
 *
 * Author: Du, Yuyang <yuyang.du@intel.com>
 *
 * CPU Workload Consolidation consolidate workload to the smallest
 * number of CPUs that are capable of handling it. We measure
 * capability of CPU by CC, then compare it with a threshold,
 * and finally run the workload on non-shielded CPUs if they are
 * predicted capable after the consolidation.
 *
 * Copyright (C) 2013 Intel, Inc.,
 * Author: Rudramuni, Vishwesh M <vishwesh.m.rudramuni@intel.com>
 *	   Du, Yuyang <yuyang.du@intel.com>
 */

#ifdef CONFIG_CPU_CONCURRENCY

#include "sched.h"

/*
 * the sum period of time is 2^26 ns (~64) by default
 */
unsigned long sysctl_concurrency_sum_period = 26UL;

/*
 * the number of sum periods, after which the original
 * will be reduced/decayed to half
 */
unsigned long sysctl_concurrency_decay_rate = 1UL;

/*
 * the contrib period of time is 2^10 (~1us) by default,
 * us has better precision than ms, and
 * 1024 makes use of faster shift than div
 */
static unsigned long cc_contrib_period = 10UL;

#ifdef CONFIG_WORKLOAD_CONSOLIDATION
/*
 * whether we use concurrency to select cpu to run
 * the woken up task
 */
static unsigned long wc_wakeup = 1UL;

/*
 * concurrency lower than percentage of this number
 * is capable of running wakee
 */
static unsigned long wc_wakeup_threshold = 80UL;

/*
 * aggressively push the task even it is hot
 */
static unsigned long wc_push_hot_task = 1UL;
#endif

/*
 * the concurrency is scaled up for decaying,
 * thus, concurrency 1 is effectively 2^cc_resolution (1024),
 * which can be halved by 10 half-life periods
 */
static unsigned long cc_resolution = 10UL;

/*
 * after this number of half-life periods, even
 * (1>>32)-1 (which is sufficiently large) is less than 1
 */
static unsigned long cc_decay_max_pds = 32UL;

static inline unsigned long cc_scale_up(unsigned long c)
{
	return c << cc_resolution;
}

static inline unsigned long cc_scale_down(unsigned long c)
{
	return c >> cc_resolution;
}

/* from nanoseconds to sum periods */
static inline u64 cc_sum_pds(u64 n)
{
	return n >> sysctl_concurrency_sum_period;
}

/* from sum period to timestamp in ns */
static inline u64 cc_timestamp(u64 p)
{
	return p << sysctl_concurrency_sum_period;
}

/*
 * from nanoseconds to contrib periods, because
 * ns so risky that can overflow cc->contrib
 */
static inline u64 cc_contrib_pds(u64 n)
{
	return n >> cc_contrib_period;
}

/*
 * cc_decay_factor only works for 32bit integer,
 * cc_decay_factor_x, x indicates the number of periods
 * as half-life (sysctl_concurrency_decay_rate)
 */
static const unsigned long cc_decay_factor_1[] = {
	0xFFFFFFFF,
};

static const unsigned long cc_decay_factor_2[] = {
	0xFFFFFFFF, 0xB504F333,
};

static const unsigned long cc_decay_factor_4[] = {
	0xFFFFFFFF, 0xD744FCCA, 0xB504F333, 0x9837F051,
};

static const unsigned long cc_decay_factor_8[] = {
	0xFFFFFFFF, 0xEAC0C6E7, 0xD744FCCA, 0xC5672A11,
	0xB504F333, 0xA5FED6A9, 0x9837F051, 0x8B95C1E3,
};

/* by default sysctl_concurrency_decay_rate */
static const unsigned long *cc_decay_factor =
	cc_decay_factor_1;

/*
 * cc_decayed_sum depends on cc_resolution (fixed 10),
 * cc_decayed_sum_x, x indicates the number of periods
 * as half-life (sysctl_concurrency_decay_rate)
 */
static const unsigned long cc_decayed_sum_1[] = {
	0, 512, 768, 896, 960, 992,
	1008, 1016, 1020, 1022, 1023,
};

static const unsigned long cc_decayed_sum_2[] = {
	0, 724, 1235, 1597, 1853, 2034, 2162, 2252,
	2316, 2361, 2393, 2416, 2432, 2443, 2451,
	2457, 2461, 2464, 2466, 2467, 2468, 2469,
};

static const unsigned long cc_decayed_sum_4[] = {
	0, 861, 1585, 2193, 2705, 3135, 3497, 3801, 4057,
	4272, 4453, 4605, 4733, 4840, 4930, 5006, 5070,
	5124, 5169, 5207, 5239, 5266, 5289, 5308, 5324,
	5337, 5348, 5358, 5366, 5373, 5379, 5384, 5388,
	5391, 5394, 5396, 5398, 5400, 5401, 5402, 5403,
	5404, 5405, 5406,
};

static const unsigned long cc_decayed_sum_8[] = {
	0, 939, 1800, 2589, 3313, 3977, 4585, 5143,
	5655, 6124, 6554, 6949, 7311, 7643, 7947, 8226,
	8482, 8717, 8932, 9129, 9310, 9476, 9628, 9767,
	9895, 10012, 10120, 10219, 10309, 10392, 10468, 10538,
	10602, 10661, 10715, 10764, 10809, 10850, 10888, 10923,
	10955, 10984, 11011, 11036, 11059, 11080, 11099, 11116,
	11132, 11147, 11160, 11172, 11183, 11193, 11203, 11212,
	11220, 11227, 11234, 11240, 11246, 11251, 11256, 11260,
	11264, 11268, 11271, 11274, 11277, 11280, 11282, 11284,
	11286, 11288, 11290, 11291, 11292, 11293, 11294, 11295,
	11296, 11297, 11298, 11299, 11300, 11301, 11302,
};

/* by default sysctl_concurrency_decay_rate */
static const unsigned long *cc_decayed_sum = cc_decayed_sum_1;

/*
 * the last index of cc_decayed_sum array
 */
static unsigned long cc_decayed_sum_len =
	sizeof(cc_decayed_sum_1) / sizeof(cc_decayed_sum_1[0]) - 1;

/*
 * sysctl handler to update decay rate
 */
int concurrency_decay_rate_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret = proc_dointvec(table, write, buffer, lenp, ppos);

	if (ret || !write)
		return ret;

	switch (sysctl_concurrency_decay_rate) {
	case 1:
		cc_decay_factor = cc_decay_factor_1;
		cc_decayed_sum = cc_decayed_sum_1;
		cc_decayed_sum_len = sizeof(cc_decayed_sum_1) /
			sizeof(cc_decayed_sum_1[0]) - 1;
		break;
	case 2:
		cc_decay_factor = cc_decay_factor_2;
		cc_decayed_sum = cc_decayed_sum_2;
		cc_decayed_sum_len = sizeof(cc_decayed_sum_2) /
			sizeof(cc_decayed_sum_2[0]) - 1;
		break;
	case 4:
		cc_decay_factor = cc_decay_factor_4;
		cc_decayed_sum = cc_decayed_sum_4;
		cc_decayed_sum_len = sizeof(cc_decayed_sum_4) /
			sizeof(cc_decayed_sum_4[0]) - 1;
		break;
	case 8:
		cc_decay_factor = cc_decay_factor_8;
		cc_decayed_sum = cc_decayed_sum_8;
		cc_decayed_sum_len = sizeof(cc_decayed_sum_8) /
			sizeof(cc_decayed_sum_8[0]) - 1;
		break;
	default:
		return -EINVAL;
	}

	cc_decay_max_pds *= sysctl_concurrency_decay_rate;

	return 0;
}

/*
 * decay concurrency at some decay rate
 */
static inline u64 decay_cc(u64 cc, u64 periods)
{
	u32 periods_l;

	if (periods <= 0)
		return cc;

	if (unlikely(periods >= cc_decay_max_pds))
		return 0;

	/* now period is not too large */
	periods_l = (u32)periods;
	if (periods_l >= sysctl_concurrency_decay_rate) {
		cc >>= periods_l / sysctl_concurrency_decay_rate;
		periods_l %= sysctl_concurrency_decay_rate;
	}

	if (!periods_l)
		return cc;

	cc *= cc_decay_factor[periods_l];

	return cc >> 32;
}

/*
 * add missed periods by predefined constants
 */
static inline u64 cc_missed_pds(u64 periods)
{
	if (periods <= 0)
		return 0;

	if (periods > cc_decayed_sum_len)
		periods = cc_decayed_sum_len;

	return cc_decayed_sum[periods];
}

/*
 * scale up nr_running, because we decay
 */
static inline unsigned long cc_weight(unsigned long nr_running)
{
	/*
	 * scaling factor, this should be tunable
	 */
	return cc_scale_up(nr_running);
}

static inline void
__update_concurrency(struct rq *rq, u64 now, struct cpu_concurrency_t *cc)
{
	u64 sum_pds, sum_pds_s, sum_pds_e;
	u64 contrib_pds, ts_contrib, contrib_pds_one;
	u64 sum_now;
	unsigned long weight;
	int updated = 0;

	/*
	 * guarantee contrib_timestamp always >= sum_timestamp,
	 * and sum_timestamp is at period boundary
	 */
	if (now <= cc->sum_timestamp) {
		cc->sum_timestamp = cc_timestamp(cc_sum_pds(now));
		cc->contrib_timestamp = now;
		return;
	}

	weight = cc_weight(cc->nr_running);

	/* start and end of sum periods */
	sum_pds_s = cc_sum_pds(cc->sum_timestamp);
	sum_pds_e = cc_sum_pds(now);
	sum_pds = sum_pds_e - sum_pds_s;
	/* number of contrib periods in one sum period */
	contrib_pds_one = cc_contrib_pds(cc_timestamp(1));

	/*
	 * if we have passed at least one period,
	 * we need to do four things:
	 */
	if (sum_pds) {
		/* 1) complete the last period */
		ts_contrib = cc_timestamp(sum_pds_s + 1);
		contrib_pds = cc_contrib_pds(ts_contrib);
		contrib_pds -= cc_contrib_pds(cc->contrib_timestamp);

		if (likely(contrib_pds))
			cc->contrib += weight * contrib_pds;

		cc->contrib = div64_u64(cc->contrib, contrib_pds_one);

		cc->sum += cc->contrib;
		cc->contrib = 0;

		/* 2) update/decay them */
		cc->sum = decay_cc(cc->sum, sum_pds);
		sum_now = decay_cc(cc->sum, sum_pds - 1);

		/* 3) compensate missed periods if any */
		sum_pds -= 1;
		cc->sum += cc->nr_running * cc_missed_pds(sum_pds);
		sum_now += cc->nr_running * cc_missed_pds(sum_pds - 1);
		updated = 1;

		/* 4) update contrib timestamp to period boundary */
		ts_contrib = cc_timestamp(sum_pds_e);

		cc->sum_timestamp = ts_contrib;
		cc->contrib_timestamp = ts_contrib;
	}

	/* current period */
	contrib_pds = cc_contrib_pds(now);
	contrib_pds -= cc_contrib_pds(cc->contrib_timestamp);

	if (likely(contrib_pds))
		cc->contrib += weight * contrib_pds;

	/* new nr_running for next update */
	cc->nr_running = rq->nr_running;

	/*
	 * we need to account for the current sum period,
	 * if now has passed 1/2 of sum period, we contribute,
	 * otherwise, we use the last complete sum period
	 */
	contrib_pds = cc_contrib_pds(now - cc->sum_timestamp);

	if (contrib_pds > contrib_pds_one / 2) {
		sum_now = div64_u64(cc->contrib, contrib_pds);
		sum_now += cc->sum;
		updated = 1;
	}

	if (updated == 1)
		cc->sum_now = sum_now;
	cc->contrib_timestamp = now;
}

void init_cpu_concurrency(struct rq *rq)
{
	rq->concurrency.sum = 0;
	rq->concurrency.sum_now = 0;
	rq->concurrency.contrib = 0;
	rq->concurrency.nr_running = 0;
	rq->concurrency.sum_timestamp = ULLONG_MAX;
	rq->concurrency.contrib_timestamp = ULLONG_MAX;
#ifdef CONFIG_WORKLOAD_CONSOLIDATION
	rq->concurrency.unload = 0;
#endif
}

/*
 * we update cpu concurrency at:
 * 1) enqueue task, which increases concurrency
 * 2) dequeue task, which decreases concurrency
 * 3) periodic scheduler tick, in case no en/dequeue for long
 * 4) enter and exit idle (necessary?)
 */
void update_cpu_concurrency(struct rq *rq)
{
	/*
	 * protected under rq->lock
	 */
	struct cpu_concurrency_t *cc = &rq->concurrency;
	u64 now = rq->clock;

	__update_concurrency(rq, now, cc);
}

#endif

#ifdef CONFIG_WORKLOAD_CONSOLIDATION
/*
 * whether cpu is capable of having more concurrency
 */
static int cpu_cc_capable(int cpu)
{
	u64 sum = cpu_rq(cpu)->concurrency.sum_now;
	u64 threshold = cc_weight(1);

	sum *= 100;
	sum *= cpu_rq(cpu)->cpu_power;

	threshold *= wc_wakeup_threshold;
	threshold <<= SCHED_POWER_SHIFT;

	if (sum <= threshold)
		return 1;

	return 0;
}

/*
 * we do not select idle, if the cc of the
 * wakee and waker (in this order) is capable
 * of handling the wakee task
 */
int workload_consolidation_wakeup(int prev, int target)
{
	if (!wc_wakeup) {
		if (idle_cpu(target))
			return target;
		return nr_cpu_ids;
	}

	if (idle_cpu(prev) || cpu_cc_capable(prev))
		return prev;

	if (prev != target && (idle_cpu(target) || cpu_cc_capable(target)))
		return target;

	return nr_cpu_ids;
}

static inline u64 sched_group_cc(struct sched_group *sg)
{
	u64 sg_cc = 0;
	int i;

	for_each_cpu(i, sched_group_cpus(sg))
		sg_cc += cpu_rq(i)->concurrency.sum_now *
			cpu_rq(i)->cpu_power;

	return sg_cc;
}

static inline u64 sched_domain_cc(struct sched_domain *sd)
{
	struct sched_group *sg = sd->groups;
	u64 sd_cc = 0;

	do {
		sd_cc += sched_group_cc(sg);
		sg = sg->next;
	} while (sg != sd->groups);

	return sd_cc;
}

static inline struct sched_group *
find_lowest_cc_group(struct sched_group *sg, int span)
{
	u64 grp_cc, min = ULLONG_MAX;
	struct sched_group *lowest = NULL;
	int i;

	for (i = 0; i < span; ++i) {
		grp_cc = sched_group_cc(sg);

		if (grp_cc < min) {
			min = grp_cc;
			lowest = sg;
		}

		sg = sg->next;
	}

	return lowest;
}

static inline u64 __calc_cc_thr(int cpus, unsigned int asym_cc)
{
	u64 thr = cpus;

	thr *= cc_weight(1);
	thr *= asym_cc;
	thr <<= SCHED_POWER_SHIFT;

	return thr;
}

/*
 * can @src_cc of @src_nr cpus be consolidated
 * to @dst_cc of @dst_nr cpus
 */
static inline int
__can_consolidate_cc(u64 src_cc, int src_nr, u64 dst_cc, int dst_nr)
{
	dst_cc *= dst_nr;
	src_nr -= dst_nr;

	if (unlikely(src_nr <= 0))
		return 0;

	src_nr = ilog2(src_nr);
	src_nr += dst_nr;
	src_cc *= src_nr;

	if (src_cc > dst_cc)
		return 0;

	return 1;
}

/*
 * find the group for asymmetric concurrency
 * problem to address: traverse sd from top to down
 */
struct sched_group *
workload_consolidation_find_group(struct sched_domain *sd,
	struct task_struct *p, int this_cpu)
{
	int half, sg_weight, ns_half = 0;
	struct sched_group *sg;
	u64 sd_cc;

	half = DIV_ROUND_CLOSEST(sd->total_groups, 2);
	sg_weight = sd->groups->group_weight;

	sd_cc = sched_domain_cc(sd);
	sd_cc *= 100;

	while (half) {
		int allowed = 0, i;
		int cpus = sg_weight * half;
		u64 threshold = __calc_cc_thr(cpus,
		sd->asym_concurrency);

		/*
		 * we did not consider the added cc by this
		 * wakeup (mostly from fork/exec)
		 */
		if (!__can_consolidate_cc(sd_cc, sd->span_weight,
			threshold, cpus))
			break;

		sg = sd->first_group;
		for (i = 0; i < half; ++i) {
			/* if it has no cpus allowed */
			if (!cpumask_intersects(sched_group_cpus(sg),
					tsk_cpus_allowed(p)))
				continue;

			allowed = 1;
			sg = sg->next;
		}

		if (!allowed)
			break;

		ns_half = half;
		half /= 2;
	}

	if (!ns_half)
		return NULL;

	if (ns_half == 1)
		return sd->first_group;

	return find_lowest_cc_group(sd->first_group, ns_half);
}

/*
 * top_flag_domain - return top sched_domain containing flag.
 * @cpu:       the cpu whose highest level of sched domain is to
 *             be returned.
 * @flag:      the flag to check for the highest sched_domain
 *             for the given cpu.
 *
 * returns the highest sched_domain of a cpu which contains the given flag.
 * different from highest_flag_domain in that along the domain upward chain
 * domain may or may not contain the flag.
 */
static inline struct sched_domain *top_flag_domain(int cpu, int flag)
{
	struct sched_domain *sd, *hsd = NULL;

	for_each_domain(cpu, sd) {
		if (!(sd->flags & flag))
			continue;
		hsd = sd;
	}

	return hsd;
}

/*
 * workload_consolidation_cpu_shielded - return whether @cpu is shielded or not
 *
 * traverse downward the sched_domain tree when the sched_domain contains
 * flag SD_ASYM_CONCURRENCY, each sd may have more than two groups, but
 * we assume 1) every sched_group has the same weight, 2) every CPU has
 * the same computing power
 */
int workload_consolidation_cpu_shielded(int cpu)
{
	struct sched_domain *sd;

	sd = top_flag_domain(cpu, SD_ASYM_CONCURRENCY);

	while (sd) {
		int half, sg_weight, this_sg_nr;
		u64 sd_cc;

		if (!(sd->flags & SD_ASYM_CONCURRENCY)) {
			sd = sd->child;
			continue;
		}

		half = DIV_ROUND_CLOSEST(sd->total_groups, 2);
		sg_weight = sd->groups->group_weight;
		this_sg_nr = sd->group_number;

		sd_cc = sched_domain_cc(sd);
		sd_cc *= 100;

		while (half) {
			int cpus = sg_weight * half;
			u64 threshold = __calc_cc_thr(cpus,
				sd->asym_concurrency);

		if (!__can_consolidate_cc(sd_cc, sd->span_weight,
			threshold, cpus))
			return 0;

		if (this_sg_nr >= half)
			return 1;

			half /= 2;
		}

		sd = sd->child;
	}

	return 0;
}

/*
 * as of now, we have the following assumption
 * 1) every sched_group has the same weight
 * 2) every CPU has the same computing power
 */
static inline int __nonshielded_groups(struct sched_domain *sd)
{
	int half, sg_weight, ret = 0;
	u64 sd_cc;

	half = DIV_ROUND_CLOSEST(sd->total_groups, 2);
	sg_weight = sd->groups->group_weight;

	sd_cc = sched_domain_cc(sd);
	sd_cc *= 100;

	while (half) {
		int cpus = sg_weight * half;
		u64 threshold = __calc_cc_thr(cpus,
			sd->asym_concurrency);

		if (!__can_consolidate_cc(sd_cc, sd->span_weight,
			threshold, cpus))
			return ret;

		ret = half;
		half /= 2;
	}

	return ret;
}

static DEFINE_PER_CPU(struct cpumask, nonshielded_cpumask);

/*
 * workload_consolidation_nonshielded_mask
 * return the nonshielded cpus in the @mask, which is unmasked
 * by the shielded cpus
 *
 * traverse downward the sched_domain tree when the sched_domain contains
 * flag SD_ASYM_CONCURRENCY, each sd may have more than two groups
 */
void workload_consolidation_nonshielded_mask(int cpu, struct cpumask *mask)
{
	struct sched_domain *sd;
	struct cpumask *pcpu_mask = &per_cpu(nonshielded_cpumask, cpu);
	int i;

	sd = top_flag_domain(cpu, SD_ASYM_CONCURRENCY);

	if (!sd)
		return;

	while (sd) {
		struct sched_group *sg;
		int this_sg_nr, ns_half;

		if (!(sd->flags & SD_ASYM_CONCURRENCY)) {
			sd = sd->child;
			continue;
		}

		ns_half = __nonshielded_groups(sd);

		if (!ns_half)
			break;

		cpumask_clear(pcpu_mask);
		sg = sd->first_group;

		for (i = 0; i < ns_half; ++i) {
			cpumask_or(pcpu_mask, pcpu_mask,
				sched_group_cpus(sg));
			sg = sg->next;
		}

		cpumask_and(mask, mask, pcpu_mask);

		this_sg_nr = sd->group_number;
		if (this_sg_nr)
			break;

		sd = sd->child;
	}
}

static int cpu_task_hot(struct task_struct *p, u64 now)
{
	s64 delta;

	if (p->sched_class != &fair_sched_class)
		return 0;

	if (unlikely(p->policy == SCHED_IDLE))
		return 0;

	if (sysctl_sched_migration_cost == -1)
		return 1;

	if (sysctl_sched_migration_cost == 0)
		return 0;

	if (wc_push_hot_task)
		return 0;

	/*
	 * buddy candidates are cache hot:
	 */
	if (sched_feat(CACHE_HOT_BUDDY) && this_rq()->nr_running &&
			(&p->se == p->se.cfs_rq->next ||
			&p->se == p->se.cfs_rq->last)) {
		return 1;
	}

	delta = now - p->se.exec_start;

	if (delta < (s64)sysctl_sched_migration_cost)
		return 1;

	return 0;
}

static int
cpu_move_task(struct task_struct *p, struct rq *src_rq, struct rq *dst_rq)
{
	/*
	 * we do not migrate tasks that are:
	 * 1) running (obviously), or
	 * 2) cannot be migrated to this CPU due to cpus_allowed, or
	 * 3) are cache-hot on their current CPU.
	 */
	if (!cpumask_test_cpu(dst_rq->cpu, tsk_cpus_allowed(p)))
		return 0;

	if (task_running(src_rq, p))
		return 0;

	/*
	 * aggressive migration if task is cache cold
	 */
	if (!cpu_task_hot(p, src_rq->clock_task)) {
		/*
		 * move a task
		 */
		deactivate_task(src_rq, p, 0);
		set_task_cpu(p, dst_rq->cpu);
		activate_task(dst_rq, p, 0);
		check_preempt_curr(dst_rq, p, 0);
		return 1;
	}

	return 0;
}

/*
 * __unload_cpu_work is run by src cpu stopper, which pushes running
 * tasks off src cpu onto dst cpu
 */
static int __unload_cpu_work(void *data)
{
	struct rq *src_rq = data;
	int src_cpu = cpu_of(src_rq);
	struct cpu_concurrency_t *cc = &src_rq->concurrency;
	struct rq *dst_rq = cpu_rq(cc->dst_cpu);

	struct list_head *tasks = &src_rq->cfs_tasks;
	struct task_struct *p, *n;
	int pushed = 0;
	int nr_migrate_break = 1;

	raw_spin_lock_irq(&src_rq->lock);

	/* make sure the requested cpu hasn't gone down in the meantime */
	if (unlikely(src_cpu != smp_processor_id() || !cc->unload))
		goto out_unlock;

	/* Is there any task to move? */
	if (src_rq->nr_running <= 1)
		goto out_unlock;

	double_lock_balance(src_rq, dst_rq);

	list_for_each_entry_safe(p, n, tasks, se.group_node) {

		if (!cpu_move_task(p, src_rq, dst_rq))
			continue;

		pushed++;

		if (pushed >= nr_migrate_break)
			break;
	}

	double_unlock_balance(src_rq, dst_rq);
out_unlock:
	cc->unload = 0;
	raw_spin_unlock_irq(&src_rq->lock);

	return 0;
}

/*
 * unload src_cpu to dst_cpu
 */
static void unload_cpu(int src_cpu, int dst_cpu)
{
	unsigned long flags;
	struct rq *src_rq = cpu_rq(src_cpu);
	struct cpu_concurrency_t *cc = &src_rq->concurrency;
	int unload = 0;

	raw_spin_lock_irqsave(&src_rq->lock, flags);

	if (!cc->unload) {
		cc->unload = 1;
		cc->dst_cpu = dst_cpu;
		unload = 1;
	}

	raw_spin_unlock_irqrestore(&src_rq->lock, flags);

	if (unload)
		stop_one_cpu_nowait(src_cpu, __unload_cpu_work, src_rq,
			&cc->unload_work);
}

static inline int find_lowest_cc_cpu(struct cpumask *mask)
{
	u64 cpu_cc, min = ULLONG_MAX;
	int i, lowest = nr_cpu_ids;
	struct rq *rq;

	for_each_cpu(i, mask) {
		rq = cpu_rq(i);
		cpu_cc = rq->concurrency.sum_now * rq->cpu_power;

		if (cpu_cc < min) {
			min = cpu_cc;
			lowest = i;
		}
	}

	return lowest;
}

/*
 * find the lowest cc cpu in shielded and nonshielded cpus,
 * aggressively unload the shielded to the nonshielded
 */
void workload_consolidation_unload(struct cpumask *nonshielded)
{
	int src_cpu = nr_cpu_ids, dst_cpu, i;
	u64 cpu_cc, min = ULLONG_MAX;
	struct rq *rq;

	for_each_cpu_not(i, nonshielded) {
		if (i >= nr_cpu_ids)
			break;

		rq = cpu_rq(i);
		if (rq->nr_running <= 0)
			continue;

		cpu_cc = rq->concurrency.sum_now * rq->cpu_power;
		if (cpu_cc < min) {
			min = cpu_cc;
			src_cpu = i;
		}
	}

	if (src_cpu >= nr_cpu_ids)
		return;

	dst_cpu = find_lowest_cc_cpu(nonshielded);
	if (dst_cpu >= nr_cpu_ids)
		return;

	if (src_cpu != dst_cpu)
		unload_cpu(src_cpu, dst_cpu);
}

#endif /* CONFIG_WORKLOAD_CONSOLIDATION */
