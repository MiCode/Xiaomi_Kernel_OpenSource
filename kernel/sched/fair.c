/*
 * Completely Fair Scheduling (CFS) Class (SCHED_NORMAL/SCHED_BATCH)
 *
 *  Copyright (C) 2007 Red Hat, Inc., Ingo Molnar <mingo@redhat.com>
 *
 *  Interactivity improvements by Mike Galbraith
 *  (C) 2007 Mike Galbraith <efault@gmx.de>
 *
 *  Various enhancements by Dmitry Adamushko.
 *  (C) 2007 Dmitry Adamushko <dmitry.adamushko@gmail.com>
 *
 *  Group scheduling enhancements by Srivatsa Vaddagiri
 *  Copyright IBM Corporation, 2007
 *  Author: Srivatsa Vaddagiri <vatsa@linux.vnet.ibm.com>
 *
 *  Scaled math optimizations by Thomas Gleixner
 *  Copyright (C) 2007, Thomas Gleixner <tglx@linutronix.de>
 *
 *  Adaptive scheduling granularity, math enhancements by Peter Zijlstra
 *  Copyright (C) 2007 Red Hat, Inc., Peter Zijlstra <pzijlstr@redhat.com>
 */

#include <linux/latencytop.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/cpuidle.h>
#include <linux/slab.h>
#include <linux/profile.h>
#include <linux/interrupt.h>
#include <linux/mempolicy.h>
#include <linux/migrate.h>
#include <linux/task_work.h>
#include <linux/stop_machine.h>

#include <trace/events/sched.h>
#ifdef CONFIG_HMP_FREQUENCY_INVARIANT_SCALE
/* Include cpufreq header to add a notifier so that cpu frequency
 * scaling can track the current CPU frequency
 */
#include <linux/cpufreq.h>
#endif /* CONFIG_HMP_FREQUENCY_INVARIANT_SCALE */

#include "sched.h"

/*
 * Targeted preemption latency for CPU-bound tasks:
 * (default: 6ms * (1 + ilog(ncpus)), units: nanoseconds)
 *
 * NOTE: this latency value is not the same as the concept of
 * 'timeslice length' - timeslices in CFS are of variable length
 * and have no persistent notion like in traditional, time-slice
 * based scheduling concepts.
 *
 * (to see the precise effective timeslice length of your workload,
 *  run vmstat and monitor the context-switches (cs) field)
 */
unsigned int sysctl_sched_latency = 6000000ULL;
unsigned int normalized_sysctl_sched_latency = 6000000ULL;

/*
 * The initial- and re-scaling of tunables is configurable
 * (default SCHED_TUNABLESCALING_LOG = *(1+ilog(ncpus))
 *
 * Options are:
 * SCHED_TUNABLESCALING_NONE - unscaled, always *1
 * SCHED_TUNABLESCALING_LOG - scaled logarithmical, *1+ilog(ncpus)
 * SCHED_TUNABLESCALING_LINEAR - scaled linear, *ncpus
 */
enum sched_tunable_scaling sysctl_sched_tunable_scaling
	= SCHED_TUNABLESCALING_LOG;

/*
 * Minimal preemption granularity for CPU-bound tasks:
 * (default: 0.75 msec * (1 + ilog(ncpus)), units: nanoseconds)
 */
unsigned int sysctl_sched_min_granularity = 750000ULL;
unsigned int normalized_sysctl_sched_min_granularity = 750000ULL;

/*
 * is kept at sysctl_sched_latency / sysctl_sched_min_granularity
 */
static unsigned int sched_nr_latency = 8;

/*
 * After fork, child runs first. If set to 0 (default) then
 * parent will (try to) run first.
 */
unsigned int sysctl_sched_child_runs_first __read_mostly;

/*
 * SCHED_OTHER wake-up granularity.
 * (default: 1 msec * (1 + ilog(ncpus)), units: nanoseconds)
 *
 * This option delays the preemption effects of decoupled workloads
 * and reduces their over-scheduling. Synchronous workloads will still
 * have immediate wakeup/sleep latencies.
 */
unsigned int sysctl_sched_wakeup_granularity = 1000000UL;
unsigned int normalized_sysctl_sched_wakeup_granularity = 1000000UL;

#ifdef CONFIG_MT_LOAD_BALANCE_ENHANCEMENT
/* shorten the schedule migration cost and let the idle balance more aggregative */
const_debug unsigned int sysctl_sched_migration_cost = 33000UL;
#else
const_debug unsigned int sysctl_sched_migration_cost = 500000UL;
#endif

/*
 * The exponential sliding  window over which load is averaged for shares
 * distribution.
 * (default: 10msec)
 */
unsigned int __read_mostly sysctl_sched_shares_window = 10000000UL;

#ifdef CONFIG_CFS_BANDWIDTH
/*
 * Amount of runtime to allocate from global (tg) to local (per-cfs_rq) pool
 * each time a cfs_rq requests quota.
 *
 * Note: in the case that the slice exceeds the runtime remaining (either due
 * to consumption or the quota being specified to be smaller than the slice)
 * we will always only issue the remaining available time.
 *
 * default: 5 msec, units: microseconds
  */
unsigned int sysctl_sched_cfs_bandwidth_slice = 5000UL;
#endif

static inline void update_load_add(struct load_weight *lw, unsigned long inc)
{
	lw->weight += inc;
	lw->inv_weight = 0;
}

static inline void update_load_sub(struct load_weight *lw, unsigned long dec)
{
	lw->weight -= dec;
	lw->inv_weight = 0;
}

static inline void update_load_set(struct load_weight *lw, unsigned long w)
{
	lw->weight = w;
	lw->inv_weight = 0;
}

/*
 * Increase the granularity value when there are more CPUs,
 * because with more CPUs the 'effective latency' as visible
 * to users decreases. But the relationship is not linear,
 * so pick a second-best guess by going with the log2 of the
 * number of CPUs.
 *
 * This idea comes from the SD scheduler of Con Kolivas:
 */
static int get_update_sysctl_factor(void)
{
	unsigned int cpus = min_t(int, num_online_cpus(), 8);
	unsigned int factor;

	switch (sysctl_sched_tunable_scaling) {
	case SCHED_TUNABLESCALING_NONE:
		factor = 1;
		break;
	case SCHED_TUNABLESCALING_LINEAR:
		factor = cpus;
		break;
	case SCHED_TUNABLESCALING_LOG:
	default:
		factor = 1 + ilog2(cpus);
		break;
	}

	return factor;
}

static void update_sysctl(void)
{
	unsigned int factor = get_update_sysctl_factor();

#define SET_SYSCTL(name) \
	(sysctl_##name = (factor) * normalized_sysctl_##name)
	SET_SYSCTL(sched_min_granularity);
	SET_SYSCTL(sched_latency);
	SET_SYSCTL(sched_wakeup_granularity);
#undef SET_SYSCTL
}

void sched_init_granularity(void)
{
	update_sysctl();
}
#ifdef CONFIG_HMP_PACK_SMALL_TASK
/*
 * Save the id of the optimal CPU that should be used to pack small tasks
 * The value -1 is used when no buddy has been found
 */
DEFINE_PER_CPU(int, sd_pack_buddy) = {-1};

/* Look for the best buddy CPU that can be used to pack small tasks
 * We make the assumption that it doesn't wort to pack on CPU that share the
 * same powerline. We looks for the 1st sched_domain without the
 * SD_SHARE_POWERLINE flag. Then We look for the sched_group witht the lowest
 * power per core based on the assumption that their power efficiency is
 * better */
void update_packing_domain(int cpu)
{
	struct sched_domain *sd;
	int id = -1;

	sd = rcu_dereference_check_sched_domain(cpu_rq(cpu)->sd);
	while (sd) {
		struct sched_group *sg = sd->groups;
		struct sched_group *pack = sg;
		struct sched_group *tmp = sg->next;

		/* 1st CPU of the sched domain is a good candidate */
		if (id == -1)
			id = cpumask_first(sched_domain_span(sd));

		/* Find sched group of candidate */
		tmp = sd->groups;
		do {
			if (cpumask_test_cpu(id, sched_group_cpus(tmp))) {
				sg = tmp;
				break;
			}
		} while (tmp = tmp->next, tmp != sd->groups);

		pack = sg;
		tmp = sg->next;

		/* loop the sched groups to find the best one
		 * Stop find the best one in the same Load Balance Domain
		 */
		while (tmp != sg && !(sd->flags & SD_LOAD_BALANCE)) {
			if (tmp->sgc->capacity * sg->group_weight <
					sg->sgc->capacity * tmp->group_weight)
				pack = tmp;
			tmp = tmp->next;
		}

		/* we have found a better group */
		if (pack != sg)
			id = cpumask_first(sched_group_cpus(pack));

		/* Look for another CPU than itself */
		if ((id != cpu) ||
			((sd->parent) && (sd->parent->flags & SD_LOAD_BALANCE)))
			break;

		sd = sd->parent;
	}

	per_cpu(sd_pack_buddy, cpu) = id;
}

static inline bool is_buddy_busy(int cpu)
{
	struct rq *rq;

	if (cpu < 0)
		return 0;

	rq = cpu_rq(cpu);

	/*
	 * A busy buddy is a CPU with a high load or a small load with a lot of
	 * running tasks.
	 */
	return ((rq->avg.running_avg_sum << rq->nr_running) >
			rq->avg.avg_period);
}

static inline bool is_light_task(struct task_struct *p)
{
	/* A light task runs less than 25% in average */
	return ((p->se.avg.running_avg_sum << 2) < p->se.avg.avg_period);
}

static int check_pack_buddy(int cpu, struct task_struct *p);
#endif /* CONFIG_HMP_PACK_SMALL_TASK */

#define WMULT_CONST	(~0U)
#define WMULT_SHIFT	32

static void __update_inv_weight(struct load_weight *lw)
{
	unsigned long w;

	if (likely(lw->inv_weight))
		return;

	w = scale_load_down(lw->weight);

	if (BITS_PER_LONG > 32 && unlikely(w >= WMULT_CONST))
		lw->inv_weight = 1;
	else if (unlikely(!w))
		lw->inv_weight = WMULT_CONST;
	else
		lw->inv_weight = WMULT_CONST / w;
}

/*
 * delta_exec * weight / lw.weight
 *   OR
 * (delta_exec * (weight * lw->inv_weight)) >> WMULT_SHIFT
 *
 * Either weight := NICE_0_LOAD and lw \e prio_to_wmult[], in which case
 * we're guaranteed shift stays positive because inv_weight is guaranteed to
 * fit 32 bits, and NICE_0_LOAD gives another 10 bits; therefore shift >= 22.
 *
 * Or, weight =< lw.weight (because lw.weight is the runqueue weight), thus
 * weight/lw.weight <= 1, and therefore our shift will also be positive.
 */
static u64 __calc_delta(u64 delta_exec, unsigned long weight, struct load_weight *lw)
{
	u64 fact = scale_load_down(weight);
	int shift = WMULT_SHIFT;

	__update_inv_weight(lw);

	if (unlikely(fact >> 32)) {
		while (fact >> 32) {
			fact >>= 1;
			shift--;
		}
	}

	/* hint to use a 32x32->64 mul */
	fact = (u64)(u32)fact * lw->inv_weight;

	while (fact >> 32) {
		fact >>= 1;
		shift--;
	}

	return mul_u64_u32_shr(delta_exec, fact, shift);
}


const struct sched_class fair_sched_class;

/**************************************************************
 * CFS operations on generic schedulable entities:
 */

#ifdef CONFIG_FAIR_GROUP_SCHED

/* cpu runqueue to which this cfs_rq is attached */
static inline struct rq *rq_of(struct cfs_rq *cfs_rq)
{
	return cfs_rq->rq;
}

/* An entity is a task if it doesn't "own" a runqueue */
#define entity_is_task(se)	(!se->my_q)

static inline struct task_struct *task_of(struct sched_entity *se)
{
#ifdef CONFIG_SCHED_DEBUG
	/* WARN_ON_ONCE(!entity_is_task(se)); */
#endif
	return container_of(se, struct task_struct, se);
}

/* Walk up scheduling entities hierarchy */
#define for_each_sched_entity(se) \
		for (; se; se = se->parent)

static inline struct cfs_rq *task_cfs_rq(struct task_struct *p)
{
	return p->se.cfs_rq;
}

/* runqueue on which this entity is (to be) queued */
static inline struct cfs_rq *cfs_rq_of(struct sched_entity *se)
{
	return se->cfs_rq;
}

/* runqueue "owned" by this group */
static inline struct cfs_rq *group_cfs_rq(struct sched_entity *grp)
{
	return grp->my_q;
}

static void update_cfs_rq_blocked_load(struct cfs_rq *cfs_rq,
				       int force_update);

static inline void list_add_leaf_cfs_rq(struct cfs_rq *cfs_rq)
{
	if (!cfs_rq->on_list) {
		/*
		 * Ensure we either appear before our parent (if already
		 * enqueued) or force our parent to appear after us when it is
		 * enqueued.  The fact that we always enqueue bottom-up
		 * reduces this to two cases.
		 */
		if (cfs_rq->tg->parent &&
		    cfs_rq->tg->parent->cfs_rq[cpu_of(rq_of(cfs_rq))]->on_list) {
			list_add_rcu(&cfs_rq->leaf_cfs_rq_list,
				&rq_of(cfs_rq)->leaf_cfs_rq_list);
		} else {
			list_add_tail_rcu(&cfs_rq->leaf_cfs_rq_list,
				&rq_of(cfs_rq)->leaf_cfs_rq_list);
		}

		cfs_rq->on_list = 1;
		/* We should have no load, but we need to update last_decay. */
		update_cfs_rq_blocked_load(cfs_rq, 0);
	}
}

static inline void list_del_leaf_cfs_rq(struct cfs_rq *cfs_rq)
{
	if (cfs_rq->on_list) {
		list_del_rcu(&cfs_rq->leaf_cfs_rq_list);
		cfs_rq->on_list = 0;
	}
}

/* Iterate thr' all leaf cfs_rq's on a runqueue */
#define for_each_leaf_cfs_rq(rq, cfs_rq) \
	list_for_each_entry_rcu(cfs_rq, &rq->leaf_cfs_rq_list, leaf_cfs_rq_list)

/* Do the two (enqueued) entities belong to the same group ? */
static inline struct cfs_rq *
is_same_group(struct sched_entity *se, struct sched_entity *pse)
{
	if (se->cfs_rq == pse->cfs_rq)
		return se->cfs_rq;

	return NULL;
}

static inline struct sched_entity *parent_entity(struct sched_entity *se)
{
	return se->parent;
}

static void
find_matching_se(struct sched_entity **se, struct sched_entity **pse)
{
	int se_depth, pse_depth;

	/*
	 * preemption test can be made between sibling entities who are in the
	 * same cfs_rq i.e who have a common parent. Walk up the hierarchy of
	 * both tasks until we find their ancestors who are siblings of common
	 * parent.
	 */

	/* First walk up until both entities are at same depth */
	se_depth = (*se)->depth;
	pse_depth = (*pse)->depth;

	while (se_depth > pse_depth) {
		se_depth--;
		*se = parent_entity(*se);
	}

	while (pse_depth > se_depth) {
		pse_depth--;
		*pse = parent_entity(*pse);
	}

	while (!is_same_group(*se, *pse)) {
		*se = parent_entity(*se);
		*pse = parent_entity(*pse);
	}
}

#else	/* !CONFIG_FAIR_GROUP_SCHED */

static inline struct task_struct *task_of(struct sched_entity *se)
{
	return container_of(se, struct task_struct, se);
}

static inline struct rq *rq_of(struct cfs_rq *cfs_rq)
{
	return container_of(cfs_rq, struct rq, cfs);
}

#define entity_is_task(se)	1

#define for_each_sched_entity(se) \
		for (; se; se = NULL)

static inline struct cfs_rq *task_cfs_rq(struct task_struct *p)
{
	return &task_rq(p)->cfs;
}

static inline struct cfs_rq *cfs_rq_of(struct sched_entity *se)
{
	struct task_struct *p = task_of(se);
	struct rq *rq = task_rq(p);

	return &rq->cfs;
}

/* runqueue "owned" by this group */
static inline struct cfs_rq *group_cfs_rq(struct sched_entity *grp)
{
	return NULL;
}

static inline void list_add_leaf_cfs_rq(struct cfs_rq *cfs_rq)
{
}

static inline void list_del_leaf_cfs_rq(struct cfs_rq *cfs_rq)
{
}

#define for_each_leaf_cfs_rq(rq, cfs_rq) \
		for (cfs_rq = &rq->cfs; cfs_rq; cfs_rq = NULL)

static inline struct sched_entity *parent_entity(struct sched_entity *se)
{
	return NULL;
}

static inline void
find_matching_se(struct sched_entity **se, struct sched_entity **pse)
{
}

#endif	/* CONFIG_FAIR_GROUP_SCHED */

static __always_inline
void account_cfs_rq_runtime(struct cfs_rq *cfs_rq, u64 delta_exec);

/**************************************************************
 * Scheduling class tree data structure manipulation methods:
 */

static inline u64 max_vruntime(u64 max_vruntime, u64 vruntime)
{
	s64 delta = (s64)(vruntime - max_vruntime);
	if (delta > 0)
		max_vruntime = vruntime;

	return max_vruntime;
}

static inline u64 min_vruntime(u64 min_vruntime, u64 vruntime)
{
	s64 delta = (s64)(vruntime - min_vruntime);
	if (delta < 0)
		min_vruntime = vruntime;

	return min_vruntime;
}

static inline int entity_before(struct sched_entity *a,
				struct sched_entity *b)
{
	return (s64)(a->vruntime - b->vruntime) < 0;
}

static void update_min_vruntime(struct cfs_rq *cfs_rq)
{
	u64 vruntime = cfs_rq->min_vruntime;

	if (cfs_rq->curr)
		vruntime = cfs_rq->curr->vruntime;

	if (cfs_rq->rb_leftmost) {
		struct sched_entity *se = rb_entry(cfs_rq->rb_leftmost,
						   struct sched_entity,
						   run_node);

		if (!cfs_rq->curr)
			vruntime = se->vruntime;
		else
			vruntime = min_vruntime(vruntime, se->vruntime);
	}

	/* ensure we never gain time by being placed backwards. */
	cfs_rq->min_vruntime = max_vruntime(cfs_rq->min_vruntime, vruntime);
#ifndef CONFIG_64BIT
	smp_wmb();
	cfs_rq->min_vruntime_copy = cfs_rq->min_vruntime;
#endif
}

/*
 * Enqueue an entity into the rb-tree:
 */
static void __enqueue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	struct rb_node **link = &cfs_rq->tasks_timeline.rb_node;
	struct rb_node *parent = NULL;
	struct sched_entity *entry;
	int leftmost = 1;

	/*
	 * Find the right place in the rbtree:
	 */
	while (*link) {
		parent = *link;
		entry = rb_entry(parent, struct sched_entity, run_node);
		/*
		 * We dont care about collisions. Nodes with
		 * the same key stay together.
		 */
		if (entity_before(se, entry)) {
			link = &parent->rb_left;
		} else {
			link = &parent->rb_right;
			leftmost = 0;
		}
	}

	/*
	 * Maintain a cache of leftmost tree entries (it is frequently
	 * used):
	 */
	if (leftmost)
		cfs_rq->rb_leftmost = &se->run_node;

	rb_link_node(&se->run_node, parent, link);
	rb_insert_color(&se->run_node, &cfs_rq->tasks_timeline);
}

static void __dequeue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	if (cfs_rq->rb_leftmost == &se->run_node) {
		struct rb_node *next_node;

		next_node = rb_next(&se->run_node);
		cfs_rq->rb_leftmost = next_node;
	}

	rb_erase(&se->run_node, &cfs_rq->tasks_timeline);
}

struct sched_entity *__pick_first_entity(struct cfs_rq *cfs_rq)
{
	struct rb_node *left = cfs_rq->rb_leftmost;

	if (!left)
		return NULL;

	return rb_entry(left, struct sched_entity, run_node);
}

static struct sched_entity *__pick_next_entity(struct sched_entity *se)
{
	struct rb_node *next = rb_next(&se->run_node);

	if (!next)
		return NULL;

	return rb_entry(next, struct sched_entity, run_node);
}

#ifdef CONFIG_SCHED_DEBUG
struct sched_entity *__pick_last_entity(struct cfs_rq *cfs_rq)
{
	struct rb_node *last = rb_last(&cfs_rq->tasks_timeline);

	if (!last)
		return NULL;

	return rb_entry(last, struct sched_entity, run_node);
}

/**************************************************************
 * Scheduling class statistics methods:
 */

int sched_proc_update_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp,
		loff_t *ppos)
{
	int ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);
	int factor = get_update_sysctl_factor();

	if (ret || !write)
		return ret;

	sched_nr_latency = DIV_ROUND_UP(sysctl_sched_latency,
					sysctl_sched_min_granularity);

#define WRT_SYSCTL(name) \
	(normalized_sysctl_##name = sysctl_##name / (factor))
	WRT_SYSCTL(sched_min_granularity);
	WRT_SYSCTL(sched_latency);
	WRT_SYSCTL(sched_wakeup_granularity);
#undef WRT_SYSCTL

	return 0;
}
#endif

/*
 * delta /= w
 */
static inline u64 calc_delta_fair(u64 delta, struct sched_entity *se)
{
	if (unlikely(se->load.weight != NICE_0_LOAD))
		delta = __calc_delta(delta, NICE_0_LOAD, &se->load);

	return delta;
}

/*
 * The idea is to set a period in which each task runs once.
 *
 * When there are too many tasks (sched_nr_latency) we have to stretch
 * this period because otherwise the slices get too small.
 *
 * p = (nr <= nl) ? l : l*nr/nl
 */
static u64 __sched_period(unsigned long nr_running)
{
	u64 period = sysctl_sched_latency;
	unsigned long nr_latency = sched_nr_latency;

	if (unlikely(nr_running > nr_latency)) {
		period = sysctl_sched_min_granularity;
		period *= nr_running;
	}

	return period;
}

/*
 * We calculate the wall-time slice from the period by taking a part
 * proportional to the weight.
 *
 * s = p*P[w/rw]
 */
static u64 sched_slice(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	u64 slice = __sched_period(cfs_rq->nr_running + !se->on_rq);

	for_each_sched_entity(se) {
		struct load_weight *load;
		struct load_weight lw;

		cfs_rq = cfs_rq_of(se);
		load = &cfs_rq->load;

		if (unlikely(!se->on_rq)) {
			lw = cfs_rq->load;

			update_load_add(&lw, se->load.weight);
			load = &lw;
		}
		slice = __calc_delta(slice, se->load.weight, load);
	}
	return slice;
}

/*
 * We calculate the vruntime slice of a to-be-inserted task.
 *
 * vs = s/w
 */
static u64 sched_vslice(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	return calc_delta_fair(sched_slice(cfs_rq, se), se);
}

#ifdef CONFIG_SMP
static int select_idle_sibling(struct task_struct *p, int cpu);
static unsigned long task_h_load(struct task_struct *p);

static inline void __update_task_entity_contrib(struct sched_entity *se);
static inline void __update_task_entity_utilization(struct sched_entity *se);

#ifdef CONFIG_SCHED_HMP
unsigned int init_task_load_period = 4000;
#else
unsigned int init_task_load_period = 0;
#endif

/* Give new task start runnable values to heavy its load in infant time */
void init_task_runnable_average(struct task_struct *p)
{
	u32 slice;

	p->se.avg.decay_count = 0;
	slice = sched_slice(task_cfs_rq(p), &p->se) >> 10;
	p->se.avg.runnable_avg_sum = p->se.avg.running_avg_sum = (init_task_load_period) ? 0 : slice;
	p->se.avg.avg_period = (init_task_load_period) ? (init_task_load_period) : slice;
	__update_task_entity_contrib(&p->se);
	__update_task_entity_utilization(&p->se);
	/* sched: add trace_sched */
	trace_sched_task_entity_avg(0, p, &p->se.avg);
}
#else
void init_task_runnable_average(struct task_struct *p)
{
}
#endif

/*
 * Update the current task's runtime statistics.
 */
static void update_curr(struct cfs_rq *cfs_rq)
{
	struct sched_entity *curr = cfs_rq->curr;
	u64 now = rq_clock_task(rq_of(cfs_rq));
	u64 delta_exec;

	if (unlikely(!curr))
		return;

	delta_exec = now - curr->exec_start;
	if (unlikely((s64)delta_exec <= 0))
		return;

	curr->exec_start = now;

	schedstat_set(curr->statistics.exec_max,
		      max(delta_exec, curr->statistics.exec_max));

	curr->sum_exec_runtime += delta_exec;
	schedstat_add(cfs_rq, exec_clock, delta_exec);

	curr->vruntime += calc_delta_fair(delta_exec, curr);
	update_min_vruntime(cfs_rq);

	if (entity_is_task(curr)) {
		struct task_struct *curtask = task_of(curr);

		trace_sched_stat_runtime(curtask, delta_exec, curr->vruntime);
		cpuacct_charge(curtask, delta_exec);
		account_group_exec_runtime(curtask, delta_exec);
	}

	account_cfs_rq_runtime(cfs_rq, delta_exec);
}

static void update_curr_fair(struct rq *rq)
{
	update_curr(cfs_rq_of(&rq->curr->se));
}

static inline void
update_stats_wait_start(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	schedstat_set(se->statistics.wait_start, rq_clock(rq_of(cfs_rq)));
}

/*
 * Task is being enqueued - update stats:
 */
static void update_stats_enqueue(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	/*
	 * Are we enqueueing a waiting task? (for current tasks
	 * a dequeue/enqueue event is a NOP)
	 */
	if (se != cfs_rq->curr)
		update_stats_wait_start(cfs_rq, se);
}

static void
update_stats_wait_end(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	schedstat_set(se->statistics.wait_max, max(se->statistics.wait_max,
			rq_clock(rq_of(cfs_rq)) - se->statistics.wait_start));
	schedstat_set(se->statistics.wait_count, se->statistics.wait_count + 1);
	schedstat_set(se->statistics.wait_sum, se->statistics.wait_sum +
			rq_clock(rq_of(cfs_rq)) - se->statistics.wait_start);
#ifdef CONFIG_SCHEDSTATS
	if (entity_is_task(se)) {
		trace_sched_stat_wait(task_of(se),
			rq_clock(rq_of(cfs_rq)) - se->statistics.wait_start);
	}
#endif
	schedstat_set(se->statistics.wait_start, 0);
}

static inline void
update_stats_dequeue(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	/*
	 * Mark the end of the wait period if dequeueing a
	 * waiting task:
	 */
	if (se != cfs_rq->curr)
		update_stats_wait_end(cfs_rq, se);
}

/*
 * We are picking a new current task - update its stats:
 */
static inline void
update_stats_curr_start(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	/*
	 * We are starting a new run period:
	 */
	se->exec_start = rq_clock_task(rq_of(cfs_rq));
}

/**************************************************
 * Scheduling class queueing methods:
 */

#ifdef CONFIG_NUMA_BALANCING
/*
 * Approximate time to scan a full NUMA task in ms. The task scan period is
 * calculated based on the tasks virtual memory size and
 * numa_balancing_scan_size.
 */
unsigned int sysctl_numa_balancing_scan_period_min = 1000;
unsigned int sysctl_numa_balancing_scan_period_max = 60000;

/* Portion of address space to scan in MB */
unsigned int sysctl_numa_balancing_scan_size = 256;

/* Scan @scan_size MB every @scan_period after an initial @scan_delay in ms */
unsigned int sysctl_numa_balancing_scan_delay = 1000;

static unsigned int task_nr_scan_windows(struct task_struct *p)
{
	unsigned long rss = 0;
	unsigned long nr_scan_pages;

	/*
	 * Calculations based on RSS as non-present and empty pages are skipped
	 * by the PTE scanner and NUMA hinting faults should be trapped based
	 * on resident pages
	 */
	nr_scan_pages = sysctl_numa_balancing_scan_size << (20 - PAGE_SHIFT);
	rss = get_mm_rss(p->mm);
	if (!rss)
		rss = nr_scan_pages;

	rss = round_up(rss, nr_scan_pages);
	return rss / nr_scan_pages;
}

/* For sanitys sake, never scan more PTEs than MAX_SCAN_WINDOW MB/sec. */
#define MAX_SCAN_WINDOW 2560

static unsigned int task_scan_min(struct task_struct *p)
{
	unsigned int scan_size = ACCESS_ONCE(sysctl_numa_balancing_scan_size);
	unsigned int scan, floor;
	unsigned int windows = 1;

	if (scan_size < MAX_SCAN_WINDOW)
		windows = MAX_SCAN_WINDOW / scan_size;
	floor = 1000 / windows;

	scan = sysctl_numa_balancing_scan_period_min / task_nr_scan_windows(p);
	return max_t(unsigned int, floor, scan);
}

static unsigned int task_scan_max(struct task_struct *p)
{
	unsigned int smin = task_scan_min(p);
	unsigned int smax;

	/* Watch for min being lower than max due to floor calculations */
	smax = sysctl_numa_balancing_scan_period_max / task_nr_scan_windows(p);
	return max(smin, smax);
}

static void account_numa_enqueue(struct rq *rq, struct task_struct *p)
{
	rq->nr_numa_running += (p->numa_preferred_nid != -1);
	rq->nr_preferred_running += (p->numa_preferred_nid == task_node(p));
}

static void account_numa_dequeue(struct rq *rq, struct task_struct *p)
{
	rq->nr_numa_running -= (p->numa_preferred_nid != -1);
	rq->nr_preferred_running -= (p->numa_preferred_nid == task_node(p));
}

struct numa_group {
	atomic_t refcount;

	spinlock_t lock; /* nr_tasks, tasks */
	int nr_tasks;
	pid_t gid;
	struct list_head task_list;

	struct rcu_head rcu;
	nodemask_t active_nodes;
	unsigned long total_faults;
	/*
	 * Faults_cpu is used to decide whether memory should move
	 * towards the CPU. As a consequence, these stats are weighted
	 * more by CPU use than by memory faults.
	 */
	unsigned long *faults_cpu;
	unsigned long faults[0];
};

/* Shared or private faults. */
#define NR_NUMA_HINT_FAULT_TYPES 2

/* Memory and CPU locality */
#define NR_NUMA_HINT_FAULT_STATS (NR_NUMA_HINT_FAULT_TYPES * 2)

/* Averaged statistics, and temporary buffers. */
#define NR_NUMA_HINT_FAULT_BUCKETS (NR_NUMA_HINT_FAULT_STATS * 2)

pid_t task_numa_group_id(struct task_struct *p)
{
	return p->numa_group ? p->numa_group->gid : 0;
}

static inline int task_faults_idx(int nid, int priv)
{
	return NR_NUMA_HINT_FAULT_TYPES * nid + priv;
}

static inline unsigned long task_faults(struct task_struct *p, int nid)
{
	if (!p->numa_faults_memory)
		return 0;

	return p->numa_faults_memory[task_faults_idx(nid, 0)] +
		p->numa_faults_memory[task_faults_idx(nid, 1)];
}

static inline unsigned long group_faults(struct task_struct *p, int nid)
{
	if (!p->numa_group)
		return 0;

	return p->numa_group->faults[task_faults_idx(nid, 0)] +
		p->numa_group->faults[task_faults_idx(nid, 1)];
}

static inline unsigned long group_faults_cpu(struct numa_group *group, int nid)
{
	return group->faults_cpu[task_faults_idx(nid, 0)] +
		group->faults_cpu[task_faults_idx(nid, 1)];
}

/*
 * These return the fraction of accesses done by a particular task, or
 * task group, on a particular numa node.  The group weight is given a
 * larger multiplier, in order to group tasks together that are almost
 * evenly spread out between numa nodes.
 */
static inline unsigned long task_weight(struct task_struct *p, int nid)
{
	unsigned long total_faults;

	if (!p->numa_faults_memory)
		return 0;

	total_faults = p->total_numa_faults;

	if (!total_faults)
		return 0;

	return 1000 * task_faults(p, nid) / total_faults;
}

static inline unsigned long group_weight(struct task_struct *p, int nid)
{
	if (!p->numa_group || !p->numa_group->total_faults)
		return 0;

	return 1000 * group_faults(p, nid) / p->numa_group->total_faults;
}

bool should_numa_migrate_memory(struct task_struct *p, struct page * page,
				int src_nid, int dst_cpu)
{
	struct numa_group *ng = p->numa_group;
	int dst_nid = cpu_to_node(dst_cpu);
	int last_cpupid, this_cpupid;

	this_cpupid = cpu_pid_to_cpupid(dst_cpu, current->pid);

	/*
	 * Multi-stage node selection is used in conjunction with a periodic
	 * migration fault to build a temporal task<->page relation. By using
	 * a two-stage filter we remove short/unlikely relations.
	 *
	 * Using P(p) ~ n_p / n_t as per frequentist probability, we can equate
	 * a task's usage of a particular page (n_p) per total usage of this
	 * page (n_t) (in a given time-span) to a probability.
	 *
	 * Our periodic faults will sample this probability and getting the
	 * same result twice in a row, given these samples are fully
	 * independent, is then given by P(n)^2, provided our sample period
	 * is sufficiently short compared to the usage pattern.
	 *
	 * This quadric squishes small probabilities, making it less likely we
	 * act on an unlikely task<->page relation.
	 */
	last_cpupid = page_cpupid_xchg_last(page, this_cpupid);
	if (!cpupid_pid_unset(last_cpupid) &&
				cpupid_to_nid(last_cpupid) != dst_nid)
		return false;

	/* Always allow migrate on private faults */
	if (cpupid_match_pid(p, last_cpupid))
		return true;

	/* A shared fault, but p->numa_group has not been set up yet. */
	if (!ng)
		return true;

	/*
	 * Do not migrate if the destination is not a node that
	 * is actively used by this numa group.
	 */
	if (!node_isset(dst_nid, ng->active_nodes))
		return false;

	/*
	 * Source is a node that is not actively used by this
	 * numa group, while the destination is. Migrate.
	 */
	if (!node_isset(src_nid, ng->active_nodes))
		return true;

	/*
	 * Both source and destination are nodes in active
	 * use by this numa group. Maximize memory bandwidth
	 * by migrating from more heavily used groups, to less
	 * heavily used ones, spreading the load around.
	 * Use a 1/4 hysteresis to avoid spurious page movement.
	 */
	return group_faults(p, dst_nid) < (group_faults(p, src_nid) * 3 / 4);
}

static unsigned long weighted_cpuload(const int cpu);
static unsigned long source_load(int cpu, int type);
static unsigned long target_load(int cpu, int type);
static unsigned long capacity_of(int cpu);
static long effective_load(struct task_group *tg, int cpu, long wl, long wg);

/* Cached statistics for all CPUs within a node */
struct numa_stats {
	unsigned long nr_running;
	unsigned long load;

	/* Total compute capacity of CPUs on a node */
	unsigned long compute_capacity;

	/* Approximate capacity in terms of runnable tasks on a node */
	unsigned long task_capacity;
	int has_free_capacity;
};

/*
 * XXX borrowed from update_sg_lb_stats
 */
static void update_numa_stats(struct numa_stats *ns, int nid)
{
	int smt, cpu, cpus = 0;
	unsigned long capacity;

	memset(ns, 0, sizeof(*ns));
	for_each_cpu(cpu, cpumask_of_node(nid)) {
		struct rq *rq = cpu_rq(cpu);

		ns->nr_running += rq->nr_running;
		ns->load += weighted_cpuload(cpu);
		ns->compute_capacity += capacity_of(cpu);

		cpus++;
	}

	/*
	 * If we raced with hotplug and there are no CPUs left in our mask
	 * the @ns structure is NULL'ed and task_numa_compare() will
	 * not find this node attractive.
	 *
	 * We'll either bail at !has_free_capacity, or we'll detect a huge
	 * imbalance and bail there.
	 */
	if (!cpus)
		return;

	/* smt := ceil(cpus / capacity), assumes: 1 < smt_power < 2 */
	smt = DIV_ROUND_UP(SCHED_CAPACITY_SCALE * cpus, ns->compute_capacity);
	capacity = cpus / smt; /* cores */

	ns->task_capacity = min_t(unsigned, capacity,
		DIV_ROUND_CLOSEST(ns->compute_capacity, SCHED_CAPACITY_SCALE));
	ns->has_free_capacity = (ns->nr_running < ns->task_capacity);
}

struct task_numa_env {
	struct task_struct *p;

	int src_cpu, src_nid;
	int dst_cpu, dst_nid;

	struct numa_stats src_stats, dst_stats;

	int imbalance_pct;

	struct task_struct *best_task;
	long best_imp;
	int best_cpu;
};

static void task_numa_assign(struct task_numa_env *env,
			     struct task_struct *p, long imp)
{
	if (env->best_task)
		put_task_struct(env->best_task);
	if (p)
		get_task_struct(p);

	env->best_task = p;
	env->best_imp = imp;
	env->best_cpu = env->dst_cpu;
}

static bool load_too_imbalanced(long src_load, long dst_load,
				struct task_numa_env *env)
{
	long imb, old_imb;
	long orig_src_load, orig_dst_load;
	long src_capacity, dst_capacity;

	/*
	 * The load is corrected for the CPU capacity available on each node.
	 *
	 * src_load        dst_load
	 * ------------ vs ---------
	 * src_capacity    dst_capacity
	 */
	src_capacity = env->src_stats.compute_capacity;
	dst_capacity = env->dst_stats.compute_capacity;

	/* We care about the slope of the imbalance, not the direction. */
	if (dst_load < src_load)
		swap(dst_load, src_load);

	/* Is the difference below the threshold? */
	imb = dst_load * src_capacity * 100 -
	      src_load * dst_capacity * env->imbalance_pct;
	if (imb <= 0)
		return false;

	/*
	 * The imbalance is above the allowed threshold.
	 * Compare it with the old imbalance.
	 */
	orig_src_load = env->src_stats.load;
	orig_dst_load = env->dst_stats.load;

	if (orig_dst_load < orig_src_load)
		swap(orig_dst_load, orig_src_load);

	old_imb = orig_dst_load * src_capacity * 100 -
		  orig_src_load * dst_capacity * env->imbalance_pct;

	/* Would this change make things worse? */
	return (imb > old_imb);
}

/*
 * This checks if the overall compute and NUMA accesses of the system would
 * be improved if the source tasks was migrated to the target dst_cpu taking
 * into account that it might be best if task running on the dst_cpu should
 * be exchanged with the source task
 */
static void task_numa_compare(struct task_numa_env *env,
			      long taskimp, long groupimp)
{
	struct rq *src_rq = cpu_rq(env->src_cpu);
	struct rq *dst_rq = cpu_rq(env->dst_cpu);
	struct task_struct *cur;
	long src_load, dst_load;
	long load;
	long imp = env->p->numa_group ? groupimp : taskimp;
	long moveimp = imp;

	rcu_read_lock();

	raw_spin_lock_irq(&dst_rq->lock);
	cur = dst_rq->curr;
	/*
	 * No need to move the exiting task, and this ensures that ->curr
	 * wasn't reaped and thus get_task_struct() in task_numa_assign()
	 * is safe under RCU read lock.
	 * Note that rcu_read_lock() itself can't protect from the final
	 * put_task_struct() after the last schedule().
	 */
	if ((cur->flags & PF_EXITING) || is_idle_task(cur))
		cur = NULL;
	raw_spin_unlock_irq(&dst_rq->lock);

	/*
	 * Because we have preemption enabled we can get migrated around and
	 * end try selecting ourselves (current == env->p) as a swap candidate.
	 */
	if (cur == env->p)
		goto unlock;

	/*
	 * "imp" is the fault differential for the source task between the
	 * source and destination node. Calculate the total differential for
	 * the source task and potential destination task. The more negative
	 * the value is, the more rmeote accesses that would be expected to
	 * be incurred if the tasks were swapped.
	 */
	if (cur) {
		/* Skip this swap candidate if cannot move to the source cpu */
		if (!cpumask_test_cpu(env->src_cpu, tsk_cpus_allowed(cur)))
			goto unlock;

		/*
		 * If dst and source tasks are in the same NUMA group, or not
		 * in any group then look only at task weights.
		 */
		if (cur->numa_group == env->p->numa_group) {
			imp = taskimp + task_weight(cur, env->src_nid) -
			      task_weight(cur, env->dst_nid);
			/*
			 * Add some hysteresis to prevent swapping the
			 * tasks within a group over tiny differences.
			 */
			if (cur->numa_group)
				imp -= imp/16;
		} else {
			/*
			 * Compare the group weights. If a task is all by
			 * itself (not part of a group), use the task weight
			 * instead.
			 */
			if (cur->numa_group)
				imp += group_weight(cur, env->src_nid) -
				       group_weight(cur, env->dst_nid);
			else
				imp += task_weight(cur, env->src_nid) -
				       task_weight(cur, env->dst_nid);
		}
	}

	if (imp <= env->best_imp && moveimp <= env->best_imp)
		goto unlock;

	if (!cur) {
		/* Is there capacity at our destination? */
		if (env->src_stats.nr_running <= env->src_stats.task_capacity &&
		    !env->dst_stats.has_free_capacity)
			goto unlock;

		goto balance;
	}

	/* Balance doesn't matter much if we're running a task per cpu */
	if (imp > env->best_imp && src_rq->nr_running == 1 &&
			dst_rq->nr_running == 1)
		goto assign;

	/*
	 * In the overloaded case, try and keep the load balanced.
	 */
balance:
	load = task_h_load(env->p);
	dst_load = env->dst_stats.load + load;
	src_load = env->src_stats.load - load;

	if (moveimp > imp && moveimp > env->best_imp) {
		/*
		 * If the improvement from just moving env->p direction is
		 * better than swapping tasks around, check if a move is
		 * possible. Store a slightly smaller score than moveimp,
		 * so an actually idle CPU will win.
		 */
		if (!load_too_imbalanced(src_load, dst_load, env)) {
			imp = moveimp - 1;
			cur = NULL;
			goto assign;
		}
	}

	if (imp <= env->best_imp)
		goto unlock;

	if (cur) {
		load = task_h_load(cur);
		dst_load -= load;
		src_load += load;
	}

	if (load_too_imbalanced(src_load, dst_load, env))
		goto unlock;

	/*
	 * One idle CPU per node is evaluated for a task numa move.
	 * Call select_idle_sibling to maybe find a better one.
	 */
	if (!cur)
		env->dst_cpu = select_idle_sibling(env->p, env->dst_cpu);

assign:
	task_numa_assign(env, cur, imp);
unlock:
	rcu_read_unlock();
}

static void task_numa_find_cpu(struct task_numa_env *env,
				long taskimp, long groupimp)
{
	int cpu;

	for_each_cpu(cpu, cpumask_of_node(env->dst_nid)) {
		/* Skip this CPU if the source task cannot migrate */
		if (!cpumask_test_cpu(cpu, tsk_cpus_allowed(env->p)))
			continue;

		env->dst_cpu = cpu;
		task_numa_compare(env, taskimp, groupimp);
	}
}

static int task_numa_migrate(struct task_struct *p)
{
	struct task_numa_env env = {
		.p = p,

		.src_cpu = task_cpu(p),
		.src_nid = task_node(p),

		.imbalance_pct = 112,

		.best_task = NULL,
		.best_imp = 0,
		.best_cpu = -1
	};
	struct sched_domain *sd;
	unsigned long taskweight, groupweight;
	int nid, ret;
	long taskimp, groupimp;

	/*
	 * Pick the lowest SD_NUMA domain, as that would have the smallest
	 * imbalance and would be the first to start moving tasks about.
	 *
	 * And we want to avoid any moving of tasks about, as that would create
	 * random movement of tasks -- counter the numa conditions we're trying
	 * to satisfy here.
	 */
	rcu_read_lock();
	sd = rcu_dereference(per_cpu(sd_numa, env.src_cpu));
	if (sd)
		env.imbalance_pct = 100 + (sd->imbalance_pct - 100) / 2;
	rcu_read_unlock();

	/*
	 * Cpusets can break the scheduler domain tree into smaller
	 * balance domains, some of which do not cross NUMA boundaries.
	 * Tasks that are "trapped" in such domains cannot be migrated
	 * elsewhere, so there is no point in (re)trying.
	 */
	if (unlikely(!sd)) {
		p->numa_preferred_nid = task_node(p);
		return -EINVAL;
	}

	taskweight = task_weight(p, env.src_nid);
	groupweight = group_weight(p, env.src_nid);
	update_numa_stats(&env.src_stats, env.src_nid);
	env.dst_nid = p->numa_preferred_nid;
	taskimp = task_weight(p, env.dst_nid) - taskweight;
	groupimp = group_weight(p, env.dst_nid) - groupweight;
	update_numa_stats(&env.dst_stats, env.dst_nid);

	/* Try to find a spot on the preferred nid. */
	task_numa_find_cpu(&env, taskimp, groupimp);

	/* No space available on the preferred nid. Look elsewhere. */
	if (env.best_cpu == -1) {
		for_each_online_node(nid) {
			if (nid == env.src_nid || nid == p->numa_preferred_nid)
				continue;

			/* Only consider nodes where both task and groups benefit */
			taskimp = task_weight(p, nid) - taskweight;
			groupimp = group_weight(p, nid) - groupweight;
			if (taskimp < 0 && groupimp < 0)
				continue;

			env.dst_nid = nid;
			update_numa_stats(&env.dst_stats, env.dst_nid);
			task_numa_find_cpu(&env, taskimp, groupimp);
		}
	}

	/*
	 * If the task is part of a workload that spans multiple NUMA nodes,
	 * and is migrating into one of the workload's active nodes, remember
	 * this node as the task's preferred numa node, so the workload can
	 * settle down.
	 * A task that migrated to a second choice node will be better off
	 * trying for a better one later. Do not set the preferred node here.
	 */
	if (p->numa_group) {
		if (env.best_cpu == -1)
			nid = env.src_nid;
		else
			nid = env.dst_nid;

		if (node_isset(nid, p->numa_group->active_nodes))
			sched_setnuma(p, env.dst_nid);
	}

	/* No better CPU than the current one was found. */
	if (env.best_cpu == -1)
		return -EAGAIN;

	/*
	 * Reset the scan period if the task is being rescheduled on an
	 * alternative node to recheck if the tasks is now properly placed.
	 */
	p->numa_scan_period = task_scan_min(p);

	if (env.best_task == NULL) {
		ret = migrate_task_to(p, env.best_cpu);
		if (ret != 0)
			trace_sched_stick_numa(p, env.src_cpu, env.best_cpu);
		return ret;
	}

	ret = migrate_swap(p, env.best_task);
	if (ret != 0)
		trace_sched_stick_numa(p, env.src_cpu, task_cpu(env.best_task));
	put_task_struct(env.best_task);
	return ret;
}

/* Attempt to migrate a task to a CPU on the preferred node. */
static void numa_migrate_preferred(struct task_struct *p)
{
	unsigned long interval = HZ;

	/* This task has no NUMA fault statistics yet */
	if (unlikely(p->numa_preferred_nid == -1 || !p->numa_faults_memory))
		return;

	/* Periodically retry migrating the task to the preferred node */
	interval = min(interval, msecs_to_jiffies(p->numa_scan_period) / 16);
	p->numa_migrate_retry = jiffies + interval;

	/* Success if task is already running on preferred CPU */
	if (task_node(p) == p->numa_preferred_nid)
		return;

	/* Otherwise, try migrate to a CPU on the preferred node */
	task_numa_migrate(p);
}

/*
 * Find the nodes on which the workload is actively running. We do this by
 * tracking the nodes from which NUMA hinting faults are triggered. This can
 * be different from the set of nodes where the workload's memory is currently
 * located.
 *
 * The bitmask is used to make smarter decisions on when to do NUMA page
 * migrations, To prevent flip-flopping, and excessive page migrations, nodes
 * are added when they cause over 6/16 of the maximum number of faults, but
 * only removed when they drop below 3/16.
 */
static void update_numa_active_node_mask(struct numa_group *numa_group)
{
	unsigned long faults, max_faults = 0;
	int nid;

	for_each_online_node(nid) {
		faults = group_faults_cpu(numa_group, nid);
		if (faults > max_faults)
			max_faults = faults;
	}

	for_each_online_node(nid) {
		faults = group_faults_cpu(numa_group, nid);
		if (!node_isset(nid, numa_group->active_nodes)) {
			if (faults > max_faults * 6 / 16)
				node_set(nid, numa_group->active_nodes);
		} else if (faults < max_faults * 3 / 16)
			node_clear(nid, numa_group->active_nodes);
	}
}

/*
 * When adapting the scan rate, the period is divided into NUMA_PERIOD_SLOTS
 * increments. The more local the fault statistics are, the higher the scan
 * period will be for the next scan window. If local/(local+remote) ratio is
 * below NUMA_PERIOD_THRESHOLD (where range of ratio is 1..NUMA_PERIOD_SLOTS)
 * the scan period will decrease. Aim for 70% local accesses.
 */
#define NUMA_PERIOD_SLOTS 10
#define NUMA_PERIOD_THRESHOLD 7

/*
 * Increase the scan period (slow down scanning) if the majority of
 * our memory is already on our local node, or if the majority of
 * the page accesses are shared with other processes.
 * Otherwise, decrease the scan period.
 */
static void update_task_scan_period(struct task_struct *p,
			unsigned long shared, unsigned long private)
{
	unsigned int period_slot;
	int ratio;
	int diff;

	unsigned long remote = p->numa_faults_locality[0];
	unsigned long local = p->numa_faults_locality[1];

	/*
	 * If there were no record hinting faults then either the task is
	 * completely idle or all activity is areas that are not of interest
	 * to automatic numa balancing. Scan slower
	 */
	if (local + shared == 0) {
		p->numa_scan_period = min(p->numa_scan_period_max,
			p->numa_scan_period << 1);

		p->mm->numa_next_scan = jiffies +
			msecs_to_jiffies(p->numa_scan_period);

		return;
	}

	/*
	 * Prepare to scale scan period relative to the current period.
	 *	 == NUMA_PERIOD_THRESHOLD scan period stays the same
	 *       <  NUMA_PERIOD_THRESHOLD scan period decreases (scan faster)
	 *	 >= NUMA_PERIOD_THRESHOLD scan period increases (scan slower)
	 */
	period_slot = DIV_ROUND_UP(p->numa_scan_period, NUMA_PERIOD_SLOTS);
	ratio = (local * NUMA_PERIOD_SLOTS) / (local + remote);
	if (ratio >= NUMA_PERIOD_THRESHOLD) {
		int slot = ratio - NUMA_PERIOD_THRESHOLD;
		if (!slot)
			slot = 1;
		diff = slot * period_slot;
	} else {
		diff = -(NUMA_PERIOD_THRESHOLD - ratio) * period_slot;

		/*
		 * Scale scan rate increases based on sharing. There is an
		 * inverse relationship between the degree of sharing and
		 * the adjustment made to the scanning period. Broadly
		 * speaking the intent is that there is little point
		 * scanning faster if shared accesses dominate as it may
		 * simply bounce migrations uselessly
		 */
		ratio = DIV_ROUND_UP(private * NUMA_PERIOD_SLOTS, (private + shared + 1));
		diff = (diff * ratio) / NUMA_PERIOD_SLOTS;
	}

	p->numa_scan_period = clamp(p->numa_scan_period + diff,
			task_scan_min(p), task_scan_max(p));
	memset(p->numa_faults_locality, 0, sizeof(p->numa_faults_locality));
}

/*
 * Get the fraction of time the task has been running since the last
 * NUMA placement cycle. The scheduler keeps similar statistics, but
 * decays those on a 32ms period, which is orders of magnitude off
 * from the dozens-of-seconds NUMA balancing period. Use the scheduler
 * stats only if the task is so new there are no NUMA statistics yet.
 */
static u64 numa_get_avg_runtime(struct task_struct *p, u64 *period)
{
	u64 runtime, delta, now;
	/* Use the start of this time slice to avoid calculations. */
	now = p->se.exec_start;
	runtime = p->se.sum_exec_runtime;

	if (p->last_task_numa_placement) {
		delta = runtime - p->last_sum_exec_runtime;
		*period = now - p->last_task_numa_placement;
	} else {
		delta = p->se.avg.runnable_avg_sum;
		*period = p->se.avg.avg_period;
	}

	p->last_sum_exec_runtime = runtime;
	p->last_task_numa_placement = now;

	return delta;
}

static void task_numa_placement(struct task_struct *p)
{
	int seq, nid, max_nid = -1, max_group_nid = -1;
	unsigned long max_faults = 0, max_group_faults = 0;
	unsigned long fault_types[2] = { 0, 0 };
	unsigned long total_faults;
	u64 runtime, period;
	spinlock_t *group_lock = NULL;

	seq = ACCESS_ONCE(p->mm->numa_scan_seq);
	if (p->numa_scan_seq == seq)
		return;
	p->numa_scan_seq = seq;
	p->numa_scan_period_max = task_scan_max(p);

	total_faults = p->numa_faults_locality[0] +
		       p->numa_faults_locality[1];
	runtime = numa_get_avg_runtime(p, &period);

	/* If the task is part of a group prevent parallel updates to group stats */
	if (p->numa_group) {
		group_lock = &p->numa_group->lock;
		spin_lock_irq(group_lock);
	}

	/* Find the node with the highest number of faults */
	for_each_online_node(nid) {
		unsigned long faults = 0, group_faults = 0;
		int priv, i;

		for (priv = 0; priv < NR_NUMA_HINT_FAULT_TYPES; priv++) {
			long diff, f_diff, f_weight;

			i = task_faults_idx(nid, priv);

			/* Decay existing window, copy faults since last scan */
			diff = p->numa_faults_buffer_memory[i] - p->numa_faults_memory[i] / 2;
			fault_types[priv] += p->numa_faults_buffer_memory[i];
			p->numa_faults_buffer_memory[i] = 0;

			/*
			 * Normalize the faults_from, so all tasks in a group
			 * count according to CPU use, instead of by the raw
			 * number of faults. Tasks with little runtime have
			 * little over-all impact on throughput, and thus their
			 * faults are less important.
			 */
			f_weight = div64_u64(runtime << 16, period + 1);
			f_weight = (f_weight * p->numa_faults_buffer_cpu[i]) /
				   (total_faults + 1);
			f_diff = f_weight - p->numa_faults_cpu[i] / 2;
			p->numa_faults_buffer_cpu[i] = 0;

			p->numa_faults_memory[i] += diff;
			p->numa_faults_cpu[i] += f_diff;
			faults += p->numa_faults_memory[i];
			p->total_numa_faults += diff;
			if (p->numa_group) {
				/* safe because we can only change our own group */
				p->numa_group->faults[i] += diff;
				p->numa_group->faults_cpu[i] += f_diff;
				p->numa_group->total_faults += diff;
				group_faults += p->numa_group->faults[i];
			}
		}

		if (faults > max_faults) {
			max_faults = faults;
			max_nid = nid;
		}

		if (group_faults > max_group_faults) {
			max_group_faults = group_faults;
			max_group_nid = nid;
		}
	}

	update_task_scan_period(p, fault_types[0], fault_types[1]);

	if (p->numa_group) {
		update_numa_active_node_mask(p->numa_group);
		spin_unlock_irq(group_lock);
		max_nid = max_group_nid;
	}

	if (max_faults) {
		/* Set the new preferred node */
		if (max_nid != p->numa_preferred_nid)
			sched_setnuma(p, max_nid);

		if (task_node(p) != p->numa_preferred_nid)
			numa_migrate_preferred(p);
	}
}

static inline int get_numa_group(struct numa_group *grp)
{
	return atomic_inc_not_zero(&grp->refcount);
}

static inline void put_numa_group(struct numa_group *grp)
{
	if (atomic_dec_and_test(&grp->refcount))
		kfree_rcu(grp, rcu);
}

static void task_numa_group(struct task_struct *p, int cpupid, int flags,
			int *priv)
{
	struct numa_group *grp, *my_grp;
	struct task_struct *tsk;
	bool join = false;
	int cpu = cpupid_to_cpu(cpupid);
	int i;

	if (unlikely(!p->numa_group)) {
		unsigned int size = sizeof(struct numa_group) +
				    4*nr_node_ids*sizeof(unsigned long);

		grp = kzalloc(size, GFP_KERNEL | __GFP_NOWARN);
		if (!grp)
			return;

		atomic_set(&grp->refcount, 1);
		spin_lock_init(&grp->lock);
		INIT_LIST_HEAD(&grp->task_list);
		grp->gid = p->pid;
		/* Second half of the array tracks nids where faults happen */
		grp->faults_cpu = grp->faults + NR_NUMA_HINT_FAULT_TYPES *
						nr_node_ids;

		node_set(task_node(current), grp->active_nodes);

		for (i = 0; i < NR_NUMA_HINT_FAULT_STATS * nr_node_ids; i++)
			grp->faults[i] = p->numa_faults_memory[i];

		grp->total_faults = p->total_numa_faults;

		list_add(&p->numa_entry, &grp->task_list);
		grp->nr_tasks++;
		rcu_assign_pointer(p->numa_group, grp);
	}

	rcu_read_lock();
	tsk = ACCESS_ONCE(cpu_rq(cpu)->curr);

	if (!cpupid_match_pid(tsk, cpupid))
		goto no_join;

	grp = rcu_dereference(tsk->numa_group);
	if (!grp)
		goto no_join;

	my_grp = p->numa_group;
	if (grp == my_grp)
		goto no_join;

	/*
	 * Only join the other group if its bigger; if we're the bigger group,
	 * the other task will join us.
	 */
	if (my_grp->nr_tasks > grp->nr_tasks)
		goto no_join;

	/*
	 * Tie-break on the grp address.
	 */
	if (my_grp->nr_tasks == grp->nr_tasks && my_grp > grp)
		goto no_join;

	/* Always join threads in the same process. */
	if (tsk->mm == current->mm)
		join = true;

	/* Simple filter to avoid false positives due to PID collisions */
	if (flags & TNF_SHARED)
		join = true;

	/* Update priv based on whether false sharing was detected */
	*priv = !join;

	if (join && !get_numa_group(grp))
		goto no_join;

	rcu_read_unlock();

	if (!join)
		return;

	BUG_ON(irqs_disabled());
	double_lock_irq(&my_grp->lock, &grp->lock);

	for (i = 0; i < NR_NUMA_HINT_FAULT_STATS * nr_node_ids; i++) {
		my_grp->faults[i] -= p->numa_faults_memory[i];
		grp->faults[i] += p->numa_faults_memory[i];
	}
	my_grp->total_faults -= p->total_numa_faults;
	grp->total_faults += p->total_numa_faults;

	list_move(&p->numa_entry, &grp->task_list);
	my_grp->nr_tasks--;
	grp->nr_tasks++;

	spin_unlock(&my_grp->lock);
	spin_unlock_irq(&grp->lock);

	rcu_assign_pointer(p->numa_group, grp);

	put_numa_group(my_grp);
	return;

no_join:
	rcu_read_unlock();
	return;
}

void task_numa_free(struct task_struct *p)
{
	struct numa_group *grp = p->numa_group;
	void *numa_faults = p->numa_faults_memory;
	unsigned long flags;
	int i;

	if (grp) {
		spin_lock_irqsave(&grp->lock, flags);
		for (i = 0; i < NR_NUMA_HINT_FAULT_STATS * nr_node_ids; i++)
			grp->faults[i] -= p->numa_faults_memory[i];
		grp->total_faults -= p->total_numa_faults;

		list_del(&p->numa_entry);
		grp->nr_tasks--;
		spin_unlock_irqrestore(&grp->lock, flags);
		RCU_INIT_POINTER(p->numa_group, NULL);
		put_numa_group(grp);
	}

	p->numa_faults_memory = NULL;
	p->numa_faults_buffer_memory = NULL;
	p->numa_faults_cpu = NULL;
	p->numa_faults_buffer_cpu = NULL;
	kfree(numa_faults);
}

/*
 * Got a PROT_NONE fault for a page on @node.
 */
void task_numa_fault(int last_cpupid, int mem_node, int pages, int flags)
{
	struct task_struct *p = current;
	bool migrated = flags & TNF_MIGRATED;
	int cpu_node = task_node(current);
	int local = !!(flags & TNF_FAULT_LOCAL);
	int priv;

	if (!numabalancing_enabled)
		return;

	/* for example, ksmd faulting in a user's mm */
	if (!p->mm)
		return;

	/* Allocate buffer to track faults on a per-node basis */
	if (unlikely(!p->numa_faults_memory)) {
		int size = sizeof(*p->numa_faults_memory) *
			   NR_NUMA_HINT_FAULT_BUCKETS * nr_node_ids;

		p->numa_faults_memory = kzalloc(size, GFP_KERNEL|__GFP_NOWARN);
		if (!p->numa_faults_memory)
			return;

		BUG_ON(p->numa_faults_buffer_memory);
		/*
		 * The averaged statistics, shared & private, memory & cpu,
		 * occupy the first half of the array. The second half of the
		 * array is for current counters, which are averaged into the
		 * first set by task_numa_placement.
		 */
		p->numa_faults_cpu = p->numa_faults_memory + (2 * nr_node_ids);
		p->numa_faults_buffer_memory = p->numa_faults_memory + (4 * nr_node_ids);
		p->numa_faults_buffer_cpu = p->numa_faults_memory + (6 * nr_node_ids);
		p->total_numa_faults = 0;
		memset(p->numa_faults_locality, 0, sizeof(p->numa_faults_locality));
	}

	/*
	 * First accesses are treated as private, otherwise consider accesses
	 * to be private if the accessing pid has not changed
	 */
	if (unlikely(last_cpupid == (-1 & LAST_CPUPID_MASK))) {
		priv = 1;
	} else {
		priv = cpupid_match_pid(p, last_cpupid);
		if (!priv && !(flags & TNF_NO_GROUP))
			task_numa_group(p, last_cpupid, flags, &priv);
	}

	/*
	 * If a workload spans multiple NUMA nodes, a shared fault that
	 * occurs wholly within the set of nodes that the workload is
	 * actively using should be counted as local. This allows the
	 * scan rate to slow down when a workload has settled down.
	 */
	if (!priv && !local && p->numa_group &&
			node_isset(cpu_node, p->numa_group->active_nodes) &&
			node_isset(mem_node, p->numa_group->active_nodes))
		local = 1;

	task_numa_placement(p);

	/*
	 * Retry task to preferred node migration periodically, in case it
	 * case it previously failed, or the scheduler moved us.
	 */
	if (time_after(jiffies, p->numa_migrate_retry))
		numa_migrate_preferred(p);

	if (migrated)
		p->numa_pages_migrated += pages;

	p->numa_faults_buffer_memory[task_faults_idx(mem_node, priv)] += pages;
	p->numa_faults_buffer_cpu[task_faults_idx(cpu_node, priv)] += pages;
	p->numa_faults_locality[local] += pages;
}

static void reset_ptenuma_scan(struct task_struct *p)
{
	ACCESS_ONCE(p->mm->numa_scan_seq)++;
	p->mm->numa_scan_offset = 0;
}

/*
 * The expensive part of numa migration is done from task_work context.
 * Triggered from task_tick_numa().
 */
void task_numa_work(struct callback_head *work)
{
	unsigned long migrate, next_scan, now = jiffies;
	struct task_struct *p = current;
	struct mm_struct *mm = p->mm;
	struct vm_area_struct *vma;
	unsigned long start, end;
	unsigned long nr_pte_updates = 0;
	long pages;

	WARN_ON_ONCE(p != container_of(work, struct task_struct, numa_work));

	work->next = work; /* protect against double add */
	/*
	 * Who cares about NUMA placement when they're dying.
	 *
	 * NOTE: make sure not to dereference p->mm before this check,
	 * exit_task_work() happens _after_ exit_mm() so we could be called
	 * without p->mm even though we still had it when we enqueued this
	 * work.
	 */
	if (p->flags & PF_EXITING)
		return;

	if (!mm->numa_next_scan) {
		mm->numa_next_scan = now +
			msecs_to_jiffies(sysctl_numa_balancing_scan_delay);
	}

	/*
	 * Enforce maximal scan/migration frequency..
	 */
	migrate = mm->numa_next_scan;
	if (time_before(now, migrate))
		return;

	if (p->numa_scan_period == 0) {
		p->numa_scan_period_max = task_scan_max(p);
		p->numa_scan_period = task_scan_min(p);
	}

	next_scan = now + msecs_to_jiffies(p->numa_scan_period);
	if (cmpxchg(&mm->numa_next_scan, migrate, next_scan) != migrate)
		return;

	/*
	 * Delay this task enough that another task of this mm will likely win
	 * the next time around.
	 */
	p->node_stamp += 2 * TICK_NSEC;

	start = mm->numa_scan_offset;
	pages = sysctl_numa_balancing_scan_size;
	pages <<= 20 - PAGE_SHIFT; /* MB in pages */
	if (!pages)
		return;

	down_read(&mm->mmap_sem);
	vma = find_vma(mm, start);
	if (!vma) {
		reset_ptenuma_scan(p);
		start = 0;
		vma = mm->mmap;
	}
	for (; vma; vma = vma->vm_next) {
		if (!vma_migratable(vma) || !vma_policy_mof(vma) ||
		    is_vm_hugetlb_page(vma) || (vma->vm_flags & VM_MIXEDMAP)) {
			continue;
		}

		/*
		 * Shared library pages mapped by multiple processes are not
		 * migrated as it is expected they are cache replicated. Avoid
		 * hinting faults in read-only file-backed mappings or the vdso
		 * as migrating the pages will be of marginal benefit.
		 */
		if (!vma->vm_mm ||
		    (vma->vm_file && (vma->vm_flags & (VM_READ|VM_WRITE)) == (VM_READ)))
			continue;

		/*
		 * Skip inaccessible VMAs to avoid any confusion between
		 * PROT_NONE and NUMA hinting ptes
		 */
		if (!(vma->vm_flags & (VM_READ | VM_EXEC | VM_WRITE)))
			continue;

		do {
			start = max(start, vma->vm_start);
			end = ALIGN(start + (pages << PAGE_SHIFT), HPAGE_SIZE);
			end = min(end, vma->vm_end);
			nr_pte_updates += change_prot_numa(vma, start, end);

			/*
			 * Scan sysctl_numa_balancing_scan_size but ensure that
			 * at least one PTE is updated so that unused virtual
			 * address space is quickly skipped.
			 */
			if (nr_pte_updates)
				pages -= (end - start) >> PAGE_SHIFT;

			start = end;
			if (pages <= 0)
				goto out;

			cond_resched();
		} while (end != vma->vm_end);
	}

out:
	/*
	 * It is possible to reach the end of the VMA list but the last few
	 * VMAs are not guaranteed to the vma_migratable. If they are not, we
	 * would find the !migratable VMA on the next scan but not reset the
	 * scanner to the start so check it now.
	 */
	if (vma)
		mm->numa_scan_offset = start;
	else
		reset_ptenuma_scan(p);
	up_read(&mm->mmap_sem);
}

/*
 * Drive the periodic memory faults..
 */
void task_tick_numa(struct rq *rq, struct task_struct *curr)
{
	struct callback_head *work = &curr->numa_work;
	u64 period, now;

	/*
	 * We don't care about NUMA placement if we don't have memory.
	 */
	if (!curr->mm || (curr->flags & PF_EXITING) || work->next != work)
		return;

	/*
	 * Using runtime rather than walltime has the dual advantage that
	 * we (mostly) drive the selection from busy threads and that the
	 * task needs to have done some actual work before we bother with
	 * NUMA placement.
	 */
	now = curr->se.sum_exec_runtime;
	period = (u64)curr->numa_scan_period * NSEC_PER_MSEC;

	if (now - curr->node_stamp > period) {
		if (!curr->node_stamp)
			curr->numa_scan_period = task_scan_min(curr);
		curr->node_stamp += period;

		if (!time_before(jiffies, curr->mm->numa_next_scan)) {
			init_task_work(work, task_numa_work); /* TODO: move this into sched_fork() */
			task_work_add(curr, work, true);
		}
	}
}
#else
static void task_tick_numa(struct rq *rq, struct task_struct *curr)
{
}

static inline void account_numa_enqueue(struct rq *rq, struct task_struct *p)
{
}

static inline void account_numa_dequeue(struct rq *rq, struct task_struct *p)
{
}
#endif /* CONFIG_NUMA_BALANCING */

static void
account_entity_enqueue(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	update_load_add(&cfs_rq->load, se->load.weight);
	if (!parent_entity(se))
		update_load_add(&rq_of(cfs_rq)->load, se->load.weight);
#ifdef CONFIG_SMP
	if (entity_is_task(se)) {
		struct rq *rq = rq_of(cfs_rq);

		account_numa_enqueue(rq, task_of(se));
		list_add(&se->group_node, &rq->cfs_tasks);
	}
#endif
	cfs_rq->nr_running++;
}

static void
account_entity_dequeue(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	update_load_sub(&cfs_rq->load, se->load.weight);
	if (!parent_entity(se))
		update_load_sub(&rq_of(cfs_rq)->load, se->load.weight);
	if (entity_is_task(se)) {
		account_numa_dequeue(rq_of(cfs_rq), task_of(se));
		list_del_init(&se->group_node);
	}
	cfs_rq->nr_running--;
}

#ifdef CONFIG_FAIR_GROUP_SCHED
# ifdef CONFIG_SMP
static inline long calc_tg_weight(struct task_group *tg, struct cfs_rq *cfs_rq)
{
	long tg_weight;

	/*
	 * Use this CPU's actual weight instead of the last load_contribution
	 * to gain a more accurate current total weight. See
	 * update_cfs_rq_load_contribution().
	 */
	tg_weight = atomic_long_read(&tg->load_avg);
	tg_weight -= cfs_rq->tg_load_contrib;
	tg_weight += cfs_rq->load.weight;

	return tg_weight;
}

static long calc_cfs_shares(struct cfs_rq *cfs_rq, struct task_group *tg)
{
	long tg_weight, load, shares;

	tg_weight = calc_tg_weight(tg, cfs_rq);
	load = cfs_rq->load.weight;

	shares = (tg->shares * load);
	if (tg_weight)
		shares /= tg_weight;

	if (shares < MIN_SHARES)
		shares = MIN_SHARES;
	if (shares > tg->shares)
		shares = tg->shares;

	return shares;
}
# else /* CONFIG_SMP */
static inline long calc_cfs_shares(struct cfs_rq *cfs_rq, struct task_group *tg)
{
	return tg->shares;
}
# endif /* CONFIG_SMP */
static void reweight_entity(struct cfs_rq *cfs_rq, struct sched_entity *se,
			    unsigned long weight)
{
	if (se->on_rq) {
		/* commit outstanding execution time */
		if (cfs_rq->curr == se)
			update_curr(cfs_rq);
		account_entity_dequeue(cfs_rq, se);
	}

	update_load_set(&se->load, weight);

	if (se->on_rq)
		account_entity_enqueue(cfs_rq, se);
}

static inline int throttled_hierarchy(struct cfs_rq *cfs_rq);

static void update_cfs_shares(struct cfs_rq *cfs_rq)
{
	struct task_group *tg;
	struct sched_entity *se;
	long shares;

	tg = cfs_rq->tg;
	se = tg->se[cpu_of(rq_of(cfs_rq))];
	if (!se || throttled_hierarchy(cfs_rq))
		return;
#ifndef CONFIG_SMP
	if (likely(se->load.weight == tg->shares))
		return;
#endif
	shares = calc_cfs_shares(cfs_rq, tg);

	reweight_entity(cfs_rq_of(se), se, shares);
}
#else /* CONFIG_FAIR_GROUP_SCHED */
static inline void update_cfs_shares(struct cfs_rq *cfs_rq)
{
}
#endif /* CONFIG_FAIR_GROUP_SCHED */

#ifdef CONFIG_SMP

#ifdef CONFIG_MTK_SCHED_CMP
void get_cluster_cpus(struct cpumask *cpus, int cluster_id,
			    bool exclusive_offline)
{
	struct cpumask cls_cpus;

	arch_get_cluster_cpus(&cls_cpus, cluster_id);
	if (exclusive_offline)
		cpumask_and(cpus, cpu_online_mask, &cls_cpus);
	else
		cpumask_copy(cpus, &cls_cpus);
}

#ifdef CONFIG_MTK_SCHED_CMP_TGS
static int nr_cpus_in_cluster(int cluster_id, bool exclusive_offline)
{
	struct cpumask cls_cpus;
	int nr_cpus;

	arch_get_cluster_cpus(&cls_cpus, cluster_id);
	if (exclusive_offline) {
		struct cpumask online_cpus;
		cpumask_and(&online_cpus, cpu_online_mask, &cls_cpus);
		nr_cpus = cpumask_weight(&online_cpus);
	} else
		nr_cpus = cpumask_weight(&cls_cpus);

	return nr_cpus;
}
#endif
#endif /* CONFIG_MTK_SCHED_CMP */

/*
 * generic entry point for cpu mask construction, dedicated for
 * mediatek scheduler.
 */
static __init inline void cmp_cputopo_domain_setup(void)
{
	WARN(smp_processor_id() != 0, "%s is supposed runs on CPU0 while kernel init", __func__);
#ifdef CONFIG_MTK_CPU_TOPOLOGY
	/*
	 * sched_init
	 *   |-> cmp_cputopo_domain_seutp()
	 * ...
	 * rest_init
	 *   ^ fork kernel_init
	 *       |-> kernel_init_freeable
	 *        ...
	 *           |-> arch_build_cpu_topology_domain
	 *
	 * here, we focus to build up cpu topology and domain before scheduler runs.
	 */
	pr_debug("[CPUTOPO][%s] build CPU topology and cluster.\n", __func__);
	arch_build_cpu_topology_domain();
#endif
}

/*
 * We choose a half-life close to 1 scheduling period.
 * Note: The tables below are dependent on this value.
 */
#define LOAD_AVG_PERIOD 32
#define LOAD_AVG_MAX 47742 /* maximum possible load avg */
#define LOAD_AVG_MAX_N 345 /* number of full periods to produce LOAD_MAX_AVG */

/* Precomputed fixed inverse multiplies for multiplication by y^n */
static const u32 runnable_avg_yN_inv[] = {
	0xffffffff, 0xfa83b2da, 0xf5257d14, 0xefe4b99a, 0xeac0c6e6, 0xe5b906e6,
	0xe0ccdeeb, 0xdbfbb796, 0xd744fcc9, 0xd2a81d91, 0xce248c14, 0xc9b9bd85,
	0xc5672a10, 0xc12c4cc9, 0xbd08a39e, 0xb8fbaf46, 0xb504f333, 0xb123f581,
	0xad583ee9, 0xa9a15ab4, 0xa5fed6a9, 0xa2704302, 0x9ef5325f, 0x9b8d39b9,
	0x9837f050, 0x94f4efa8, 0x91c3d373, 0x8ea4398a, 0x8b95c1e3, 0x88980e80,
	0x85aac367, 0x82cd8698,
};

/*
 * Precomputed \Sum y^k { 1<=k<=n }.  These are floor(true_value) to prevent
 * over-estimates when re-combining.
 */
static const u32 runnable_avg_yN_sum[] = {
	    0, 1002, 1982, 2941, 3880, 4798, 5697, 6576, 7437, 8279, 9103,
	 9909, 10698, 11470, 12226, 12966, 13690, 14398, 15091, 15769, 16433, 17082,
	17718, 18340, 18949, 19545, 20128, 20698, 21256, 21802, 22336, 22859, 23371,
};

/*
 * Approximate:
 *   val * y^n,    where y^32 ~= 0.5 (~1 scheduling period)
 */
static __always_inline u64 decay_load(u64 val, u64 n)
{
	unsigned int local_n;

	if (!n)
		return val;
	else if (unlikely(n > LOAD_AVG_PERIOD * 63))
		return 0;

	/* after bounds checking we can collapse to 32-bit */
	local_n = n;

	/*
	 * As y^PERIOD = 1/2, we can combine
	 *    y^n = 1/2^(n/PERIOD) * y^(n%PERIOD)
	 * With a look-up table which covers y^n (n<PERIOD)
	 *
	 * To achieve constant time decay_load.
	 */
	if (unlikely(local_n >= LOAD_AVG_PERIOD)) {
		val >>= local_n / LOAD_AVG_PERIOD;
		local_n %= LOAD_AVG_PERIOD;
	}

	val *= runnable_avg_yN_inv[local_n];
	/* We don't use SRR here since we always want to round down. */
	return val >> 32;
}

/*
 * For updates fully spanning n periods, the contribution to runnable
 * average will be: \Sum 1024*y^n
 *
 * We can compute this reasonably efficiently by combining:
 *   y^PERIOD = 1/2 with precomputed \Sum 1024*y^n {for  n <PERIOD}
 */
static u32 __compute_runnable_contrib(u64 n)
{
	u32 contrib = 0;

	if (likely(n <= LOAD_AVG_PERIOD))
		return runnable_avg_yN_sum[n];
	else if (unlikely(n >= LOAD_AVG_MAX_N))
		return LOAD_AVG_MAX;

	/* Compute \Sum k^n combining precomputed values for k^i, \Sum k^j */
	do {
		contrib /= 2; /* y^LOAD_AVG_PERIOD = 1/2 */
		contrib += runnable_avg_yN_sum[LOAD_AVG_PERIOD];

		n -= LOAD_AVG_PERIOD;
	} while (n > LOAD_AVG_PERIOD);

	contrib = decay_load(contrib, n);
	return contrib + runnable_avg_yN_sum[n];
}

/*
 * We can represent the historical contribution to runnable average as the
 * coefficients of a geometric series.  To do this we sub-divide our runnable
 * history into segments of approximately 1ms (1024us); label the segment that
 * occurred N-ms ago p_N, with p_0 corresponding to the current period, e.g.
 *
 * [<- 1024us ->|<- 1024us ->|<- 1024us ->| ...
 *      p0            p1           p2
 *     (now)       (~1ms ago)  (~2ms ago)
 *
 * Let u_i denote the fraction of p_i that the entity was runnable.
 *
 * We then designate the fractions u_i as our co-efficients, yielding the
 * following representation of historical load:
 *   u_0 + u_1*y + u_2*y^2 + u_3*y^3 + ...
 *
 * We choose y based on the with of a reasonably scheduling period, fixing:
 *   y^32 = 0.5
 *
 * This means that the contribution to load ~32ms ago (u_32) will be weighted
 * approximately half as much as the contribution to load within the last ms
 * (u_0).
 *
 * When a period "rolls over" and we have new u_0`, multiplying the previous
 * sum again by y is sufficient to update:
 *   load_avg = u_0` + y*(u_0 + u_1*y + u_2*y^2 + ... )
 *            = u_0 + u_1*y + u_2*y^2 + ... [re-labeling u_i --> u_{i+1}]
 */
static __always_inline int __update_entity_runnable_avg(u64 now, int cpu,
							struct sched_avg *sa,
							int runnable,
							int running)
{
	u64 delta, scaled_delta, periods;
	u32 runnable_contrib, scaled_runnable_contrib;
	int delta_w, scaled_delta_w, decayed = 0;
	unsigned long scale_freq = arch_scale_freq_capacity(NULL, cpu);
	unsigned long scale_cpu = arch_scale_cpu_capacity(NULL, cpu);

	delta = now - sa->last_runnable_update;
	/*
	 * This should only happen when time goes backwards, which it
	 * unfortunately does during sched clock init when we swap over to TSC.
	 */
	if ((s64)delta < 0) {
		sa->last_runnable_update = now;
		return 0;
	}

	/*
	 * Use 1024ns as the unit of measurement since it's a reasonable
	 * approximation of 1us and fast to compute.
	 */
	delta >>= 10;
	if (!delta)
		return 0;
	sa->last_runnable_update = now;

	/* delta_w is the amount already accumulated against our next period */
	delta_w = sa->avg_period % 1024;
	if (delta + delta_w >= 1024) {
		/* period roll-over */
		decayed = 1;

		/*
		 * Now that we know we're crossing a period boundary, figure
		 * out how much from delta we need to complete the current
		 * period and accrue it.
		 */
		delta_w = 1024 - delta_w;
		scaled_delta_w = (delta_w * scale_freq) >> SCHED_CAPACITY_SHIFT;

		if (runnable)
			sa->runnable_avg_sum += scaled_delta_w;

		scaled_delta_w *= scale_cpu;
		scaled_delta_w >>= SCHED_CAPACITY_SHIFT;

		if (running)
			sa->running_avg_sum += scaled_delta_w;
		sa->avg_period += delta_w;

		mt_sched_printf(sched_lb_info,
			"[%s] cpu=%d freq=%lu cap=%lu delta=%d scaled_delta_w=%d running_sum=%u runnable=%u period=%u",
			__func__, cpu, scale_freq, scale_cpu, delta_w, scaled_delta_w,
			sa->running_avg_sum, sa->runnable_avg_sum,
			sa->avg_period);

		delta -= delta_w;

		/* Figure out how many additional periods this update spans */
		periods = delta / 1024;
		delta %= 1024;

		sa->runnable_avg_sum = decay_load(sa->runnable_avg_sum,
						  periods + 1);
		sa->running_avg_sum = decay_load(sa->running_avg_sum,
						  periods + 1);
		sa->avg_period = decay_load(sa->avg_period,
						     periods + 1);

		/* Efficiently calculate \sum (1..n_period) 1024*y^i */
		runnable_contrib = __compute_runnable_contrib(periods);
		scaled_runnable_contrib = (runnable_contrib * scale_freq)
						>> SCHED_CAPACITY_SHIFT;

		if (runnable)
			sa->runnable_avg_sum += scaled_runnable_contrib;

		scaled_runnable_contrib *= scale_cpu;
		scaled_runnable_contrib >>= SCHED_CAPACITY_SHIFT;

		if (running)
			sa->running_avg_sum += scaled_runnable_contrib;
		sa->avg_period += runnable_contrib;
	}

	/* Remainder of delta accrued against u_0` */
	scaled_delta = (delta * scale_freq) >> SCHED_CAPACITY_SHIFT;

	if (runnable)
		sa->runnable_avg_sum += scaled_delta;

	scaled_delta *= scale_cpu;
	scaled_delta >>= SCHED_CAPACITY_SHIFT;

	if (running)
		sa->running_avg_sum += scaled_delta;
	sa->avg_period += delta;

	return decayed;
}

/* Synchronize an entity's decay with its parenting cfs_rq.*/
static inline u64 __synchronize_entity_decay(struct sched_entity *se)
{
	struct cfs_rq *cfs_rq = cfs_rq_of(se);
	u64 decays = atomic64_read(&cfs_rq->decay_counter);

	decays -= se->avg.decay_count;
	if (!decays)
		return 0;

	se->avg.load_avg_contrib = decay_load(se->avg.load_avg_contrib, decays);
	se->avg.utilization_avg_contrib =
		decay_load(se->avg.utilization_avg_contrib, decays);

	se->avg.decay_count = 0;

	return decays;
}

#ifdef CONFIG_FAIR_GROUP_SCHED
static inline void __update_cfs_rq_tg_load_contrib(struct cfs_rq *cfs_rq,
						 int force_update)
{
	struct task_group *tg = cfs_rq->tg;
	long tg_contrib;

	tg_contrib = cfs_rq->runnable_load_avg + cfs_rq->blocked_load_avg;
	tg_contrib -= cfs_rq->tg_load_contrib;

	if (!tg_contrib)
		return;

	if (force_update || abs(tg_contrib) > cfs_rq->tg_load_contrib / 8) {
		atomic_long_add(tg_contrib, &tg->load_avg);
		cfs_rq->tg_load_contrib += tg_contrib;
	}
}

/*
 * Aggregate cfs_rq runnable averages into an equivalent task_group
 * representation for computing load contributions.
 */
static inline void __update_tg_runnable_avg(struct sched_avg *sa,
						  struct cfs_rq *cfs_rq)
{
	struct task_group *tg = cfs_rq->tg;
	long contrib;

	/* The fraction of a cpu used by this cfs_rq */
	contrib = div_u64((u64)sa->runnable_avg_sum << NICE_0_SHIFT,
			  sa->avg_period + 1);
	contrib -= cfs_rq->tg_runnable_contrib;

	if (abs(contrib) > cfs_rq->tg_runnable_contrib / 64) {
		atomic_add(contrib, &tg->runnable_avg);
		cfs_rq->tg_runnable_contrib += contrib;
	}
}

static inline void __update_group_entity_contrib(struct sched_entity *se)
{
	struct cfs_rq *cfs_rq = group_cfs_rq(se);
	struct task_group *tg = cfs_rq->tg;
	int runnable_avg;

	u64 contrib;

	contrib = cfs_rq->tg_load_contrib * tg->shares;
	se->avg.load_avg_contrib = div_u64(contrib,
				     atomic_long_read(&tg->load_avg) + 1);

	/*
	 * For group entities we need to compute a correction term in the case
	 * that they are consuming <1 cpu so that we would contribute the same
	 * load as a task of equal weight.
	 *
	 * Explicitly co-ordinating this measurement would be expensive, but
	 * fortunately the sum of each cpus contribution forms a usable
	 * lower-bound on the true value.
	 *
	 * Consider the aggregate of 2 contributions.  Either they are disjoint
	 * (and the sum represents true value) or they are disjoint and we are
	 * understating by the aggregate of their overlap.
	 *
	 * Extending this to N cpus, for a given overlap, the maximum amount we
	 * understand is then n_i(n_i+1)/2 * w_i where n_i is the number of
	 * cpus that overlap for this interval and w_i is the interval width.
	 *
	 * On a small machine; the first term is well-bounded which bounds the
	 * total error since w_i is a subset of the period.  Whereas on a
	 * larger machine, while this first term can be larger, if w_i is the
	 * of consequential size guaranteed to see n_i*w_i quickly converge to
	 * our upper bound of 1-cpu.
	 */
	runnable_avg = atomic_read(&tg->runnable_avg);
	if (runnable_avg < NICE_0_LOAD) {
		se->avg.load_avg_contrib *= runnable_avg;
		se->avg.load_avg_contrib >>= NICE_0_SHIFT;
	}
}

static inline void update_rq_runnable_avg(struct rq *rq, int runnable)
{
	__update_entity_runnable_avg(rq_clock_task(rq), cpu_of(rq), &rq->avg,
			runnable, runnable);
	__update_tg_runnable_avg(&rq->avg, &rq->cfs);
}
#else /* CONFIG_FAIR_GROUP_SCHED */
static inline void __update_cfs_rq_tg_load_contrib(struct cfs_rq *cfs_rq,
						 int force_update) {}
static inline void __update_tg_runnable_avg(struct sched_avg *sa,
						  struct cfs_rq *cfs_rq) {}
static inline void __update_group_entity_contrib(struct sched_entity *se) {}
static inline void update_rq_runnable_avg(struct rq *rq, int runnable) {}
#endif /* CONFIG_FAIR_GROUP_SCHED */

static inline void __update_task_entity_contrib(struct sched_entity *se)
{
	u32 contrib;

	/* avoid overflowing a 32-bit type w/ SCHED_LOAD_SCALE */
	contrib = se->avg.runnable_avg_sum * scale_load_down(se->load.weight);
	contrib /= (se->avg.avg_period + 1);
	se->avg.load_avg_contrib = scale_load(contrib);
}

/* Compute the current contribution to load_avg by se, return any delta */
static long __update_entity_load_avg_contrib(struct sched_entity *se)
{
	long old_contrib = se->avg.load_avg_contrib;

	if (entity_is_task(se)) {
		__update_task_entity_contrib(se);
	} else {
		__update_tg_runnable_avg(&se->avg, group_cfs_rq(se));
		__update_group_entity_contrib(se);
	}

	return se->avg.load_avg_contrib - old_contrib;
}

static inline void __update_task_entity_utilization(struct sched_entity *se)
{
	u32 contrib;

	/* avoid overflowing a 32-bit type w/ SCHED_LOAD_SCALE */
	contrib = se->avg.running_avg_sum * scale_load_down(SCHED_LOAD_SCALE);
	contrib /= (se->avg.avg_period + 1);
	se->avg.utilization_avg_contrib = scale_load(contrib);

	/* runnable utilization */
	contrib = se->avg.runnable_avg_sum * scale_load_down(SCHED_LOAD_SCALE);
	contrib /= (se->avg.avg_period + 1);
	se->avg.loadwop_avg_contrib = scale_load(contrib);
}

static void __update_entity_utilization_avg_contrib(struct sched_entity *se,
						 long *running_delta, long *runnable_delta)
{
	long old_running = se->avg.utilization_avg_contrib;
	long old_runnable = se->avg.loadwop_avg_contrib;

	if (entity_is_task(se))
		__update_task_entity_utilization(se);
	else
		se->avg.utilization_avg_contrib =
				group_cfs_rq(se)->utilization_load_avg +
				group_cfs_rq(se)->utilization_blocked_avg;

	*running_delta = se->avg.utilization_avg_contrib - old_running;
	*runnable_delta = se->avg.loadwop_avg_contrib - old_runnable;
}

static inline void subtract_blocked_load_contrib(struct cfs_rq *cfs_rq,
						 long load_contrib)
{
	if (likely(load_contrib < cfs_rq->blocked_load_avg))
		cfs_rq->blocked_load_avg -= load_contrib;
	else
		cfs_rq->blocked_load_avg = 0;
}

static inline void subtract_utilization_blocked_contrib(struct cfs_rq *cfs_rq,
						long utilization_contrib)
{
	if (likely(utilization_contrib < cfs_rq->utilization_blocked_avg))
		cfs_rq->utilization_blocked_avg -= utilization_contrib;
	else
		cfs_rq->utilization_blocked_avg = 0;
}

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

#ifdef CONFIG_SCHED_HMP
/* Schedule entity */
#define se_load(se) se->avg.loadwop_avg_contrib
#define se_contrib(se) se->avg.load_avg_contrib

/* CPU related : load information */
#define cfs_pending_load(cpu) cpu_rq(cpu)->cfs.avg.pending_load
#define cfs_load(cpu) cpu_rq(cpu)->cfs.avg.loadwop_avg_contrib
#define cfs_contrib(cpu) cpu_rq(cpu)->cfs.avg.load_avg_contrib

/* CPU related : the number of tasks */
#define cfs_nr_normal_prio(cpu) cpu_rq(cpu)->cfs.avg.nr_normal_prio
#define cfs_nr_pending(cpu) cpu_rq(cpu)->cfs.avg.nr_pending
#define cfs_length(cpu) cpu_rq(cpu)->cfs.h_nr_running
#define rq_length(cpu) (cpu_rq(cpu)->nr_running + cfs_nr_pending(cpu))
#endif /* CONFIG_SCHED_HMP */

static inline u64 cfs_rq_clock_task(struct cfs_rq *cfs_rq);

#ifdef CONFIG_MTK_SCHED_CMP_TGS
int group_leader_is_empty(struct task_struct *p)
{

#ifdef CONFIG_SLUB_DEBUG
	struct task_struct *tg = p->group_leader;
#endif

	if ((SIGNAL_GROUP_EXIT & p->signal->flags) || (PF_EXITING & p->flags))
		return 1;

#ifdef CONFIG_SLUB_DEBUG

# ifdef CONFIG_ARM64
	if (tg->state == 0x6b6b6b6b6b6b6b6b) {
# else
	if (tg->state == 0x6b6b6b6b) {
# endif
		/*
		      pr_warn("[%s] (0x%p/0x%p)(#%d/%s) leader: state(%d) exit_state(%d)\n", __func__,
		      p, tg, get_nr_threads(p), thread_group_empty(p) ? "empty" : "not empty",
		      tg->state, tg->exit_state);
		*/
		return 1;
	}
#endif

	return 0;
}

static inline void update_tg_info(struct cfs_rq *cfs_rq, struct sched_entity *se, long ratio_delta)
{
	struct task_struct *p = task_of(se);
	struct task_struct *tg;
	int id;
	unsigned long flags;

	if (!entity_is_task(se))
		return;

	if (group_leader_is_empty(p))
		return;

	tg = p->group_leader;

	id = arch_get_cluster_id(cpu_of(rq_of(cfs_rq)));
	if (unlikely(WARN_ON(id < 0)))
		return;

	raw_spin_lock_irqsave(&tg->thread_group_info_lock, flags);
	tg->thread_group_info[id].loadwop_avg_contrib += ratio_delta;
	raw_spin_unlock_irqrestore(&tg->thread_group_info_lock, flags);

	mt_sched_printf(sched_cmp_info, "[%s] %d:%s %d:%s %ld %ld %d %d %lu:%lu:%lu update", __func__,
	   tg->pid, tg->comm, p->pid, p->comm,
	   se->avg.loadwop_avg_contrib, ratio_delta,
	   cfs_rq->rq->cpu, id,
	   tg->thread_group_info[id].nr_running,
	   tg->thread_group_info[id].cfs_nr_running,
	   tg->thread_group_info[id].loadwop_avg_contrib);
}
#endif

/* Update a sched_entity's runnable average */
static inline void update_entity_load_avg(struct sched_entity *se,
					  int update_cfs_rq)
{
	struct cfs_rq *cfs_rq = cfs_rq_of(se);
	long contrib_delta, running_delta, runnable_delta;
	int cpu = cpu_of(rq_of(cfs_rq));
	u64 now;

	/*
	 * For a group entity we need to use their owned cfs_rq_clock_task() in
	 * case they are the parent of a throttled hierarchy.
	 */
	if (entity_is_task(se))
		now = cfs_rq_clock_task(cfs_rq);
	else
		now = cfs_rq_clock_task(group_cfs_rq(se));

	if (!__update_entity_runnable_avg(now, cpu, &se->avg, se->on_rq, cfs_rq->curr == se)) {
		/* sched: add trace_sched */
		if (entity_is_task(se))
			trace_sched_task_entity_avg(2, task_of(se), &se->avg);
		return;
	}

	contrib_delta = __update_entity_load_avg_contrib(se);
	__update_entity_utilization_avg_contrib(se, &running_delta, &runnable_delta);

	if (!update_cfs_rq)
		return;

	if (se->on_rq) {
		cfs_rq->runnable_load_avg += contrib_delta;
		cfs_rq->utilization_load_avg += running_delta;
		if (entity_is_task(se)) {
#ifdef CONFIG_MTK_SCHED_CMP_TGS
			update_tg_info(cfs_rq, se, runnable_delta);
#endif
#ifdef CONFIG_SCHED_HMP
			cpu_rq(cpu)->cfs.avg.load_avg_contrib += contrib_delta;
			cpu_rq(cpu)->cfs.avg.utilization_avg_contrib += running_delta;
			cpu_rq(cpu)->cfs.avg.loadwop_avg_contrib += runnable_delta;
#ifdef CONFIG_HMP_TRACER
			trace_sched_cfs_load_update(task_of(se), se_load(se), runnable_delta, cpu);
#endif /* CONFIG_HMP_TRACER */
#endif
		}
	} else {
		subtract_blocked_load_contrib(cfs_rq, -contrib_delta);
		subtract_utilization_blocked_contrib(cfs_rq,
							-running_delta);
	}
}

/*
 * Decay the load contributed by all blocked children and account this so that
 * their contribution may appropriately discounted when they wake up.
 */
static void update_cfs_rq_blocked_load(struct cfs_rq *cfs_rq, int force_update)
{
	u64 now = cfs_rq_clock_task(cfs_rq) >> 20;
	u64 decays;

	decays = now - cfs_rq->last_decay;
	if (!decays && !force_update)
		return;

	if (atomic_long_read(&cfs_rq->removed_load)) {
		unsigned long removed_load, removed_utilization;
		removed_load = atomic_long_xchg(&cfs_rq->removed_load, 0);
		removed_utilization =
			atomic_long_xchg(&cfs_rq->removed_utilization, 0);
		subtract_blocked_load_contrib(cfs_rq, removed_load);
		subtract_utilization_blocked_contrib(cfs_rq,
							removed_utilization);
	}

	if (decays) {
		cfs_rq->blocked_load_avg = decay_load(cfs_rq->blocked_load_avg,
						      decays);
		cfs_rq->utilization_blocked_avg =
			decay_load(cfs_rq->utilization_blocked_avg, decays);
		atomic64_add(decays, &cfs_rq->decay_counter);
		cfs_rq->last_decay = now;
	}

	__update_cfs_rq_tg_load_contrib(cfs_rq, force_update);
}

/* Add the load generated by se into cfs_rq's child load-average */
static inline void enqueue_entity_load_avg(struct cfs_rq *cfs_rq,
						  struct sched_entity *se,
						  int wakeup)
{
#ifdef CONFIG_SCHED_HMP
	int cpu = cfs_rq->rq->cpu;
#endif
	/*
	 * We track migrations using entity decay_count <= 0, on a wake-up
	 * migration we use a negative decay count to track the remote decays
	 * accumulated while sleeping.
	 *
	 * Newly forked tasks are enqueued with se->avg.decay_count == 0, they
	 * are seen by enqueue_entity_load_avg() as a migration with an already
	 * constructed load_avg_contrib.
	 */
	if (unlikely(se->avg.decay_count <= 0)) {
		se->avg.last_runnable_update = rq_clock_task(rq_of(cfs_rq));
		if (se->avg.decay_count) {
			/*
			 * In a wake-up migration we have to approximate the
			 * time sleeping.  This is because we can't synchronize
			 * clock_task between the two cpus, and it is not
			 * guaranteed to be read-safe.  Instead, we can
			 * approximate this using our carried decays, which are
			 * explicitly atomically readable.
			 */
			se->avg.last_runnable_update -= (-se->avg.decay_count)
							<< 20;
			update_entity_load_avg(se, 0);
			/* Indicate that we're now synchronized and on-rq */
			se->avg.decay_count = 0;
		}
		wakeup = 0;
	} else {
		__synchronize_entity_decay(se);
	}

	/* migrated tasks did not contribute to our blocked load */
	if (wakeup) {
		subtract_blocked_load_contrib(cfs_rq, se->avg.load_avg_contrib);
		subtract_utilization_blocked_contrib(cfs_rq,
					se->avg.utilization_avg_contrib);
		update_entity_load_avg(se, 0);
	}

	cfs_rq->runnable_load_avg += se->avg.load_avg_contrib;
	cfs_rq->utilization_load_avg += se->avg.utilization_avg_contrib;
#ifdef CONFIG_MTK_SCHED_CMP_TGS
	update_tg_info(cfs_rq, se, se->avg.loadwop_avg_contrib);
#endif
	if (sched_feat(SCHED_HMP) && entity_is_task(se)) {
#ifdef CONFIG_SCHED_HMP
		cpu_rq(cpu)->cfs.avg.load_avg_contrib += se->avg.load_avg_contrib;
		cpu_rq(cpu)->cfs.avg.loadwop_avg_contrib += se->avg.loadwop_avg_contrib;
		cfs_nr_pending(cpu) = 0;
		cfs_pending_load(cpu) = 0;
#ifdef CONFIG_SCHED_HMP_PRIO_FILTER
		if (!task_low_priority(task_of(se)->prio))
			cfs_nr_normal_prio(cpu)++;
#endif
#ifdef CONFIG_HMP_TRACER
		trace_sched_cfs_enqueue_task(task_of(se), se_load(se), cpu);
#endif
#endif /* CONFIG_SCHED_HMP */
	}
	/* we force update consideration on load-balancer moves */
	update_cfs_rq_blocked_load(cfs_rq, !wakeup);
}

/*
 * Remove se's load from this cfs_rq child load-average, if the entity is
 * transitioning to a blocked state we track its projected decay using
 * blocked_load_avg.
 */
static inline void dequeue_entity_load_avg(struct cfs_rq *cfs_rq,
						  struct sched_entity *se,
						  int sleep)
{
#ifdef CONFIG_SCHED_HMP
	int cpu = cfs_rq->rq->cpu;
#endif

	update_entity_load_avg(se, 1);
	/* we force update consideration on load-balancer moves */
	update_cfs_rq_blocked_load(cfs_rq, !sleep);

	cfs_rq->runnable_load_avg -= se->avg.load_avg_contrib;
	cfs_rq->utilization_load_avg -= se->avg.utilization_avg_contrib;
#ifdef CONFIG_MTK_SCHED_CMP_TGS
	update_tg_info(cfs_rq, se, -se->avg.loadwop_avg_contrib);
#endif
	if (sleep) {
		cfs_rq->blocked_load_avg += se->avg.load_avg_contrib;
		cfs_rq->utilization_blocked_avg +=
						se->avg.utilization_avg_contrib;
		se->avg.decay_count = atomic64_read(&cfs_rq->decay_counter);
	} /* migrations, e.g. sleep=0 leave decay_count == 0 */

	if (sched_feat(SCHED_HMP) && entity_is_task(se)) {
#ifdef CONFIG_SCHED_HMP
		cpu_rq(cpu)->cfs.avg.load_avg_contrib -= se->avg.load_avg_contrib;
		cpu_rq(cpu)->cfs.avg.utilization_avg_contrib -= se->avg.utilization_avg_contrib;
		cpu_rq(cpu)->cfs.avg.loadwop_avg_contrib -= se->avg.loadwop_avg_contrib;
#ifdef CONFIG_SCHED_HMP_PRIO_FILTER
		cfs_reset_nr_dequeuing_low_prio(cpu);
		if (!task_low_priority(task_of(se)->prio))
			cfs_nr_normal_prio(cpu)--;
#endif
#ifdef CONFIG_HMP_TRACER
		trace_sched_cfs_dequeue_task(task_of(se), se_load(se), cpu);
#endif
#endif /* CONFIG_SCHED_HMP */
	}
}

/*
 * Update the rq's load with the elapsed running time before entering
 * idle. if the last scheduled task is not a CFS task, idle_enter will
 * be the only way to update the runnable statistic.
 */
void idle_enter_fair(struct rq *this_rq)
{
	update_rq_runnable_avg(this_rq, 1);
}

/*
 * Update the rq's load with the elapsed idle time before a task is
 * scheduled. if the newly scheduled task is not a CFS task, idle_exit will
 * be the only way to update the runnable statistic.
 */
void idle_exit_fair(struct rq *this_rq)
{
	update_rq_runnable_avg(this_rq, 0);
}

static int idle_balance(struct rq *this_rq);

#else /* CONFIG_SMP */

static inline void update_entity_load_avg(struct sched_entity *se,
					  int update_cfs_rq) {}
static inline void update_rq_runnable_avg(struct rq *rq, int runnable) {}
static inline void enqueue_entity_load_avg(struct cfs_rq *cfs_rq,
					   struct sched_entity *se,
					   int wakeup) {}
static inline void dequeue_entity_load_avg(struct cfs_rq *cfs_rq,
					   struct sched_entity *se,
					   int sleep) {}
static inline void update_cfs_rq_blocked_load(struct cfs_rq *cfs_rq,
					      int force_update) {}

static inline int idle_balance(struct rq *rq)
{
	return 0;
}

#endif /* CONFIG_SMP */

static void enqueue_sleeper(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
#ifdef CONFIG_SCHEDSTATS
	struct task_struct *tsk = NULL;

	if (entity_is_task(se))
		tsk = task_of(se);

	if (se->statistics.sleep_start) {
		u64 delta = rq_clock(rq_of(cfs_rq)) - se->statistics.sleep_start;

		if ((s64)delta < 0)
			delta = 0;

		if (unlikely(delta > se->statistics.sleep_max))
			se->statistics.sleep_max = delta;

		se->statistics.sleep_start = 0;
		se->statistics.sum_sleep_runtime += delta;

		if (tsk) {
			account_scheduler_latency(tsk, delta >> 10, 1);
			trace_sched_stat_sleep(tsk, delta);
		}
	}
	if (se->statistics.block_start) {
		u64 delta = rq_clock(rq_of(cfs_rq)) - se->statistics.block_start;

		if ((s64)delta < 0)
			delta = 0;

		if (unlikely(delta > se->statistics.block_max))
			se->statistics.block_max = delta;

		se->statistics.block_start = 0;
		se->statistics.sum_sleep_runtime += delta;

		if (tsk) {
			if (tsk->in_iowait) {
				se->statistics.iowait_sum += delta;
				se->statistics.iowait_count++;
				trace_sched_stat_iowait(tsk, delta);
			}

			trace_sched_stat_blocked(tsk, delta);
			trace_sched_blocked_reason(tsk);

			/*
			 * Blocking time is in units of nanosecs, so shift by
			 * 20 to get a milliseconds-range estimation of the
			 * amount of time that the task spent sleeping:
			 */
			if (unlikely(prof_on == SLEEP_PROFILING)) {
				profile_hits(SLEEP_PROFILING,
						(void *)get_wchan(tsk),
						delta >> 20);
			}
			account_scheduler_latency(tsk, delta >> 10, 0);
		}
	}
#endif
}

static void check_spread(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
#ifdef CONFIG_SCHED_DEBUG
	s64 d = se->vruntime - cfs_rq->min_vruntime;

	if (d < 0)
		d = -d;

	if (d > 3*sysctl_sched_latency)
		schedstat_inc(cfs_rq, nr_spread_over);
#endif
}

static void
place_entity(struct cfs_rq *cfs_rq, struct sched_entity *se, int initial)
{
	u64 vruntime = cfs_rq->min_vruntime;

	/*
	 * The 'current' period is already promised to the current tasks,
	 * however the extra weight of the new task will slow them down a
	 * little, place the new task so that it fits in the slot that
	 * stays open at the end.
	 */
	if (initial && sched_feat(START_DEBIT))
		vruntime += sched_vslice(cfs_rq, se);

	/* sleeps up to a single latency don't count. */
	if (!initial) {
		unsigned long thresh = sysctl_sched_latency;

		/*
		 * Halve their sleep time's effect, to allow
		 * for a gentler effect of sleepers:
		 */
		if (sched_feat(GENTLE_FAIR_SLEEPERS))
			thresh >>= 1;

		vruntime -= thresh;
	}

	/* ensure we never gain time by being placed backwards. */
	se->vruntime = max_vruntime(se->vruntime, vruntime);
}

static void check_enqueue_throttle(struct cfs_rq *cfs_rq);

static void
enqueue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se, int flags)
{
	/*
	 * Update the normalized vruntime before updating min_vruntime
	 * through calling update_curr().
	 */
	if (!(flags & ENQUEUE_WAKEUP) || (flags & ENQUEUE_WAKING))
		se->vruntime += cfs_rq->min_vruntime;

	/*
	 * Update run-time statistics of the 'current'.
	 */
	update_curr(cfs_rq);
	enqueue_entity_load_avg(cfs_rq, se, flags & ENQUEUE_WAKEUP);
	account_entity_enqueue(cfs_rq, se);
	update_cfs_shares(cfs_rq);

	if (flags & ENQUEUE_WAKEUP) {
		place_entity(cfs_rq, se, 0);
		enqueue_sleeper(cfs_rq, se);
	}

	update_stats_enqueue(cfs_rq, se);
	check_spread(cfs_rq, se);
	if (se != cfs_rq->curr)
		__enqueue_entity(cfs_rq, se);
	se->on_rq = 1;

	if (cfs_rq->nr_running == 1) {
		list_add_leaf_cfs_rq(cfs_rq);
		check_enqueue_throttle(cfs_rq);
	}
}

static void __clear_buddies_last(struct sched_entity *se)
{
	for_each_sched_entity(se) {
		struct cfs_rq *cfs_rq = cfs_rq_of(se);
		if (cfs_rq->last != se)
			break;

		cfs_rq->last = NULL;
	}
}

static void __clear_buddies_next(struct sched_entity *se)
{
	for_each_sched_entity(se) {
		struct cfs_rq *cfs_rq = cfs_rq_of(se);
		if (cfs_rq->next != se)
			break;

		cfs_rq->next = NULL;
	}
}

static void __clear_buddies_skip(struct sched_entity *se)
{
	for_each_sched_entity(se) {
		struct cfs_rq *cfs_rq = cfs_rq_of(se);
		if (cfs_rq->skip != se)
			break;

		cfs_rq->skip = NULL;
	}
}

static void clear_buddies(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	if (cfs_rq->last == se)
		__clear_buddies_last(se);

	if (cfs_rq->next == se)
		__clear_buddies_next(se);

	if (cfs_rq->skip == se)
		__clear_buddies_skip(se);
}

static __always_inline void return_cfs_rq_runtime(struct cfs_rq *cfs_rq);

static void
dequeue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se, int flags)
{
	/*
	 * Update run-time statistics of the 'current'.
	 */
	update_curr(cfs_rq);
	dequeue_entity_load_avg(cfs_rq, se, flags & DEQUEUE_SLEEP);

	update_stats_dequeue(cfs_rq, se);
	if (flags & DEQUEUE_SLEEP) {
#ifdef CONFIG_SCHEDSTATS
		if (entity_is_task(se)) {
			struct task_struct *tsk = task_of(se);

			if (tsk->state & TASK_INTERRUPTIBLE)
				se->statistics.sleep_start = rq_clock(rq_of(cfs_rq));
			if (tsk->state & TASK_UNINTERRUPTIBLE)
				se->statistics.block_start = rq_clock(rq_of(cfs_rq));
		}
#endif
	}

	clear_buddies(cfs_rq, se);

	if (se != cfs_rq->curr)
		__dequeue_entity(cfs_rq, se);
	se->on_rq = 0;
	account_entity_dequeue(cfs_rq, se);

	/*
	 * Normalize the entity after updating the min_vruntime because the
	 * update can refer to the ->curr item and we need to reflect this
	 * movement in our normalized position.
	 */
	if (!(flags & DEQUEUE_SLEEP))
		se->vruntime -= cfs_rq->min_vruntime;

	/* return excess runtime on last dequeue */
	return_cfs_rq_runtime(cfs_rq);

	update_min_vruntime(cfs_rq);
	update_cfs_shares(cfs_rq);
}

/*
 * Preempt the current task with a newly woken task if needed:
 */
static void
check_preempt_tick(struct cfs_rq *cfs_rq, struct sched_entity *curr)
{
	unsigned long ideal_runtime, delta_exec;
	struct sched_entity *se;
	s64 delta;

	ideal_runtime = sched_slice(cfs_rq, curr);
	delta_exec = curr->sum_exec_runtime - curr->prev_sum_exec_runtime;
	if (delta_exec > ideal_runtime) {
		resched_curr(rq_of(cfs_rq));
		/*
		 * The current task ran long enough, ensure it doesn't get
		 * re-elected due to buddy favours.
		 */
		clear_buddies(cfs_rq, curr);
		return;
	}

	/*
	 * Ensure that a task that missed wakeup preemption by a
	 * narrow margin doesn't have to wait for a full slice.
	 * This also mitigates buddy induced latencies under load.
	 */
	if (delta_exec < sysctl_sched_min_granularity)
		return;

	se = __pick_first_entity(cfs_rq);
	delta = curr->vruntime - se->vruntime;

	if (delta < 0)
		return;

	if (delta > ideal_runtime)
		resched_curr(rq_of(cfs_rq));
}

static void
set_next_entity(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	/* 'current' is not kept within the tree. */
	if (se->on_rq) {
		/*
		 * Any task has to be enqueued before it get to execute on
		 * a CPU. So account for the time it spent waiting on the
		 * runqueue.
		 */
		update_stats_wait_end(cfs_rq, se);
		__dequeue_entity(cfs_rq, se);
		update_entity_load_avg(se, 1);
	}

	update_stats_curr_start(cfs_rq, se);
	cfs_rq->curr = se;
#ifdef CONFIG_SCHEDSTATS
	/*
	 * Track our maximum slice length, if the CPU's load is at
	 * least twice that of our own weight (i.e. dont track it
	 * when there are only lesser-weight tasks around):
	 */
	if (rq_of(cfs_rq)->load.weight >= 2*se->load.weight) {
		se->statistics.slice_max = max(se->statistics.slice_max,
			se->sum_exec_runtime - se->prev_sum_exec_runtime);
	}
#endif
	se->prev_sum_exec_runtime = se->sum_exec_runtime;
}

static int
wakeup_preempt_entity(struct sched_entity *curr, struct sched_entity *se);

/*
 * Pick the next process, keeping these things in mind, in this order:
 * 1) keep things fair between processes/task groups
 * 2) pick the "next" process, since someone really wants that to run
 * 3) pick the "last" process, for cache locality
 * 4) do not run the "skip" process, if something else is available
 */
static struct sched_entity *
pick_next_entity(struct cfs_rq *cfs_rq, struct sched_entity *curr)
{
	struct sched_entity *left = __pick_first_entity(cfs_rq);
	struct sched_entity *se;

	/*
	 * If curr is set we have to see if its left of the leftmost entity
	 * still in the tree, provided there was anything in the tree at all.
	 */
	if (!left || (curr && entity_before(curr, left)))
		left = curr;

	se = left; /* ideally we run the leftmost entity */

	/*
	 * Avoid running the skip buddy, if running something else can
	 * be done without getting too unfair.
	 */
	if (cfs_rq->skip == se) {
		struct sched_entity *second;

		if (se == curr) {
			second = __pick_first_entity(cfs_rq);
		} else {
			second = __pick_next_entity(se);
			if (!second || (curr && entity_before(curr, second)))
				second = curr;
		}

		if (second && wakeup_preempt_entity(second, left) < 1)
			se = second;
	}

	/*
	 * Prefer last buddy, try to return the CPU to a preempted task.
	 */
	if (cfs_rq->last && wakeup_preempt_entity(cfs_rq->last, left) < 1)
		se = cfs_rq->last;

	/*
	 * Someone really wants this to run. If it's not unfair, run it.
	 */
	if (cfs_rq->next && wakeup_preempt_entity(cfs_rq->next, left) < 1)
		se = cfs_rq->next;

	clear_buddies(cfs_rq, se);

	return se;
}

static bool check_cfs_rq_runtime(struct cfs_rq *cfs_rq);

static void put_prev_entity(struct cfs_rq *cfs_rq, struct sched_entity *prev)
{
	/*
	 * If still on the runqueue then deactivate_task()
	 * was not called and update_curr() has to be done:
	 */
	if (prev->on_rq)
		update_curr(cfs_rq);

	/* throttle cfs_rqs exceeding runtime */
	check_cfs_rq_runtime(cfs_rq);

	check_spread(cfs_rq, prev);
	if (prev->on_rq) {
		update_stats_wait_start(cfs_rq, prev);
		/* Put 'current' back into the tree. */
		__enqueue_entity(cfs_rq, prev);
		/* in !on_rq case, update occurred at dequeue */
		update_entity_load_avg(prev, 1);
	}
	cfs_rq->curr = NULL;
}

static void
entity_tick(struct cfs_rq *cfs_rq, struct sched_entity *curr, int queued)
{
	/*
	 * Update run-time statistics of the 'current'.
	 */
	update_curr(cfs_rq);

	/*
	 * Ensure that runnable average is periodically updated.
	 */
	update_entity_load_avg(curr, 1);
	update_cfs_rq_blocked_load(cfs_rq, 1);
	update_cfs_shares(cfs_rq);

#ifdef CONFIG_SCHED_HRTICK
	/*
	 * queued ticks are scheduled to match the slice, so don't bother
	 * validating it and just reschedule.
	 */
	if (queued) {
		resched_curr(rq_of(cfs_rq));
		return;
	}
	/*
	 * don't let the period tick interfere with the hrtick preemption
	 */
	if (!sched_feat(DOUBLE_TICK) &&
			hrtimer_active(&rq_of(cfs_rq)->hrtick_timer))
		return;
#endif

	if (cfs_rq->nr_running > 1)
		check_preempt_tick(cfs_rq, curr);
}


/**************************************************
 * CFS bandwidth control machinery
 */

#ifdef CONFIG_CFS_BANDWIDTH

#ifdef HAVE_JUMP_LABEL
static struct static_key __cfs_bandwidth_used;

static inline bool cfs_bandwidth_used(void)
{
	return static_key_false(&__cfs_bandwidth_used);
}

void cfs_bandwidth_usage_inc(void)
{
	static_key_slow_inc(&__cfs_bandwidth_used);
}

void cfs_bandwidth_usage_dec(void)
{
	static_key_slow_dec(&__cfs_bandwidth_used);
}
#else /* HAVE_JUMP_LABEL */
static bool cfs_bandwidth_used(void)
{
	return true;
}

void cfs_bandwidth_usage_inc(void) {}
void cfs_bandwidth_usage_dec(void) {}
#endif /* HAVE_JUMP_LABEL */

/*
 * default period for cfs group bandwidth.
 * default: 0.1s, units: nanoseconds
 */
static inline u64 default_cfs_period(void)
{
	return 100000000ULL;
}

static inline u64 sched_cfs_bandwidth_slice(void)
{
	return (u64)sysctl_sched_cfs_bandwidth_slice * NSEC_PER_USEC;
}

/*
 * Replenish runtime according to assigned quota and update expiration time.
 * We use sched_clock_cpu directly instead of rq->clock to avoid adding
 * additional synchronization around rq->lock.
 *
 * requires cfs_b->lock
 */
void __refill_cfs_bandwidth_runtime(struct cfs_bandwidth *cfs_b)
{
	u64 now;

	if (cfs_b->quota == RUNTIME_INF)
		return;

	now = sched_clock_cpu(smp_processor_id());
	cfs_b->runtime = cfs_b->quota;
	cfs_b->runtime_expires = now + ktime_to_ns(cfs_b->period);
}

static inline struct cfs_bandwidth *tg_cfs_bandwidth(struct task_group *tg)
{
	return &tg->cfs_bandwidth;
}

/* rq->task_clock normalized against any time this cfs_rq has spent throttled */
static inline u64 cfs_rq_clock_task(struct cfs_rq *cfs_rq)
{
	if (unlikely(cfs_rq->throttle_count))
		return cfs_rq->throttled_clock_task;

	return rq_clock_task(rq_of(cfs_rq)) - cfs_rq->throttled_clock_task_time;
}

/* returns 0 on failure to allocate runtime */
static int assign_cfs_rq_runtime(struct cfs_rq *cfs_rq)
{
	struct task_group *tg = cfs_rq->tg;
	struct cfs_bandwidth *cfs_b = tg_cfs_bandwidth(tg);
	u64 amount = 0, min_amount, expires;

	/* note: this is a positive sum as runtime_remaining <= 0 */
	min_amount = sched_cfs_bandwidth_slice() - cfs_rq->runtime_remaining;

	raw_spin_lock(&cfs_b->lock);
	if (cfs_b->quota == RUNTIME_INF)
		amount = min_amount;
	else {
		/*
		 * If the bandwidth pool has become inactive, then at least one
		 * period must have elapsed since the last consumption.
		 * Refresh the global state and ensure bandwidth timer becomes
		 * active.
		 */
		if (!cfs_b->timer_active) {
			__refill_cfs_bandwidth_runtime(cfs_b);
			__start_cfs_bandwidth(cfs_b, false);
		}

		if (cfs_b->runtime > 0) {
			amount = min(cfs_b->runtime, min_amount);
			cfs_b->runtime -= amount;
			cfs_b->idle = 0;
		}
	}
	expires = cfs_b->runtime_expires;
	raw_spin_unlock(&cfs_b->lock);

	cfs_rq->runtime_remaining += amount;
	/*
	 * we may have advanced our local expiration to account for allowed
	 * spread between our sched_clock and the one on which runtime was
	 * issued.
	 */
	if ((s64)(expires - cfs_rq->runtime_expires) > 0)
		cfs_rq->runtime_expires = expires;

	return cfs_rq->runtime_remaining > 0;
}

/*
 * Note: This depends on the synchronization provided by sched_clock and the
 * fact that rq->clock snapshots this value.
 */
static void expire_cfs_rq_runtime(struct cfs_rq *cfs_rq)
{
	struct cfs_bandwidth *cfs_b = tg_cfs_bandwidth(cfs_rq->tg);

	/* if the deadline is ahead of our clock, nothing to do */
	if (likely((s64)(rq_clock(rq_of(cfs_rq)) - cfs_rq->runtime_expires) < 0))
		return;

	if (cfs_rq->runtime_remaining < 0)
		return;

	/*
	 * If the local deadline has passed we have to consider the
	 * possibility that our sched_clock is 'fast' and the global deadline
	 * has not truly expired.
	 *
	 * Fortunately we can check determine whether this the case by checking
	 * whether the global deadline has advanced. It is valid to compare
	 * cfs_b->runtime_expires without any locks since we only care about
	 * exact equality, so a partial write will still work.
	 */

	if (cfs_rq->runtime_expires != cfs_b->runtime_expires) {
		/* extend local deadline, drift is bounded above by 2 ticks */
		cfs_rq->runtime_expires += TICK_NSEC;
	} else {
		/* global deadline is ahead, expiration has passed */
		cfs_rq->runtime_remaining = 0;
	}
}

static void __account_cfs_rq_runtime(struct cfs_rq *cfs_rq, u64 delta_exec)
{
	/* dock delta_exec before expiring quota (as it could span periods) */
	cfs_rq->runtime_remaining -= delta_exec;
	expire_cfs_rq_runtime(cfs_rq);

	if (likely(cfs_rq->runtime_remaining > 0))
		return;

	/*
	 * if we're unable to extend our runtime we resched so that the active
	 * hierarchy can be throttled
	 */
	if (!assign_cfs_rq_runtime(cfs_rq) && likely(cfs_rq->curr))
		resched_curr(rq_of(cfs_rq));
}

static __always_inline
void account_cfs_rq_runtime(struct cfs_rq *cfs_rq, u64 delta_exec)
{
	if (!cfs_bandwidth_used() || !cfs_rq->runtime_enabled)
		return;

	__account_cfs_rq_runtime(cfs_rq, delta_exec);
}

static inline int cfs_rq_throttled(struct cfs_rq *cfs_rq)
{
	return cfs_bandwidth_used() && cfs_rq->throttled;
}

/* check whether cfs_rq, or any parent, is throttled */
static inline int throttled_hierarchy(struct cfs_rq *cfs_rq)
{
	return cfs_bandwidth_used() && cfs_rq->throttle_count;
}

/*
 * Ensure that neither of the group entities corresponding to src_cpu or
 * dest_cpu are members of a throttled hierarchy when performing group
 * load-balance operations.
 */
static inline int throttled_lb_pair(struct task_group *tg,
				    int src_cpu, int dest_cpu)
{
	struct cfs_rq *src_cfs_rq, *dest_cfs_rq;

	src_cfs_rq = tg->cfs_rq[src_cpu];
	dest_cfs_rq = tg->cfs_rq[dest_cpu];

	return throttled_hierarchy(src_cfs_rq) ||
	       throttled_hierarchy(dest_cfs_rq);
}

/* updated child weight may affect parent so we have to do this bottom up */
static int tg_unthrottle_up(struct task_group *tg, void *data)
{
	struct rq *rq = data;
	struct cfs_rq *cfs_rq = tg->cfs_rq[cpu_of(rq)];

	cfs_rq->throttle_count--;
#ifdef CONFIG_SMP
	if (!cfs_rq->throttle_count) {
		/* adjust cfs_rq_clock_task() */
		cfs_rq->throttled_clock_task_time += rq_clock_task(rq) -
					     cfs_rq->throttled_clock_task;
	}
#endif

	return 0;
}

static int tg_throttle_down(struct task_group *tg, void *data)
{
	struct rq *rq = data;
	struct cfs_rq *cfs_rq = tg->cfs_rq[cpu_of(rq)];

	/* group is entering throttled state, stop time */
	if (!cfs_rq->throttle_count)
		cfs_rq->throttled_clock_task = rq_clock_task(rq);
	cfs_rq->throttle_count++;

	return 0;
}

static void throttle_cfs_rq(struct cfs_rq *cfs_rq)
{
	struct rq *rq = rq_of(cfs_rq);
	struct cfs_bandwidth *cfs_b = tg_cfs_bandwidth(cfs_rq->tg);
	struct sched_entity *se;
	long task_delta, dequeue = 1;

	se = cfs_rq->tg->se[cpu_of(rq_of(cfs_rq))];

	/* freeze hierarchy runnable averages while throttled */
	rcu_read_lock();
	walk_tg_tree_from(cfs_rq->tg, tg_throttle_down, tg_nop, (void *)rq);
	rcu_read_unlock();

	task_delta = cfs_rq->h_nr_running;
	for_each_sched_entity(se) {
		struct cfs_rq *qcfs_rq = cfs_rq_of(se);
		/* throttled entity or throttle-on-deactivate */
		if (!se->on_rq)
			break;

		if (dequeue)
			dequeue_entity(qcfs_rq, se, DEQUEUE_SLEEP);
		qcfs_rq->h_nr_running -= task_delta;

		if (qcfs_rq->load.weight)
			dequeue = 0;
	}

	if (!se)
		sub_nr_running(rq, task_delta);

	cfs_rq->throttled = 1;
	cfs_rq->throttled_clock = rq_clock(rq);
	raw_spin_lock(&cfs_b->lock);
	/*
	 * Add to the _head_ of the list, so that an already-started
	 * distribute_cfs_runtime will not see us
	 */
	list_add_rcu(&cfs_rq->throttled_list, &cfs_b->throttled_cfs_rq);
	if (!cfs_b->timer_active)
		__start_cfs_bandwidth(cfs_b, false);
	raw_spin_unlock(&cfs_b->lock);
}

void unthrottle_cfs_rq(struct cfs_rq *cfs_rq)
{
	struct rq *rq = rq_of(cfs_rq);
	struct cfs_bandwidth *cfs_b = tg_cfs_bandwidth(cfs_rq->tg);
	struct sched_entity *se;
	int enqueue = 1;
	long task_delta;

	se = cfs_rq->tg->se[cpu_of(rq)];

	cfs_rq->throttled = 0;

	update_rq_clock(rq);

	raw_spin_lock(&cfs_b->lock);
	cfs_b->throttled_time += rq_clock(rq) - cfs_rq->throttled_clock;
	list_del_rcu(&cfs_rq->throttled_list);
	raw_spin_unlock(&cfs_b->lock);

	/* update hierarchical throttle state */
	walk_tg_tree_from(cfs_rq->tg, tg_nop, tg_unthrottle_up, (void *)rq);

	if (!cfs_rq->load.weight)
		return;

	task_delta = cfs_rq->h_nr_running;
	for_each_sched_entity(se) {
		if (se->on_rq)
			enqueue = 0;

		cfs_rq = cfs_rq_of(se);
		if (enqueue)
			enqueue_entity(cfs_rq, se, ENQUEUE_WAKEUP);
		cfs_rq->h_nr_running += task_delta;

		if (cfs_rq_throttled(cfs_rq))
			break;
	}

	if (!se)
		add_nr_running(rq, task_delta);

	/* determine whether we need to wake up potentially idle cpu */
	if (rq->curr == rq->idle && rq->cfs.nr_running)
		resched_curr(rq);
}

static u64 distribute_cfs_runtime(struct cfs_bandwidth *cfs_b,
		u64 remaining, u64 expires)
{
	struct cfs_rq *cfs_rq;
	u64 runtime;
	u64 starting_runtime = remaining;

	rcu_read_lock();
	list_for_each_entry_rcu(cfs_rq, &cfs_b->throttled_cfs_rq,
				throttled_list) {
		struct rq *rq = rq_of(cfs_rq);

		raw_spin_lock(&rq->lock);
		if (!cfs_rq_throttled(cfs_rq))
			goto next;

		runtime = -cfs_rq->runtime_remaining + 1;
		if (runtime > remaining)
			runtime = remaining;
		remaining -= runtime;

		cfs_rq->runtime_remaining += runtime;
		cfs_rq->runtime_expires = expires;

		/* we check whether we're throttled above */
		if (cfs_rq->runtime_remaining > 0)
			unthrottle_cfs_rq(cfs_rq);

next:
		raw_spin_unlock(&rq->lock);

		if (!remaining)
			break;
	}
	rcu_read_unlock();

	return starting_runtime - remaining;
}

/*
 * Responsible for refilling a task_group's bandwidth and unthrottling its
 * cfs_rqs as appropriate. If there has been no activity within the last
 * period the timer is deactivated until scheduling resumes; cfs_b->idle is
 * used to track this state.
 */
static int do_sched_cfs_period_timer(struct cfs_bandwidth *cfs_b, int overrun)
{
	u64 runtime, runtime_expires;
	int throttled;

	/* no need to continue the timer with no bandwidth constraint */
	if (cfs_b->quota == RUNTIME_INF)
		goto out_deactivate;

	throttled = !list_empty(&cfs_b->throttled_cfs_rq);
	cfs_b->nr_periods += overrun;

	/*
	 * idle depends on !throttled (for the case of a large deficit), and if
	 * we're going inactive then everything else can be deferred
	 */
	if (cfs_b->idle && !throttled)
		goto out_deactivate;

	/*
	 * if we have relooped after returning idle once, we need to update our
	 * status as actually running, so that other cpus doing
	 * __start_cfs_bandwidth will stop trying to cancel us.
	 */
	cfs_b->timer_active = 1;

	__refill_cfs_bandwidth_runtime(cfs_b);

	if (!throttled) {
		/* mark as potentially idle for the upcoming period */
		cfs_b->idle = 1;
		return 0;
	}

	/* account preceding periods in which throttling occurred */
	cfs_b->nr_throttled += overrun;

	runtime_expires = cfs_b->runtime_expires;

	/*
	 * This check is repeated as we are holding onto the new bandwidth while
	 * we unthrottle. This can potentially race with an unthrottled group
	 * trying to acquire new bandwidth from the global pool. This can result
	 * in us over-using our runtime if it is all used during this loop, but
	 * only by limited amounts in that extreme case.
	 */
	while (throttled && cfs_b->runtime > 0) {
		runtime = cfs_b->runtime;
		raw_spin_unlock(&cfs_b->lock);
		/* we can't nest cfs_b->lock while distributing bandwidth */
		runtime = distribute_cfs_runtime(cfs_b, runtime,
						 runtime_expires);
		raw_spin_lock(&cfs_b->lock);

		throttled = !list_empty(&cfs_b->throttled_cfs_rq);

		cfs_b->runtime -= min(runtime, cfs_b->runtime);
	}

	/*
	 * While we are ensured activity in the period following an
	 * unthrottle, this also covers the case in which the new bandwidth is
	 * insufficient to cover the existing bandwidth deficit.  (Forcing the
	 * timer to remain active while there are any throttled entities.)
	 */
	cfs_b->idle = 0;

	return 0;

out_deactivate:
	cfs_b->timer_active = 0;
	return 1;
}

/* a cfs_rq won't donate quota below this amount */
static const u64 min_cfs_rq_runtime = 1 * NSEC_PER_MSEC;
/* minimum remaining period time to redistribute slack quota */
static const u64 min_bandwidth_expiration = 2 * NSEC_PER_MSEC;
/* how long we wait to gather additional slack before distributing */
static const u64 cfs_bandwidth_slack_period = 5 * NSEC_PER_MSEC;

/*
 * Are we near the end of the current quota period?
 *
 * Requires cfs_b->lock for hrtimer_expires_remaining to be safe against the
 * hrtimer base being cleared by __hrtimer_start_range_ns. In the case of
 * migrate_hrtimers, base is never cleared, so we are fine.
 */
static int runtime_refresh_within(struct cfs_bandwidth *cfs_b, u64 min_expire)
{
	struct hrtimer *refresh_timer = &cfs_b->period_timer;
	u64 remaining;

	/* if the call-back is running a quota refresh is already occurring */
	if (hrtimer_callback_running(refresh_timer))
		return 1;

	/* is a quota refresh about to occur? */
	remaining = ktime_to_ns(hrtimer_expires_remaining(refresh_timer));
	if (remaining < min_expire)
		return 1;

	return 0;
}

static void start_cfs_slack_bandwidth(struct cfs_bandwidth *cfs_b)
{
	u64 min_left = cfs_bandwidth_slack_period + min_bandwidth_expiration;

	/* if there's a quota refresh soon don't bother with slack */
	if (runtime_refresh_within(cfs_b, min_left))
		return;

	start_bandwidth_timer(&cfs_b->slack_timer,
				ns_to_ktime(cfs_bandwidth_slack_period));
}

/* we know any runtime found here is valid as update_curr() precedes return */
static void __return_cfs_rq_runtime(struct cfs_rq *cfs_rq)
{
	struct cfs_bandwidth *cfs_b = tg_cfs_bandwidth(cfs_rq->tg);
	s64 slack_runtime = cfs_rq->runtime_remaining - min_cfs_rq_runtime;

	if (slack_runtime <= 0)
		return;

	raw_spin_lock(&cfs_b->lock);
	if (cfs_b->quota != RUNTIME_INF &&
	    cfs_rq->runtime_expires == cfs_b->runtime_expires) {
		cfs_b->runtime += slack_runtime;

		/* we are under rq->lock, defer unthrottling using a timer */
		if (cfs_b->runtime > sched_cfs_bandwidth_slice() &&
		    !list_empty(&cfs_b->throttled_cfs_rq))
			start_cfs_slack_bandwidth(cfs_b);
	}
	raw_spin_unlock(&cfs_b->lock);

	/* even if it's not valid for return we don't want to try again */
	cfs_rq->runtime_remaining -= slack_runtime;
}

static __always_inline void return_cfs_rq_runtime(struct cfs_rq *cfs_rq)
{
	if (!cfs_bandwidth_used())
		return;

	if (!cfs_rq->runtime_enabled || cfs_rq->nr_running)
		return;

	__return_cfs_rq_runtime(cfs_rq);
}

/*
 * This is done with a timer (instead of inline with bandwidth return) since
 * it's necessary to juggle rq->locks to unthrottle their respective cfs_rqs.
 */
static void do_sched_cfs_slack_timer(struct cfs_bandwidth *cfs_b)
{
	u64 runtime = 0, slice = sched_cfs_bandwidth_slice();
	u64 expires;

	/* confirm we're still not at a refresh boundary */
	raw_spin_lock(&cfs_b->lock);
	if (runtime_refresh_within(cfs_b, min_bandwidth_expiration)) {
		raw_spin_unlock(&cfs_b->lock);
		return;
	}

	if (cfs_b->quota != RUNTIME_INF && cfs_b->runtime > slice)
		runtime = cfs_b->runtime;

	expires = cfs_b->runtime_expires;
	raw_spin_unlock(&cfs_b->lock);

	if (!runtime)
		return;

	runtime = distribute_cfs_runtime(cfs_b, runtime, expires);

	raw_spin_lock(&cfs_b->lock);
	if (expires == cfs_b->runtime_expires)
		cfs_b->runtime -= min(runtime, cfs_b->runtime);
	raw_spin_unlock(&cfs_b->lock);
}

/*
 * When a group wakes up we want to make sure that its quota is not already
 * expired/exceeded, otherwise it may be allowed to steal additional ticks of
 * runtime as update_curr() throttling can not not trigger until it's on-rq.
 */
static void check_enqueue_throttle(struct cfs_rq *cfs_rq)
{
	if (!cfs_bandwidth_used())
		return;

	/* an active group must be handled by the update_curr()->put() path */
	if (!cfs_rq->runtime_enabled || cfs_rq->curr)
		return;

	/* ensure the group is not already throttled */
	if (cfs_rq_throttled(cfs_rq))
		return;

	/* update runtime allocation */
	account_cfs_rq_runtime(cfs_rq, 0);
	if (cfs_rq->runtime_remaining <= 0)
		throttle_cfs_rq(cfs_rq);
}

/* conditionally throttle active cfs_rq's from put_prev_entity() */
static bool check_cfs_rq_runtime(struct cfs_rq *cfs_rq)
{
	if (!cfs_bandwidth_used())
		return false;

	if (likely(!cfs_rq->runtime_enabled || cfs_rq->runtime_remaining > 0))
		return false;

	/*
	 * it's possible for a throttled entity to be forced into a running
	 * state (e.g. set_curr_task), in this case we're finished.
	 */
	if (cfs_rq_throttled(cfs_rq))
		return true;

	throttle_cfs_rq(cfs_rq);
	return true;
}

static enum hrtimer_restart sched_cfs_slack_timer(struct hrtimer *timer)
{
	struct cfs_bandwidth *cfs_b =
		container_of(timer, struct cfs_bandwidth, slack_timer);
	do_sched_cfs_slack_timer(cfs_b);

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart sched_cfs_period_timer(struct hrtimer *timer)
{
	struct cfs_bandwidth *cfs_b =
		container_of(timer, struct cfs_bandwidth, period_timer);
	ktime_t now;
	int overrun;
	int idle = 0;

	raw_spin_lock(&cfs_b->lock);
	for (;;) {
		now = hrtimer_cb_get_time(timer);
		overrun = hrtimer_forward(timer, now, cfs_b->period);

		if (!overrun)
			break;

		idle = do_sched_cfs_period_timer(cfs_b, overrun);
	}
	raw_spin_unlock(&cfs_b->lock);

	return idle ? HRTIMER_NORESTART : HRTIMER_RESTART;
}

void init_cfs_bandwidth(struct cfs_bandwidth *cfs_b)
{
	raw_spin_lock_init(&cfs_b->lock);
	cfs_b->runtime = 0;
	cfs_b->quota = RUNTIME_INF;
	cfs_b->period = ns_to_ktime(default_cfs_period());

	INIT_LIST_HEAD(&cfs_b->throttled_cfs_rq);
	hrtimer_init(&cfs_b->period_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cfs_b->period_timer.function = sched_cfs_period_timer;
	hrtimer_init(&cfs_b->slack_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cfs_b->slack_timer.function = sched_cfs_slack_timer;
}

static void init_cfs_rq_runtime(struct cfs_rq *cfs_rq)
{
	cfs_rq->runtime_enabled = 0;
	INIT_LIST_HEAD(&cfs_rq->throttled_list);
}

/* requires cfs_b->lock, may release to reprogram timer */
void __start_cfs_bandwidth(struct cfs_bandwidth *cfs_b, bool force)
{
	/*
	 * The timer may be active because we're trying to set a new bandwidth
	 * period or because we're racing with the tear-down path
	 * (timer_active==0 becomes visible before the hrtimer call-back
	 * terminates).  In either case we ensure that it's re-programmed
	 */
	while (unlikely(hrtimer_active(&cfs_b->period_timer)) &&
	       hrtimer_try_to_cancel(&cfs_b->period_timer) < 0) {
		/* bounce the lock to allow do_sched_cfs_period_timer to run */
		raw_spin_unlock(&cfs_b->lock);
		cpu_relax();
		raw_spin_lock(&cfs_b->lock);
		/* if someone else restarted the timer then we're done */
		if (!force && cfs_b->timer_active)
			return;
	}

	cfs_b->timer_active = 1;
	start_bandwidth_timer(&cfs_b->period_timer, cfs_b->period);
}

static void destroy_cfs_bandwidth(struct cfs_bandwidth *cfs_b)
{
	hrtimer_cancel(&cfs_b->period_timer);
	hrtimer_cancel(&cfs_b->slack_timer);
}

static void __maybe_unused update_runtime_enabled(struct rq *rq)
{
	struct cfs_rq *cfs_rq;

	for_each_leaf_cfs_rq(rq, cfs_rq) {
		struct cfs_bandwidth *cfs_b = &cfs_rq->tg->cfs_bandwidth;

		raw_spin_lock(&cfs_b->lock);
		cfs_rq->runtime_enabled = cfs_b->quota != RUNTIME_INF;
		raw_spin_unlock(&cfs_b->lock);
	}
}

static void __maybe_unused unthrottle_offline_cfs_rqs(struct rq *rq)
{
	struct cfs_rq *cfs_rq;

	for_each_leaf_cfs_rq(rq, cfs_rq) {
		if (!cfs_rq->runtime_enabled)
			continue;

		/*
		 * clock_task is not advancing so we just need to make sure
		 * there's some valid quota amount
		 */
		cfs_rq->runtime_remaining = 1;
		/*
		 * Offline rq is schedulable till cpu is completely disabled
		 * in take_cpu_down(), so we prevent new cfs throttling here.
		 */
		cfs_rq->runtime_enabled = 0;

		if (cfs_rq_throttled(cfs_rq))
			unthrottle_cfs_rq(cfs_rq);
	}
}

#else /* CONFIG_CFS_BANDWIDTH */
static inline u64 cfs_rq_clock_task(struct cfs_rq *cfs_rq)
{
	return rq_clock_task(rq_of(cfs_rq));
}

static void account_cfs_rq_runtime(struct cfs_rq *cfs_rq, u64 delta_exec) {}
static bool check_cfs_rq_runtime(struct cfs_rq *cfs_rq) { return false; }
static void check_enqueue_throttle(struct cfs_rq *cfs_rq) {}
static __always_inline void return_cfs_rq_runtime(struct cfs_rq *cfs_rq) {}

static inline int cfs_rq_throttled(struct cfs_rq *cfs_rq)
{
	return 0;
}

static inline int throttled_hierarchy(struct cfs_rq *cfs_rq)
{
	return 0;
}

static inline int throttled_lb_pair(struct task_group *tg,
				    int src_cpu, int dest_cpu)
{
	return 0;
}

void init_cfs_bandwidth(struct cfs_bandwidth *cfs_b) {}

#ifdef CONFIG_FAIR_GROUP_SCHED
static void init_cfs_rq_runtime(struct cfs_rq *cfs_rq) {}
#endif

static inline struct cfs_bandwidth *tg_cfs_bandwidth(struct task_group *tg)
{
	return NULL;
}
static inline void destroy_cfs_bandwidth(struct cfs_bandwidth *cfs_b) {}
static inline void update_runtime_enabled(struct rq *rq) {}
static inline void unthrottle_offline_cfs_rqs(struct rq *rq) {}

#endif /* CONFIG_CFS_BANDWIDTH */

/**************************************************
 * CFS operations on tasks:
 */

#ifdef CONFIG_SCHED_HRTICK
static void hrtick_start_fair(struct rq *rq, struct task_struct *p)
{
	struct sched_entity *se = &p->se;
	struct cfs_rq *cfs_rq = cfs_rq_of(se);

	WARN_ON(task_rq(p) != rq);

	if (cfs_rq->nr_running > 1) {
		u64 slice = sched_slice(cfs_rq, se);
		u64 ran = se->sum_exec_runtime - se->prev_sum_exec_runtime;
		s64 delta = slice - ran;

		if (delta < 0) {
			if (rq->curr == p)
				resched_curr(rq);
			return;
		}
		hrtick_start(rq, delta);
	}
}

/*
 * called from enqueue/dequeue and updates the hrtick when the
 * current task is from our class and nr_running is low enough
 * to matter.
 */
static void hrtick_update(struct rq *rq)
{
	struct task_struct *curr = rq->curr;

	if (!hrtick_enabled(rq) || curr->sched_class != &fair_sched_class)
		return;

	if (cfs_rq_of(&curr->se)->nr_running < sched_nr_latency)
		hrtick_start_fair(rq, curr);
}
#else /* !CONFIG_SCHED_HRTICK */
static inline void
hrtick_start_fair(struct rq *rq, struct task_struct *p)
{
}

static inline void hrtick_update(struct rq *rq)
{
}
#endif

#if defined(CONFIG_MTK_SCHED_CMP) || defined(CONFIG_SCHED_HMP)
#define HMP_LB (0x4000)
struct clb_env {
	struct clb_stats bstats;
	struct clb_stats lstats;
	int btarget, ltarget;

	struct cpumask bcpus;
	struct cpumask lcpus;

	unsigned int flags;
	struct mcheck {
		int status; /* Details of this migration check */
		int result; /* Indicate whether we should perform this task migration */
	} mcheck;
};
#endif

#ifdef CONFIG_MTK_SCHED_CMP
/* Check if cpu is in fastest hmp_domain */
unsigned int cmp_up_threshold = 512;
unsigned int cmp_down_threshold = 256;
#endif /* CONFIG_MTK_SCHED_CMP */

#ifdef CONFIG_MTK_SCHED_CMP_TGS
static void sched_tg_enqueue_fair(struct rq *rq, struct task_struct *p)
{
	int id;
	unsigned long flags;
	struct task_struct *tg = p->group_leader;

	if (group_leader_is_empty(p))
		return;
	id = arch_get_cluster_id(rq->cpu);
	if (unlikely(WARN_ON(id < 0)))
		return;

	raw_spin_lock_irqsave(&tg->thread_group_info_lock, flags);
	tg->thread_group_info[id].cfs_nr_running++;
	raw_spin_unlock_irqrestore(&tg->thread_group_info_lock, flags);
}

static void sched_tg_dequeue_fair(struct rq *rq, struct task_struct *p)
{
	int id;
	unsigned long flags;
	struct task_struct *tg = p->group_leader;

	if (group_leader_is_empty(p))
		return;
	id = arch_get_cluster_id(rq->cpu);
	if (unlikely(WARN_ON(id < 0)))
		return;

	raw_spin_lock_irqsave(&tg->thread_group_info_lock, flags);
	tg->thread_group_info[id].cfs_nr_running--;
	raw_spin_unlock_irqrestore(&tg->thread_group_info_lock, flags);
}

#endif

#ifdef CONFIG_SCHED_HMP
/* CPU cluster statistics for task migration control */
#define HMP_GB (0x1000)
#define HMP_SELECT_RQ (0x2000)
#define HMP_LB (0x4000)
#define HMP_MAX_LOAD (NICE_0_LOAD - 1)

/*
 * Returns the current capacity of cpu after applying both
 * cpu and freq scaling.
 */
unsigned long capacity_curr_of(int cpu)
{
	return cpu_rq(cpu)->cpu_capacity_orig *
	       arch_scale_freq_capacity(NULL, cpu)
	       >> SCHED_CAPACITY_SHIFT;
}

static void collect_cluster_stats(struct clb_stats *clbs, struct cpumask *cluster_cpus, int target)
{
#define HMP_RESOLUTION_SCALING (4)
#define hmp_scale_down(w) ((w) >> HMP_RESOLUTION_SCALING)

	/* Update cluster informatics */
	int cpu;

	for_each_cpu(cpu, cluster_cpus) {
		if (cpu_online(cpu)) {
			clbs->ncpu++;
			clbs->ntask += cpu_rq(cpu)->cfs.h_nr_running;
			clbs->load_avg += cpu_rq(cpu)->cfs.avg.loadwop_avg_contrib;
#ifdef CONFIG_SCHED_HMP_PRIO_FILTER
			clbs->nr_normal_prio_task += cfs_nr_normal_prio(cpu);
			clbs->nr_dequeuing_low_prio += cfs_nr_dequeuing_low_prio(cpu);
#endif
		}
	}

	if (!clbs->ncpu || target >= num_possible_cpus() || !cpumask_test_cpu(target, cluster_cpus))
		return;

	/*
	 * Calculate available CPU capacity
	 * Calculate available task space
	 *
	 * Why load ratio should be multiplied by the number of task ?
	 * The task is the entity of scheduling unit so that we should consider
	 * it in scheduler. Only considering task load is not enough.
	 * Thus, multiplying the number of tasks can adjust load ratio to a more
	 * reasonable value.
	 */
	clbs->load_avg /= clbs->ncpu;
	clbs->acap = SCHED_CAPACITY_SCALE - cpu_rq(target)->cfs.avg.loadwop_avg_contrib;
	clbs->scaled_acap = hmp_scale_down(clbs->acap);
	clbs->scaled_atask = cpu_rq(target)->cfs.h_nr_running * cpu_rq(target)->cfs.avg.loadwop_avg_contrib;
	clbs->scaled_atask =  SCHED_CAPACITY_SCALE - clbs->scaled_atask;
	clbs->scaled_atask = hmp_scale_down(clbs->scaled_atask);

	mt_sched_printf(sched_log, "[%s] cpu/cluster:%d/%02lx load/len:%lu/%u stats:%d,%d,%d,%d,%d,%d,%d,%d\n",
					__func__, target, *cpumask_bits(cluster_cpus),
					cpu_rq(target)->cfs.avg.loadwop_avg_contrib,
					cpu_rq(target)->cfs.h_nr_running,
					clbs->ncpu, clbs->ntask, clbs->load_avg, clbs->cpu_capacity,
					clbs->acap, clbs->scaled_acap, clbs->scaled_atask, clbs->threshold);
}

/*
 * Task Dynamic Migration Threshold Adjustment.
 *
 * If the workload between clusters is not balanced, adjust migration
 * threshold in an attempt to move task precisely.
 *
 * Diff. = Max Threshold - Min Threshold
 *
 * Dynamic UP-Threshold =
 *                               B_nacap               B_natask
 * Max Threshold - Diff. x  -----------------  x  -------------------
 *                          B_nacap + L_nacap     B_natask + L_natask
 *
 *
 * Dynamic Down-Threshold =
 *                               L_nacap               L_natask
 * Min Threshold + Diff. x  -----------------  x  -------------------
 *                          B_nacap + L_nacap     B_natask + L_natask
 */
static void adj_threshold(struct clb_env *clbenv)
{
#define POSITIVE(x) ((int)(x) < 0 ? 0 : (x))

	unsigned long b_cap = 0, l_cap = 0;
	int b_nacap, l_nacap, b_natask, l_natask;

	b_cap = clbenv->bstats.cpu_power;
	l_cap = clbenv->lstats.cpu_power;
	b_nacap = POSITIVE(clbenv->bstats.scaled_acap *
					   clbenv->bstats.cpu_power / (clbenv->lstats.cpu_power+1));
	b_natask = POSITIVE(clbenv->bstats.scaled_atask *
						clbenv->bstats.cpu_power / (clbenv->lstats.cpu_power+1));
	l_nacap = POSITIVE(clbenv->lstats.scaled_acap);
	l_natask = POSITIVE(clbenv->lstats.scaled_atask);

	clbenv->bstats.threshold = HMP_MAX_LOAD - HMP_MAX_LOAD * b_nacap * b_natask /
	    ((b_nacap + l_nacap) * (b_natask + l_natask) + 1);
	clbenv->lstats.threshold = HMP_MAX_LOAD * l_nacap * l_natask /
	    ((b_nacap + l_nacap) * (b_natask + l_natask) + 1);

	mt_sched_printf(sched_log, "[%s]\tup/dl:%4d/%4d L(%d:%4lu) b(%d:%4lu)\n", __func__,
					clbenv->bstats.threshold, clbenv->lstats.threshold,
					clbenv->ltarget, l_cap,	clbenv->btarget, b_cap);
}

static void sched_update_clbstats(struct clb_env *clbenv)
{
	/* init cpu power and capacity */
	clbenv->bstats.cpu_power = (int) arch_scale_cpu_capacity(NULL, clbenv->btarget);
	clbenv->lstats.cpu_power = (int) arch_scale_cpu_capacity(NULL, clbenv->ltarget);
	clbenv->lstats.cpu_capacity = SCHED_CAPACITY_SCALE;
	clbenv->bstats.cpu_capacity = SCHED_CAPACITY_SCALE * clbenv->bstats.cpu_power / (clbenv->lstats.cpu_power+1);

	collect_cluster_stats(&clbenv->bstats, &clbenv->bcpus, clbenv->btarget);
	collect_cluster_stats(&clbenv->lstats, &clbenv->lcpus, clbenv->ltarget);
	adj_threshold(clbenv);
}

/*
 * Heterogenous multiprocessor (HMP) optimizations
 *
 * The cpu types are distinguished using a list of hmp_domains
 * which each represent one cpu type using a cpumask.
 * The list is assumed ordered by compute capacity with the
 * fastest domain first.
 */
DEFINE_PER_CPU(struct hmp_domain *, hmp_cpu_domain);

/* Setup hmp_domains */
static int __init hmp_cpu_mask_setup(void)
{
	char buf[64];
	struct hmp_domain *domain;
	struct list_head *pos;
	int dc, cpu;


	pr_debug("Initializing HMP scheduler:\n");

	/* Initialize hmp_domains using platform code */
	arch_get_hmp_domains(&hmp_domains);
	if (list_empty(&hmp_domains)) {
		pr_debug("HMP domain list is empty!\n");
		return 0;
	}

	/* Print hmp_domains */
	dc = 0;
	list_for_each(pos, &hmp_domains) {
		domain = list_entry(pos, struct hmp_domain, hmp_domains);
		cpulist_scnprintf(buf, 64, &domain->possible_cpus);
		pr_warn("  HMP domain %d: %s\n", dc, buf);

		for_each_cpu_mask(cpu, domain->possible_cpus) {
			per_cpu(hmp_cpu_domain, cpu) = domain;
		}
		dc++;
	}

	return 1;
}

static struct hmp_domain *hmp_get_hmp_domain_for_cpu(int cpu)
{
	struct hmp_domain *domain;
	struct list_head *pos;

	list_for_each(pos, &hmp_domains) {
		domain = list_entry(pos, struct hmp_domain, hmp_domains);
		if (cpumask_test_cpu(cpu, &domain->possible_cpus))
			return domain;
	}
	return NULL;
}

static void hmp_online_cpu(int cpu)
{
	struct hmp_domain *domain = hmp_get_hmp_domain_for_cpu(cpu);

	if (domain)
		cpumask_set_cpu(cpu, &domain->cpus);
}

static void hmp_offline_cpu(int cpu)
{
	struct hmp_domain *domain = hmp_get_hmp_domain_for_cpu(cpu);

	if (domain)
		cpumask_clear_cpu(cpu, &domain->cpus);
}

unsigned int hmp_next_up_threshold = 4096;
unsigned int hmp_next_down_threshold = 4096;
#define hmp_last_up_migration(cpu) \
			cpu_rq(cpu)->cfs.avg.hmp_last_up_migration
#define hmp_last_down_migration(cpu) \
			cpu_rq(cpu)->cfs.avg.hmp_last_down_migration
static int hmp_select_task_rq_fair(int sd_flag, struct task_struct *p,
			int prev_cpu, int new_cpu);
#ifdef CONFIG_SCHED_HMP_PLUS
static unsigned int hmp_idle_pull(int this_cpu);
#endif

static inline unsigned int hmp_domain_min_load(struct hmp_domain *hmpd,
						int *min_cpu);

/* Check if cpu is in fastest hmp_domain */
static inline unsigned int hmp_cpu_is_fastest(int cpu)
{
	struct list_head *pos;

	pos = &hmp_cpu_domain(cpu)->hmp_domains;
	return pos == hmp_domains.next;
}

/* Check if cpu is in slowest hmp_domain */
static inline unsigned int hmp_cpu_is_slowest(int cpu)
{
	struct list_head *pos;

	pos = &hmp_cpu_domain(cpu)->hmp_domains;
	return list_is_last(pos, &hmp_domains);
}

/* Next (slower) hmp_domain relative to cpu */
static inline struct hmp_domain *hmp_slower_domain(int cpu)
{
	struct list_head *pos;

	pos = &hmp_cpu_domain(cpu)->hmp_domains;
	return list_entry(pos->next, struct hmp_domain, hmp_domains);
}

/* Previous (faster) hmp_domain relative to cpu */
static inline struct hmp_domain *hmp_faster_domain(int cpu)
{
	struct list_head *pos;

	pos = &hmp_cpu_domain(cpu)->hmp_domains;
	return list_entry(pos->prev, struct hmp_domain, hmp_domains);
}

/*
 * Selects a cpu in previous (faster) hmp_domain
 * Note that cpumask_any_and() returns the first cpu in the cpumask
 */
static inline unsigned int hmp_select_faster_cpu(struct task_struct *tsk,
							int cpu)
{
	int lowest_cpu = num_possible_cpus();
	__always_unused int lowest_ratio = hmp_domain_min_load(hmp_faster_domain(cpu), &lowest_cpu);
	/*
	 * If the lowest-loaded CPU in the domain is allowed by the task affinity
	 * select that one, otherwise select one which is allowed
	 */
	if (lowest_cpu < nr_cpu_ids && cpumask_test_cpu(lowest_cpu, tsk_cpus_allowed(tsk)))
		return lowest_cpu;
	else
		return cpumask_any_and(&hmp_faster_domain(cpu)->cpus,
				tsk_cpus_allowed(tsk));
}

static inline void hmp_next_up_delay(struct sched_entity *se, int cpu)
{
	struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;

	hmp_last_up_migration(cpu) = cfs_rq_clock_task(cfs_rq);
	hmp_last_down_migration(cpu) = 0;
}

static inline void hmp_next_down_delay(struct sched_entity *se, int cpu)
{
	struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;

	hmp_last_down_migration(cpu) = cfs_rq_clock_task(cfs_rq);
	hmp_last_up_migration(cpu) = 0;
}

static inline unsigned int hmp_domain_min_load(struct hmp_domain *hmpd,
						int *min_cpu)
{
	int cpu;
	int min_cpu_runnable_temp = num_possible_cpus();
	unsigned long min_runnable_load = INT_MAX;
	unsigned long contrib;

	for_each_cpu_mask(cpu, hmpd->cpus) {
		/* don't use the divisor in the loop, just at the end */
		contrib = cpu_rq(cpu)->avg.runnable_avg_sum * scale_load_down(1024);
		if (contrib < min_runnable_load) {
			min_runnable_load = contrib;
			min_cpu_runnable_temp = cpu;
		}
	}

	if (min_cpu)
		*min_cpu = min_cpu_runnable_temp;

	/* domain will often have at least one empty CPU */
	return min_runnable_load ? min_runnable_load / (LOAD_AVG_MAX + 1) : 0;
}
#else
static int hmp_select_task_rq_fair(int sd_flag, struct task_struct *p,
			int prev_cpu, int new_cpu) { return new_cpu; }
static void hmp_online_cpu(int cpu) {}
static void hmp_offline_cpu(int cpu) {}
#endif /* CONFIG_SCHED_HMP */


/*
 * The enqueue_task method is called before nr_running is
 * increased. Here we update the fair scheduling stats and
 * then put the task into the rbtree:
 */
static void
enqueue_task_fair(struct rq *rq, struct task_struct *p, int flags)
{
	struct cfs_rq *cfs_rq;
	struct sched_entity *se = &p->se;

	for_each_sched_entity(se) {
		if (se->on_rq)
			break;
		cfs_rq = cfs_rq_of(se);
		enqueue_entity(cfs_rq, se, flags);

		/*
		 * end evaluation on encountering a throttled cfs_rq
		 *
		 * note: in the case of encountering a throttled cfs_rq we will
		 * post the final h_nr_running increment below.
		*/
		if (cfs_rq_throttled(cfs_rq))
			break;
		cfs_rq->h_nr_running++;

		flags = ENQUEUE_WAKEUP;
	}

	for_each_sched_entity(se) {
		cfs_rq = cfs_rq_of(se);
		cfs_rq->h_nr_running++;

		if (cfs_rq_throttled(cfs_rq))
			break;

		update_cfs_shares(cfs_rq);
		update_entity_load_avg(se, 1);
	}

	if (!se) {
		update_rq_runnable_avg(rq, rq->nr_running);
		add_nr_running(rq, 1);
#ifndef CONFIG_CFS_BANDWIDTH
		BUG_ON(rq->cfs.nr_running > rq->cfs.h_nr_running);
#endif
	}
	hrtick_update(rq);
#ifdef CONFIG_MTK_SCHED_CMP_TGS
	sched_tg_enqueue_fair(rq, p);
#endif
#ifdef CONFIG_HMP_TRACER
	trace_sched_runqueue_length(rq->cpu, rq->nr_running);
	trace_sched_cfs_length(rq->cpu, rq->cfs.h_nr_running);
#endif
}

static void set_next_buddy(struct sched_entity *se);

/*
 * The dequeue_task method is called before nr_running is
 * decreased. We remove the task from the rbtree and
 * update the fair scheduling stats:
 */
static void dequeue_task_fair(struct rq *rq, struct task_struct *p, int flags)
{
	struct cfs_rq *cfs_rq;
	struct sched_entity *se = &p->se;
	int task_sleep = flags & DEQUEUE_SLEEP;

	for_each_sched_entity(se) {
		cfs_rq = cfs_rq_of(se);
		dequeue_entity(cfs_rq, se, flags);

		/*
		 * end evaluation on encountering a throttled cfs_rq
		 *
		 * note: in the case of encountering a throttled cfs_rq we will
		 * post the final h_nr_running decrement below.
		*/
		if (cfs_rq_throttled(cfs_rq))
			break;
		cfs_rq->h_nr_running--;

		/* Don't dequeue parent if it has other entities besides us */
		if (cfs_rq->load.weight) {
			/*
			 * Bias pick_next to pick a task from this cfs_rq, as
			 * p is sleeping when it is within its sched_slice.
			 */
			if (task_sleep && parent_entity(se))
				set_next_buddy(parent_entity(se));

			/* avoid re-evaluating load for this entity */
			se = parent_entity(se);
			break;
		}
		flags |= DEQUEUE_SLEEP;
	}

	for_each_sched_entity(se) {
		cfs_rq = cfs_rq_of(se);
		cfs_rq->h_nr_running--;

		if (cfs_rq_throttled(cfs_rq))
			break;

		update_cfs_shares(cfs_rq);
		update_entity_load_avg(se, 1);
	}

	if (!se) {
		sub_nr_running(rq, 1);
#ifndef CONFIG_CFS_BANDWIDTH
		BUG_ON(rq->cfs.nr_running > rq->cfs.h_nr_running);
#endif
		update_rq_runnable_avg(rq, 1);
	}
	hrtick_update(rq);
#ifdef CONFIG_MTK_SCHED_CMP_TGS
	sched_tg_dequeue_fair(rq, p);
#endif
#ifdef CONFIG_HMP_TRACER
	trace_sched_runqueue_length(rq->cpu, rq->nr_running);
	trace_sched_cfs_length(rq->cpu, rq->cfs.h_nr_running);
#endif
}

#ifdef CONFIG_SMP
/* Used instead of source_load when we know the type == 0 */
static unsigned long weighted_cpuload(const int cpu)
{
	return cpu_rq(cpu)->cfs.runnable_load_avg;
}

/*
 * Return a low guess at the load of a migration-source cpu weighted
 * according to the scheduling class and "nice" value.
 *
 * We want to under-estimate the load of migration sources, to
 * balance conservatively.
 */
static unsigned long source_load(int cpu, int type)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long total = weighted_cpuload(cpu);

	if (type == 0 || !sched_feat(LB_BIAS))
		return total;

	return min(rq->cpu_load[type-1], total);
}

/*
 * Return a high guess at the load of a migration-target cpu weighted
 * according to the scheduling class and "nice" value.
 */
static unsigned long target_load(int cpu, int type)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long total = weighted_cpuload(cpu);

	if (type == 0 || !sched_feat(LB_BIAS))
		return total;

	return max(rq->cpu_load[type-1], total);
}

static unsigned long capacity_of(int cpu)
{
	return cpu_rq(cpu)->cpu_capacity;
}

static unsigned long capacity_orig_of(int cpu)
{
	return cpu_rq(cpu)->cpu_capacity_orig;
}

static unsigned long cpu_avg_load_per_task(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long nr_running = ACCESS_ONCE(rq->cfs.h_nr_running);
	unsigned long load_avg = rq->cfs.runnable_load_avg;

	if (nr_running)
		return load_avg / nr_running;

	return 0;
}

static void record_wakee(struct task_struct *p)
{
	/*
	 * Rough decay (wiping) for cost saving, don't worry
	 * about the boundary, really active task won't care
	 * about the loss.
	 */
	if (time_after(jiffies, current->wakee_flip_decay_ts + HZ)) {
		current->wakee_flips >>= 1;
		current->wakee_flip_decay_ts = jiffies;
	}

	if (current->last_wakee != p) {
		current->last_wakee = p;
		current->wakee_flips++;
	}
}

static void task_waking_fair(struct task_struct *p)
{
	struct sched_entity *se = &p->se;
	struct cfs_rq *cfs_rq = cfs_rq_of(se);
	u64 min_vruntime;

#ifndef CONFIG_64BIT
	u64 min_vruntime_copy;

	do {
		min_vruntime_copy = cfs_rq->min_vruntime_copy;
		smp_rmb();
		min_vruntime = cfs_rq->min_vruntime;
	} while (min_vruntime != min_vruntime_copy);
#else
	min_vruntime = cfs_rq->min_vruntime;
#endif

	se->vruntime -= min_vruntime;
	record_wakee(p);
}

#ifdef CONFIG_FAIR_GROUP_SCHED
/*
 * effective_load() calculates the load change as seen from the root_task_group
 *
 * Adding load to a group doesn't make a group heavier, but can cause movement
 * of group shares between cpus. Assuming the shares were perfectly aligned one
 * can calculate the shift in shares.
 *
 * Calculate the effective load difference if @wl is added (subtracted) to @tg
 * on this @cpu and results in a total addition (subtraction) of @wg to the
 * total group weight.
 *
 * Given a runqueue weight distribution (rw_i) we can compute a shares
 * distribution (s_i) using:
 *
 *   s_i = rw_i / \Sum rw_j						(1)
 *
 * Suppose we have 4 CPUs and our @tg is a direct child of the root group and
 * has 7 equal weight tasks, distributed as below (rw_i), with the resulting
 * shares distribution (s_i):
 *
 *   rw_i = {   2,   4,   1,   0 }
 *   s_i  = { 2/7, 4/7, 1/7,   0 }
 *
 * As per wake_affine() we're interested in the load of two CPUs (the CPU the
 * task used to run on and the CPU the waker is running on), we need to
 * compute the effect of waking a task on either CPU and, in case of a sync
 * wakeup, compute the effect of the current task going to sleep.
 *
 * So for a change of @wl to the local @cpu with an overall group weight change
 * of @wl we can compute the new shares distribution (s'_i) using:
 *
 *   s'_i = (rw_i + @wl) / (@wg + \Sum rw_j)				(2)
 *
 * Suppose we're interested in CPUs 0 and 1, and want to compute the load
 * differences in waking a task to CPU 0. The additional task changes the
 * weight and shares distributions like:
 *
 *   rw'_i = {   3,   4,   1,   0 }
 *   s'_i  = { 3/8, 4/8, 1/8,   0 }
 *
 * We can then compute the difference in effective weight by using:
 *
 *   dw_i = S * (s'_i - s_i)						(3)
 *
 * Where 'S' is the group weight as seen by its parent.
 *
 * Therefore the effective change in loads on CPU 0 would be 5/56 (3/8 - 2/7)
 * times the weight of the group. The effect on CPU 1 would be -4/56 (4/8 -
 * 4/7) times the weight of the group.
 */
static long effective_load(struct task_group *tg, int cpu, long wl, long wg)
{
	struct sched_entity *se = tg->se[cpu];

	if (!tg->parent)	/* the trivial, non-cgroup case */
		return wl;

	for_each_sched_entity(se) {
		long w, W;

		tg = se->my_q->tg;

		/*
		 * W = @wg + \Sum rw_j
		 */
		W = wg + calc_tg_weight(tg, se->my_q);

		/*
		 * w = rw_i + @wl
		 */
		w = se->my_q->load.weight + wl;

		/*
		 * wl = S * s'_i; see (2)
		 */
		if (W > 0 && w < W)
			wl = (w * tg->shares) / W;
		else
			wl = tg->shares;

		/*
		 * Per the above, wl is the new se->load.weight value; since
		 * those are clipped to [MIN_SHARES, ...) do so now. See
		 * calc_cfs_shares().
		 */
		if (wl < MIN_SHARES)
			wl = MIN_SHARES;

		/*
		 * wl = dw_i = S * (s'_i - s_i); see (3)
		 */
		wl -= se->load.weight;

		/*
		 * Recursively apply this logic to all parent groups to compute
		 * the final effective load change on the root group. Since
		 * only the @tg group gets extra weight, all parent groups can
		 * only redistribute existing shares. @wl is the shift in shares
		 * resulting from this level per the above.
		 */
		wg = 0;
	}

	return wl;
}
#else

static long effective_load(struct task_group *tg, int cpu, long wl, long wg)
{
	return wl;
}

#endif

static int wake_wide(struct task_struct *p)
{
	int factor = this_cpu_read(sd_llc_size);

	/*
	 * Yeah, it's the switching-frequency, could means many wakee or
	 * rapidly switch, use factor here will just help to automatically
	 * adjust the loose-degree, so bigger node will lead to more pull.
	 */
	if (p->wakee_flips > factor) {
		/*
		 * wakee is somewhat hot, it needs certain amount of cpu
		 * resource, so if waker is far more hot, prefer to leave
		 * it alone.
		 */
		if (current->wakee_flips > (factor * p->wakee_flips))
			return 1;
	}

	return 0;
}

static int wake_affine(struct sched_domain *sd, struct task_struct *p, int sync)
{
	s64 this_load, load;
	s64 this_eff_load, prev_eff_load;
	int idx, this_cpu, prev_cpu;
	struct task_group *tg;
	unsigned long weight;
	int balanced;

	/*
	 * If we wake multiple tasks be careful to not bounce
	 * ourselves around too much.
	 */
	if (wake_wide(p))
		return 0;

	idx	  = sd->wake_idx;
	this_cpu  = smp_processor_id();
	prev_cpu  = task_cpu(p);
	load	  = source_load(prev_cpu, idx);
	this_load = target_load(this_cpu, idx);

	/*
	 * If sync wakeup then subtract the (maximum possible)
	 * effect of the currently running task from the load
	 * of the current CPU:
	 */
	if (sync) {
		tg = task_group(current);
		weight = current->se.load.weight;

		this_load += effective_load(tg, this_cpu, -weight, -weight);
		load += effective_load(tg, prev_cpu, 0, -weight);
	}

	tg = task_group(p);
	weight = p->se.load.weight;

	/*
	 * In low-load situations, where prev_cpu is idle and this_cpu is idle
	 * due to the sync cause above having dropped this_load to 0, we'll
	 * always have an imbalance, but there's really nothing you can do
	 * about that, so that's good too.
	 *
	 * Otherwise check if either cpus are near enough in load to allow this
	 * task to be woken on this_cpu.
	 */
	this_eff_load = 100;
	this_eff_load *= capacity_of(prev_cpu);

	prev_eff_load = 100 + (sd->imbalance_pct - 100) / 2;
	prev_eff_load *= capacity_of(this_cpu);

	if (this_load > 0) {
		this_eff_load *= this_load +
			effective_load(tg, this_cpu, weight, weight);

		prev_eff_load *= load + effective_load(tg, prev_cpu, 0, weight);
	}

	balanced = this_eff_load <= prev_eff_load;

	schedstat_inc(p, se.statistics.nr_wakeups_affine_attempts);

	if (!balanced)
		return 0;

	schedstat_inc(sd, ttwu_move_affine);
	schedstat_inc(p, se.statistics.nr_wakeups_affine);

	return 1;
}

#ifdef CONFIG_MT_SCHED_INTEROP
#define MT_RT_LOAD (2*1023*scale_load_down(scale_load(prio_to_weight[0])))
static inline unsigned long mt_rt_load(int cpu)
{
	return cpu_rq(cpu)->rt.rt_nr_running * MT_RT_LOAD;
}
#endif

/*
 * find_idlest_group finds and returns the least busy CPU group within the
 * domain.
 */
static struct sched_group *
find_idlest_group(struct sched_domain *sd, struct task_struct *p,
		  int this_cpu, int sd_flag)
{
	struct sched_group *idlest = NULL, *group = sd->groups;
	unsigned long min_load = ULONG_MAX, this_load = 0;
	int load_idx = sd->forkexec_idx;
	int imbalance = 100 + (sd->imbalance_pct-100)/2;

	if (sd_flag & SD_BALANCE_WAKE)
		load_idx = sd->wake_idx;

	do {
		unsigned long load, avg_load;
		int local_group;
		int i;

		/* Skip over this group if it has no CPUs allowed */
		if (!cpumask_intersects(sched_group_cpus(group),
					tsk_cpus_allowed(p)))
			continue;

		local_group = cpumask_test_cpu(this_cpu,
					       sched_group_cpus(group));

		/* Tally up the load of all CPUs in the group */
		avg_load = 0;

		for_each_cpu(i, sched_group_cpus(group)) {
			/* Bias balancing toward cpus of our domain */
			if (local_group)
				load = source_load(i, load_idx);
			else
				load = target_load(i, load_idx);

#ifdef CONFIG_MT_SCHED_INTEROP
			load += mt_rt_load(i);
#endif
#ifdef CONFIG_MT_LOAD_BALANCE_ENHANCEMENT
			/* sched: scaling load with CPU capacity */
			avg_load += (load * capacity_orig_of(i)) >> SCHED_CAPACITY_SHIFT;
#else
			avg_load += load;
#endif
			mt_sched_printf(sched_log, "find_idlest_group cpu=%d avg=%lu",
				i, avg_load);
		}

		/* Adjust by relative CPU capacity of the group */
		avg_load = (avg_load * SCHED_CAPACITY_SCALE) / group->sgc->capacity;

		if (local_group) {
			this_load = avg_load;
			mt_sched_printf(sched_log, "find_idlest_group this_load=%lu",
				this_load);
		} else if (avg_load < min_load) {
			min_load = avg_load;
			idlest = group;
			mt_sched_printf(sched_log, "find_idlest_group min_load=%lu",
				min_load);
		}
	} while (group = group->next, group != sd->groups);

	if (!idlest || 100 * this_load < imbalance * min_load) {
		mt_sched_printf(sched_log, "find_idlest_group fail this_load=%lu min_load=%lu, imbalance=%d",
				this_load, min_load, imbalance);
		return NULL;
	}
	return idlest;
}

/*
 * find_idlest_cpu - find the idlest cpu among the cpus in group.
 */
static int
find_idlest_cpu(struct sched_group *group, struct task_struct *p, int this_cpu)
{
	unsigned long load, min_load = ULONG_MAX;
	unsigned int min_exit_latency = UINT_MAX;
	u64 latest_idle_timestamp = 0;
	int least_loaded_cpu = this_cpu;
	int shallowest_idle_cpu = -1;
	int i;

	/* Traverse only the allowed CPUs */
	for_each_cpu_and(i, sched_group_cpus(group), tsk_cpus_allowed(p)) {
		if (idle_cpu(i)) {
			struct rq *rq = cpu_rq(i);
			struct cpuidle_state *idle = idle_get_state(rq);
			if (idle && idle->exit_latency < min_exit_latency) {
				/*
				 * We give priority to a CPU whose idle state
				 * has the smallest exit latency irrespective
				 * of any idle timestamp.
				 */
				min_exit_latency = idle->exit_latency;
				latest_idle_timestamp = rq->idle_stamp;
				shallowest_idle_cpu = i;
			} else if ((!idle || idle->exit_latency == min_exit_latency) &&
				   rq->idle_stamp > latest_idle_timestamp) {
				/*
				 * If equal or no active idle state, then
				 * the most recently idled CPU might have
				 * a warmer cache.
				 */
				latest_idle_timestamp = rq->idle_stamp;
				shallowest_idle_cpu = i;
			}
		} else {
			load = weighted_cpuload(i);
#ifdef CONFIG_MT_SCHED_INTEROP
			load += mt_rt_load(i);
#endif
			if (load < min_load || (load == min_load && i == this_cpu)) {
				min_load = load;
				least_loaded_cpu = i;
			}
		}
	}

	return shallowest_idle_cpu != -1 ? shallowest_idle_cpu : least_loaded_cpu;
}

/*
 * Try and locate an idle CPU in the sched_domain.
 */
static int select_idle_sibling(struct task_struct *p, int target)
{
	struct sched_domain *sd;
	struct sched_group *sg;
	int i = task_cpu(p);

	if (idle_cpu(target))
		return target;

	/*
	 * If the prevous cpu is cache affine and idle, don't be stupid.
	 */
	if (i != target && cpus_share_cache(i, target) && idle_cpu(i))
		return i;

	/*
	 * Otherwise, iterate the domains and find an elegible idle cpu.
	 */
	sd = rcu_dereference(per_cpu(sd_llc, target));
	for_each_lower_domain(sd) {
		sg = sd->groups;
		do {
			if (!cpumask_intersects(sched_group_cpus(sg),
						tsk_cpus_allowed(p)))
				goto next;

			for_each_cpu(i, sched_group_cpus(sg)) {
				if (i == target || !idle_cpu(i))
					goto next;
			}

			target = cpumask_first_and(sched_group_cpus(sg),
					tsk_cpus_allowed(p));
			goto done;
next:
			sg = sg->next;
		} while (sg != sd->groups);
	}
done:
	return target;
}


/*
 * @p: the task want to be located at.
 * @prev_cpu: last cpu p located at.
 *
 * Return:
 *
 * cpu id or
 * nr_cpu_ids if target CPU is not found
 */
static int find_best_idle_cpu(struct task_struct *p, int prev_cpu)
{
	int j = 0, found = nr_cpu_ids;
	struct cpumask allowed_mask;
	unsigned long cap, max_cap = 0;

	/* single cluster SMP architecture */
	if (!arch_is_multi_cluster() && idle_cpu(prev_cpu))
		return prev_cpu;

	/* return nr_cpu_ids to go cluster-based cpu selection for multi-cluster SMP architecture */
	if (arch_is_multi_cluster() && arch_is_smp())
		return nr_cpu_ids;

	cpumask_and(&allowed_mask, cpu_online_mask, tsk_cpus_allowed(p));
	for_each_cpu(j, &allowed_mask) {
		if (!idle_cpu(j))
			continue;

		if (!arch_is_smp()) { /* multi-cluster HMP architecture */
			cap = arch_get_max_cpu_capacity(j);
			if (cap > max_cap) {
				found = j;
				max_cap = cap;
				mt_sched_printf(sched_lb,
					"found=%d max_cap=%lu allowed_mask=%lu",
					found, max_cap, allowed_mask.bits[0]);
			}
		} else {
			found = j;
			break;
		}
	}

	return found;
}

#ifdef CONFIG_MTK_SCHED_CMP_TGS_WAKEUP
static int cmp_select_task_rq_fair(struct task_struct *p, int prev_cpu)
{
	int i, j;
	int in_prev, prev_cluster, target_cpu;
	int max_tg_cnt, tg_cnt;
	int max_idle_cnt, idle_cnt;
	struct cpumask max_idle_mask, idle_mask;
	struct cpumask cls_cpus, allowed_mask;
	int num_cluster;

	in_prev = 0;
	max_tg_cnt = 0;
	max_idle_cnt = 0;
	cpumask_clear(&max_idle_mask);

	/* best idle cpu selection */
	target_cpu = find_best_idle_cpu(p, prev_cpu);
	if (target_cpu < nr_cpu_ids)
		return target_cpu;

	/* cluster-based cpu selection */
	num_cluster = arch_get_nr_clusters();
	for (i = 0; i < num_cluster; i++) {

		cpumask_clear(&idle_mask);
		get_cluster_cpus(&cls_cpus, i, true);
		cpumask_and(&allowed_mask, &cls_cpus, tsk_cpus_allowed(p));

		if (!cpumask_weight(&allowed_mask))
			continue;

		for_each_cpu(j, &allowed_mask) {
			if (idle_cpu(j))
				cpumask_set_cpu(j, &idle_mask);
		}

		if (group_leader_is_empty(p))
			continue;

		tg_cnt = p->group_leader->thread_group_info[i].nr_running;
		mt_sched_printf(sched_cmp,
			"wakeup pid=%d name=%s load=%ld, cluster=%d allowed_cpu=%02lx, idle_cpu=%02lx",
			p->pid, p->comm, p->se.avg.loadwop_avg_contrib, i, *cpumask_bits(&allowed_mask),
			*cpumask_bits(&idle_mask));

		mt_sched_printf(sched_cmp, "tg_cnt=%d max_tg_cnt=%d, onlineCPU=%02lx",
			tg_cnt, max_tg_cnt, *cpumask_bits(cpu_online_mask));

		idle_cnt = cpumask_weight(&idle_mask);
		if (0 == idle_cnt)
			continue;

		prev_cluster = (i == arch_get_cluster_id(prev_cpu)) ? 1 : 0;

		if (tg_cnt > 0) {
			/*  1. prefer the cpu in a cluster with more affined threads inside  */
			if ((tg_cnt > max_tg_cnt) || ((tg_cnt == max_tg_cnt) && prev_cluster)) {
				in_prev = prev_cluster;
				max_idle_cnt = idle_cnt;
				max_tg_cnt = tg_cnt;
				cpumask_copy(&max_idle_mask, &idle_mask);
			}
		} else if (0 == max_tg_cnt) {
			/* 2. otherwise, prefer the cpu in a cluster with more idle CPUs */
			if ((idle_cnt > max_idle_cnt) || ((idle_cnt == max_idle_cnt) && prev_cluster)) {
				in_prev = prev_cluster;
				max_idle_cnt = idle_cnt;
				cpumask_copy(&max_idle_mask, &idle_mask);
			}

		}
		mt_sched_printf(sched_cmp,
		"wakeup %d %s cluster=%d, max_idle_cpu=%02lx max_tg_cnt=%d max_idle_cnt=%d prev_cpu=%d in_prev=%d",
			p->pid, p->comm, i, *cpumask_bits(&max_idle_mask), max_tg_cnt, max_idle_cnt, prev_cpu, in_prev);
	}

	/* no idle CPU */
	if (!max_idle_cnt)
		return nr_cpu_ids;

	if (in_prev && idle_cpu(prev_cpu)) {
		mt_sched_printf(sched_cmp, "wakeup %d %s prev_cpu=%d", p->pid, p->comm, prev_cpu);
		return prev_cpu;
	}

	for_each_cpu(j, &max_idle_mask) {
		if (idle_cpu(j)) {
			mt_sched_printf(sched_cmp, "wakeup %d %s idle_cpu=%d", p->pid, p->comm, j);
			return j;
		}
	}

	return nr_cpu_ids;

}
#endif

static int mt_select_task_rq_fair(struct task_struct *p, int prev_cpu)
{
#ifdef CONFIG_MTK_SCHED_CMP_TGS_WAKEUP
	return cmp_select_task_rq_fair(p, prev_cpu);
#else
	return find_best_idle_cpu(p, prev_cpu);
#endif
}

#ifdef CONFIG_MTK_SCHED_TRACERS
#define LB_RESET		0
#define LB_AFFINITY		0x10
#define LB_BUDDY		0x20
#define LB_FORK			0x30
#define LB_CMP_SHIFT	8
#define LB_CMP			0x4000
#define LB_SMP_SHIFT	16
#define LB_SMP			0x500000
#define LB_HMP_SHIFT	24
#define LB_HMP			0x60000000
#endif

/*
 * select_task_rq_fair: Select target runqueue for the waking task in domains
 * that have the 'sd_flag' flag set. In practice, this is SD_BALANCE_WAKE,
 * SD_BALANCE_FORK, or SD_BALANCE_EXEC.
 *
 * Balances load by selecting the idlest cpu in the idlest group, or under
 * certain conditions an idle sibling cpu if the domain has SD_WAKE_AFFINE set.
 *
 * Returns the target cpu number.
 *
 * preempt must be disabled.
 */
static int
select_task_rq_fair(struct task_struct *p, int prev_cpu, int sd_flag, int wake_flags)
{
	struct sched_domain *tmp, *affine_sd = NULL, *sd = NULL;
	int cpu = smp_processor_id();
	int new_cpu = cpu;
	int want_affine = 0;
	int sync = wake_flags & WF_SYNC;
	int prefer_cpu;
#ifdef CONFIG_MTK_SCHED_TRACERS
	int policy = 0;
#endif
	if (p->nr_cpus_allowed == 1) {
#ifdef CONFIG_MTK_SCHED_TRACERS
		trace_sched_select_task_rq(p, (LB_AFFINITY | prev_cpu), prev_cpu, prev_cpu);
#endif
		return prev_cpu;
	}

#ifdef CONFIG_HMP_PACK_SMALL_TASK
	if (check_pack_buddy(cpu, p))
		return per_cpu(sd_pack_buddy, cpu);
#endif /* CONFIG_HMP_PACK_SMALL_TASK */

	/* always put non-kernel forking tasks on a big domain */
	if (sched_feat(SCHED_HMP) && p->mm && (sd_flag & SD_BALANCE_FORK)) {
		/* TODO: This part should be functionalized */
#ifdef CONFIG_SCHED_HMP
		if (hmp_cpu_is_fastest(prev_cpu)) {
			struct hmp_domain *hmpdom = list_entry(&hmp_cpu_domain(prev_cpu)->hmp_domains,
						struct hmp_domain, hmp_domains);
			__always_unused int lowest_ratio = hmp_domain_min_load(hmpdom, &new_cpu);

			if (new_cpu < nr_cpu_ids && cpumask_test_cpu(new_cpu, tsk_cpus_allowed(p))) {
#ifdef CONFIG_MTK_SCHED_TRACERS
				trace_sched_select_task_rq(p, (LB_FORK | new_cpu), prev_cpu, new_cpu);
#endif
				return new_cpu;
			}
			new_cpu = cpumask_any_and(&hmp_faster_domain(cpu)->cpus,
						  tsk_cpus_allowed(p));
			if (new_cpu < nr_cpu_ids) {
#ifdef CONFIG_MTK_SCHED_TRACERS
				trace_sched_select_task_rq(p, (LB_FORK | new_cpu), prev_cpu, new_cpu);
#endif
				return new_cpu;
			}
		} else {
			new_cpu = hmp_select_faster_cpu(p, prev_cpu);
			if (new_cpu < nr_cpu_ids) {
#ifdef CONFIG_MTK_SCHED_TRACERS
				trace_sched_select_task_rq(p, (LB_FORK | new_cpu), prev_cpu, new_cpu);
#endif
				return new_cpu;
			}
		}
		/* to recover new_cpu value */
		if (new_cpu >= nr_cpu_ids)
			new_cpu = cpu;
#endif /* CONFIG_SCHED_HMP */
	}

	if (sd_flag & SD_BALANCE_WAKE)
		want_affine = cpumask_test_cpu(cpu, tsk_cpus_allowed(p));

	prefer_cpu = mt_select_task_rq_fair(p, prev_cpu);
	if (prefer_cpu < nr_cpu_ids) {
		cpu = prefer_cpu;
		new_cpu = prefer_cpu;
#ifdef CONFIG_MTK_SCHED_TRACERS
		policy |= (new_cpu << LB_CMP_SHIFT);
		policy |= LB_CMP;
#endif
		mt_sched_printf(sched_log, "cmp/interop wakeup %d %s to cpu %d",
				p->pid, p->comm, cpu);
		goto mt_found;
	}

	rcu_read_lock();
	for_each_domain(cpu, tmp) {
		mt_sched_printf(sched_log, "wakeup %d %s tmp->flags=%x, cpu=%d, prev_cpu=%d, new_cpu=%d",
				p->pid, p->comm, tmp->flags, cpu, prev_cpu, new_cpu);

		if (!(tmp->flags & SD_LOAD_BALANCE))
			continue;

		/*
		 * If both cpu and prev_cpu are part of this domain,
		 * cpu is a valid SD_WAKE_AFFINE target.
		 */
		if (want_affine && (tmp->flags & SD_WAKE_AFFINE) &&
		    cpumask_test_cpu(prev_cpu, sched_domain_span(tmp))) {
			affine_sd = tmp;
			break;
		}

		if (tmp->flags & sd_flag)
			sd = tmp;
	}

	mt_sched_printf(sched_log, "wakeup %d %s sd=%p affine_sd=%p cpu=%d prev_cpu=%d",
		p->pid, p->comm, sd, affine_sd, cpu, prev_cpu);
	if (affine_sd && cpu != prev_cpu && wake_affine(affine_sd, p, sync))
		prev_cpu = cpu;

#ifdef CONFIG_MT_LOAD_BALANCE_ENHANCEMENT
	/* sched: only when sd is null, not to do wakeup balance */
	if (!sd) {
#endif
		if (sd_flag & SD_BALANCE_WAKE) {
			new_cpu = select_idle_sibling(p, prev_cpu);
			mt_sched_printf(sched_log, "wakeup %d %s new_cpu=%d", p->pid, p->comm, new_cpu);
			goto unlock;
		}
#ifdef CONFIG_MT_LOAD_BALANCE_ENHANCEMENT
	} else
	while (sd) {
#else
	while (sd) {
#endif
		struct sched_group *group;
		int weight;

		mt_sched_printf(sched_log, "wakeup %d %s find_idlest_group cpu=%d sd->flags=%x sd_flag=%x",
				p->pid, p->comm, cpu, sd->flags, sd_flag);
		if (!(sd->flags & sd_flag)) {
			sd = sd->child;
			continue;
		}

		mt_sched_printf(sched_log, "wakeup %d %s find_idlest_group cpu=%d",
				p->pid, p->comm, cpu);
		group = find_idlest_group(sd, p, cpu, sd_flag);
		if (!group) {
			sd = sd->child;
			mt_sched_printf(sched_log, "wakeup %d %s find_idlest_group child",
					p->pid, p->comm);
			continue;
		}

		new_cpu = find_idlest_cpu(group, p, cpu);
		if (new_cpu == -1 || new_cpu == cpu) {
			/* Now try balancing at a lower domain level of cpu */
			sd = sd->child;
			mt_sched_printf(sched_log, "wakeup %d %s find_idlest_cpu sd->child=%p",
					p->pid, p->comm, sd);
			continue;
		}

		/* Now try balancing at a lower domain level of new_cpu */
		mt_sched_printf(sched_log, "wakeup %d %s find_idlest_cpu cpu=%d sd=%p",
				p->pid, p->comm, new_cpu, sd);
		cpu = new_cpu;
		weight = sd->span_weight;
		sd = NULL;
		for_each_domain(cpu, tmp) {
			if (weight <= tmp->span_weight)
				break;
			if (tmp->flags & sd_flag)
				sd = tmp;
			mt_sched_printf(sched_log, "wakeup %d %s sd=%p weight=%d, tmp->span_weight=%d",
				p->pid, p->comm, sd, weight, tmp->span_weight);
		}
		/* while loop will break here if sd == NULL */
	}

#ifdef CONFIG_MTK_SCHED_TRACERS
	policy |= (new_cpu << LB_SMP_SHIFT);
	policy |= LB_SMP;
#endif

unlock:
	rcu_read_unlock();
	mt_sched_printf(sched_log, "wakeup %d %s new_cpu=%x", p->pid, p->comm, new_cpu);

mt_found:
	if (sched_feat(SCHED_HMP)) {
		new_cpu = hmp_select_task_rq_fair(sd_flag, p, prev_cpu, new_cpu);
#ifdef CONFIG_MTK_SCHED_TRACERS
		policy |= (new_cpu << LB_HMP_SHIFT);
		policy |= LB_HMP;
#endif
	}
#ifdef CONFIG_MTK_SCHED_TRACERS
	trace_sched_select_task_rq(p, policy, prev_cpu, new_cpu);
#endif
	return new_cpu;
}
/*
 * get_cpu_usage returns the amount of capacity of a CPU that is used by CFS
 * tasks. The unit of the return value must capacity so we can compare the
 * usage with the capacity of the CPU that is available for CFS task (ie
 * cpu_capacity).
 * cfs.utilization_load_avg is the sum of running time of runnable tasks on a
 * CPU. It represents the amount of utilization of a CPU in the range
 * [0..SCHED_LOAD_SCALE].  The usage of a CPU can't be higher than the full
 * capacity of the CPU because it's about the running time on this CPU.
 * Nevertheless, cfs.utilization_load_avg can be higher than SCHED_LOAD_SCALE
 * because of unfortunate rounding in avg_period and running_load_avg or just
 * after migrating tasks until the average stabilizes with the new running
 * time. So we need to check that the usage stays into the range
 * [0..cpu_capacity_orig] and cap if necessary.
 * Without capping the usage, a group could be seen as overloaded (CPU0 usage
 * at 121% + CPU1 usage at 80%) whereas CPU1 has 20% of available capacity/
 */
static int get_cpu_usage(int cpu)
{
	unsigned long usage = cpu_rq(cpu)->cfs.utilization_load_avg;
	unsigned long blocked = cpu_rq(cpu)->cfs.utilization_blocked_avg;

	mt_sched_printf(sched_lb_info, "[%s] cpu=%d usage=%lu blocked=%lu",
		__func__, cpu, usage, blocked);
	if (usage + blocked >= SCHED_LOAD_SCALE)
		return capacity_orig_of(cpu);

	return usage + blocked;
}

/*
 * Called immediately before a task is migrated to a new cpu; task_cpu(p) and
 * cfs_rq_of(p) references at time of call are still valid and identify the
 * previous cpu.  However, the caller only guarantees p->pi_lock is held; no
 * other assumptions, including the state of rq->lock, should be made.
 */
static void
migrate_task_rq_fair(struct task_struct *p, int next_cpu)
{
	struct sched_entity *se = &p->se;
	struct cfs_rq *cfs_rq = cfs_rq_of(se);

	/*
	 * Load tracking: accumulate removed load so that it can be processed
	 * when we next update owning cfs_rq under rq->lock.  Tasks contribute
	 * to blocked load iff they have a positive decay-count.  It can never
	 * be negative here since on-rq tasks have decay-count == 0.
	 */
	if (se->avg.decay_count) {
		se->avg.decay_count = -__synchronize_entity_decay(se);
		atomic_long_add(se->avg.load_avg_contrib,
						&cfs_rq->removed_load);
		atomic_long_add(se->avg.utilization_avg_contrib,
					&cfs_rq->removed_utilization);
	}

	/* We have migrated, no longer consider this task hot */
	se->exec_start = 0;
}
#endif /* CONFIG_SMP */

static unsigned long
wakeup_gran(struct sched_entity *curr, struct sched_entity *se)
{
	unsigned long gran = sysctl_sched_wakeup_granularity;

	/*
	 * Since its curr running now, convert the gran from real-time
	 * to virtual-time in his units.
	 *
	 * By using 'se' instead of 'curr' we penalize light tasks, so
	 * they get preempted easier. That is, if 'se' < 'curr' then
	 * the resulting gran will be larger, therefore penalizing the
	 * lighter, if otoh 'se' > 'curr' then the resulting gran will
	 * be smaller, again penalizing the lighter task.
	 *
	 * This is especially important for buddies when the leftmost
	 * task is higher priority than the buddy.
	 */
	return calc_delta_fair(gran, se);
}

/*
 * Should 'se' preempt 'curr'.
 *
 *             |s1
 *        |s2
 *   |s3
 *         g
 *      |<--->|c
 *
 *  w(c, s1) = -1
 *  w(c, s2) =  0
 *  w(c, s3) =  1
 *
 */
static int
wakeup_preempt_entity(struct sched_entity *curr, struct sched_entity *se)
{
	s64 gran, vdiff = curr->vruntime - se->vruntime;

	if (vdiff <= 0)
		return -1;

	gran = wakeup_gran(curr, se);
	if (vdiff > gran)
		return 1;

	return 0;
}

static void set_last_buddy(struct sched_entity *se)
{
	if (entity_is_task(se) && unlikely(task_of(se)->policy == SCHED_IDLE))
		return;

	for_each_sched_entity(se)
		cfs_rq_of(se)->last = se;
}

static void set_next_buddy(struct sched_entity *se)
{
	if (entity_is_task(se) && unlikely(task_of(se)->policy == SCHED_IDLE))
		return;

	for_each_sched_entity(se)
		cfs_rq_of(se)->next = se;
}

static void set_skip_buddy(struct sched_entity *se)
{
	for_each_sched_entity(se)
		cfs_rq_of(se)->skip = se;
}

/*
 * Preempt the current task with a newly woken task if needed:
 */
static void check_preempt_wakeup(struct rq *rq, struct task_struct *p, int wake_flags)
{
	struct task_struct *curr = rq->curr;
	struct sched_entity *se = &curr->se, *pse = &p->se;
	struct cfs_rq *cfs_rq = task_cfs_rq(curr);
	int scale = cfs_rq->nr_running >= sched_nr_latency;
	int next_buddy_marked = 0;

	if (unlikely(se == pse))
		return;

	/*
	 * This is possible from callers such as attach_tasks(), in which we
	 * unconditionally check_prempt_curr() after an enqueue (which may have
	 * lead to a throttle).  This both saves work and prevents false
	 * next-buddy nomination below.
	 */
	if (unlikely(throttled_hierarchy(cfs_rq_of(pse))))
		return;

	if (sched_feat(NEXT_BUDDY) && scale && !(wake_flags & WF_FORK)) {
		set_next_buddy(pse);
		next_buddy_marked = 1;
	}

	/*
	 * We can come here with TIF_NEED_RESCHED already set from new task
	 * wake up path.
	 *
	 * Note: this also catches the edge-case of curr being in a throttled
	 * group (e.g. via set_curr_task), since update_curr() (in the
	 * enqueue of curr) will have resulted in resched being set.  This
	 * prevents us from potentially nominating it as a false LAST_BUDDY
	 * below.
	 */
	if (test_tsk_need_resched(curr))
		return;

	/* Idle tasks are by definition preempted by non-idle tasks. */
	if (unlikely(curr->policy == SCHED_IDLE) &&
	    likely(p->policy != SCHED_IDLE))
		goto preempt;

	/*
	 * Batch and idle tasks do not preempt non-idle tasks (their preemption
	 * is driven by the tick):
	 */
	if (unlikely(p->policy != SCHED_NORMAL) || !sched_feat(WAKEUP_PREEMPTION))
		return;

	find_matching_se(&se, &pse);
	update_curr(cfs_rq_of(se));
	BUG_ON(!pse);
	if (wakeup_preempt_entity(se, pse) == 1) {
		/*
		 * Bias pick_next to pick the sched entity that is
		 * triggering this preemption.
		 */
		if (!next_buddy_marked)
			set_next_buddy(pse);
		goto preempt;
	}

	return;

preempt:
	resched_curr(rq);
	/*
	 * Only set the backward buddy when the current task is still
	 * on the rq. This can happen when a wakeup gets interleaved
	 * with schedule on the ->pre_schedule() or idle_balance()
	 * point, either of which can * drop the rq lock.
	 *
	 * Also, during early boot the idle thread is in the fair class,
	 * for obvious reasons its a bad idea to schedule back to it.
	 */
	if (unlikely(!se->on_rq || curr == rq->idle))
		return;

	if (sched_feat(LAST_BUDDY) && scale && entity_is_task(se))
		set_last_buddy(se);
}

static struct task_struct *
pick_next_task_fair(struct rq *rq, struct task_struct *prev)
{
	struct cfs_rq *cfs_rq = &rq->cfs;
	struct sched_entity *se;
	struct task_struct *p;
	int new_tasks;

again:
#ifdef CONFIG_FAIR_GROUP_SCHED
	/* in case nr_running!=0 but h_nr_running==0 */
	if (!cfs_rq->h_nr_running)
		goto idle;

	if (!cfs_rq->nr_running)
		goto idle;

	if (prev->sched_class != &fair_sched_class)
		goto simple;

	/*
	 * Because of the set_next_buddy() in dequeue_task_fair() it is rather
	 * likely that a next task is from the same cgroup as the current.
	 *
	 * Therefore attempt to avoid putting and setting the entire cgroup
	 * hierarchy, only change the part that actually changes.
	 */

	do {
		struct sched_entity *curr = cfs_rq->curr;

		/*
		 * Since we got here without doing put_prev_entity() we also
		 * have to consider cfs_rq->curr. If it is still a runnable
		 * entity, update_curr() will update its vruntime, otherwise
		 * forget we've ever seen it.
		 */
		if (curr) {
			if (curr->on_rq)
				update_curr(cfs_rq);
			else
				curr = NULL;

			/*
			 * This call to check_cfs_rq_runtime() will do the
			 * throttle and dequeue its entity in the parent(s).
			 * Therefore the 'simple' nr_running test will indeed
			 * be correct.
			 */
			if (unlikely(check_cfs_rq_runtime(cfs_rq)))
				goto simple;
		}

		se = pick_next_entity(cfs_rq, curr);
		cfs_rq = group_cfs_rq(se);
	} while (cfs_rq);

	p = task_of(se);

	/*
	 * Since we haven't yet done put_prev_entity and if the selected task
	 * is a different task than we started out with, try and touch the
	 * least amount of cfs_rqs.
	 */
	if (prev != p) {
		struct sched_entity *pse = &prev->se;

		while (!(cfs_rq = is_same_group(se, pse))) {
			int se_depth = se->depth;
			int pse_depth = pse->depth;

			if (se_depth <= pse_depth) {
				put_prev_entity(cfs_rq_of(pse), pse);
				pse = parent_entity(pse);
			}
			if (se_depth >= pse_depth) {
				set_next_entity(cfs_rq_of(se), se);
				se = parent_entity(se);
			}
		}

		put_prev_entity(cfs_rq, pse);
		set_next_entity(cfs_rq, se);
	}

	if (hrtick_enabled(rq))
		hrtick_start_fair(rq, p);

	return p;
simple:
	cfs_rq = &rq->cfs;
#endif

	if (!cfs_rq->nr_running)
		goto idle;

	put_prev_task(rq, prev);

	do {
		se = pick_next_entity(cfs_rq, NULL);
		set_next_entity(cfs_rq, se);
		cfs_rq = group_cfs_rq(se);
	} while (cfs_rq);

	p = task_of(se);

	if (hrtick_enabled(rq))
		hrtick_start_fair(rq, p);

	return p;

idle:
	new_tasks = idle_balance(rq);
	/*
	 * Because idle_balance() releases (and re-acquires) rq->lock, it is
	 * possible for any higher priority task to appear. In that case we
	 * must re-start the pick_next_entity() loop.
	 */
	if (new_tasks < 0)
		return RETRY_TASK;

	if (new_tasks > 0)
		goto again;

	return NULL;
}

/*
 * Account for a descheduled task:
 */
static void put_prev_task_fair(struct rq *rq, struct task_struct *prev)
{
	struct sched_entity *se = &prev->se;
	struct cfs_rq *cfs_rq;

	for_each_sched_entity(se) {
		cfs_rq = cfs_rq_of(se);
		put_prev_entity(cfs_rq, se);
	}
}

/*
 * sched_yield() is very simple
 *
 * The magic of dealing with the ->skip buddy is in pick_next_entity.
 */
static void yield_task_fair(struct rq *rq)
{
	struct task_struct *curr = rq->curr;
	struct cfs_rq *cfs_rq = task_cfs_rq(curr);
	struct sched_entity *se = &curr->se;

	/*
	 * Are we the only task in the tree?
	 */
	if (unlikely(rq->nr_running == 1))
		return;

	clear_buddies(cfs_rq, se);

	if (curr->policy != SCHED_BATCH) {
		update_rq_clock(rq);
		/*
		 * Update run-time statistics of the 'current'.
		 */
		update_curr(cfs_rq);
		/*
		 * Tell update_rq_clock() that we've just updated,
		 * so we don't do microscopic update in schedule()
		 * and double the fastpath cost.
		 */
		 rq->skip_clock_update = 1;
	}

	set_skip_buddy(se);
}

static bool yield_to_task_fair(struct rq *rq, struct task_struct *p, bool preempt)
{
	struct sched_entity *se = &p->se;

	/* throttled hierarchies are not runnable */
	if (!se->on_rq || throttled_hierarchy(cfs_rq_of(se)))
		return false;

	/* Tell the scheduler that we'd really like pse to run next. */
	set_next_buddy(se);

	yield_task_fair(rq);

	return true;
}

#ifdef CONFIG_SMP
/**************************************************
 * Fair scheduling class load-balancing methods.
 *
 * BASICS
 *
 * The purpose of load-balancing is to achieve the same basic fairness the
 * per-cpu scheduler provides, namely provide a proportional amount of compute
 * time to each task. This is expressed in the following equation:
 *
 *   W_i,n/P_i == W_j,n/P_j for all i,j                               (1)
 *
 * Where W_i,n is the n-th weight average for cpu i. The instantaneous weight
 * W_i,0 is defined as:
 *
 *   W_i,0 = \Sum_j w_i,j                                             (2)
 *
 * Where w_i,j is the weight of the j-th runnable task on cpu i. This weight
 * is derived from the nice value as per prio_to_weight[].
 *
 * The weight average is an exponential decay average of the instantaneous
 * weight:
 *
 *   W'_i,n = (2^n - 1) / 2^n * W_i,n + 1 / 2^n * W_i,0               (3)
 *
 * C_i is the compute capacity of cpu i, typically it is the
 * fraction of 'recent' time available for SCHED_OTHER task execution. But it
 * can also include other factors [XXX].
 *
 * To achieve this balance we define a measure of imbalance which follows
 * directly from (1):
 *
 *   imb_i,j = max{ avg(W/C), W_i/C_i } - min{ avg(W/C), W_j/C_j }    (4)
 *
 * We them move tasks around to minimize the imbalance. In the continuous
 * function space it is obvious this converges, in the discrete case we get
 * a few fun cases generally called infeasible weight scenarios.
 *
 * [XXX expand on:
 *     - infeasible weights;
 *     - local vs global optima in the discrete case. ]
 *
 *
 * SCHED DOMAINS
 *
 * In order to solve the imbalance equation (4), and avoid the obvious O(n^2)
 * for all i,j solution, we create a tree of cpus that follows the hardware
 * topology where each level pairs two lower groups (or better). This results
 * in O(log n) layers. Furthermore we reduce the number of cpus going up the
 * tree to only the first of the previous level and we decrease the frequency
 * of load-balance at each level inv. proportional to the number of cpus in
 * the groups.
 *
 * This yields:
 *
 *     log_2 n     1     n
 *   \Sum       { --- * --- * 2^i } = O(n)                            (5)
 *     i = 0      2^i   2^i
 *                               `- size of each group
 *         |         |     `- number of cpus doing load-balance
 *         |         `- freq
 *         `- sum over all levels
 *
 * Coupled with a limit on how many tasks we can migrate every balance pass,
 * this makes (5) the runtime complexity of the balancer.
 *
 * An important property here is that each CPU is still (indirectly) connected
 * to every other cpu in at most O(log n) steps:
 *
 * The adjacency matrix of the resulting graph is given by:
 *
 *             log_2 n     
 *   A_i,j = \Union     (i % 2^k == 0) && i / 2^(k+1) == j / 2^(k+1)  (6)
 *             k = 0
 *
 * And you'll find that:
 *
 *   A^(log_2 n)_i,j != 0  for all i,j                                (7)
 *
 * Showing there's indeed a path between every cpu in at most O(log n) steps.
 * The task movement gives a factor of O(m), giving a convergence complexity
 * of:
 *
 *   O(nm log n),  n := nr_cpus, m := nr_tasks                        (8)
 *
 *
 * WORK CONSERVING
 *
 * In order to avoid CPUs going idle while there's still work to do, new idle
 * balancing is more aggressive and has the newly idle cpu iterate up the domain
 * tree itself instead of relying on other CPUs to bring it work.
 *
 * This adds some complexity to both (5) and (8) but it reduces the total idle
 * time.
 *
 * [XXX more?]
 *
 *
 * CGROUPS
 *
 * Cgroups make a horror show out of (2), instead of a simple sum we get:
 *
 *                                s_k,i
 *   W_i,0 = \Sum_j \Prod_k w_k * -----                               (9)
 *                                 S_k
 *
 * Where
 *
 *   s_k,i = \Sum_j w_i,j,k  and  S_k = \Sum_i s_k,i                 (10)
 *
 * w_i,j,k is the weight of the j-th runnable task in the k-th cgroup on cpu i.
 *
 * The big problem is S_k, its a global sum needed to compute a local (W_i)
 * property.
 *
 * [XXX write more on how we solve this.. _after_ merging pjt's patches that
 *      rewrite all of this once again.]
 */

static unsigned long __read_mostly max_load_balance_interval = HZ/10;

enum fbq_type { regular, remote, all };

#define LBF_ALL_PINNED	0x01
#define LBF_NEED_BREAK	0x02
#define LBF_DST_PINNED  0x04
#define LBF_SOME_PINNED	0x08

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
	/* The set of CPUs under consideration for load-balancing */
	struct cpumask		*cpus;

	unsigned int		flags;

	unsigned int		loop;
	unsigned int		loop_break;
	unsigned int		loop_max;

	enum fbq_type		fbq_type;
	struct list_head	tasks;
#ifdef CONFIG_MT_LOAD_BALANCE_ENHANCEMENT
	int			mt_ignore_cachehot_in_idle;
#endif
};

#ifdef CONFIG_SCHED_HMP
/*
 * move_task - move a task from one runqueue to another runqueue.
 * Both runqueues must be locked.
 */
static void move_task(struct task_struct *p, struct lb_env *env)
{
	deactivate_task(env->src_rq, p, 0);
	set_task_cpu(p, env->dst_cpu);
	activate_task(env->dst_rq, p, 0);
	check_preempt_curr(env->dst_rq, p, 0);
}
#endif

/*
 * Is this task likely cache-hot:
 */
static int task_hot(struct task_struct *p, struct lb_env *env)
{
	s64 delta;

	lockdep_assert_held(&env->src_rq->lock);

	if (p->sched_class != &fair_sched_class)
		return 0;

	if (unlikely(p->policy == SCHED_IDLE))
		return 0;

#ifdef CONFIG_MT_LOAD_BALANCE_ENHANCEMENT
	/*
	force ignore the cache hot when current rq is idle and src cpu have more than 2 tasks
	 */
	if (env->mt_ignore_cachehot_in_idle) {
		if (!this_rq()->nr_running && (task_rq(p)->nr_running >= 2))
			return 0;
	}
#endif

	/*
	 * Buddy candidates are cache hot:
	 */
	if (sched_feat(CACHE_HOT_BUDDY) && env->dst_rq->nr_running &&
			(&p->se == cfs_rq_of(&p->se)->next ||
			 &p->se == cfs_rq_of(&p->se)->last))
		return 1;

	if (sysctl_sched_migration_cost == -1)
		return 1;
	if (sysctl_sched_migration_cost == 0)
		return 0;

	delta = rq_clock_task(env->src_rq) - p->se.exec_start;

	return delta < (s64)sysctl_sched_migration_cost;
}

#ifdef CONFIG_NUMA_BALANCING
/* Returns true if the destination node has incurred more faults */
static bool migrate_improves_locality(struct task_struct *p, struct lb_env *env)
{
	struct numa_group *numa_group = rcu_dereference(p->numa_group);
	int src_nid, dst_nid;

	if (!sched_feat(NUMA_FAVOUR_HIGHER) || !p->numa_faults_memory ||
	    !(env->sd->flags & SD_NUMA)) {
		return false;
	}

	src_nid = cpu_to_node(env->src_cpu);
	dst_nid = cpu_to_node(env->dst_cpu);

	if (src_nid == dst_nid)
		return false;

	if (numa_group) {
		/* Task is already in the group's interleave set. */
		if (node_isset(src_nid, numa_group->active_nodes))
			return false;

		/* Task is moving into the group's interleave set. */
		if (node_isset(dst_nid, numa_group->active_nodes))
			return true;

		return group_faults(p, dst_nid) > group_faults(p, src_nid);
	}

	/* Encourage migration to the preferred node. */
	if (dst_nid == p->numa_preferred_nid)
		return true;

	return task_faults(p, dst_nid) > task_faults(p, src_nid);
}


static bool migrate_degrades_locality(struct task_struct *p, struct lb_env *env)
{
	struct numa_group *numa_group = rcu_dereference(p->numa_group);
	int src_nid, dst_nid;

	if (!sched_feat(NUMA) || !sched_feat(NUMA_RESIST_LOWER))
		return false;

	if (!p->numa_faults_memory || !(env->sd->flags & SD_NUMA))
		return false;

	src_nid = cpu_to_node(env->src_cpu);
	dst_nid = cpu_to_node(env->dst_cpu);

	if (src_nid == dst_nid)
		return false;

	if (numa_group) {
		/* Task is moving within/into the group's interleave set. */
		if (node_isset(dst_nid, numa_group->active_nodes))
			return false;

		/* Task is moving out of the group's interleave set. */
		if (node_isset(src_nid, numa_group->active_nodes))
			return true;

		return group_faults(p, dst_nid) < group_faults(p, src_nid);
	}

	/* Migrating away from the preferred node is always bad. */
	if (src_nid == p->numa_preferred_nid)
		return true;

	return task_faults(p, dst_nid) < task_faults(p, src_nid);
}

#else
static inline bool migrate_improves_locality(struct task_struct *p,
					     struct lb_env *env)
{
	return false;
}

static inline bool migrate_degrades_locality(struct task_struct *p,
					     struct lb_env *env)
{
	return false;
}
#endif

/*
 * can_migrate_task - may task p from runqueue rq be migrated to this_cpu?
 */
static
int can_migrate_task(struct task_struct *p, struct lb_env *env)
{
	int tsk_cache_hot = 0;

	lockdep_assert_held(&env->src_rq->lock);

	/*
	 * We do not migrate tasks that are:
	 * 1) throttled_lb_pair, or
	 * 2) cannot be migrated to this CPU due to cpus_allowed, or
	 * 3) running (obviously), or
	 * 4) are cache-hot on their current CPU.
	 */
	if (throttled_lb_pair(task_group(p), env->src_cpu, env->dst_cpu))
		return 0;

	if (!cpumask_test_cpu(env->dst_cpu, tsk_cpus_allowed(p))) {
		int cpu;

		schedstat_inc(p, se.statistics.nr_failed_migrations_affine);
		mt_sched_printf(sched_lb, "[%s] %d %s affinity fail 0x%lu ",
			__func__, p->pid, p->comm, p->cpus_allowed.bits[0]);

		env->flags |= LBF_SOME_PINNED;

		/*
		 * Remember if this task can be migrated to any other cpu in
		 * our sched_group. We may want to revisit it if we couldn't
		 * meet load balance goals by pulling other tasks on src_cpu.
		 *
		 * Also avoid computing new_dst_cpu if we have already computed
		 * one in current iteration.
		 */
		if (!env->dst_grpmask || (env->flags & LBF_DST_PINNED))
			return 0;

		/* Prevent to re-select dst_cpu via env's cpus */
		for_each_cpu_and(cpu, env->dst_grpmask, env->cpus) {
			if (cpumask_test_cpu(cpu, tsk_cpus_allowed(p))) {
				env->flags |= LBF_DST_PINNED;
				env->new_dst_cpu = cpu;
				break;
			}
		}

		return 0;
	}

	/* Record that we found atleast one task that could run on dst_cpu */
	env->flags &= ~LBF_ALL_PINNED;

	if (task_running(env->src_rq, p)) {
		schedstat_inc(p, se.statistics.nr_failed_migrations_running);
		return 0;
	}

	/*
	 * Aggressive migration if:
	 * 1) destination numa is preferred
	 * 2) task is cache cold, or
	 * 3) too many balance attempts have failed.
	 */
	tsk_cache_hot = task_hot(p, env);
	if (!tsk_cache_hot)
		tsk_cache_hot = migrate_degrades_locality(p, env);

	if (migrate_improves_locality(p, env) || !tsk_cache_hot ||
	    env->sd->nr_balance_failed > env->sd->cache_nice_tries) {
		if (tsk_cache_hot) {
			schedstat_inc(env->sd, lb_hot_gained[env->idle]);
			schedstat_inc(p, se.statistics.nr_forced_migrations);
			mt_sched_printf(sched_lb, "[%s] %d %s running fail",
				__func__, p->pid, p->comm);
		}
		return 1;
	}

	schedstat_inc(p, se.statistics.nr_failed_migrations_hot);
	return 0;
}

#ifdef CONFIG_MTK_SCHED_CMP
#ifdef CONFIG_MTK_SCHED_CMP_TGS
static int cmp_can_migrate_task(struct task_struct *p, struct lb_env *env)
{
	struct sched_domain *sd = env->sd;

	BUG_ON(sd == NULL);

	if (!(sd->flags & SD_BALANCE_TG))
		return 0;

	if (arch_is_multi_cluster()) {
		int src_clid, dst_clid;
		int src_nr_cpus;

		src_clid = arch_get_cluster_id(env->src_cpu);
		dst_clid = arch_get_cluster_id(env->dst_cpu);
		BUG_ON(dst_clid == -1 || src_clid == -1);
		BUG_ON(p == NULL);
		src_nr_cpus = nr_cpus_in_cluster(src_clid, false);

#ifdef CONFIG_MTK_SCHED_CMP_TGS_WAKEUP
		if (!thread_group_empty(p)) {
			struct thread_group_info_t *src_tginfo, *dst_tginfo;

			if (group_leader_is_empty(p))
				return 0;

			src_tginfo = &p->group_leader->thread_group_info[src_clid];
			dst_tginfo = &p->group_leader->thread_group_info[dst_clid];

			mt_sched_printf(sched_cmp_info,
				"check rule0: pid=%d comm=%s load=%ld src:clid=%d src_tg->nr_running=%ld nr_cpus=%d dst_tg->nr_running=%ld",
				p->pid, p->comm, p->se.avg.loadwop_avg_contrib,
				src_clid, src_tginfo->nr_running, src_nr_cpus,
				dst_tginfo->nr_running);

			/* # of task in src is larger than # of tasks in dst, don't migration */
			if ((src_tginfo->nr_running <= src_nr_cpus) &&
				(src_tginfo->nr_running > dst_tginfo->nr_running)) {
				mt_sched_printf(sched_log,
					"hit ruleA: bypass pid=%d comm=%s src:nr_running=%lu nr_cpus=%d dst:nr_running=%lu",
					p->pid, p->comm, src_tginfo->nr_running, src_nr_cpus, dst_tginfo->nr_running);
				return 0;
			}
		}
#endif
	}
	return 1;
}

static int need_migrate_task_immediately(struct task_struct *p,
					 struct lb_env *env, struct clb_env *clbenv)
{
	struct sched_domain *sd = env->sd;
#ifdef CONFIG_SCHED_HMP
	unsigned long src_cap, dst_cap;
#endif

	BUG_ON(sd == NULL);

#ifdef CONFIG_SCHED_HMP
	src_cap = arch_get_max_cpu_capacity(env->src_cpu);
	dst_cap = arch_get_max_cpu_capacity(env->dst_cpu);
	if (src_cap != dst_cap) {
		mt_sched_printf(sched_cmp,
			"[%s] diffcap src: cpu/cap(%u/%lu) dst: cpu/cap(%u/%lu) th: up/dl(%d/%d)", __func__,
			env->src_cpu, src_cap, env->dst_cpu, dst_cap, clbenv->bstats.threshold,
			clbenv->lstats.threshold);
		mt_sched_printf(sched_cmp_info,
			"check rule0: pid=%d comm=%s src=%d dst=%d p->prio=%d p->se.avg.loadwop_avg_contrib=%ld",
			p->pid, p->comm, env->src_cpu, env->dst_cpu, p->prio, p->se.avg.loadwop_avg_contrib);

		/* from small to big caps */
		if (src_cap < dst_cap) {
			BUG_ON(env->src_cpu != clbenv->ltarget);
			if (p->se.avg.loadwop_avg_contrib >= clbenv->bstats.threshold)
				return 1;
		/* from big to small caps */
		} else if (src_cap > dst_cap) {
			BUG_ON(env->src_cpu != clbenv->btarget);
			if (p->se.avg.loadwop_avg_contrib < clbenv->lstats.threshold)
				return 1;
		}
		return 0;
	}
#endif

	if (arch_is_multi_cluster() && (sd->flags & SD_BALANCE_TG)) {
		int src_clid, dst_clid;
		int src_nr_cpus;
		struct thread_group_info_t *src_tginfo, *dst_tginfo;

		src_clid = arch_get_cluster_id(env->src_cpu);
		dst_clid = arch_get_cluster_id(env->dst_cpu);
		BUG_ON(dst_clid == -1 || src_clid == -1);
		BUG_ON(p == NULL);

		if (group_leader_is_empty(p))
			return 0;

		src_tginfo = &p->group_leader->thread_group_info[src_clid];
		dst_tginfo = &p->group_leader->thread_group_info[dst_clid];
		src_nr_cpus = nr_cpus_in_cluster(src_clid, false);
		mt_sched_printf(sched_cmp, "[%s] L.L arch", __func__);

		if ((p->se.avg.loadwop_avg_contrib * 4 >= NICE_0_LOAD * 3) &&
		    src_tginfo->nr_running > src_nr_cpus &&
		    src_tginfo->loadwop_avg_contrib * 10 > NICE_0_LOAD * src_nr_cpus * 9) {
			/*pr_warn("[%s] hit rule0, candidate_load_move/load_move (%ld/%ld)\n",
			      __func__, candidate_load_move, env->imbalance);*/
			return 1;
		}
	}

	return 0;
}
#endif
#endif /* CONFIG_MTK_SCHED_CMP */

/*
 * detach_task() -- detach the task for the migration specified in env
 */
static void detach_task(struct task_struct *p, struct lb_env *env)
{
	lockdep_assert_held(&env->src_rq->lock);

	deactivate_task(env->src_rq, p, 0);
	p->on_rq = TASK_ON_RQ_MIGRATING;
	set_task_cpu(p, env->dst_cpu);
}

#ifdef CONFIG_MT_LOAD_BALANCE_ENHANCEMENT
static int is_heavy_task(struct task_struct *p)
{
	return p->se.avg.loadwop_avg_contrib >= 650 ? 1 : 0;
}
#endif

/*
 * detach_one_task() -- tries to dequeue exactly one task from env->src_rq, as
 * part of active balancing operations within "domain".
 *
 * Returns a task if successful and NULL otherwise.
 */
static struct task_struct *detach_one_task(struct lb_env *env)
{
	struct task_struct *p, *n;
#ifdef CONFIG_MT_LOAD_BALANCE_ENHANCEMENT
	int bypass_once = 1;

	env->mt_ignore_cachehot_in_idle = 0;
#endif
	lockdep_assert_held(&env->src_rq->lock);

	list_for_each_entry_safe(p, n, &env->src_rq->cfs_tasks, se.group_node) {
		if (!can_migrate_task(p, env))
			continue;

#ifdef CONFIG_MT_LOAD_BALANCE_ENHANCEMENT
		if (bypass_once && is_heavy_task(p) &&
			(capacity_orig_of(env->src_cpu) > capacity_orig_of(env->dst_cpu))) {
			bypass_once = 0;
			continue;
		}
#endif

		detach_task(p, env);

		/*
		 * Right now, this is only the second place where
		 * lb_gained[env->idle] is updated (other is detach_tasks)
		 * so we can safely collect stats here rather than
		 * inside detach_tasks().
		 */
		schedstat_inc(env->sd, lb_gained[env->idle]);
		return p;
	}
	return NULL;
}

static const unsigned int sched_nr_migrate_break = 32;

/* in second round load balance, we migrate heavy load_weight task
     as long as RT tasks exist in busy cpu*/
#ifdef CONFIG_MT_LOAD_BALANCE_ENHANCEMENT
	#define over_imbalance(lw, im) \
		(((lw)/2 > (im)) && \
		((env->mt_ignore_cachehot_in_idle == 0) || \
		(env->src_rq->rt.rt_nr_running == 0) || \
		(detached > 0)))
#else
	#define over_imbalance(lw, im) (((lw) / 2) > (im))
#endif

/*
 * detach_tasks() -- tries to detach up to imbalance weighted load from
 * busiest_rq, as part of a balancing operation within domain "sd".
 *
 * Returns number of detached tasks if successful and 0 otherwise.
 */
static int detach_tasks(struct lb_env *env)
{
	struct list_head *tasks = &env->src_rq->cfs_tasks;
	struct task_struct *p;
	unsigned long load;
	int detached = 0;

	lockdep_assert_held(&env->src_rq->lock);

	if (env->imbalance <= 0)
		return 0;

	/* sched: add trace_sched */
	mt_sched_printf(sched_lb, "[%s] start: src=%d dst=%d",
		__func__, env->src_cpu, env->dst_cpu);

	while (!list_empty(tasks)) {
		p = list_first_entry(tasks, struct task_struct, se.group_node);

		env->loop++;
		/* We've more or less seen every task there is, call it quits */
		if (env->loop > env->loop_max)
			break;

		/* take a breather every nr_migrate tasks */
		if (env->loop > env->loop_break) {
			env->loop_break += sched_nr_migrate_break;
			env->flags |= LBF_NEED_BREAK;
			break;
		}

		if (!can_migrate_task(p, env))
			goto next;

		load = task_h_load(p);

		if (sched_feat(LB_MIN) && load < 16 && !env->sd->nr_balance_failed)
			goto next;

		if (over_imbalance(load, env->imbalance))
			goto next;

		detach_task(p, env);
		list_add(&p->se.group_node, &env->tasks);

		detached++;
		env->imbalance -= load;

#ifdef CONFIG_PREEMPT
		/*
		 * NEWIDLE balancing is a source of latency, so preemptible
		 * kernels will stop after the first task is detached to minimize
		 * the critical section.
		 */
		if (env->idle == CPU_NEWLY_IDLE)
			break;
#endif

		/*
		 * We only want to steal up to the prescribed amount of
		 * weighted load.
		 */
		if (env->imbalance <= 0)
			break;

		continue;
next:
		list_move_tail(&p->se.group_node, tasks);
	}

	/*
	 * Right now, this is one of only two places we collect this stat
	 * so we can safely collect detach_one_task() stats here rather
	 * than inside detach_one_task().
	 */
	schedstat_add(env->sd, lb_gained[env->idle], detached);
	/* sched: add trace_sched */
	mt_sched_printf(sched_lb, "[%s] detached=%d", __func__, detached);

	return detached;
}

#ifdef CONFIG_MTK_SCHED_CMP_TGS
/*
 * detach_tasks() -- tries to detach up to imbalance weighted load from
 * busiest_rq, as part of a balancing operation within domain "sd".
 *
 * Returns number of detached tasks if successful and 0 otherwise.
 */
static int tgs_detach_tasks(struct lb_env *env)
{
	struct list_head *tasks = &env->src_rq->cfs_tasks;
	struct task_struct *p;
	unsigned long load;
	int detached = 0;
	long tg_load_move, other_load_move;
	struct list_head tg_tasks, other_tasks;
	int src_clid, dst_clid;
#ifdef MTK_QUICK
	int flag = 0;
#endif
	struct clb_env clbenv;
	struct sched_domain *sd = env->sd;
#ifdef CONFIG_SCHED_HMP
	unsigned long src_cap, dst_cap;
#endif

	lockdep_assert_held(&env->src_rq->lock);

	if (env->imbalance <= 0)
		return 0;

	other_load_move = env->imbalance;
	INIT_LIST_HEAD(&other_tasks);

	tg_load_move = env->imbalance;
	INIT_LIST_HEAD(&tg_tasks);
	src_clid = arch_get_cluster_id(env->src_cpu);
	dst_clid = arch_get_cluster_id(env->dst_cpu);
	BUG_ON(dst_clid == -1 || src_clid == -1);

	mt_sched_printf(sched_cmp,
		"[%s] start: src:cpu=%d clid=%d runnable_load=%lu dst:cpu=%d clid=%d" "runnable_load=%lu",
		__func__,
		env->src_cpu, src_clid, cpu_rq(env->src_cpu)->cfs.runnable_load_avg,
		env->dst_cpu, dst_clid, cpu_rq(env->dst_cpu)->cfs.runnable_load_avg);

	mt_sched_printf(sched_cmp, "imbalance=%ld curr->on_rq=%d flag=%d max=%d busiest->nr_running=%d",
			env->imbalance, env->dst_rq->curr->on_rq, sd->flags, env->loop_max,
			cpu_rq(env->src_cpu)->nr_running);

#ifdef CONFIG_SCHED_HMP
	src_cap = arch_get_max_cpu_capacity(env->src_cpu);
	dst_cap = arch_get_max_cpu_capacity(env->dst_cpu);

	memset(&clbenv, 0, sizeof(clbenv));
	clbenv.ltarget = (int) src_cap;
	clbenv.btarget = (int) dst_cap;

	if (src_cap != dst_cap) {
		clbenv.flags |= HMP_LB;
		if (src_cap < dst_cap) {
			clbenv.ltarget = env->src_cpu;
			clbenv.btarget = env->dst_cpu;
			get_cluster_cpus(&clbenv.lcpus, src_clid, true);
			get_cluster_cpus(&clbenv.bcpus, dst_clid, true);
		} else {
			clbenv.ltarget = env->dst_cpu;
			clbenv.btarget = env->src_cpu;
			get_cluster_cpus(&clbenv.lcpus, dst_clid, true);
			get_cluster_cpus(&clbenv.bcpus, src_clid, true);
		}
		sched_update_clbstats(&clbenv);
	}
#endif

	while (!list_empty(tasks)) {

		p = list_first_entry(tasks, struct task_struct, se.group_node);

		mt_sched_printf(sched_cmp_info,
			"check: pid=%d comm=%s contrib=%lu runnable=%lu loop=%d, imbalance=%ld tg_load_move=%ld",
			p->pid, p->comm, p->se.avg.loadwop_avg_contrib,
			task_cfs_rq(p)->runnable_load_avg,
			env->loop, env->imbalance, tg_load_move);
		env->loop++;
		/* We've more or less seen every task there is, call it quits */
		if (env->loop > env->loop_max)
			break;

#if 0		/* TO check */
		/* take a breather every nr_migrate tasks */
		if (env->loop > env->loop_break) {
			env->loop_break += sched_nr_migrate_break;
			env->flags |= LBF_NEED_BREAK;
			break;
		}
#endif
		BUG_ON(p == NULL);


		/* rule0 */
		if (!can_migrate_task(p, env)) {
			mt_sched_printf(sched_cmp, "can not migrate: pid=%d comm=%s",
				p->pid, p->comm);
			goto next;
		}

		load = task_h_load(p);

		if (sched_feat(LB_MIN) && load < 16 && !env->sd->nr_balance_failed) {
			mt_sched_printf(sched_cmp, "can not migrate: pid=%d comm=%s sched_feat load=%lu",
					p->pid, p->comm, load);
			goto next;
		}

		if (over_imbalance(load, env->imbalance)) {
			mt_sched_printf(sched_cmp, "can not migrate: pid=%d comm=%s load=%lu imbalance=%ld",
					p->pid, p->comm, load, env->imbalance);
			goto next;
		}

		/* meet rule0 , migrate immediately */
		if (need_migrate_task_immediately(p, env, &clbenv)) {
			detach_task(p, env);
			list_add(&p->se.group_node, &env->tasks);
			detached++;
			env->imbalance -= load;
			tg_load_move -= load;
			other_load_move -= load;
			mt_sched_printf(sched_cmp,
				"hit rule0: pid=%d comm=%s load=%lu imbalance=%ld tg_imbalance=%ld other_load_move=%ld",
				p->pid, p->comm, load, env->imbalance, tg_load_move, other_load_move);
			if (env->imbalance <= 0)
				break;
			continue;
		}

		/* for TGS */
		if (!cmp_can_migrate_task(p, env))
			goto next;

		if (sd->flags & SD_BALANCE_TG) {
			if (over_imbalance(load, tg_load_move)) {
				mt_sched_printf(sched_cmp, "can not migrate: pid=%d comm=%s load=%lu imbalance=%ld",
						p->pid, p->comm, load, tg_load_move);
				goto next;
			}

#ifdef MTK_QUICK
			if (candidate_load_move <= 0) {
				mt_sched_printf(sched_cmp, "check: pid=%d comm=%s candidate_load_move=%d",
						p->pid, p->comm, candidate_load_move);
				goto next;
			}
#endif

			/* rule1, single thread */
			mt_sched_printf(sched_cmp_info,
				"check rule1: pid=%d p->comm=%s thread_group_cnt=%lu thread_group_empty(p)=%d",
				p->pid, p->comm,
				p->group_leader->thread_group_info[0].nr_running +
				p->group_leader->thread_group_info[1].nr_running,
				thread_group_empty(p));

			if (thread_group_empty(p)) {
				list_move_tail(&p->se.group_node, &tg_tasks);
				tg_load_move -= load;
				other_load_move -= load;
				mt_sched_printf(sched_cmp, "hit rule1: pid=%d p->comm=%s load=%lu tg_imbalance=%ld",
						p->pid, p->comm, load, tg_load_move);
				continue;
			}

			/* rule2 */
			if (!group_leader_is_empty(p)) {
				struct thread_group_info_t *src_tginfo, *dst_tginfo;

				src_tginfo = &p->group_leader->thread_group_info[src_clid];
				dst_tginfo = &p->group_leader->thread_group_info[dst_clid];

				mt_sched_printf(sched_cmp_info, "check rule2:pid=%d p->comm=%s %ld, %ld, %ld, %ld, %ld",
							p->pid, p->comm, src_tginfo->nr_running,
							src_tginfo->cfs_nr_running, dst_tginfo->nr_running,
							p->se.avg.loadwop_avg_contrib,
							src_tginfo->loadwop_avg_contrib);
				if ((src_tginfo->nr_running < dst_tginfo->nr_running) &&
				   ((p->se.avg.loadwop_avg_contrib * src_tginfo->cfs_nr_running) <=
					src_tginfo->loadwop_avg_contrib)) {
					list_move_tail(&p->se.group_node, &tg_tasks);
					tg_load_move -= load;
					other_load_move -= load;
					mt_sched_printf(sched_cmp, "hit rule2:pid=%d p->comm=%s load=%lu imbalance=%ld",
						p->pid, p->comm, load, tg_load_move);
					continue;
				}
			}

			if (over_imbalance(load, other_load_move))
				goto next;
/*
			if (other_load_move <= 0)
				goto next;
*/

			list_move_tail(&p->se.group_node, &other_tasks);
			other_load_move -= load;
			continue;
		} else {
			list_move_tail(&p->se.group_node, &other_tasks);
			other_load_move -= load;
			continue;
		}

next:
		/* original rule */
		list_move_tail(&p->se.group_node, tasks);
	}			/* end of while() */

	if (sd->flags & SD_BALANCE_TG) {
		while (!list_empty(&tg_tasks)) {
			p = list_first_entry(&tg_tasks, struct task_struct, se.group_node);
			list_move_tail(&p->se.group_node, tasks);

			if (env->imbalance > 0) {
				load = task_h_load(p);
				if (over_imbalance(load, env->imbalance)) {
					mt_sched_printf(sched_cmp,
						"overload rule1,2: pid=%d p->comm=%s load=%lu imbalance=%ld",
						p->pid, p->comm, load, env->imbalance);
#ifdef MTK_QUICK

					flag = 1;
#endif
					continue;
				}

				detach_task(p, env);
				list_add(&p->se.group_node, &env->tasks);
				detached++;
				env->imbalance -= load;

				mt_sched_printf(sched_cmp,
					"migrate hit rule1,2: pid=%d p->comm=%s load=%lu imbalance=%ld",
					p->pid, p->comm, load, env->imbalance);
			}
		}
	}

	mt_sched_printf(sched_cmp, "[%s] finish rule migrate", __func__);

	while (!list_empty(&other_tasks)) {
		p = list_first_entry(&other_tasks, struct task_struct, se.group_node);
		list_move_tail(&p->se.group_node, tasks);

#ifdef MTK_QUICK
		if (!flag && (env->imbalance > 0)) {
#else
		if (env->imbalance > 0) {
#endif
			load = task_h_load(p);

			if (over_imbalance(load, env->imbalance)) {
				mt_sched_printf(sched_cmp, "overload others: pid=%d p->comm=%s load=%lu imbalance=%ld",
						p->pid, p->comm, load, env->imbalance);
				continue;
			}

			detach_task(p, env);
			list_add(&p->se.group_node, &env->tasks);
			detached++;
			env->imbalance -= load;

			mt_sched_printf(sched_cmp, "migrate others: pid=%d p->comm=%s load=%lu imbalance=%ld",
					p->pid, p->comm, load, env->imbalance);
		}
	}

	/*
	 * Right now, this is one of only two places we collect this stat
	 * so we can safely collect detach_one_task() stats here rather
	 * than inside detach_one_task().
	 */
	schedstat_add(env->sd, lb_gained[env->idle], detached);

	mt_sched_printf(sched_cmp, "[%s] finish pulled=%d imbalance=%ld",
			__func__, detached, env->imbalance);

	return detached;
}
#endif

/*
 * attach_task() -- attach the task detached by detach_task() to its new rq.
 */
static void attach_task(struct rq *rq, struct task_struct *p)
{
	lockdep_assert_held(&rq->lock);

	BUG_ON(task_rq(p) != rq);
	p->on_rq = TASK_ON_RQ_QUEUED;
	activate_task(rq, p, 0);
	check_preempt_curr(rq, p, 0);
}

/*
 * attach_one_task() -- attaches the task returned from detach_one_task() to
 * its new rq.
 */
static void attach_one_task(struct rq *rq, struct task_struct *p)
{
	raw_spin_lock(&rq->lock);
	attach_task(rq, p);
	raw_spin_unlock(&rq->lock);
}

/*
 * attach_tasks() -- attaches all tasks detached by detach_tasks() to their
 * new rq.
 */
static void attach_tasks(struct lb_env *env)
{
	struct list_head *tasks = &env->tasks;
	struct task_struct *p;

	raw_spin_lock(&env->dst_rq->lock);

	while (!list_empty(tasks)) {
		p = list_first_entry(tasks, struct task_struct, se.group_node);
		list_del_init(&p->se.group_node);

		attach_task(env->dst_rq, p);
	}

	raw_spin_unlock(&env->dst_rq->lock);
}

#ifdef CONFIG_FAIR_GROUP_SCHED
/*
 * update tg->load_weight by folding this cpu's load_avg
 */
static void __update_blocked_averages_cpu(struct task_group *tg, int cpu)
{
	struct sched_entity *se = tg->se[cpu];
	struct cfs_rq *cfs_rq = tg->cfs_rq[cpu];

	/* throttled entities do not contribute to load */
	if (throttled_hierarchy(cfs_rq))
		return;

	update_cfs_rq_blocked_load(cfs_rq, 1);

	if (se) {
		update_entity_load_avg(se, 1);
		/*
		 * We pivot on our runnable average having decayed to zero for
		 * list removal.  This generally implies that all our children
		 * have also been removed (modulo rounding error or bandwidth
		 * control); however, such cases are rare and we can fix these
		 * at enqueue.
		 *
		 * TODO: fix up out-of-order children on enqueue.
		 */
		if (!se->avg.runnable_avg_sum && !cfs_rq->nr_running)
			list_del_leaf_cfs_rq(cfs_rq);
	} else {
		struct rq *rq = rq_of(cfs_rq);
		update_rq_runnable_avg(rq, rq->nr_running);
	}
}

static void update_blocked_averages(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	struct cfs_rq *cfs_rq;
	unsigned long flags;

	raw_spin_lock_irqsave(&rq->lock, flags);
	update_rq_clock(rq);
	/*
	 * Iterates the task_group tree in a bottom up fashion, see
	 * list_add_leaf_cfs_rq() for details.
	 */
	for_each_leaf_cfs_rq(rq, cfs_rq) {
		/*
		 * Note: We may want to consider periodically releasing
		 * rq->lock about these updates so that creating many task
		 * groups does not result in continually extending hold time.
		 */
		__update_blocked_averages_cpu(cfs_rq->tg, rq->cpu);
	}

	raw_spin_unlock_irqrestore(&rq->lock, flags);
}

/*
 * Compute the hierarchical load factor for cfs_rq and all its ascendants.
 * This needs to be done in a top-down fashion because the load of a child
 * group is a fraction of its parents load.
 */
static void update_cfs_rq_h_load(struct cfs_rq *cfs_rq)
{
	struct rq *rq = rq_of(cfs_rq);
	struct sched_entity *se = cfs_rq->tg->se[cpu_of(rq)];
	u64 now = sched_clock_cpu(cpu_of(rq));
	unsigned long load;

	/* sched: change to jiffies */
	now = now * HZ >> 30;

	if (cfs_rq->last_h_load_update == now)
		return;

	cfs_rq->h_load_next = NULL;
	for_each_sched_entity(se) {
		cfs_rq = cfs_rq_of(se);
		cfs_rq->h_load_next = se;
		if (cfs_rq->last_h_load_update == now)
			break;
	}

	if (!se) {
		cfs_rq->h_load = cfs_rq->runnable_load_avg;
		cfs_rq->last_h_load_update = now;
	}

	while ((se = cfs_rq->h_load_next) != NULL) {
		load = cfs_rq->h_load;
		load = div64_ul(load * se->avg.load_avg_contrib,
				cfs_rq->runnable_load_avg + 1);
		cfs_rq = group_cfs_rq(se);
		cfs_rq->h_load = load;
		cfs_rq->last_h_load_update = now;
	}
}

static unsigned long task_h_load(struct task_struct *p)
{
	struct cfs_rq *cfs_rq = task_cfs_rq(p);

	update_cfs_rq_h_load(cfs_rq);
	return div64_ul(p->se.avg.load_avg_contrib * cfs_rq->h_load,
			cfs_rq->runnable_load_avg + 1);
}
#else
static inline void update_blocked_averages(int cpu)
{
}

static unsigned long task_h_load(struct task_struct *p)
{
	return p->se.avg.load_avg_contrib;
}
#endif

/********** Helpers for find_busiest_group ************************/

enum group_type {
	group_other = 0,
	group_imbalanced,
	group_overloaded,
};

/*
 * sg_lb_stats - stats of a sched_group required for load_balancing
 */
struct sg_lb_stats {
	unsigned long avg_load; /*Avg load across the CPUs of the group */
	unsigned long group_load; /* Total load over the CPUs of the group */
	unsigned long sum_weighted_load; /* Weighted load of group's tasks */
	unsigned long load_per_task;
	unsigned long group_capacity;
	unsigned long group_usage; /* Total usage of the group */
	unsigned int sum_nr_running; /* Nr tasks running in the group */
	unsigned int idle_cpus;
	unsigned int group_weight;
	enum group_type group_type;
	int group_no_capacity;
#ifdef CONFIG_NUMA_BALANCING
	unsigned int nr_numa_running;
	unsigned int nr_preferred_running;
#endif
};

/*
 * sd_lb_stats - Structure to store the statistics of a sched_domain
 *		 during load balancing.
 */
struct sd_lb_stats {
	struct sched_group *busiest;	/* Busiest group in this sd */
	struct sched_group *local;	/* Local group in this sd */
	unsigned long total_load;	/* Total load of all groups in sd */
	unsigned long total_capacity;	/* Total capacity of all groups in sd */
	unsigned long avg_load;	/* Average load across all groups in sd */

	struct sg_lb_stats busiest_stat;/* Statistics of the busiest group */
	struct sg_lb_stats local_stat;	/* Statistics of the local group */
};

static inline void init_sd_lb_stats(struct sd_lb_stats *sds)
{
	/*
	 * Skimp on the clearing to avoid duplicate work. We can avoid clearing
	 * local_stat because update_sg_lb_stats() does a full clear/assignment.
	 * We must however clear busiest_stat::avg_load because
	 * update_sd_pick_busiest() reads this before assignment.
	 */
	*sds = (struct sd_lb_stats){
		.busiest = NULL,
		.local = NULL,
		.total_load = 0UL,
		.total_capacity = 0UL,
		.busiest_stat = {
			.avg_load = 0UL,
			.sum_nr_running = 0,
			.group_type = group_other,
		},
	};
}

/**
 * get_sd_load_idx - Obtain the load index for a given sched domain.
 * @sd: The sched_domain whose load_idx is to be obtained.
 * @idle: The idle status of the CPU for whose sd load_idx is obtained.
 *
 * Return: The load index.
 */
static inline int get_sd_load_idx(struct sched_domain *sd,
					enum cpu_idle_type idle)
{
	int load_idx;

	switch (idle) {
	case CPU_NOT_IDLE:
		load_idx = sd->busy_idx;
		break;

	case CPU_NEWLY_IDLE:
		load_idx = sd->newidle_idx;
		break;
	default:
		load_idx = sd->idle_idx;
		break;
	}

	return load_idx;
}

static unsigned long default_scale_capacity(struct sched_domain *sd, int cpu)
{
	return SCHED_CAPACITY_SCALE;
}

unsigned long __weak arch_scale_freq_capacity(struct sched_domain *sd, int cpu)
{
	return default_scale_capacity(sd, cpu);
}

static unsigned long default_scale_cpu_capacity(struct sched_domain *sd, int cpu)
{
	if (sd && (sd->flags & SD_SHARE_CPUCAPACITY) && (sd->span_weight > 1))
		return sd->smt_gain / sd->span_weight;

	return SCHED_CAPACITY_SCALE;
}

unsigned long __weak arch_scale_cpu_capacity(struct sched_domain *sd, int cpu)
{
	return default_scale_cpu_capacity(sd, cpu);
}

static unsigned long scale_rt_capacity(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	u64 total, used, age_stamp, avg;
	s64 delta;

	/*
	 * Since we're reading these variables without serialization make sure
	 * we read them once before doing sanity checks on them.
	 */
	age_stamp = ACCESS_ONCE(rq->age_stamp);
	avg = ACCESS_ONCE(rq->rt_avg);

	delta = rq_clock(rq) - age_stamp;
	if (unlikely(delta < 0))
		delta = 0;

	total = sched_avg_period() + delta;

	used = div_u64(avg, total);

	if (likely(used < SCHED_CAPACITY_SCALE))
		return SCHED_CAPACITY_SCALE - used;

	return 1;
}

static void update_cpu_capacity(struct sched_domain *sd, int cpu)
{
	unsigned long capacity = SCHED_CAPACITY_SCALE;
	struct sched_group *sdg = sd->groups;

	if (sched_feat(ARCH_CAPACITY))
		capacity *= arch_scale_cpu_capacity(sd, cpu);
	else
		capacity *= default_scale_cpu_capacity(sd, cpu);

	capacity >>= SCHED_CAPACITY_SHIFT;

	cpu_rq(cpu)->cpu_capacity_orig = capacity;

	capacity *= scale_rt_capacity(cpu);
	capacity >>= SCHED_CAPACITY_SHIFT;

	if (!capacity)
		capacity = 1;

	cpu_rq(cpu)->cpu_capacity = capacity;
	sdg->sgc->capacity = capacity;

	mt_sched_printf(sched_lb_info, "[%s] %d: %lu ", __func__, cpu, capacity);

}

void update_group_capacity(struct sched_domain *sd, int cpu)
{
	struct sched_domain *child = sd->child;
	struct sched_group *group, *sdg = sd->groups;
	unsigned long capacity;
	unsigned long interval;

	interval = msecs_to_jiffies(sd->balance_interval);
	interval = clamp(interval, 1UL, max_load_balance_interval);
	sdg->sgc->next_update = jiffies + interval;

	if (!child) {
		update_cpu_capacity(sd, cpu);
		return;
	}

	capacity = 0;

	if (child->flags & SD_OVERLAP) {
		/*
		 * SD_OVERLAP domains cannot assume that child groups
		 * span the current group.
		 */

		for_each_cpu(cpu, sched_group_cpus(sdg)) {
			struct sched_group_capacity *sgc;
			struct rq *rq = cpu_rq(cpu);

			/*
			 * build_sched_domains() -> init_sched_groups_capacity()
			 * gets here before we've attached the domains to the
			 * runqueues.
			 *
			 * Use capacity_of(), which is set irrespective of domains
			 * in update_cpu_capacity().
			 *
			 * This avoids capacity from being 0 and
			 * causing divide-by-zero issues on boot.
			 */
			if (unlikely(!rq->sd)) {
				capacity += capacity_of(cpu);
				continue;
			}

			sgc = rq->sd->groups->sgc;
			capacity += sgc->capacity;
		}
	} else  {
		/*
		 * !SD_OVERLAP domains can assume that child groups
		 * span the current group.
		 */ 

		group = child->groups;
		do {
			capacity += group->sgc->capacity;
			mt_sched_printf(sched_lb_info, "[%s] %lu: %lu ",
				__func__, sched_group_cpus(group)->bits[0], capacity);
			group = group->next;
		} while (group != child->groups);
	}

	sdg->sgc->capacity = capacity;
	mt_sched_printf(sched_lb_info, "[%s] final %d: %lu ", __func__, cpu, capacity);
}

/*
 * Check whether the capacity of the rq has been noticeably reduced by side
 * activity. The imbalance_pct is used for the threshold.
 * Return true is the capacity is reduced
 */
static inline int
check_cpu_capacity(struct rq *rq, struct sched_domain *sd)
{
	return (rq->cpu_capacity * sd->imbalance_pct) <
				(rq->cpu_capacity_orig * 100);
}

/*
 * Group imbalance indicates (and tries to solve) the problem where balancing
 * groups is inadequate due to tsk_cpus_allowed() constraints.
 *
 * Imagine a situation of two groups of 4 cpus each and 4 tasks each with a
 * cpumask covering 1 cpu of the first group and 3 cpus of the second group.
 * Something like:
 *
 * 	{ 0 1 2 3 } { 4 5 6 7 }
 * 	        *     * * *
 *
 * If we were to balance group-wise we'd place two tasks in the first group and
 * two tasks in the second group. Clearly this is undesired as it will overload
 * cpu 3 and leave one of the cpus in the second group unused.
 *
 * The current solution to this issue is detecting the skew in the first group
 * by noticing the lower domain failed to reach balance and had difficulty
 * moving tasks due to affinity constraints.
 *
 * When this is so detected; this group becomes a candidate for busiest; see
 * update_sd_pick_busiest(). And calculate_imbalance() and
 * find_busiest_group() avoid some of the usual balance conditions to allow it
 * to create an effective group imbalance.
 *
 * This is a somewhat tricky proposition since the next run might not find the
 * group imbalance and decide the groups need to be balanced again. A most
 * subtle and fragile situation.
 */

static inline int sg_imbalanced(struct sched_group *group)
{
	return group->sgc->imbalance;
}

/*
 * group_has_capacity returns true if the group has spare capacity that could
 * be used by some tasks.
 * We consider that a group has spare capacity if the  * number of task is
 * smaller than the number of CPUs or if the usage is lower than the available
 * capacity for CFS tasks.
 * For the latter, we use a threshold to stabilize the state, to take into
 * account the variance of the tasks' load and to return true if the available
 * capacity in meaningful for the load balancer.
 * As an example, an available capacity of 1% can appear but it doesn't make
 * any benefit for the load balance.
 */
static inline bool
group_has_capacity(struct lb_env *env, struct sg_lb_stats *sgs)
{
	if (sgs->sum_nr_running < sgs->group_weight)
		return true;

	if ((sgs->group_capacity * 100) >
			(sgs->group_usage * env->sd->imbalance_pct))
		return true;

	return false;
}

/*
 *  group_is_overloaded returns true if the group has more tasks than it can
 *  handle.
 *  group_is_overloaded is not equals to !group_has_capacity because a group
 *  with the exact right number of tasks, has no more spare capacity but is not
 *  overloaded so both group_has_capacity and group_is_overloaded return
 *  false.
 */
static inline bool
group_is_overloaded(struct lb_env *env, struct sg_lb_stats *sgs)
{
	if (sgs->sum_nr_running <= sgs->group_weight)
		return false;

	if ((sgs->group_capacity * 100) <
			(sgs->group_usage * env->sd->imbalance_pct))
		return true;

	return false;
}

static enum group_type group_classify(struct lb_env *env,
		struct sched_group *group,
		struct sg_lb_stats *sgs)
{
	if (sgs->group_no_capacity)
		return group_overloaded;

	if (sg_imbalanced(group))
		return group_imbalanced;

	return group_other;
}

/**
 * update_sg_lb_stats - Update sched_group's statistics for load balancing.
 * @env: The load balancing environment.
 * @group: sched_group whose statistics are to be updated.
 * @load_idx: Load index of sched_domain of this_cpu for load calc.
 * @local_group: Does group contain this_cpu.
 * @sgs: variable to hold the statistics for this group.
 * @overload: Indicate more than one runnable task for any CPU.
 */
static inline void update_sg_lb_stats(struct lb_env *env,
			struct sched_group *group, int load_idx,
			int local_group, struct sg_lb_stats *sgs,
			bool *overload)
{
	unsigned long load;
	int i;

	memset(sgs, 0, sizeof(*sgs));

	for_each_cpu_and(i, sched_group_cpus(group), env->cpus) {
		struct rq *rq = cpu_rq(i);

		/* Bias balancing toward cpus of our domain */
		if (local_group)
			load = target_load(i, load_idx);
		else
			load = source_load(i, load_idx);

#ifdef CONFIG_MT_LOAD_BALANCE_ENHANCEMENT
		sgs->group_load += (load * capacity_orig_of(i)) >> SCHED_CAPACITY_SHIFT;
#else
		sgs->group_load += load;
#endif
		sgs->group_usage += get_cpu_usage(i);
		sgs->sum_nr_running += rq->cfs.h_nr_running;

		if (rq->nr_running > 1)
			*overload = true;

#ifdef CONFIG_NUMA_BALANCING
		sgs->nr_numa_running += rq->nr_numa_running;
		sgs->nr_preferred_running += rq->nr_preferred_running;
#endif
		sgs->sum_weighted_load += weighted_cpuload(i);
		if (idle_cpu(i))
			sgs->idle_cpus++;

		mt_sched_printf(sched_lb_info, "[%s] cpu=%d grp_load=%lu load=%lu cap=%lu nr=%d sum_nr=%d overload=%d",
			__func__,
			i, sgs->group_load, load, capacity_orig_of(i), rq->nr_running, sgs->sum_nr_running, *overload);
	}

	/* Adjust by relative CPU capacity of the group */
	sgs->group_capacity = group->sgc->capacity;
	sgs->avg_load = (sgs->group_load*SCHED_CAPACITY_SCALE) / sgs->group_capacity;

	mt_sched_printf(sched_lb_info, "[%s] cpu=0x%lx avg_load=%lu group_load=%lu cap=%lu",
		__func__, sched_group_cpus(group)->bits[0],
		sgs->avg_load, sgs->group_load, sgs->group_capacity);

	if (sgs->sum_nr_running)
		sgs->load_per_task = sgs->sum_weighted_load / sgs->sum_nr_running;

	sgs->group_weight = group->group_weight;

	sgs->group_no_capacity = group_is_overloaded(env, sgs);
	sgs->group_type = group_classify(env, group, sgs);
	mt_sched_printf(sched_lb_info, "[%s] cpu=0x%lx no_capacity=%d weight=%d nr=%d type=%d, usage=%lu",
		__func__, sched_group_cpus(group)->bits[0],
		sgs->group_no_capacity, sgs->group_weight, sgs->sum_nr_running,
		sgs->group_type, sgs->group_usage);
}

/**
 * update_sd_pick_busiest - return 1 on busiest group
 * @env: The load balancing environment.
 * @sds: sched_domain statistics
 * @sg: sched_group candidate to be checked for being the busiest
 * @sgs: sched_group statistics
 *
 * Determine if @sg is a busier group than the previously selected
 * busiest group.
 *
 * Return: %true if @sg is a busier group than the previously selected
 * busiest group. %false otherwise.
 */
static bool update_sd_pick_busiest(struct lb_env *env,
				   struct sd_lb_stats *sds,
				   struct sched_group *sg,
				   struct sg_lb_stats *sgs)
{
	struct sg_lb_stats *busiest = &sds->busiest_stat;

	mt_sched_printf(sched_lb_info, "[%s] cpu=0x%lx sgs_type=%d b_type=%d %lu %lu sum_nr_running=%d grp_weight=%d",
		__func__,
		sched_group_cpus(sg)->bits[0], sgs->group_type,
		busiest->group_type, sgs->avg_load, busiest->avg_load, sgs->sum_nr_running, sgs->group_weight);

	if (sgs->group_type > busiest->group_type)
		return true;

	if (sgs->group_type < busiest->group_type) {
		mt_sched_printf(sched_lb_info, "[%s] %d: fail busiest 1", __func__, env->src_cpu);
		return false;
	}

	if (sgs->avg_load <= busiest->avg_load) {
		mt_sched_printf(sched_lb_info, "[%s] %d: fail busiest 2", __func__, env->src_cpu);
		return false;
	}

	/* This is the busiest node in its class. */
	if (!(env->sd->flags & SD_ASYM_PACKING))
		return true;

	/*
	 * ASYM_PACKING needs to move all the work to the lowest
	 * numbered CPUs in the group, therefore mark all groups
	 * higher than ourself as busy.
	 */
	if (sgs->sum_nr_running && env->dst_cpu < group_first_cpu(sg)) {
		if (!sds->busiest)
			return true;

		if (group_first_cpu(sds->busiest) > group_first_cpu(sg))
			return true;
	}

	mt_sched_printf(sched_lb_info, "[%s] %d: fail busiest 3", __func__, env->src_cpu);

	return false;
}

#ifdef CONFIG_NUMA_BALANCING
static inline enum fbq_type fbq_classify_group(struct sg_lb_stats *sgs)
{
	if (sgs->sum_nr_running > sgs->nr_numa_running)
		return regular;
	if (sgs->sum_nr_running > sgs->nr_preferred_running)
		return remote;
	return all;
}

static inline enum fbq_type fbq_classify_rq(struct rq *rq)
{
	if (rq->nr_running > rq->nr_numa_running)
		return regular;
	if (rq->nr_running > rq->nr_preferred_running)
		return remote;
	return all;
}
#else
static inline enum fbq_type fbq_classify_group(struct sg_lb_stats *sgs)
{
	return all;
}

static inline enum fbq_type fbq_classify_rq(struct rq *rq)
{
	return regular;
}
#endif /* CONFIG_NUMA_BALANCING */

/**
 * update_sd_lb_stats - Update sched_domain's statistics for load balancing.
 * @env: The load balancing environment.
 * @sds: variable to hold the statistics for this sched_domain.
 */
static inline void update_sd_lb_stats(struct lb_env *env, struct sd_lb_stats *sds)
{
	struct sched_domain *child = env->sd->child;
	struct sched_group *sg = env->sd->groups;
	struct sg_lb_stats tmp_sgs;
	int load_idx, prefer_sibling = 0;
	bool overload = false;

	if (child && child->flags & SD_PREFER_SIBLING)
		prefer_sibling = 1;

	load_idx = get_sd_load_idx(env->sd, env->idle);

	do {
		struct sg_lb_stats *sgs = &tmp_sgs;
		int local_group;

		local_group = cpumask_test_cpu(env->dst_cpu, sched_group_cpus(sg));
		if (local_group) {
			sds->local = sg;
			sgs = &sds->local_stat;

			if (env->idle != CPU_NEWLY_IDLE ||
			    time_after_eq(jiffies, sg->sgc->next_update))
				update_group_capacity(env->sd, env->dst_cpu);
		}

		update_sg_lb_stats(env, sg, load_idx, local_group, sgs,
						&overload);

		mt_sched_printf(sched_lb_info, "[%s] cpu=0x%lx load=%lu",
			__func__, sched_group_cpus(sg)->bits[0], sgs->avg_load);

		if (local_group)
			goto next_group;

		/*
		 * In case the child domain prefers tasks go to siblings
		 * first, lower the sg capacity so that we'll try
		 * and move all the excess tasks away. We lower the capacity
		 * of a group only if the local group has the capacity to fit
		 * these excess tasks. The extra check prevents the case where
		 * you always pull from the heaviest group when it is already
		 * under-utilized (possible with a large weight task outweighs
		 * the tasks on the system).
		 */
		if (prefer_sibling && sds->local &&
		    group_has_capacity(env, &sds->local_stat) &&
			(sgs->sum_nr_running > 1)) {
			sgs->group_no_capacity = 1;
			sgs->group_type = group_overloaded;
		}

		if (update_sd_pick_busiest(env, sds, sg, sgs)) {
			sds->busiest = sg;
			sds->busiest_stat = *sgs;
		}

next_group:
		/* Now, start updating sd_lb_stats */
		sds->total_load += sgs->group_load;
		sds->total_capacity += sgs->group_capacity;
		mt_sched_printf(sched_lb, "[%s] total_load=%lu grp_load=%lu total_capacity=%lu total_grp_capacity=%lu",
			__func__, sds->total_load, sgs->group_load, sds->total_capacity, sgs->group_capacity);

		sg = sg->next;
	} while (sg != env->sd->groups);

	if (env->sd->flags & SD_NUMA)
		env->fbq_type = fbq_classify_group(&sds->busiest_stat);

	if (!env->sd->parent) {
		/* update overload indicator if we are at root domain */
		if (env->dst_rq->rd->overload != overload)
			env->dst_rq->rd->overload = overload;
	}

}

/**
 * check_asym_packing - Check to see if the group is packed into the
 *			sched doman.
 *
 * This is primarily intended to used at the sibling level.  Some
 * cores like POWER7 prefer to use lower numbered SMT threads.  In the
 * case of POWER7, it can move to lower SMT modes only when higher
 * threads are idle.  When in lower SMT modes, the threads will
 * perform better since they share less core resources.  Hence when we
 * have idle threads, we want them to be the higher ones.
 *
 * This packing function is run on idle threads.  It checks to see if
 * the busiest CPU in this domain (core in the P7 case) has a higher
 * CPU number than the packing function is being run on.  Here we are
 * assuming lower CPU number will be equivalent to lower a SMT thread
 * number.
 *
 * Return: 1 when packing is required and a task should be moved to
 * this CPU.  The amount of the imbalance is returned in *imbalance.
 *
 * @env: The load balancing environment.
 * @sds: Statistics of the sched_domain which is to be packed
 */
static int check_asym_packing(struct lb_env *env, struct sd_lb_stats *sds)
{
	int busiest_cpu;

	if (!(env->sd->flags & SD_ASYM_PACKING))
		return 0;

	if (!sds->busiest)
		return 0;

	busiest_cpu = group_first_cpu(sds->busiest);
	if (env->dst_cpu > busiest_cpu)
		return 0;

	env->imbalance = DIV_ROUND_CLOSEST(
		sds->busiest_stat.avg_load * sds->busiest_stat.group_capacity,
		SCHED_CAPACITY_SCALE);

	return 1;
}

/**
 * fix_small_imbalance - Calculate the minor imbalance that exists
 *			amongst the groups of a sched_domain, during
 *			load balancing.
 * @env: The load balancing environment.
 * @sds: Statistics of the sched_domain whose imbalance is to be calculated.
 */
static inline
void fix_small_imbalance(struct lb_env *env, struct sd_lb_stats *sds)
{
	unsigned long tmp, capa_now = 0, capa_move = 0;
	unsigned int imbn = 2;
	unsigned long scaled_busy_load_per_task;
	struct sg_lb_stats *local, *busiest;

	local = &sds->local_stat;
	busiest = &sds->busiest_stat;

	if (!local->sum_nr_running)
		local->load_per_task = cpu_avg_load_per_task(env->dst_cpu);
	else if (busiest->load_per_task > local->load_per_task)
		imbn = 1;

	scaled_busy_load_per_task =
		(busiest->load_per_task * SCHED_CAPACITY_SCALE) /
		busiest->group_capacity;

	if (busiest->avg_load + scaled_busy_load_per_task >=
	    local->avg_load + (scaled_busy_load_per_task * imbn)) {
		env->imbalance = busiest->load_per_task;
		return;
	}

	/*
	 * OK, we don't have enough imbalance to justify moving tasks,
	 * however we may be able to increase total CPU capacity used by
	 * moving them.
	 */

	capa_now += busiest->group_capacity *
			min(busiest->load_per_task, busiest->avg_load);
	capa_now += local->group_capacity *
			min(local->load_per_task, local->avg_load);
	capa_now /= SCHED_CAPACITY_SCALE;

	/* Amount of load we'd subtract */
	if (busiest->avg_load > scaled_busy_load_per_task) {
		capa_move += busiest->group_capacity *
			    min(busiest->load_per_task,
				busiest->avg_load - scaled_busy_load_per_task);
	}

	/* Amount of load we'd add */
	if (busiest->avg_load * busiest->group_capacity <
	    busiest->load_per_task * SCHED_CAPACITY_SCALE) {
		tmp = (busiest->avg_load * busiest->group_capacity) /
		      local->group_capacity;
	} else {
		tmp = (busiest->load_per_task * SCHED_CAPACITY_SCALE) /
		      local->group_capacity;
	}
	capa_move += local->group_capacity *
		    min(local->load_per_task, local->avg_load + tmp);
	capa_move /= SCHED_CAPACITY_SCALE;

	/* Move if we gain throughput */
	if (capa_move > capa_now)
		env->imbalance = busiest->load_per_task;
}

/**
 * calculate_imbalance - Calculate the amount of imbalance present within the
 *			 groups of a given sched_domain during load balance.
 * @env: load balance environment
 * @sds: statistics of the sched_domain whose imbalance is to be calculated.
 */
static inline void calculate_imbalance(struct lb_env *env, struct sd_lb_stats *sds)
{
	unsigned long max_pull, load_above_capacity = ~0UL;
	struct sg_lb_stats *local, *busiest;

	local = &sds->local_stat;
	busiest = &sds->busiest_stat;

	if (busiest->group_type == group_imbalanced) {
		/*
		 * In the group_imb case we cannot rely on group-wide averages
		 * to ensure cpu-load equilibrium, look at wider averages. XXX
		 */
		busiest->load_per_task =
			min(busiest->load_per_task, sds->avg_load);
	}

	/*
	 * In the presence of smp nice balancing, certain scenarios can have
	 * max load less than avg load(as we skip the groups at or below
	 * its cpu_capacity, while calculating max_load..)
	 */
	if (busiest->avg_load <= sds->avg_load ||
	    local->avg_load >= sds->avg_load) {
		env->imbalance = 0;
		return fix_small_imbalance(env, sds);
	}

	/*
	 * If there aren't any idle cpus, avoid creating some.
	 */
	if (busiest->group_type == group_overloaded &&
	    local->group_type   == group_overloaded) {
		load_above_capacity = busiest->sum_nr_running *
					SCHED_LOAD_SCALE;
		if (load_above_capacity > busiest->group_capacity)
			load_above_capacity -= busiest->group_capacity;
		else
			load_above_capacity = ~0UL;
	}

	/*
	 * We're trying to get all the cpus to the average_load, so we don't
	 * want to push ourselves above the average load, nor do we wish to
	 * reduce the max loaded cpu below the average load. At the same time,
	 * we also don't want to reduce the group load below the group capacity
	 * (so that we can implement power-savings policies etc). Thus we look
	 * for the minimum possible imbalance.
	 */
	max_pull = min(busiest->avg_load - sds->avg_load, load_above_capacity);

	/* How much load to actually move to equalise the imbalance */
	env->imbalance = min(
		max_pull * busiest->group_capacity,
		(sds->avg_load - local->avg_load) * local->group_capacity
	) / SCHED_CAPACITY_SCALE;

	mt_sched_printf(sched_lb,
		"[%s] imbalance=%lu  max_pull=%lu,  busiest_cap=%lu, avg_load=%lu local_avg_load=%lu local_cap=%lu",
		__func__, env->imbalance, max_pull, busiest->group_capacity, sds->avg_load,
		local->avg_load, local->group_capacity);
	/*
	 * if *imbalance is less than the average load per runnable task
	 * there is no guarantee that any tasks will be moved so we'll have
	 * a think about bumping its value to force at least one task to be
	 * moved
	 */
	if (env->imbalance < busiest->load_per_task)
		return fix_small_imbalance(env, sds);
}

/******* find_busiest_group() helpers end here *********************/

/**
 * find_busiest_group - Returns the busiest group within the sched_domain
 * if there is an imbalance. If there isn't an imbalance, and
 * the user has opted for power-savings, it returns a group whose
 * CPUs can be put to idle by rebalancing those tasks elsewhere, if
 * such a group exists.
 *
 * Also calculates the amount of weighted load which should be moved
 * to restore balance.
 *
 * @env: The load balancing environment.
 *
 * Return:	- The busiest group if imbalance exists.
 *		- If no imbalance and user has opted for power-savings balance,
 *		   return the least loaded group whose CPUs can be
 *		   put to idle by rebalancing its tasks onto our group.
 */
static struct sched_group *find_busiest_group(struct lb_env *env)
{
	struct sg_lb_stats *local, *busiest;
	struct sd_lb_stats sds;

	init_sd_lb_stats(&sds);

	/*
	 * Compute the various statistics relavent for load balancing at
	 * this level.
	 */
	update_sd_lb_stats(env, &sds);
	local = &sds.local_stat;
	busiest = &sds.busiest_stat;

	/* ASYM feature bypasses nice load balance check */
	if ((env->idle == CPU_IDLE || env->idle == CPU_NEWLY_IDLE) &&
	    check_asym_packing(env, &sds))
		return sds.busiest;

	/* There is no busy sibling group to pull tasks from */
	if (!sds.busiest || busiest->sum_nr_running == 0) {
		if (!sds.busiest)
			mt_sched_printf(sched_lb, "[%s] %d: fail no busiest ", __func__, env->src_cpu);
		else
			mt_sched_printf(sched_lb, "[%s] %d: fail busiest no task ", __func__, env->src_cpu);
		goto out_balanced;
	}

	sds.avg_load = (SCHED_CAPACITY_SCALE * sds.total_load)
						/ sds.total_capacity;
	mt_sched_printf(sched_lb, "[%s] %d: avg_load=%lu total=%lu capacity=%lu",
		__func__, env->src_cpu, sds.avg_load, sds.total_load, sds.total_capacity);

	/*
	 * If the busiest group is imbalanced the below checks don't
	 * work because they assume all things are equal, which typically
	 * isn't true due to cpus_allowed constraints and the like.
	 */
	if (busiest->group_type == group_imbalanced)
		goto force_balance;


	/* SD_BALANCE_NEWIDLE trumps SMP nice when underutilized */
	if (env->idle == CPU_NEWLY_IDLE && group_has_capacity(env, local) &&
	    busiest->group_no_capacity)
		goto force_balance;

	/*
	 * If the local group is busier than the selected busiest group
	 * don't try and pull any tasks.
	 */
	if (local->avg_load >= busiest->avg_load) {
		mt_sched_printf(sched_lb, "[%s] %d: fail no larger %lu busiest=%lu", __func__,
			env->src_cpu, local->avg_load, busiest->avg_load);
		goto out_balanced;
	}

	/*
	 * Don't pull any tasks if this group is already above the domain
	 * average load.
	 */
	if (local->avg_load >= sds.avg_load) {
		mt_sched_printf(sched_lb, "[%s] %d: fail no larger %lu sds=%lu", __func__,
			env->src_cpu, local->avg_load, sds.avg_load);
		goto out_balanced;
	}

#ifdef CONFIG_MT_LOAD_BALANCE_ENHANCEMENT
	if ((env->idle == CPU_IDLE) || (env->idle == CPU_NEWLY_IDLE)) {
		int i = (env->idle == CPU_IDLE) ? 1:0;
#else
	if (env->idle == CPU_IDLE) {
#endif
		/*
		 * This cpu is idle. If the busiest group is not overloaded
		 * and there is no imbalance between this and busiest group
		 * wrt idle cpus, it is balanced. The imbalance becomes
		 * significant if the diff is greater than 1 otherwise we
		 * might end up to just move the imbalance on another group
		 */
#ifdef CONFIG_MT_LOAD_BALANCE_ENHANCEMENT
		if ((busiest->group_type != group_overloaded) &&
				(local->idle_cpus < (busiest->idle_cpus + i))) {
#else
		if ((busiest->group_type != group_overloaded) &&
				(local->idle_cpus <= (busiest->idle_cpus + 1))) {
#endif

			mt_sched_printf(sched_lb, "[%s] fail b_type=%d s_idles=%d b_idles=%d",
				__func__, busiest->group_type, local->idle_cpus, busiest->idle_cpus);
			goto out_balanced;
		}
	} else {
		/*
		 * In the CPU_NEWLY_IDLE, CPU_NOT_IDLE cases, use
		 * imbalance_pct to be conservative.
		 */
		if (100 * busiest->avg_load <=
				env->sd->imbalance_pct * local->avg_load){
			mt_sched_printf(sched_lb, "[%s] %d: fail imbalance_pct fail %lu %d %lu",
				__func__, env->src_cpu, busiest->avg_load, env->sd->imbalance_pct, local->avg_load);
			goto out_balanced;
		}
	}

force_balance:
	/* Looks like there is an imbalance. Compute it */
	calculate_imbalance(env, &sds);
#ifdef CONFIG_MT_LOAD_BALANCE_ENHANCEMENT
	mt_sched_printf(sched_lb, "[%s] %d: imbalance=%lu",
		__func__, env->src_cpu, env->imbalance);
	env->imbalance = env->imbalance * SCHED_CAPACITY_SCALE
				/ (sds.busiest->sgc->capacity / cpumask_weight(sched_group_cpus(sds.busiest)));
	mt_sched_printf(sched_lb, "[%s] %d: imbalance=%lu capacity=%d mask=0x%lx",
		__func__, env->src_cpu, env->imbalance,
		sds.busiest->sgc->capacity, sched_group_cpus(sds.busiest)->bits[0]);
#endif
	return sds.busiest;

out_balanced:
	env->imbalance = 0;
	mt_sched_printf(sched_lb, "[%s] fail out balance", __func__);
	return NULL;
}

/*
 * find_busiest_queue - find the busiest runqueue among the cpus in group.
 */
static struct rq *find_busiest_queue(struct lb_env *env,
				     struct sched_group *group)
{
	struct rq *busiest = NULL, *rq;
	unsigned long busiest_load = 0, busiest_capacity = 1;
	int i;

	for_each_cpu_and(i, sched_group_cpus(group), env->cpus) {
		unsigned long capacity, wl;
		enum fbq_type rt;

		rq = cpu_rq(i);
		rt = fbq_classify_rq(rq);

		/*
		 * We classify groups/runqueues into three groups:
		 *  - regular: there are !numa tasks
		 *  - remote:  there are numa tasks that run on the 'wrong' node
		 *  - all:     there is no distinction
		 *
		 * In order to avoid migrating ideally placed numa tasks,
		 * ignore those when there's better options.
		 *
		 * If we ignore the actual busiest queue to migrate another
		 * task, the next balance pass can still reduce the busiest
		 * queue by moving tasks around inside the node.
		 *
		 * If we cannot move enough load due to this classification
		 * the next pass will adjust the group classification and
		 * allow migration of more tasks.
		 *
		 * Both cases only affect the total convergence complexity.
		 */
		if (rt > env->fbq_type)
			continue;

		capacity = capacity_of(i);

		wl = weighted_cpuload(i);

		/*
		 * When comparing with imbalance, use weighted_cpuload()
		 * which is not scaled with the cpu capacity.
		 */

		if (rq->nr_running == 1 && wl > env->imbalance &&
		    !check_cpu_capacity(rq, env->sd))
			continue;

		/*
		 * For the load comparisons with the other cpu's, consider
		 * the weighted_cpuload() scaled with the cpu capacity, so
		 * that the load can be moved away from the cpu that is
		 * potentially running at a lower capacity.
		 *
		 * Thus we're looking for max(wl_i / capacity_i), crosswise
		 * multiplication to rid ourselves of the division works out
		 * to: wl_i * capacity_j > wl_j * capacity_i;  where j is
		 * our previous maximum.
		 */
		if (wl * busiest_capacity > busiest_load * capacity) {
			busiest_load = wl;
			busiest_capacity = capacity;
			busiest = rq;
		}
	}

	return busiest;
}

/*
 * Max backoff if we encounter pinned tasks. Pretty arbitrary value, but
 * so long as it is large enough.
 */
#define MAX_PINNED_INTERVAL	512

/* Working cpumask for load_balance and load_balance_newidle. */
DEFINE_PER_CPU(cpumask_var_t, load_balance_mask);

static int need_active_balance(struct lb_env *env)
{
	struct sched_domain *sd = env->sd;

	if (env->idle == CPU_NEWLY_IDLE) {

		/*
		 * ASYM_PACKING needs to force migrate tasks from busy but
		 * higher numbered CPUs in order to pack all tasks in the
		 * lowest numbered CPUs.
		 */
		if ((sd->flags & SD_ASYM_PACKING) && env->src_cpu > env->dst_cpu)
			return 1;
	}

	/*
	 * The dst_cpu is idle and the src_cpu CPU has only 1 CFS task.
	 * It's worth migrating the task if the src_cpu's capacity is reduced
	 * because of other sched_class or IRQs if more capacity stays
	 * available on dst_cpu.
	 */
	if ((env->idle != CPU_NOT_IDLE) &&
	    (env->src_rq->cfs.h_nr_running == 1)) {
		if ((check_cpu_capacity(env->src_rq, sd)) &&
		    (capacity_of(env->src_cpu)*sd->imbalance_pct < capacity_of(env->dst_cpu)*100))
			return 1;
	}

	return unlikely(sd->nr_balance_failed > sd->cache_nice_tries+2);
}

static int active_load_balance_cpu_stop(void *data);

static int should_we_balance(struct lb_env *env)
{
	struct sched_group *sg = env->sd->groups;
	struct cpumask *sg_cpus, *sg_mask;
	int cpu, balance_cpu = -1;

	/*
	 * In the newly idle case, we will allow all the cpu's
	 * to do the newly idle load balance.
	 */
	if (env->idle == CPU_NEWLY_IDLE)
		return 1;

	sg_cpus = sched_group_cpus(sg);
	sg_mask = sched_group_mask(sg);
	/* Try to find first idle cpu */
	for_each_cpu_and(cpu, sg_cpus, env->cpus) {
		if (!cpumask_test_cpu(cpu, sg_mask) || !idle_cpu(cpu))
			continue;

		balance_cpu = cpu;
		break;
	}

	if (balance_cpu == -1)
		balance_cpu = group_balance_cpu(sg);

	/*
	 * First idle cpu or the first cpu(busiest) in this sched group
	 * is eligible for doing load balancing at this and above domains.
	 */
	return balance_cpu == env->dst_cpu;
}

/*
 * Check this_cpu to ensure it is balanced within domain. Attempt to move
 * tasks if there is an imbalance.
 */
static int load_balance(int this_cpu, struct rq *this_rq,
			struct sched_domain *sd, enum cpu_idle_type idle,
			int *continue_balancing)
{
	int ld_moved, cur_ld_moved, active_balance = 0;
	struct sched_domain *sd_parent = sd->parent;
	struct sched_group *group;
	struct rq *busiest;
	unsigned long flags;
	struct cpumask *cpus = this_cpu_cpumask_var_ptr(load_balance_mask);

	struct lb_env env = {
		.sd		= sd,
		.dst_cpu	= this_cpu,
		.dst_rq		= this_rq,
		.dst_grpmask    = sched_group_cpus(sd->groups),
		.idle		= idle,
		.loop_break	= sched_nr_migrate_break,
		.cpus		= cpus,
		.fbq_type	= all,
		.tasks		= LIST_HEAD_INIT(env.tasks),
	};

	/*
	 * For NEWLY_IDLE load_balancing, we don't need to consider
	 * other cpus in our group
	 */
	if (idle == CPU_NEWLY_IDLE)
		env.dst_grpmask = NULL;

	cpumask_copy(cpus, cpu_active_mask);

	schedstat_inc(sd, lb_count[idle]);

redo:
	if (!should_we_balance(&env)) {
		*continue_balancing = 0;
		goto out_balanced;
	}

	group = find_busiest_group(&env);
	if (!group) {
		schedstat_inc(sd, lb_nobusyg[idle]);
		mt_sched_printf(sched_lb, "[%s] fail no group", __func__);
		goto out_balanced;
	}

	busiest = find_busiest_queue(&env, group);
	if (!busiest) {
		schedstat_inc(sd, lb_nobusyq[idle]);
		mt_sched_printf(sched_lb, "[%s] fail no busyq", __func__);
		goto out_balanced;
	}

#ifdef CONFIG_HMP_PACK_SMALL_TASK
	if (hmp_cpu_is_fastest(this_cpu)) {
		if (per_cpu(sd_pack_buddy, this_cpu) == busiest->cpu &&
			!is_buddy_busy(per_cpu(sd_pack_buddy, this_cpu))) {
			schedstat_inc(sd, lb_nobusyq[idle]);
			goto out_balanced;
		}
	}
#endif
	BUG_ON(busiest == env.dst_rq);

	schedstat_add(sd, lb_imbalance[idle], env.imbalance);

	env.src_cpu = busiest->cpu;
	env.src_rq = busiest;

	ld_moved = 0;
	if (busiest->nr_running > 1) {
		/*
		 * Attempt to move tasks. If find_busiest_group has found
		 * an imbalance but busiest->nr_running <= 1, the group is
		 * still unbalanced. ld_moved simply stays zero, so it is
		 * correctly treated as an imbalance.
		 */
		env.flags |= LBF_ALL_PINNED;
		env.loop_max  = min(sysctl_sched_nr_migrate, busiest->nr_running);
#ifdef CONFIG_MT_LOAD_BALANCE_ENHANCEMENT
		env.mt_ignore_cachehot_in_idle = 0;
#endif

more_balance:
		raw_spin_lock_irqsave(&busiest->lock, flags);
#ifdef CONFIG_MTK_SCHED_CMP_TGS
		env.loop_max	= min_t(unsigned long, sysctl_sched_nr_migrate, busiest->nr_running);
#endif /* CONFIG_MTK_SCHED_CMP */
		mt_sched_printf(sched_lb,
			"1 busiest->nr_running=%d src=%d, dst=%d, cpus_share_cache=%d loop_max=%d loop=%d imbalance=%ld"
			, busiest->nr_running, env.src_cpu, env.dst_cpu, cpus_share_cache(env.src_cpu, env.dst_cpu),
			env.loop_max, env.loop, env.imbalance);
		/*
		 * cur_ld_moved - load moved in current iteration
		 * ld_moved     - cumulative load moved across iterations
		 */
#ifdef CONFIG_MTK_SCHED_CMP_TGS
/*		if (!cpus_share_cache(env.src_cpu, env.dst_cpu ) ) */
		if (!cpus_share_cache(env.src_cpu, env.dst_cpu) && (sd->flags & SD_BALANCE_TG))
			cur_ld_moved = tgs_detach_tasks(&env);
		else
			cur_ld_moved = detach_tasks(&env);
#else
		cur_ld_moved = detach_tasks(&env);
#endif
		/*
		 * We've detached some tasks from busiest_rq. Every
		 * task is masked "TASK_ON_RQ_MIGRATING", so we can safely
		 * unlock busiest->lock, and we are able to be sure
		 * that nobody can manipulate the tasks in parallel.
		 * See task_rq_lock() family for the details.
		 */

		raw_spin_unlock(&busiest->lock);

		if (cur_ld_moved) {
			attach_tasks(&env);
			ld_moved += cur_ld_moved;
		}

		local_irq_restore(flags);

		if (env.flags & LBF_NEED_BREAK) {
			env.flags &= ~LBF_NEED_BREAK;
			goto more_balance;
		}

		/*
		 * Revisit (affine) tasks on src_cpu that couldn't be moved to
		 * us and move them to an alternate dst_cpu in our sched_group
		 * where they can run. The upper limit on how many times we
		 * iterate on same src_cpu is dependent on number of cpus in our
		 * sched_group.
		 *
		 * This changes load balance semantics a bit on who can move
		 * load to a given_cpu. In addition to the given_cpu itself
		 * (or a ilb_cpu acting on its behalf where given_cpu is
		 * nohz-idle), we now have balance_cpu in a position to move
		 * load to given_cpu. In rare situations, this may cause
		 * conflicts (balance_cpu and given_cpu/ilb_cpu deciding
		 * _independently_ and at _same_ time to move some load to
		 * given_cpu) causing exceess load to be moved to given_cpu.
		 * This however should not happen so much in practice and
		 * moreover subsequent load balance cycles should correct the
		 * excess load moved.
		 */
		if ((env.flags & LBF_DST_PINNED) && env.imbalance > 0) {

			/* Prevent to re-select dst_cpu via env's cpus */
			cpumask_clear_cpu(env.dst_cpu, env.cpus);

			env.dst_rq	 = cpu_rq(env.new_dst_cpu);
			env.dst_cpu	 = env.new_dst_cpu;
			env.flags	&= ~LBF_DST_PINNED;
			env.loop	 = 0;
			env.loop_break	 = sched_nr_migrate_break;

			/*
			 * Go back to "more_balance" rather than "redo" since we
			 * need to continue with same src_cpu.
			 */
			goto more_balance;
		}

		/*
		 * We failed to reach balance because of affinity.
		 */
		if (sd_parent) {
			int *group_imbalance = &sd_parent->groups->sgc->imbalance;

			if ((env.flags & LBF_SOME_PINNED) && env.imbalance > 0)
				*group_imbalance = 1;
		}

		/* All tasks on this runqueue were pinned by CPU affinity */
		if (unlikely(env.flags & LBF_ALL_PINNED)) {
			cpumask_clear_cpu(cpu_of(busiest), cpus);
			if (!cpumask_empty(cpus)) {
				env.loop = 0;
				env.loop_break = sched_nr_migrate_break;
				goto redo;
			}
			goto out_all_pinned;
		}

#ifdef CONFIG_MT_LOAD_BALANCE_ENHANCEMENT
		/* when move tasks fil, force migration no matter cache-hot */
		/*  use mt_ignore_cachehot_in_idle */
		if (!ld_moved && ((CPU_NEWLY_IDLE == idle) || (CPU_IDLE == idle))) {
			env.mt_ignore_cachehot_in_idle = 1;
			env.loop = 0;
			raw_spin_lock_irqsave(&busiest->lock, flags);
#ifdef CONFIG_MTK_SCHED_CMP_TGS
			env.loop_max	= min_t(unsigned long, sysctl_sched_nr_migrate, busiest->nr_running);
#endif /* CONFIG_MTK_SCHED_CMP */
			mt_sched_printf(sched_lb, "2 env.loop_max=%d, busiest->nr_running=%d",
					env.loop_max, busiest->nr_running);
#ifdef CONFIG_MTK_SCHED_CMP_TGS
/*			if (!cpus_share_cache(env.src_cpu, env.dst_cpu ) ) */
			if (!cpus_share_cache(env.src_cpu, env.dst_cpu) && (sd->flags & SD_BALANCE_TG))
				cur_ld_moved = tgs_detach_tasks(&env);
			else
				cur_ld_moved = detach_tasks(&env);
#else
			cur_ld_moved = detach_tasks(&env);
#endif
			raw_spin_unlock(&busiest->lock);

			if (cur_ld_moved) {
				attach_tasks(&env);
				ld_moved += cur_ld_moved;
			}

			local_irq_restore(flags);
		}
#endif
	}

	if (!ld_moved) {
		schedstat_inc(sd, lb_failed[idle]);
		/*
		 * Increment the failure counter only on periodic balance.
		 * We do not want newidle balance, which can be very
		 * frequent, pollute the failure counter causing
		 * excessive cache_hot migrations and active balances.
		 */
		if (idle != CPU_NEWLY_IDLE)
			sd->nr_balance_failed++;

		if (need_active_balance(&env)) {
			raw_spin_lock_irqsave(&busiest->lock, flags);

			/* don't kick the active_load_balance_cpu_stop,
			 * if the curr task on busiest cpu can't be
			 * moved to this_cpu
			 */
			if (!cpumask_test_cpu(this_cpu,
					tsk_cpus_allowed(busiest->curr))) {
				raw_spin_unlock_irqrestore(&busiest->lock,
							    flags);
				env.flags |= LBF_ALL_PINNED;
				goto out_one_pinned;
			}

			/*
			 * ->active_balance synchronizes accesses to
			 * ->active_balance_work.  Once set, it's cleared
			 * only after active load balance is finished.
			 */
			if (!busiest->active_balance && !cpu_park(cpu_of(busiest))) {
				busiest->active_balance = 1; /* load_balance */
				busiest->push_cpu = this_cpu;
				active_balance = 1;
			}
			raw_spin_unlock_irqrestore(&busiest->lock, flags);

			if (active_balance) {
				if (stop_one_cpu_dispatch(cpu_of(busiest),
					active_load_balance_cpu_stop, busiest,
					&busiest->active_balance_work)) {
					raw_spin_lock_irqsave(&busiest->lock, flags);
					busiest->active_balance = 0;
					active_balance = 0;
					raw_spin_unlock_irqrestore(&busiest->lock, flags);
				}
			}

			/*
			 * We've kicked active balancing, reset the failure
			 * counter.
			 */
			sd->nr_balance_failed = sd->cache_nice_tries+1;
		}
	} else
		sd->nr_balance_failed = 0;

	if (likely(!active_balance)) {
		/* We were unbalanced, so reset the balancing interval */
		sd->balance_interval = sd->min_interval;
	} else {
		/*
		 * If we've begun active balancing, start to back off. This
		 * case may not be covered by the all_pinned logic if there
		 * is only 1 task on the busy runqueue (because we don't call
		 * detach_tasks).
		 */
		if (sd->balance_interval < sd->max_interval)
			sd->balance_interval *= 2;
	}

	goto out;

out_balanced:
	/*
	 * We reach balance although we may have faced some affinity
	 * constraints. Clear the imbalance flag if it was set.
	 */
	if (sd_parent) {
		int *group_imbalance = &sd_parent->groups->sgc->imbalance;

		if (*group_imbalance)
			*group_imbalance = 0;
	}

out_all_pinned:
	/*
	 * We reach balance because all tasks are pinned at this level so
	 * we can't migrate them. Let the imbalance flag set so parent level
	 * can try to migrate them.
	 */
	schedstat_inc(sd, lb_balanced[idle]);

	sd->nr_balance_failed = 0;

out_one_pinned:
	/* tune up the balancing interval */
	if (((env.flags & LBF_ALL_PINNED) &&
			sd->balance_interval < MAX_PINNED_INTERVAL) ||
			(sd->balance_interval < sd->max_interval))
		sd->balance_interval *= 2;

	ld_moved = 0;
out:
	if (CPU_NEWLY_IDLE == idle)
		mt_sched_printf(sched_lb, "%s idle balance %d: moved=%d", __func__, this_cpu, ld_moved);
	else
		mt_sched_printf(sched_lb, "%s periodic balance %d: moved=%d", __func__, this_cpu, ld_moved);

	return ld_moved;
}

static inline unsigned long
get_sd_balance_interval(struct sched_domain *sd, int cpu_busy)
{
	unsigned long interval = sd->balance_interval;

	if (cpu_busy)
		interval *= sd->busy_factor;

	/* scale ms to jiffies */
	interval = msecs_to_jiffies(interval);
	interval = clamp(interval, 1UL, max_load_balance_interval);

	return interval;
}

static inline void
update_next_balance(struct sched_domain *sd, int cpu_busy, unsigned long *next_balance)
{
	unsigned long interval, next;

	interval = get_sd_balance_interval(sd, cpu_busy);
	next = sd->last_balance + interval;

	if (time_after(*next_balance, next))
		*next_balance = next;
}

/*
 * idle_balance is called by schedule() if this_cpu is about to become
 * idle. Attempts to pull tasks from other CPUs.
 */
static int idle_balance(struct rq *this_rq)
{
	unsigned long next_balance = jiffies + HZ;
	int this_cpu = this_rq->cpu;
	struct sched_domain *sd;
	int pulled_task = 0;
	u64 curr_cost = 0;
#if defined(CONFIG_MT_LOAD_BALANCE_ENHANCEMENT) && defined(CONFIG_LOCAL_TIMERS)
	unsigned long counter = 0;
#endif

	idle_enter_fair(this_rq);

	/*
	 * We must set idle_stamp _before_ calling idle_balance(), such that we
	 * measure the duration of idle_balance() as idle time.
	 */
	this_rq->idle_stamp = rq_clock(this_rq);

	if (this_rq->avg_idle < sysctl_sched_migration_cost ||
	    !this_rq->rd->overload) {
		rcu_read_lock();
		sd = rcu_dereference_check_sched_domain(this_rq->sd);
		if (sd)
			update_next_balance(sd, 0, &next_balance);
		rcu_read_unlock();
		mt_sched_printf(sched_lb, "%d:idle balance bypass: avg_idle=%llu span=%lu overload=%d",
				this_cpu, this_rq->avg_idle, this_rq->rd->span->bits[0], this_rq->rd->overload);
		goto out;
	}

#if defined(CONFIG_MT_LOAD_BALANCE_ENHANCEMENT) && defined(CONFIG_LOCAL_TIMERS)
must_do:
#endif

	/*
	 * Drop the rq->lock, but keep IRQ/preempt disabled.
	 */
	raw_spin_unlock(&this_rq->lock);

	update_blocked_averages(this_cpu);
	rcu_read_lock();
	for_each_domain(this_cpu, sd) {
		int continue_balancing = 1;
		u64 t0, domain_cost;

		if (!(sd->flags & SD_LOAD_BALANCE))
			continue;

		if (this_rq->avg_idle < curr_cost + sd->max_newidle_lb_cost) {
			update_next_balance(sd, 0, &next_balance);
			break;
		}

		if (sd->flags & SD_BALANCE_NEWIDLE) {
			t0 = sched_clock_cpu(this_cpu);

			pulled_task = load_balance(this_cpu, this_rq,
						   sd, CPU_NEWLY_IDLE,
						   &continue_balancing);

			domain_cost = sched_clock_cpu(this_cpu) - t0;
			if (domain_cost > sd->max_newidle_lb_cost)
				sd->max_newidle_lb_cost = domain_cost;

			curr_cost += domain_cost;
		}

		update_next_balance(sd, 0, &next_balance);

		/*
		 * Stop searching for tasks to pull if there are
		 * now runnable tasks on this rq.
		 */
		if (pulled_task || this_rq->nr_running > 0)
			break;
	}
	rcu_read_unlock();

#ifdef CONFIG_SCHED_HMP_PLUS
	if (!pulled_task)
		pulled_task = hmp_idle_pull(this_cpu);
#endif

	raw_spin_lock(&this_rq->lock);

	if (curr_cost > this_rq->max_idle_balance_cost)
		this_rq->max_idle_balance_cost = curr_cost;

	/*
	 * While browsing the domains, we released the rq lock, a task could
	 * have been enqueued in the meantime. Since we're not going idle,
	 * pretend we pulled a task.
	 */
	if (this_rq->cfs.h_nr_running && !pulled_task)
		pulled_task = 1;

out:
	/* Move the next balance forward */
	if (time_after(this_rq->next_balance, next_balance))
		this_rq->next_balance = next_balance;

	/* Is there a task of a high priority class? */
	if (this_rq->nr_running != this_rq->cfs.h_nr_running)
		pulled_task = -1;

	if (pulled_task) {
		idle_exit_fair(this_rq);
		this_rq->idle_stamp = 0;
	}

	return pulled_task;
}

/*
 * active_load_balance_cpu_stop is run by cpu stopper. It pushes
 * running tasks off the busiest CPU onto idle CPUs. It requires at
 * least 1 task to be running on each physical CPU where possible, and
 * avoids physical / logical imbalances.
 */
static int active_load_balance_cpu_stop(void *data)
{
	struct rq *busiest_rq = data;
	int busiest_cpu = cpu_of(busiest_rq);
	int target_cpu = busiest_rq->push_cpu;
	struct rq *target_rq = cpu_rq(target_cpu);
	struct sched_domain *sd;
	struct task_struct *p = NULL;

	raw_spin_lock_irq(&busiest_rq->lock);

	/* make sure the requested cpu hasn't gone down in the meantime */
	if (unlikely(busiest_cpu != smp_processor_id() ||
		     !busiest_rq->active_balance))
		goto out_unlock;

	/* Is there any task to move? */
	if (busiest_rq->nr_running <= 1)
		goto out_unlock;

	/*
	 * This condition is "impossible", if it occurs
	 * we need to fix it. Originally reported by
	 * Bjorn Helgaas on a 128-cpu setup.
	 */
	BUG_ON(busiest_rq == target_rq);

	/* Search for an sd spanning us and the target CPU. */
	rcu_read_lock();
	for_each_domain(target_cpu, sd) {
		if ((sd->flags & SD_LOAD_BALANCE) &&
		    cpumask_test_cpu(busiest_cpu, sched_domain_span(sd)))
				break;
	}

	if (likely(sd)) {
		struct lb_env env = {
			.sd		= sd,
			.dst_cpu	= target_cpu,
			.dst_rq		= target_rq,
			.src_cpu	= busiest_rq->cpu,
			.src_rq		= busiest_rq,
			.idle		= CPU_IDLE,
		};

		schedstat_inc(sd, alb_count);

		p = detach_one_task(&env);
		if (p)
			schedstat_inc(sd, alb_pushed);
		else
			schedstat_inc(sd, alb_failed);
	}
	rcu_read_unlock();
out_unlock:
	busiest_rq->active_balance = 0;
	raw_spin_unlock(&busiest_rq->lock);

	if (p)
		attach_one_task(target_rq, p);

	local_irq_enable();

	return 0;
}

static inline int on_null_domain(struct rq *rq)
{
	return unlikely(!rcu_dereference_sched(rq->sd));
}

#ifdef CONFIG_NO_HZ_COMMON
/*
 * idle load balancing details
 * - When one of the busy CPUs notice that there may be an idle rebalancing
 *   needed, they will kick the idle load balancer, which then does idle
 *   load balancing for all the idle CPUs.
 */
static struct {
	cpumask_var_t idle_cpus_mask;
	atomic_t nr_cpus;
	unsigned long next_balance;     /* in jiffy units */
} nohz ____cacheline_aligned;

static inline int find_new_ilb(void)
{
	int ilb = cpumask_first(nohz.idle_cpus_mask);

	if (ilb < nr_cpu_ids && idle_cpu(ilb))
		return ilb;

	return nr_cpu_ids;
}

/*
 * Kick a CPU to do the nohz balancing, if it is time for it. We pick the
 * nohz_load_balancer CPU (if there is one) otherwise fallback to any idle
 * CPU (if there is one).
 */
static void nohz_balancer_kick(void)
{
	int ilb_cpu;

	nohz.next_balance++;

	ilb_cpu = find_new_ilb();

	if (ilb_cpu >= nr_cpu_ids)
		return;

	if (test_and_set_bit(NOHZ_BALANCE_KICK, nohz_flags(ilb_cpu)))
		return;
	/*
	 * Use smp_send_reschedule() instead of resched_cpu().
	 * This way we generate a sched IPI on the target cpu which
	 * is idle. And the softirq performing nohz idle load balance
	 * will be run before returning from the IPI.
	 */
	smp_send_reschedule(ilb_cpu);
	return;
}

static inline void nohz_balance_exit_idle(int cpu)
{
	if (unlikely(test_bit(NOHZ_TICK_STOPPED, nohz_flags(cpu)))) {
		/*
		 * Completely isolated CPUs don't ever set, so we must test.
		 */
		if (likely(cpumask_test_cpu(cpu, nohz.idle_cpus_mask))) {
			cpumask_clear_cpu(cpu, nohz.idle_cpus_mask);
			atomic_dec(&nohz.nr_cpus);
		}
		clear_bit(NOHZ_TICK_STOPPED, nohz_flags(cpu));
	}
}

static inline void set_cpu_sd_state_busy(void)
{
	struct sched_domain *sd;
	int cpu = smp_processor_id();

	rcu_read_lock();
	sd = rcu_dereference(per_cpu(sd_busy, cpu));

	if (!sd || !sd->nohz_idle)
		goto unlock;
	sd->nohz_idle = 0;

	atomic_inc(&sd->groups->sgc->nr_busy_cpus);
unlock:
	rcu_read_unlock();
}

void set_cpu_sd_state_idle(void)
{
	struct sched_domain *sd;
	int cpu = smp_processor_id();

	rcu_read_lock();
	sd = rcu_dereference(per_cpu(sd_busy, cpu));

	if (!sd || sd->nohz_idle)
		goto unlock;
	sd->nohz_idle = 1;

	atomic_dec(&sd->groups->sgc->nr_busy_cpus);
unlock:
	rcu_read_unlock();
}

/*
 * This routine will record that the cpu is going idle with tick stopped.
 * This info will be used in performing idle load balancing in the future.
 */
void nohz_balance_enter_idle(int cpu)
{
	/*
	 * If this cpu is going down, then nothing needs to be done.
	 */
	if (!cpu_active(cpu))
		return;

	if (test_bit(NOHZ_TICK_STOPPED, nohz_flags(cpu)))
		return;

	/*
	 * If we're a completely isolated CPU, we don't play.
	 */
	if (on_null_domain(cpu_rq(cpu)))
		return;

	cpumask_set_cpu(cpu, nohz.idle_cpus_mask);
	atomic_inc(&nohz.nr_cpus);
	set_bit(NOHZ_TICK_STOPPED, nohz_flags(cpu));
}

static int sched_ilb_notifier(struct notifier_block *nfb,
					unsigned long action, void *hcpu)
{
	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_DYING:
		nohz_balance_exit_idle(smp_processor_id());
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
}
#endif

static DEFINE_SPINLOCK(balancing);

/*
 * Scale the max load_balance interval with the number of CPUs in the system.
 * This trades load-balance latency on larger machines for less cross talk.
 */
void update_max_interval(void)
{
	max_load_balance_interval = HZ*num_online_cpus()/10;
}

/*
 * It checks each scheduling domain to see if it is due to be balanced,
 * and initiates a balancing operation if so.
 *
 * Balancing parameters are set up in init_sched_domains.
 */
static void rebalance_domains(struct rq *rq, enum cpu_idle_type idle)
{
	int continue_balancing = 1;
	int cpu = rq->cpu;
	unsigned long interval;
	struct sched_domain *sd;
	/* Earliest time when we have to do rebalance again */
	unsigned long next_balance = jiffies + 60*HZ;
	int update_next_balance = 0;
	int need_serialize, need_decay = 0;
	u64 max_cost = 0;

	update_blocked_averages(cpu);

	rcu_read_lock();
	for_each_domain(cpu, sd) {
		/*
		 * Decay the newidle max times here because this is a regular
		 * visit to all the domains. Decay ~1% per second.
		 */
		if (time_after(jiffies, sd->next_decay_max_lb_cost)) {
			sd->max_newidle_lb_cost =
				(sd->max_newidle_lb_cost * 253) / 256;
			sd->next_decay_max_lb_cost = jiffies + HZ;
			need_decay = 1;
		}
		max_cost += sd->max_newidle_lb_cost;

		if (!(sd->flags & SD_LOAD_BALANCE))
			continue;

		/*
		 * Stop the load balance at this level. There is another
		 * CPU in our sched group which is doing load balancing more
		 * actively.
		 */
		if (!continue_balancing) {
			if (need_decay)
				continue;
			break;
		}

		interval = get_sd_balance_interval(sd, idle != CPU_IDLE);

		need_serialize = sd->flags & SD_SERIALIZE;
		if (need_serialize) {
			if (!spin_trylock(&balancing))
				goto out;
		}

		if (time_after_eq(jiffies, sd->last_balance + interval)) {
			if (load_balance(cpu, rq, sd, idle, &continue_balancing)) {
				/*
				 * The LBF_DST_PINNED logic could have changed
				 * env->dst_cpu, so we can't know our idle
				 * state even if we migrated tasks. Update it.
				 */
				idle = idle_cpu(cpu) ? CPU_IDLE : CPU_NOT_IDLE;
			}
			sd->last_balance = jiffies;
			interval = get_sd_balance_interval(sd, idle != CPU_IDLE);
		}
		if (need_serialize)
			spin_unlock(&balancing);
out:
		if (time_after(next_balance, sd->last_balance + interval)) {
			next_balance = sd->last_balance + interval;
			update_next_balance = 1;
		}
	}
	if (need_decay) {
		/*
		 * Ensure the rq-wide value also decays but keep it at a
		 * reasonable floor to avoid funnies with rq->avg_idle.
		 */
		rq->max_idle_balance_cost =
			max((u64)sysctl_sched_migration_cost, max_cost);
	}
	rcu_read_unlock();

	/*
	 * next_balance will be updated only when there is a need.
	 * When the cpu is attached to null domain for ex, it will not be
	 * updated.
	 */
	if (likely(update_next_balance))
		rq->next_balance = next_balance;
}

#ifdef CONFIG_NO_HZ_COMMON
/*
 * In CONFIG_NO_HZ_COMMON case, the idle balance kickee will do the
 * rebalancing for all the cpus for whom scheduler ticks are stopped.
 */
static void nohz_idle_balance(struct rq *this_rq, enum cpu_idle_type idle)
{
	int this_cpu = this_rq->cpu;
	struct rq *rq;
	int balance_cpu;

	if (idle != CPU_IDLE ||
	    !test_bit(NOHZ_BALANCE_KICK, nohz_flags(this_cpu)))
		goto end;

	for_each_cpu(balance_cpu, nohz.idle_cpus_mask) {
		if (balance_cpu == this_cpu || !idle_cpu(balance_cpu))
			continue;

		/*
		 * If this cpu gets work to do, stop the load balancing
		 * work being done for other cpus. Next load
		 * balancing owner will pick it up.
		 */
		if (need_resched())
			break;

		rq = cpu_rq(balance_cpu);

		/*
		 * If time for next balance is due,
		 * do the balance.
		 */
		if (time_after_eq(jiffies, rq->next_balance)) {
			raw_spin_lock_irq(&rq->lock);
			update_rq_clock(rq);
			update_idle_cpu_load(rq);
			raw_spin_unlock_irq(&rq->lock);
			rebalance_domains(rq, CPU_IDLE);
		}

		if (time_after(this_rq->next_balance, rq->next_balance))
			this_rq->next_balance = rq->next_balance;
	}
	nohz.next_balance = this_rq->next_balance;
end:
	clear_bit(NOHZ_BALANCE_KICK, nohz_flags(this_cpu));
}

/*
 * Current heuristic for kicking the idle load balancer in the presence
 * of an idle cpu in the system.
 *   - This rq has more than one task.
 *   - This rq has at least one CFS task and the capacity of the CPU is
 *     significantly reduced because of RT tasks or IRQs.
 *   - At parent of LLC scheduler domain level, this cpu's scheduler group has
 *     multiple busy cpu.
 *   - For SD_ASYM_PACKING, if the lower numbered cpu's in the scheduler
 *     domain span are idle.
 */
static inline bool nohz_kick_needed(struct rq *rq)
{
	unsigned long now = jiffies;
	struct sched_domain *sd;
	struct sched_group_capacity *sgc;
	int nr_busy, cpu = rq->cpu;
	bool kick = false;

	if (unlikely(rq->idle_balance))
		return false;

       /*
	* We may be recently in ticked or tickless idle mode. At the first
	* busy tick after returning from idle, we will update the busy stats.
	*/
	set_cpu_sd_state_busy();
	nohz_balance_exit_idle(cpu);

	/*
	 * None are in tickless mode and hence no need for NOHZ idle load
	 * balancing.
	 */
	if (likely(!atomic_read(&nohz.nr_cpus)))
		return false;

	if (time_before(now, nohz.next_balance))
		return false;

#ifdef CONFIG_SCHED_HMP
	/*
	 * Bail out if there are no nohz CPUs in our
	 * HMP domain, since we will move tasks between
	 * domains through wakeup and force balancing
	 * as necessary based upon task load.
	 */
	if (sched_feat(SCHED_HMP) && cpumask_first_and(nohz.idle_cpus_mask,
			&((struct hmp_domain *)hmp_cpu_domain(cpu))->cpus) >= nr_cpu_ids)
		return false;
#endif
	if (rq->nr_running >= 2)
		return true;

	rcu_read_lock();
	sd = rcu_dereference(per_cpu(sd_busy, cpu));
	if (sd) {
		sgc = sd->groups->sgc;
		nr_busy = atomic_read(&sgc->nr_busy_cpus);

		if (nr_busy > 1) {
			kick = true;
			goto unlock;
		}

	}

	sd = rcu_dereference(rq->sd);
	if (sd) {
		if ((rq->cfs.h_nr_running >= 1) &&
				check_cpu_capacity(rq, sd)) {
			kick = true;
			goto unlock;
		}
	}

	sd = rcu_dereference(per_cpu(sd_asym, cpu));
	if (sd && (cpumask_first_and(nohz.idle_cpus_mask,
				  sched_domain_span(sd)) < cpu)) {
		kick = true;
		goto unlock;
	}

unlock:
	rcu_read_unlock();
	return kick;
}
#else
static void nohz_idle_balance(struct rq *this_rq, enum cpu_idle_type idle) { }
#endif

#ifdef CONFIG_SCHED_HMP
/* Function Declaration */
static int hmp_up_stable(int cpu);
static int hmp_down_stable(int cpu);
static unsigned int hmp_up_migration(int cpu, int *target_cpu, struct sched_entity *se,
				struct clb_env *clbenv);
static unsigned int hmp_down_migration(int cpu, int *target_cpu, struct sched_entity *se,
				struct clb_env *clbenv);
#ifdef CONFIG_SCHED_HMP_PLUS
static struct sched_entity *hmp_get_heaviest_task(
			struct sched_entity *se, int target_cpu);
static struct sched_entity *hmp_get_lightest_task(
			struct sched_entity *se, int migrate_down);
#endif

#define hmp_caller_is_gb(caller) ((HMP_GB == caller)?1:0)

#define hmp_cpu_stable(cpu, up) (up ?	\
			hmp_up_stable(cpu) : hmp_down_stable(cpu))

#define hmp_inc(v) ((v) + 1)

#define task_created(f) ((SD_BALANCE_EXEC == f || SD_BALANCE_FORK == f)?1:0)


/*
 * Heterogenous Multi-Processor (HMP) - Utility Function
 */

/*
 * These functions add next up/down migration delay that prevents the task from
 * doing another migration in the same direction until the delay has expired.
 */
static int hmp_up_stable(int cpu)
{
	struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;
	u64 now = cfs_rq_clock_task(cfs_rq);

	if (((now - hmp_last_up_migration(cpu)) >> 10) < hmp_next_up_threshold)
		return 0;
	return 1;
}

static int hmp_down_stable(int cpu)
{
	struct cfs_rq *cfs_rq = &cpu_rq(cpu)->cfs;
	u64 now = cfs_rq_clock_task(cfs_rq);

	if (((now - hmp_last_down_migration(cpu)) >> 10) < hmp_next_down_threshold)
		return 0;
	return 1;
}

/* Select the most appropriate CPU from hmp cluster */
static unsigned int hmp_select_cpu(unsigned int caller, struct task_struct *p,
			struct cpumask *mask, int prev, int up)
{
	int curr = 0;
	int target = num_possible_cpus();
	unsigned long curr_wload = 0;
	unsigned long target_wload = 0;
	struct cpumask srcp;

	cpumask_and(&srcp, cpu_online_mask, mask);
	target = cpumask_any_and(&srcp, tsk_cpus_allowed(p));
	if (target >= num_possible_cpus())
		goto out;

	/*
	 * RT class is taken into account because CPU load is multiplied
	 * by the total number of CPU runnable tasks that includes RT tasks.
	 */
	target_wload = hmp_inc(cfs_load(target));
	target_wload += cfs_pending_load(target);
	target_wload *= rq_length(target);
	for_each_cpu(curr, mask) {
		/* Check CPU status and task affinity */
		if (!cpu_online(curr) || !cpumask_test_cpu(curr, tsk_cpus_allowed(p)))
			continue;

		/* For global load balancing, unstable CPU will be bypassed */
		if (hmp_caller_is_gb(caller) && !hmp_cpu_stable(curr, up))
			continue;

		curr_wload = hmp_inc(cfs_load(curr));
		curr_wload += cfs_pending_load(curr);
		curr_wload *= rq_length(curr);
		if (curr_wload < target_wload) {
			target_wload = curr_wload;
			target = curr;
		} else if (curr_wload == target_wload && curr == prev) {
			target = curr;
		}
	}

out:
	return target;
}

/*
 * Heterogenous Multi-Processor (HMP) - Task Runqueue Selection
 */

/* This function enhances the original task selection function */
static int hmp_select_task_rq_fair(int sd_flag, struct task_struct *p,
			int prev_cpu, int new_cpu)
{
	int step = 0;
	struct sched_entity *se = &p->se;
	int B_target = num_possible_cpus();
	int L_target = num_possible_cpus();
	struct clb_env clbenv;
	struct list_head *pos;
	struct cpumask fast_cpu_mask, slow_cpu_mask;

#ifdef CONFIG_HMP_TRACER
	int cpu = 0;

	for_each_online_cpu(cpu)
		trace_sched_cfs_runnable_load(cpu, cfs_load(cpu), cfs_length(cpu));
#endif

	/* error handling */
	if (prev_cpu >= num_possible_cpus())
		return new_cpu;

	/*
	 * Skip all the checks if only one CPU is online.
	 * Otherwise, select the most appropriate CPU from cluster.
	 */
	if (num_online_cpus() == 1)
		goto out;

	cpumask_clear(&fast_cpu_mask);
	cpumask_clear(&slow_cpu_mask);
	/* order: fast to slow hmp domain */
	list_for_each(pos, &hmp_domains) {
		struct hmp_domain *domain = list_entry(pos, struct hmp_domain, hmp_domains);

		if (!cpumask_empty(&domain->cpus)) {
			if (cpumask_empty(&fast_cpu_mask)) {
				cpumask_copy(&fast_cpu_mask, &domain->possible_cpus);
			} else {
				cpumask_copy(&slow_cpu_mask, &domain->possible_cpus);
				break;
			}
		}
	}

	if (cpumask_empty(&fast_cpu_mask) || cpumask_empty(&slow_cpu_mask))
		return new_cpu;

	B_target = hmp_select_cpu(HMP_SELECT_RQ, p, &fast_cpu_mask, new_cpu, 0);
	L_target = hmp_select_cpu(HMP_SELECT_RQ, p, &slow_cpu_mask, new_cpu, 1);

	/*
	 * Only one cluster exists or only one cluster is allowed for this task
	 * Case 1: return the runqueue whose load is minimum
	 * Case 2: return original CFS runqueue selection result
	 */
	if (B_target >= num_possible_cpus() && L_target >= num_possible_cpus())
		goto out;
	if (B_target >= num_possible_cpus())
		goto select_slow;
	if (L_target >= num_possible_cpus())
		goto select_fast;

	/*
	 * Two clusters exist and both clusters are allowed for this task
	 * Step 1: Move newly created task to the cpu where no tasks are running
	 * Step 2: Migrate heavy-load task to big
	 * Step 3: Migrate light-load task to LITTLE
	 * Step 4: Make sure the task stays in its previous hmp domain
	 */
	step = 1;
	if (task_created(sd_flag) && !task_low_priority(p->prio)) {
		if (!rq_length(B_target))
			goto select_fast;
		if (!rq_length(L_target))
			goto select_slow;
	}
	memset(&clbenv, 0, sizeof(clbenv));
	clbenv.flags |= HMP_SELECT_RQ;
	cpumask_copy(&clbenv.lcpus, &slow_cpu_mask);
	cpumask_copy(&clbenv.bcpus, &fast_cpu_mask);
	clbenv.ltarget = L_target;
	clbenv.btarget = B_target;
	sched_update_clbstats(&clbenv);
	step = 2;
	if (hmp_up_migration(L_target, &B_target, se, &clbenv))
		goto select_fast;
	step = 3;
	if (hmp_down_migration(B_target, &L_target, se, &clbenv))
		goto select_slow;
	step = 4;
	goto out; /* keep new_cpu as choice */

select_fast:
	new_cpu = B_target;
	goto out;
select_slow:
	new_cpu = L_target;
	goto out;

out:

	/* it happens when num_online_cpus=1 */
	if (new_cpu >= nr_cpu_ids) {
		/* BUG_ON(1); */
		new_cpu = prev_cpu;
	}

	cfs_nr_pending(new_cpu)++;
	cfs_pending_load(new_cpu) += se_load(se);
#ifdef CONFIG_HMP_TRACER
	trace_sched_hmp_load(clbenv.bstats.load_avg, clbenv.lstats.load_avg);
	trace_sched_hmp_select_task_rq(p, step, sd_flag, prev_cpu, new_cpu,
				       se_load(se), &clbenv.bstats, &clbenv.lstats);
#endif
	return new_cpu;
}

#define hmp_fast_cpu_has_spare_cycles(B, cpu_load) (cpu_load < \
			(B->cpu_capacity - (B->cpu_capacity >> 2)))

#define hmp_task_fast_cpu_afford(B, se, cpu) (B->acap > 0 \
			&& hmp_fast_cpu_has_spare_cycles(B, se_load(se) + cfs_load(cpu)))

#define hmp_fast_cpu_oversubscribed(caller, B, se, cpu) \
			(hmp_caller_is_gb(caller) ? \
			!hmp_fast_cpu_has_spare_cycles(B, cfs_load(cpu)) : \
			!hmp_task_fast_cpu_afford(B, se, cpu))

#define hmp_task_slow_cpu_afford(L, se) \
			(L->acap > 0 && L->acap >= se_load(se))

/* Macro used by low-priority task filter */
#define hmp_low_prio_task_up_rejected(p, B, L) \
			(task_low_priority(p->prio) && \
			(B->ntask >= B->ncpu || 0 != L->nr_normal_prio_task) && \
			 (p->se.avg.loadwop_avg_contrib < 800))

#define hmp_low_prio_task_down_allowed(p, B, L) \
			(task_low_priority(p->prio) && !B->nr_dequeuing_low_prio && \
			B->ntask >= B->ncpu && 0 != L->nr_normal_prio_task && \
			(p->se.avg.loadwop_avg_contrib < 800))

/* Migration check result */
#define HMP_BIG_NOT_OVERSUBSCRIBED           (0x01)
#define HMP_BIG_CAPACITY_INSUFFICIENT        (0x02)
#define HMP_LITTLE_CAPACITY_INSUFFICIENT     (0x04)
#define HMP_LOW_PRIORITY_FILTER              (0x08)
#define HMP_BIG_BUSY_LITTLE_IDLE             (0x10)
#define HMP_BIG_IDLE                         (0x20)
#define HMP_MIGRATION_APPROVED              (0x100)
#define HMP_TASK_UP_MIGRATION               (0x200)
#define HMP_TASK_DOWN_MIGRATION             (0x400)
/* Migration statistics */
#ifdef CONFIG_HMP_TRACER
struct hmp_statisic hmp_stats;
#endif
/*
 * Check whether this task should be migrated to big
 * Briefly summarize the flow as below;
 * 1) Migration stabilizing
 * 2) Filter low-priority task
 * 2.5) Keep all cpu busy
 * 3) Check CPU capacity
 * 4) Check dynamic migration threshold
 */
static unsigned int hmp_up_migration(int cpu, int *target_cpu, struct sched_entity *se,
									struct clb_env *clbenv)
{
	struct task_struct *p = task_of(se);
	struct clb_stats *L, *B;
	struct mcheck *check;
	int curr_cpu = cpu;
	unsigned int caller = clbenv->flags;

	L = &clbenv->lstats;
	B = &clbenv->bstats;
	check = &clbenv->mcheck;

	check->status = clbenv->flags;
	check->status |= HMP_TASK_UP_MIGRATION;
	check->result = 0;

	/*
	 * No migration is needed if
	 * 1) There is only one cluster
	 * 2) Task is already in big cluster
	 * 3) It violates task affinity
	 */
	if (!L->ncpu || !B->ncpu
		|| cpumask_test_cpu(curr_cpu, &clbenv->bcpus)
		|| !cpumask_intersects(&clbenv->bcpus, tsk_cpus_allowed(p)))
		goto out;

	/*
	 * [1] Migration stabilizing
	 * Let the task load settle before doing another up migration.
	 * It can prevent a bunch of tasks from migrating to a unstable CPU.
	 */
	if (!hmp_up_stable(*target_cpu))
		goto out;

	/* [2] Filter low-priority task */
#ifdef CONFIG_SCHED_HMP_PRIO_FILTER
	if (hmp_low_prio_task_up_rejected(p, B, L)) {
		check->status |= HMP_LOW_PRIORITY_FILTER;
		goto trace;
	}
#endif

	/* [2.5]if big is idle, just go to big */
	if (rq_length(*target_cpu) == 0) {
		check->status |= HMP_BIG_IDLE;
		check->status |= HMP_MIGRATION_APPROVED;
		check->result = 1;
		goto trace;
	}

	/*
	 * [3] Check CPU capacity
	 * Forbid up-migration if big CPU can't handle this task
	 */
	if (!hmp_task_fast_cpu_afford(B, se, *target_cpu)) {
		check->status |= HMP_BIG_CAPACITY_INSUFFICIENT;
		goto trace;
	}

	/*
	 * [4] Check dynamic migration threshold
	 * Migrate task from LITTLE to big if load is greater than up-threshold
	 */
	mt_sched_printf(sched_log, "%s: %d %s load=%lu threshold=%d",
		__func__, p->pid, p->comm, se_load(se),  B->threshold);
	if (se_load(se) > B->threshold) {
		check->status |= HMP_MIGRATION_APPROVED;
		check->result = 1;
	}

trace:
#ifdef CONFIG_HMP_TRACER
	if (check->result && hmp_caller_is_gb(caller))
		hmp_stats.nr_force_up++;
	trace_sched_hmp_stats(&hmp_stats);
	trace_sched_dynamic_threshold(task_of(se), B->threshold, check->status,
				      curr_cpu, *target_cpu, se_load(se), B, L);
#endif
out:
	return check->result;
}

/*
 * Check whether this task should be migrated to LITTLE
 * Briefly summarize the flow as below;
 * 1) Migration stabilizing
 * 1.5) Keep all cpu busy
 * 2) Filter low-priority task
 * 3) Check CPU capacity
 * 4) Check dynamic migration threshold
 */
static unsigned int hmp_down_migration(int cpu, int *target_cpu, struct sched_entity *se,
										struct clb_env *clbenv)
{
	struct task_struct *p = task_of(se);
	struct clb_stats *L, *B;
	struct mcheck *check;
	int curr_cpu = cpu;
	unsigned int caller = clbenv->flags;

	L = &clbenv->lstats;
	B = &clbenv->bstats;
	check = &clbenv->mcheck;

	check->status = caller;
	check->status |= HMP_TASK_DOWN_MIGRATION;
	check->result = 0;

	/*
	 * No migration is needed if
	 * 1) There is only one cluster
	 * 2) Task is already in LITTLE cluster
	 * 3) It violates task affinity
	 */
	if (!L->ncpu || !B->ncpu
		|| cpumask_test_cpu(curr_cpu, &clbenv->lcpus)
	    || !cpumask_intersects(&clbenv->lcpus, tsk_cpus_allowed(p)))
		goto out;

	/*
	 * [1] Migration stabilizing
	 * Let the task load settle before doing another down migration.
	 * It can prevent a bunch of tasks from migrating to a unstable CPU.
	 */
	if (!hmp_down_stable(*target_cpu))
		goto out;

	/* [1.5]if big is busy and little is idle, just go to little */
	if (rq_length(*target_cpu) == 0 && caller == HMP_SELECT_RQ && rq_length(curr_cpu) > 0) {
		struct rq *curr_rq = cpu_rq(curr_cpu);

		/* if current big core is not heavy task and wake up task is heavy task no go to little */
		if (!(!is_heavy_task(curr_rq->curr) && is_heavy_task(p))) {
			check->status |= HMP_BIG_BUSY_LITTLE_IDLE;
			check->status |= HMP_MIGRATION_APPROVED;
			check->result = 1;
			goto trace;
		}
	}

	/* [2] Filter low-priority task */
#ifdef CONFIG_SCHED_HMP_PRIO_FILTER
	if (hmp_low_prio_task_down_allowed(p, B, L)) {
		cfs_nr_dequeuing_low_prio(curr_cpu)++;
		check->status |= HMP_LOW_PRIORITY_FILTER;
		check->status |= HMP_MIGRATION_APPROVED;
		check->result = 1;
		goto trace;
	}
#endif

	/*
	 * [3] Check CPU capacity
	 * Forbid down-migration if either of the following conditions is true
	 * 1) big cpu is not oversubscribed (if big CPU seems to have spare
	 *    cycles, do not force this task to run on LITTLE CPU, but
	 *    keep it staying in its previous cluster instead)
	 * 2) LITTLE cpu doesn't have available capacity for this new task
	 */
	if (!hmp_fast_cpu_oversubscribed(caller, B, se, curr_cpu)) {
		check->status |= HMP_BIG_NOT_OVERSUBSCRIBED;
		goto trace;
	}

	if (!hmp_task_slow_cpu_afford(L, se)) {
		check->status |= HMP_LITTLE_CAPACITY_INSUFFICIENT;
		goto trace;
	}

	/*
	 * [4] Check dynamic migration threshold
	 * Migrate task from big to LITTLE if load ratio is less than
	 * or equal to down-threshold
	 */
	if (L->threshold >= se_load(se)) {
		check->status |= HMP_MIGRATION_APPROVED;
		check->result = 1;
	}

trace:
#ifdef CONFIG_HMP_TRACER
	if (check->result && hmp_caller_is_gb(caller))
		hmp_stats.nr_force_down++;
	trace_sched_hmp_stats(&hmp_stats);
	trace_sched_dynamic_threshold(task_of(se), L->threshold, check->status,
				      curr_cpu, *target_cpu, se_load(se), B, L);
#endif
out:
	return check->result;
}

static int hmp_can_migrate_task(struct task_struct *p, struct lb_env *env)
{
	int tsk_cache_hot = 0;

	/*
	 * We do not migrate tasks that are:
	 * 1) running (obviously), or
	 * 2) cannot be migrated to this CPU due to cpus_allowed
	 */
	if (!cpumask_test_cpu(env->dst_cpu, tsk_cpus_allowed(p))) {
		schedstat_inc(p, se.statistics.nr_failed_migrations_affine);
		return 0;
	}
	env->flags &= ~LBF_ALL_PINNED;

	if (task_running(env->src_rq, p)) {
		schedstat_inc(p, se.statistics.nr_failed_migrations_running);
		return 0;
	}

	/*
	 * Aggressive migration if:
	 * 1) task is cache cold, or
	 * 2) too many balance attempts have failed.
	 */

	tsk_cache_hot = task_hot(p, env);

	if (!tsk_cache_hot ||
		env->sd->nr_balance_failed > env->sd->cache_nice_tries) {
#ifdef CONFIG_SCHEDSTATS
		if (tsk_cache_hot) {
			schedstat_inc(env->sd, lb_hot_gained[env->idle]);
			schedstat_inc(p, se.statistics.nr_forced_migrations);
		}
#endif
		return 1;
	}

	return 1;
}

/*
 * move_specific_task tries to move a specific task.
 * Returns 1 if successful and 0 otherwise.
 * Called with both runqueues locked.
 */
static int move_specific_task(struct lb_env *env, struct task_struct *pm)
{
	struct task_struct *p, *n;

	list_for_each_entry_safe(p, n, &env->src_rq->cfs_tasks, se.group_node) {
		if (throttled_lb_pair(task_group(p), env->src_rq->cpu,
				env->dst_cpu))
			continue;

		if (!hmp_can_migrate_task(p, env))
			continue;
		/* Check if we found the right task */
		if (p != pm)
			continue;

		move_task(p, env);
		/*
		 * Right now, this is only the third place move_task()
		 * is called, so we can safely collect move_task()
		 * stats here rather than inside move_task().
		 */
		schedstat_inc(env->sd, lb_gained[env->idle]);
		return 1;
	}
	return 0;
}

/*
 * hmp_active_task_migration_cpu_stop is run by cpu stopper and used to
 * migrate a specific task from one runqueue to another.
 * hmp_force_up_migration uses this to push a currently running task
 * off a runqueue.
 * Based on active_load_balance_stop_cpu and can potentially be merged.
 */
static int hmp_active_task_migration_cpu_stop(void *data)
{
	struct rq *busiest_rq = data;
	struct task_struct *p = NULL;
	int busiest_cpu = cpu_of(busiest_rq);
	int target_cpu = busiest_rq->push_cpu;
	struct rq *target_rq = cpu_rq(target_cpu);
	struct sched_domain *sd;

	raw_spin_lock_irq(&busiest_rq->lock);
	p = busiest_rq->migrate_task;
	/* make sure the requested cpu hasn't gone down in the meantime */
	if (unlikely(busiest_cpu != smp_processor_id() ||
		!busiest_rq->active_balance)) {
		goto out_unlock;
	}
	/* Is there any task to move? */
	if (busiest_rq->nr_running <= 1)
		goto out_unlock;
	/* Are both target and busiest cpu online */
	if (!cpu_online(busiest_cpu) || !cpu_online(target_cpu))
		goto out_unlock;
	/* Task has migrated meanwhile, abort forced migration */
	if ((!p) || (task_rq(p) != busiest_rq))
		goto out_unlock;
	/*
	 * This condition is "impossible", if it occurs
	 * we need to fix it. Originally reported by
	 * Bjorn Helgaas on a 128-cpu setup.
	 */
	BUG_ON(busiest_rq == target_rq);

	/* move a task from busiest_rq to target_rq */
	double_lock_balance(busiest_rq, target_rq);

	/* Search for an sd spanning us and the target CPU. */
	rcu_read_lock();
	for_each_domain(target_cpu, sd) {
		if (cpumask_test_cpu(busiest_cpu, sched_domain_span(sd)))
			break;
	}

	if (likely(sd)) {
		struct lb_env env = {
			.sd		= sd,
			.dst_cpu	= target_cpu,
			.dst_rq		= target_rq,
			.src_cpu	= busiest_rq->cpu,
			.src_rq		= busiest_rq,
			.idle		= CPU_IDLE,
		};

		schedstat_inc(sd, alb_count);

		if (move_specific_task(&env, p))
			schedstat_inc(sd, alb_pushed);
		else
			schedstat_inc(sd, alb_failed);
	}
	rcu_read_unlock();
	double_unlock_balance(busiest_rq, target_rq);
out_unlock:
	busiest_rq->active_balance = 0;
	raw_spin_unlock_irq(&busiest_rq->lock);

	put_task_struct(p);
	return 0;
}

/* For debugging purpose, to depart functions of cpu_stop to make call_stack clear. */
static int hmp_idle_pull_cpu_stop(void *data)
{
	return hmp_active_task_migration_cpu_stop(data);
}

static int hmp_force_up_cpu_stop(void *data)
{
	return hmp_active_task_migration_cpu_stop(data);
}

static int hmp_force_down_cpu_stop(void *data)
{
	return hmp_active_task_migration_cpu_stop(data);
}

/*
 * Heterogenous Multi-Processor (HMP) Global Load Balance
 */
static DEFINE_SPINLOCK(hmp_force_migration);

/*
 * According to Linaro's comment, we should only check the currently running
 * tasks because selecting other tasks for migration will require extensive
 * book keeping.
 */
static void hmp_force_down_migration(int this_cpu)
{
	int target_cpu;
	struct sched_entity *se;
	struct rq *target;
	unsigned long flags;
	unsigned int force = 0;
	struct task_struct *p;
	struct clb_env clbenv;
#ifdef CONFIG_SCHED_HMP_PLUS
	struct sched_entity *orig;
	int B_cpu;
#endif
	struct hmp_domain *hmp_domain = NULL;
	struct cpumask fast_cpu_mask, slow_cpu_mask;

	cpumask_clear(&fast_cpu_mask);
	cpumask_clear(&slow_cpu_mask);

	/* Migrate light task from big to LITTLE */
	if (!hmp_cpu_is_slowest(this_cpu)) {
		hmp_domain = hmp_cpu_domain(this_cpu);
		cpumask_copy(&fast_cpu_mask, &hmp_domain->possible_cpus);
		while (!list_is_last(&hmp_domain->hmp_domains, &hmp_domains)) {
			struct list_head *pos = &hmp_domain->hmp_domains;

			hmp_domain = list_entry(pos->next, struct hmp_domain, hmp_domains);

			if (!cpumask_empty(&hmp_domain->cpus)) {
				cpumask_copy(&slow_cpu_mask, &hmp_domain->possible_cpus);
				break;
			}
		}
	}
	if (!hmp_domain || hmp_domain == hmp_cpu_domain(this_cpu))
		return;

	if (cpumask_empty(&fast_cpu_mask) || cpumask_empty(&slow_cpu_mask))
		return;

	force = 0;
	target = cpu_rq(this_cpu);
	raw_spin_lock_irqsave(&target->lock, flags);
	se = target->cfs.curr;
	if (!se) {
		raw_spin_unlock_irqrestore(&target->lock, flags);
		return;
	}

	/* Find task entity */
	if (!entity_is_task(se)) {
		struct cfs_rq *cfs_rq;

		cfs_rq = group_cfs_rq(se);
		while (cfs_rq) {
			se = cfs_rq->curr;
			cfs_rq = group_cfs_rq(se);
		}
	}
#ifdef CONFIG_SCHED_HMP_PLUS
	orig = se;
	se = hmp_get_lightest_task(orig, 1);
	if (!entity_is_task(se))
		p = task_of(orig);
	else
#endif
		p = task_of(se);
#ifdef CONFIG_SCHED_HMP_PLUS
	/* Don't offload to little if there is one idle big, let load balance to do it's work */
	/* Also, to prevent idle_balance from leading to potential ping-pong */
	B_cpu = hmp_select_cpu(HMP_GB, p, &fast_cpu_mask, this_cpu, 0);
	if (B_cpu < nr_cpu_ids && !rq_length(B_cpu)) {
		raw_spin_unlock_irqrestore(&target->lock, flags);
		return;
	}
#endif
	target_cpu = hmp_select_cpu(HMP_GB, p, &slow_cpu_mask, -1, 1);
	if (target_cpu >= num_possible_cpus()) {
		raw_spin_unlock_irqrestore(&target->lock, flags);
		return;
	}

	/* Collect cluster information */
	memset(&clbenv, 0, sizeof(clbenv));
	clbenv.flags |= HMP_GB;
	clbenv.btarget = this_cpu;
	clbenv.ltarget = target_cpu;
	cpumask_copy(&clbenv.lcpus, &slow_cpu_mask);
	cpumask_copy(&clbenv.bcpus, &fast_cpu_mask);
	sched_update_clbstats(&clbenv);

#ifdef CONFIG_SCHED_HMP_PLUS
	if (cpu_rq(this_cpu)->cfs.h_nr_running < 2) {
		raw_spin_unlock_irqrestore(&target->lock, flags);
		return;
	}
#endif

	/* Check migration threshold */
	if (!target->active_balance &&
		hmp_down_migration(this_cpu, &target_cpu, se, &clbenv) &&
		!cpu_park(cpu_of(target))) {
		if (p->state != TASK_DEAD) {
			get_task_struct(p);
			target->active_balance = 1; /* force down */
			target->push_cpu = target_cpu;
			target->migrate_task = p;
			force = 1;
			trace_sched_hmp_migrate(p, target->push_cpu, 1);
			hmp_next_down_delay(&p->se, target->push_cpu);
		}
	}
	raw_spin_unlock_irqrestore(&target->lock, flags);
	if (force) {
		if (stop_one_cpu_dispatch(cpu_of(target),
			hmp_force_down_cpu_stop,
			target, &target->active_balance_work)) {
			put_task_struct(p); /* out of rq->lock */
			raw_spin_lock_irqsave(&target->lock, flags);
			target->active_balance = 0;
			force = 0;
			raw_spin_unlock_irqrestore(&target->lock, flags);
		}
	}
}

/*
 * hmp_force_up_migration checks runqueues for tasks that need to
 * be actively migrated to a faster cpu.
 */
static void hmp_force_up_migration(int this_cpu)
{
	int curr_cpu, target_cpu;
	struct sched_entity *se;
	struct rq *target;
	unsigned long flags;
	unsigned int force = 0;
	struct task_struct *p;
	struct clb_env clbenv;
#ifdef CONFIG_SCHED_HMP_PLUS
	struct sched_entity *orig;
#endif

	if (!spin_trylock(&hmp_force_migration))
		return;

#ifdef CONFIG_HMP_TRACER
	for_each_online_cpu(curr_cpu)
		trace_sched_cfs_runnable_load(curr_cpu, cfs_load(curr_cpu), cfs_length(curr_cpu));
#endif

	/* Migrate heavy task from LITTLE to big */
	for_each_online_cpu(curr_cpu) {
		struct hmp_domain *hmp_domain = NULL;
		struct cpumask fast_cpu_mask, slow_cpu_mask;

		cpumask_clear(&fast_cpu_mask);
		cpumask_clear(&slow_cpu_mask);
		if (!hmp_cpu_is_fastest(curr_cpu)) {
			/* current cpu is slow_cpu_mask*/
			hmp_domain = hmp_cpu_domain(curr_cpu);
			cpumask_copy(&slow_cpu_mask, &hmp_domain->possible_cpus);

			while (&hmp_domain->hmp_domains != hmp_domains.next) {
				struct list_head *pos = &hmp_domain->hmp_domains;

				hmp_domain = list_entry(pos->prev, struct hmp_domain, hmp_domains);
				if (!cpumask_empty(&hmp_domain->cpus)) {
					cpumask_copy(&fast_cpu_mask, &hmp_domain->possible_cpus);
					break;
				}
			}
		} else {
			hmp_force_down_migration(this_cpu);
			continue;
		}

		if (!hmp_domain || hmp_domain == hmp_cpu_domain(curr_cpu))
			continue;

		if (cpumask_empty(&fast_cpu_mask) || cpumask_empty(&slow_cpu_mask))
			continue;

		force = 0;
		target = cpu_rq(curr_cpu);
		raw_spin_lock_irqsave(&target->lock, flags);
		se = target->cfs.curr;
		if (!se) {
			raw_spin_unlock_irqrestore(&target->lock, flags);
			continue;
		}

		/* Find task entity */
		if (!entity_is_task(se)) {
			struct cfs_rq *cfs_rq;

			cfs_rq = group_cfs_rq(se);
			while (cfs_rq) {
				se = cfs_rq->curr;
				cfs_rq = group_cfs_rq(se);
			}
		}
#ifdef CONFIG_SCHED_HMP_PLUS
		orig = se;
		se = hmp_get_heaviest_task(se, -1);
		if (!se) {
			raw_spin_unlock_irqrestore(&target->lock, flags);
			continue;
		}
		if (!entity_is_task(se))
			p = task_of(orig);
		else
#endif
			p = task_of(se);

		target_cpu = hmp_select_cpu(HMP_GB, p, &fast_cpu_mask, -1, 0);
		if (target_cpu >= num_possible_cpus()) {
			raw_spin_unlock_irqrestore(&target->lock, flags);
			continue;
		}

		/* Collect cluster information */
		memset(&clbenv, 0, sizeof(clbenv));
		clbenv.flags |= HMP_GB;
		clbenv.ltarget = curr_cpu;
		clbenv.btarget = target_cpu;
		cpumask_copy(&clbenv.lcpus, &slow_cpu_mask);
		cpumask_copy(&clbenv.bcpus, &fast_cpu_mask);
		sched_update_clbstats(&clbenv);

#ifdef CONFIG_HMP_PACK_SMALL_TASK
		if (is_light_task(p) && !is_buddy_busy(per_cpu(sd_pack_buddy, curr_cpu)))
			goto out_force_up;
#endif
		/* Check migration threshold */
		if (!target->active_balance &&
			hmp_up_migration(curr_cpu, &target_cpu, se, &clbenv) &&
			!cpu_park(cpu_of(target))) {
			if (p->state != TASK_DEAD) {
				get_task_struct(p);
				target->active_balance = 1; /* force up */
				target->push_cpu = target_cpu;
				target->migrate_task = p;
				force = 1;
				trace_sched_hmp_migrate(p, target->push_cpu, 1);
				hmp_next_up_delay(&p->se, target->push_cpu);
			}
		}

#ifdef CONFIG_HMP_PACK_SMALL_TASK
out_force_up:
#endif /* CONFIG_HMP_PACK_SMALL_TASK */

		raw_spin_unlock_irqrestore(&target->lock, flags);
		if (force) {
			if (stop_one_cpu_dispatch(cpu_of(target),
				hmp_force_up_cpu_stop,
				target, &target->active_balance_work)) {
				put_task_struct(p); /* out of rq->lock */
				raw_spin_lock_irqsave(&target->lock, flags);
				target->active_balance = 0;
				force = 0;
				raw_spin_unlock_irqrestore(&target->lock, flags);
			}
		} else
				hmp_force_down_migration(this_cpu);
	}

#ifdef CONFIG_HMP_TRACER
	trace_sched_hmp_load(clbenv.bstats.load_avg, clbenv.lstats.load_avg);
#endif
	spin_unlock(&hmp_force_migration);
}
#ifdef CONFIG_SCHED_HMP_PLUS
/*
 * hmp_idle_pull looks at little domain runqueues to see
 * if a task should be pulled.
 *
 * Reuses hmp_force_migration spinlock.
 *
 */
static unsigned int hmp_idle_pull(int this_cpu)
{
	int cpu;
	struct sched_entity *curr, *orig;
	struct hmp_domain *hmp_domain = NULL;
	struct rq *target = NULL, *rq;
	unsigned long flags, ratio = 0;
	unsigned int force = 0;
	struct task_struct *p = NULL;
	struct clb_env clbenv;
	struct task_struct *prev_selected = NULL;
	int selected = 0;

	if (!hmp_cpu_is_slowest(this_cpu))
		hmp_domain = hmp_slower_domain(this_cpu);
	if (!hmp_domain)
		return 0;

	if (!spin_trylock(&hmp_force_migration))
		return 0;

	memset(&clbenv, 0, sizeof(clbenv));
	clbenv.flags |= HMP_GB;
	clbenv.btarget = this_cpu;
	cpumask_copy(&clbenv.lcpus, &hmp_domain->possible_cpus);
	cpumask_copy(&clbenv.bcpus, &hmp_cpu_domain(this_cpu)->possible_cpus);

	/* first select a task */
	for_each_cpu(cpu, &hmp_domain->cpus) {
		rq = cpu_rq(cpu);
		raw_spin_lock_irqsave(&rq->lock, flags);
		curr = rq->cfs.curr;
		if (!curr) {
			raw_spin_unlock_irqrestore(&rq->lock, flags);
			continue;
		}
		if (!entity_is_task(curr)) {
			struct cfs_rq *cfs_rq;

			cfs_rq = group_cfs_rq(curr);
			while (cfs_rq) {
				curr = cfs_rq->curr;
				if (!entity_is_task(curr))
					cfs_rq = group_cfs_rq(curr);
				else
					cfs_rq = NULL;
			}
		}

		orig = curr;
		curr = hmp_get_heaviest_task(curr, this_cpu);
		/* check if heaviest eligible task on this
		 * CPU is heavier than previous task
		 */
		clbenv.ltarget = cpu;
		sched_update_clbstats(&clbenv);

		if (curr && entity_is_task(curr) && (se_load(curr) > clbenv.bstats.threshold) &&
				(se_load(curr) > ratio) &&
				cpumask_test_cpu(this_cpu, tsk_cpus_allowed(task_of(curr)))) {
			selected = 1;
			p = task_of(curr);
			get_task_struct(p); /* get task and selection inside rq lock  */

			target = rq;
			ratio = curr->avg.loadwop_avg_contrib;
		}

		raw_spin_unlock_irqrestore(&rq->lock, flags);

		if (selected) {
			if (prev_selected)
				put_task_struct(prev_selected); /* To put task out of rq lock */
			prev_selected = p;
			selected = 0;
		}
	}
	if (!p)
		goto done;

	/* now we have a candidate */
	raw_spin_lock_irqsave(&target->lock, flags);
	if (!target->active_balance && (task_rq(p) == target) && !cpu_park(cpu_of(target))) {
		if (p->state != TASK_DEAD) {
			get_task_struct(p);
			target->push_cpu = this_cpu;
			target->migrate_task = p;
			trace_sched_hmp_migrate(p, target->push_cpu, 3);
			hmp_next_up_delay(&p->se, target->push_cpu);
			target->active_balance = 1; /* idle pull */
			force = 1;
		}
	}
	raw_spin_unlock_irqrestore(&target->lock, flags);
	if (force) {
		if (stop_one_cpu_dispatch(cpu_of(target),
			hmp_idle_pull_cpu_stop,
			target, &target->active_balance_work)) {
			put_task_struct(p); /* out of rq->lock */
			raw_spin_lock_irqsave(&target->lock, flags);
			target->active_balance = 0;
			force = 0;
			raw_spin_unlock_irqrestore(&target->lock, flags);
		}
	}

done:
	spin_unlock(&hmp_force_migration);
	if (p)
		put_task_struct(p);
	return force;

}
/* must hold runqueue lock for queue se is currently on */
static const int hmp_max_tasks = 5;
static struct sched_entity *hmp_get_heaviest_task(
			struct sched_entity *se, int target_cpu)
{
	int num_tasks = hmp_max_tasks;
	struct sched_entity *max_se = se;
	unsigned long int max_ratio = se->avg.loadwop_avg_contrib;
	const struct cpumask *hmp_target_mask = NULL;
	struct hmp_domain *hmp;

	if (hmp_cpu_is_fastest(cpu_of(se->cfs_rq->rq)))
		return max_se;

	hmp = hmp_faster_domain(cpu_of(se->cfs_rq->rq));
	hmp_target_mask = &hmp->cpus;
	if (target_cpu >= 0) {
		/* idle_balance gets run on a CPU while
		 * it is in the middle of being hotplugged
		 * out. Bail early in that case.
		 */
		if (!cpumask_test_cpu(target_cpu, hmp_target_mask))
			return NULL;
		hmp_target_mask = cpumask_of(target_cpu);
	}
	/* The currently running task is not on the runqueue */
	se = __pick_first_entity(cfs_rq_of(se));
	while (num_tasks && se) {
		if (entity_is_task(se) && se->avg.loadwop_avg_contrib > max_ratio &&
			cpumask_intersects(hmp_target_mask, tsk_cpus_allowed(task_of(se)))) {
			max_se = se;
			max_ratio = se->avg.loadwop_avg_contrib;
		}
		se = __pick_next_entity(se);
		num_tasks--;
	}
	return max_se;
}
static struct sched_entity *hmp_get_lightest_task(
				struct sched_entity *se, int migrate_down)
{
	int num_tasks = hmp_max_tasks;
	struct sched_entity *min_se = se;
	unsigned long int min_ratio = se->avg.loadwop_avg_contrib;
	const struct cpumask *hmp_target_mask = NULL;

	if (migrate_down) {
		struct hmp_domain *hmp;

		if (hmp_cpu_is_slowest(cpu_of(se->cfs_rq->rq)))
			return min_se;
		hmp = hmp_slower_domain(cpu_of(se->cfs_rq->rq));
		hmp_target_mask = &hmp->cpus;
	}
	/* The currently running task is not on the runqueue */
	se = __pick_first_entity(cfs_rq_of(se));

	while (num_tasks && se) {
		if (entity_is_task(se) &&
			(se->avg.loadwop_avg_contrib < min_ratio && hmp_target_mask &&
			cpumask_intersects(hmp_target_mask, tsk_cpus_allowed(task_of(se))))) {
			min_se = se;
			min_ratio = se->avg.loadwop_avg_contrib;
		}
		se = __pick_next_entity(se);
		num_tasks--;
	}
	return min_se;
}

#endif /* CONFIG_SCHED_HMP_PLUS */
#else
static void hmp_force_up_migration(int this_cpu) {}
#endif /* CONFIG_SCHED_HMP */
/*
 * run_rebalance_domains is triggered when needed from the scheduler tick.
 * Also triggered for nohz idle balancing (with nohz_balancing_kick set).
 */
static void run_rebalance_domains(struct softirq_action *h)
{
	struct rq *this_rq = this_rq();
	enum cpu_idle_type idle = this_rq->idle_balance ?
						CPU_IDLE : CPU_NOT_IDLE;
	int this_cpu = smp_processor_id();

	if (sched_feat(SCHED_HMP))
		hmp_force_up_migration(this_cpu);

	rebalance_domains(this_rq, idle);

	/*
	 * If this cpu has a pending nohz_balance_kick, then do the
	 * balancing on behalf of the other idle cpus whose ticks are
	 * stopped.
	 */
	nohz_idle_balance(this_rq, idle);
}

/*
 * Trigger the SCHED_SOFTIRQ if it is time to do periodic load balancing.
 */
void trigger_load_balance(struct rq *rq)
{
	/* Don't need to rebalance while attached to NULL domain */
	if (unlikely(on_null_domain(rq)))
		return;

	if (time_after_eq(jiffies, rq->next_balance))
		raise_softirq(SCHED_SOFTIRQ);
#ifdef CONFIG_NO_HZ_COMMON
	if (nohz_kick_needed(rq))
		nohz_balancer_kick();
#endif
}

static void rq_online_fair(struct rq *rq)
{
	if (sched_feat(SCHED_HMP))
		hmp_online_cpu(rq->cpu);

	update_sysctl();

	update_runtime_enabled(rq);
}

static void rq_offline_fair(struct rq *rq)
{
	if (sched_feat(SCHED_HMP))
		hmp_offline_cpu(rq->cpu);

	update_sysctl();

	/* Ensure any throttled groups are reachable by pick_next_task */
	unthrottle_offline_cfs_rqs(rq);
}

#endif /* CONFIG_SMP */

/*
 * scheduler tick hitting a task of our scheduling class:
 */
static void task_tick_fair(struct rq *rq, struct task_struct *curr, int queued)
{
	struct cfs_rq *cfs_rq;
	struct sched_entity *se = &curr->se;

	for_each_sched_entity(se) {
		cfs_rq = cfs_rq_of(se);
		entity_tick(cfs_rq, se, queued);
	}

	if (numabalancing_enabled)
		task_tick_numa(rq, curr);

	update_rq_runnable_avg(rq, 1);
}

/*
 * called on fork with the child task as argument from the parent's context
 *  - child not yet on the tasklist
 *  - preemption disabled
 */
static void task_fork_fair(struct task_struct *p)
{
	struct cfs_rq *cfs_rq;
	struct sched_entity *se = &p->se, *curr;
	int this_cpu = smp_processor_id();
	struct rq *rq = this_rq();
	unsigned long flags;

	raw_spin_lock_irqsave(&rq->lock, flags);

	update_rq_clock(rq);

	cfs_rq = task_cfs_rq(current);
	curr = cfs_rq->curr;

	/*
	 * Not only the cpu but also the task_group of the parent might have
	 * been changed after parent->se.parent,cfs_rq were copied to
	 * child->se.parent,cfs_rq. So call __set_task_cpu() to make those
	 * of child point to valid ones.
	 */
	rcu_read_lock();
	__set_task_cpu(p, this_cpu);
	rcu_read_unlock();

	update_curr(cfs_rq);

	if (curr)
		se->vruntime = curr->vruntime;
	place_entity(cfs_rq, se, 1);

	if (sysctl_sched_child_runs_first && curr && entity_before(curr, se)) {
		/*
		 * Upon rescheduling, sched_class::put_prev_task() will place
		 * 'current' within the tree based on its new key value.
		 */
		swap(curr->vruntime, se->vruntime);
		resched_curr(rq);
	}

	se->vruntime -= cfs_rq->min_vruntime;

	raw_spin_unlock_irqrestore(&rq->lock, flags);
}

/*
 * Priority of the task has changed. Check to see if we preempt
 * the current task.
 */
static void
prio_changed_fair(struct rq *rq, struct task_struct *p, int oldprio)
{
	if (!task_on_rq_queued(p))
		return;

	/*
	 * Reschedule if we are currently running on this runqueue and
	 * our priority decreased, or if we are not currently running on
	 * this runqueue and our priority is higher than the current's
	 */
	if (rq->curr == p) {
		if (p->prio > oldprio)
			resched_curr(rq);
	} else
		check_preempt_curr(rq, p, 0);
}

static void switched_from_fair(struct rq *rq, struct task_struct *p)
{
	struct sched_entity *se = &p->se;
	struct cfs_rq *cfs_rq = cfs_rq_of(se);

	/*
	 * Ensure the task's vruntime is normalized, so that when it's
	 * switched back to the fair class the enqueue_entity(.flags=0) will
	 * do the right thing.
	 *
	 * If it's queued, then the dequeue_entity(.flags=0) will already
	 * have normalized the vruntime, if it's !queued, then only when
	 * the task is sleeping will it still have non-normalized vruntime.
	 */
	if (!task_on_rq_queued(p) && p->state != TASK_RUNNING) {
		/*
		 * Fix up our vruntime so that the current sleep doesn't
		 * cause 'unlimited' sleep bonus.
		 */
		place_entity(cfs_rq, se, 0);
		se->vruntime -= cfs_rq->min_vruntime;
	}

#ifdef CONFIG_SMP
	/*
	* Remove our load from contribution when we leave sched_fair
	* and ensure we don't carry in an old decay_count if we
	* switch back.
	*/
	if (se->avg.decay_count) {
		__synchronize_entity_decay(se);
		subtract_blocked_load_contrib(cfs_rq, se->avg.load_avg_contrib);
		subtract_utilization_blocked_contrib(cfs_rq,
					se->avg.utilization_avg_contrib);
	}
#endif
}

/*
 * We switched to the sched_fair class.
 */
static void switched_to_fair(struct rq *rq, struct task_struct *p)
{
#ifdef CONFIG_FAIR_GROUP_SCHED
	struct sched_entity *se = &p->se;
	/*
	 * Since the real-depth could have been changed (only FAIR
	 * class maintain depth value), reset depth properly.
	 */
	se->depth = se->parent ? se->parent->depth + 1 : 0;
#endif
	if (!task_on_rq_queued(p))
		return;

	/*
	 * We were most likely switched from sched_rt, so
	 * kick off the schedule if running, otherwise just see
	 * if we can still preempt the current task.
	 */
	if (rq->curr == p)
		resched_curr(rq);
	else
		check_preempt_curr(rq, p, 0);
}

/* Account for a task changing its policy or group.
 *
 * This routine is mostly called to set cfs_rq->curr field when a task
 * migrates between groups/classes.
 */
static void set_curr_task_fair(struct rq *rq)
{
	struct sched_entity *se = &rq->curr->se;

	for_each_sched_entity(se) {
		struct cfs_rq *cfs_rq = cfs_rq_of(se);

		set_next_entity(cfs_rq, se);
		/* ensure bandwidth has been allocated on our new cfs_rq */
		account_cfs_rq_runtime(cfs_rq, 0);
	}
}

void init_cfs_rq(struct cfs_rq *cfs_rq)
{
	cfs_rq->tasks_timeline = RB_ROOT;
	cfs_rq->min_vruntime = (u64)(-(1LL << 20));
#ifndef CONFIG_64BIT
	cfs_rq->min_vruntime_copy = cfs_rq->min_vruntime;
#endif
#ifdef CONFIG_SMP
	atomic64_set(&cfs_rq->decay_counter, 1);
	atomic_long_set(&cfs_rq->removed_load, 0);
	atomic_long_set(&cfs_rq->removed_utilization, 0);
#endif
}

#ifdef CONFIG_FAIR_GROUP_SCHED
static void task_move_group_fair(struct task_struct *p, int queued)
{
	struct sched_entity *se = &p->se;
	struct cfs_rq *cfs_rq;

	/*
	 * If the task was not on the rq at the time of this cgroup movement
	 * it must have been asleep, sleeping tasks keep their ->vruntime
	 * absolute on their old rq until wakeup (needed for the fair sleeper
	 * bonus in place_entity()).
	 *
	 * If it was on the rq, we've just 'preempted' it, which does convert
	 * ->vruntime to a relative base.
	 *
	 * Make sure both cases convert their relative position when migrating
	 * to another cgroup's rq. This does somewhat interfere with the
	 * fair sleeper stuff for the first placement, but who cares.
	 */
	/*
	 * When !queued, vruntime of the task has usually NOT been normalized.
	 * But there are some cases where it has already been normalized:
	 *
	 * - Moving a forked child which is waiting for being woken up by
	 *   wake_up_new_task().
	 * - Moving a task which has been woken up by try_to_wake_up() and
	 *   waiting for actually being woken up by sched_ttwu_pending().
	 *
	 * To prevent boost or penalty in the new cfs_rq caused by delta
	 * min_vruntime between the two cfs_rqs, we skip vruntime adjustment.
	 */
	if (!queued && (!se->sum_exec_runtime || p->state == TASK_WAKING))
		queued = 1;

	if (!queued)
		se->vruntime -= cfs_rq_of(se)->min_vruntime;
	set_task_rq(p, task_cpu(p));
	se->depth = se->parent ? se->parent->depth + 1 : 0;
	if (!queued) {
		cfs_rq = cfs_rq_of(se);
		se->vruntime += cfs_rq->min_vruntime;
#ifdef CONFIG_SMP
		/*
		 * migrate_task_rq_fair() will have removed our previous
		 * contribution, but we must synchronize for ongoing future
		 * decay.
		 */
		se->avg.decay_count = atomic64_read(&cfs_rq->decay_counter);
		cfs_rq->blocked_load_avg += se->avg.load_avg_contrib;
		cfs_rq->utilization_blocked_avg +=
						se->avg.utilization_avg_contrib;
#endif
	}
}

void free_fair_sched_group(struct task_group *tg)
{
	int i;

	destroy_cfs_bandwidth(tg_cfs_bandwidth(tg));

	for_each_possible_cpu(i) {
		if (tg->cfs_rq)
			kfree(tg->cfs_rq[i]);
		if (tg->se)
			kfree(tg->se[i]);
	}

	kfree(tg->cfs_rq);
	kfree(tg->se);
}

int alloc_fair_sched_group(struct task_group *tg, struct task_group *parent)
{
	struct cfs_rq *cfs_rq;
	struct sched_entity *se;
	int i;

	tg->cfs_rq = kzalloc(sizeof(cfs_rq) * nr_cpu_ids, GFP_KERNEL);
	if (!tg->cfs_rq)
		goto err;
	tg->se = kzalloc(sizeof(se) * nr_cpu_ids, GFP_KERNEL);
	if (!tg->se)
		goto err;

	tg->shares = NICE_0_LOAD;

	init_cfs_bandwidth(tg_cfs_bandwidth(tg));

	for_each_possible_cpu(i) {
		cfs_rq = kzalloc_node(sizeof(struct cfs_rq),
				      GFP_KERNEL, cpu_to_node(i));
		if (!cfs_rq)
			goto err;

		se = kzalloc_node(sizeof(struct sched_entity),
				  GFP_KERNEL, cpu_to_node(i));
		if (!se)
			goto err_free_rq;

		init_cfs_rq(cfs_rq);
		init_tg_cfs_entry(tg, cfs_rq, se, i, parent->se[i]);
	}

	return 1;

err_free_rq:
	kfree(cfs_rq);
err:
	return 0;
}

void unregister_fair_sched_group(struct task_group *tg, int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long flags;

	/*
	* Only empty task groups can be destroyed; so we can speculatively
	* check on_list without danger of it being re-added.
	*/
	if (!tg->cfs_rq[cpu]->on_list)
		return;

	raw_spin_lock_irqsave(&rq->lock, flags);
	list_del_leaf_cfs_rq(tg->cfs_rq[cpu]);
	raw_spin_unlock_irqrestore(&rq->lock, flags);
}

void init_tg_cfs_entry(struct task_group *tg, struct cfs_rq *cfs_rq,
			struct sched_entity *se, int cpu,
			struct sched_entity *parent)
{
	struct rq *rq = cpu_rq(cpu);

	cfs_rq->tg = tg;
	cfs_rq->rq = rq;
	init_cfs_rq_runtime(cfs_rq);

	tg->cfs_rq[cpu] = cfs_rq;
	tg->se[cpu] = se;

	/* se could be NULL for root_task_group */
	if (!se)
		return;

	if (!parent) {
		se->cfs_rq = &rq->cfs;
		se->depth = 0;
	} else {
		se->cfs_rq = parent->my_q;
		se->depth = parent->depth + 1;
	}

	se->my_q = cfs_rq;
	/* guarantee group entities always have weight */
	update_load_set(&se->load, NICE_0_LOAD);
	se->parent = parent;
}

static DEFINE_MUTEX(shares_mutex);

int sched_group_set_shares(struct task_group *tg, unsigned long shares)
{
	int i;
	unsigned long flags;

	/*
	 * We can't change the weight of the root cgroup.
	 */
	if (!tg->se[0])
		return -EINVAL;

	shares = clamp(shares, scale_load(MIN_SHARES), scale_load(MAX_SHARES));

	mutex_lock(&shares_mutex);
	if (tg->shares == shares)
		goto done;

	tg->shares = shares;
	for_each_possible_cpu(i) {
		struct rq *rq = cpu_rq(i);
		struct sched_entity *se;

		se = tg->se[i];
		/* Propagate contribution to hierarchy */
		raw_spin_lock_irqsave(&rq->lock, flags);

		/* Possible calls to update_curr() need rq clock */
		update_rq_clock(rq);
		for_each_sched_entity(se)
			update_cfs_shares(group_cfs_rq(se));
		raw_spin_unlock_irqrestore(&rq->lock, flags);
	}

done:
	mutex_unlock(&shares_mutex);
	return 0;
}
#else /* CONFIG_FAIR_GROUP_SCHED */

void free_fair_sched_group(struct task_group *tg) { }

int alloc_fair_sched_group(struct task_group *tg, struct task_group *parent)
{
	return 1;
}

void unregister_fair_sched_group(struct task_group *tg, int cpu) { }

#endif /* CONFIG_FAIR_GROUP_SCHED */


static unsigned int get_rr_interval_fair(struct rq *rq, struct task_struct *task)
{
	struct sched_entity *se = &task->se;
	unsigned int rr_interval = 0;

	/*
	 * Time slice is 0 for SCHED_OTHER tasks that are on an otherwise
	 * idle runqueue:
	 */
	if (rq->cfs.load.weight)
		rr_interval = NS_TO_JIFFIES(sched_slice(cfs_rq_of(se), se));

	return rr_interval;
}

/*
 * All the scheduling class methods:
 */
const struct sched_class fair_sched_class = {
	.next			= &idle_sched_class,
	.enqueue_task		= enqueue_task_fair,
	.dequeue_task		= dequeue_task_fair,
	.yield_task		= yield_task_fair,
	.yield_to_task		= yield_to_task_fair,

	.check_preempt_curr	= check_preempt_wakeup,

	.pick_next_task		= pick_next_task_fair,
	.put_prev_task		= put_prev_task_fair,

#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_fair,
	.migrate_task_rq	= migrate_task_rq_fair,

	.rq_online		= rq_online_fair,
	.rq_offline		= rq_offline_fair,

	.task_waking		= task_waking_fair,
#endif

	.set_curr_task          = set_curr_task_fair,
	.task_tick		= task_tick_fair,
	.task_fork		= task_fork_fair,

	.prio_changed		= prio_changed_fair,
	.switched_from		= switched_from_fair,
	.switched_to		= switched_to_fair,

	.get_rr_interval	= get_rr_interval_fair,

	.update_curr		= update_curr_fair,

#ifdef CONFIG_FAIR_GROUP_SCHED
	.task_move_group	= task_move_group_fair,
#endif
};

#ifdef CONFIG_SCHED_DEBUG
void print_cfs_stats(struct seq_file *m, int cpu)
{
	struct cfs_rq *cfs_rq;

	rcu_read_lock();
	for_each_leaf_cfs_rq(cpu_rq(cpu), cfs_rq)
		print_cfs_rq(m, cpu, cfs_rq);
	rcu_read_unlock();
}
#endif

__init void init_sched_fair_class(void)
{
#ifdef CONFIG_SMP
	open_softirq(SCHED_SOFTIRQ, run_rebalance_domains);

#ifdef CONFIG_NO_HZ_COMMON
	nohz.next_balance = jiffies;
	zalloc_cpumask_var(&nohz.idle_cpus_mask, GFP_NOWAIT);
	cpu_notifier(sched_ilb_notifier, 0);
#endif
#endif /* SMP */
	cmp_cputopo_domain_setup();
#ifdef CONFIG_SCHED_HMP
	hmp_cpu_mask_setup();
#endif
}

#ifdef CONFIG_HMP_FREQUENCY_INVARIANT_SCALE
static int cpufreq_callback(struct notifier_block *nb,
				unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;
	int cpu = freq->cpu;
	struct cpumask cls_cpus;
	int id;

	if (freq->flags & CPUFREQ_CONST_LOOPS)
		return NOTIFY_OK;

	if (val == CPUFREQ_PRECHANGE) {
		arch_get_cluster_cpus(&cls_cpus, arch_get_cluster_id(cpu));
		for_each_cpu(id, &cls_cpus)
			arch_scale_set_curr_freq(id, freq->new);
	}

	return NOTIFY_OK;
}

static struct notifier_block cpufreq_notifier = {
	.notifier_call = cpufreq_callback,
};

static int cpufreq_policy_callback(struct notifier_block *nb,
					unsigned long val, void *data)
{
	struct cpufreq_policy *policy = data;
	int i;

	if (val != CPUFREQ_NOTIFY)
		return NOTIFY_OK;

	for_each_cpu(i, policy->cpus) {
		arch_scale_set_curr_freq(i, policy->cur);
		arch_scale_set_max_freq(i, policy->max);
	}

	return NOTIFY_OK;
}

static struct notifier_block cpufreq_policy_notifier = {
	.notifier_call = cpufreq_policy_callback,
};

static int __init register_cpufreq_notifier(void)
{
	int ret;

	ret = cpufreq_register_notifier(&cpufreq_notifier,
						CPUFREQ_TRANSITION_NOTIFIER);
	if (ret)
		return ret;

	return cpufreq_register_notifier(&cpufreq_policy_notifier,
						CPUFREQ_POLICY_NOTIFIER);
}
core_initcall(register_cpufreq_notifier);
#endif

#ifdef CONFIG_HMP_PACK_SMALL_TASK
static int check_pack_buddy(int cpu, struct task_struct *p)
{
	int buddy;
	int L_target;

	if (cpu >= num_possible_cpus() || cpu < 0)
		return false;

	buddy = per_cpu(sd_pack_buddy, cpu);

	/* No pack buddy for this CPU */
	if (buddy == -1)
		return false;

	/* Do not pack to buddy whithin little cluster */
	if (hmp_cpu_is_slowest(cpu)) {
		buddy = cpu;
		goto check_load;
	}
	if (hmp_cpu_is_slowest(buddy)) {
		L_target = hmp_select_cpu(HMP_SELECT_RQ, p, &hmp_cpu_domain(buddy)->possible_cpus, buddy, 1);
		per_cpu(sd_pack_buddy, cpu) = (L_target >= num_possible_cpus()) ? buddy : L_target;
		buddy = per_cpu(sd_pack_buddy, cpu);
	}

check_load:
	/*
	 * If a task is waiting for running on the CPU which is its own buddy,
	 * let the default behavior to look for a better CPU if available
	 * The threshold has been set to 37.5%
	 */
	if ((buddy == cpu)
		&& ((p->se.avg.running_avg_sum << 3) < (p->se.avg.runnable_avg_sum * 5)))
		return false;

	/* buddy is not an allowed CPU */
	if (!cpumask_test_cpu(buddy, tsk_cpus_allowed(p)))
		return false;

	/*
	 * If the task is a small one and the buddy is not overloaded,
	 * we use buddy cpu
	 */
	if (!is_light_task(p) || is_buddy_busy(buddy))
		return false;

	return true;
}
#endif /* CONFIG_HMP_PACK_SMALL_TASK */
