#include <linux/cgroup.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/percpu.h>
#include <linux/printk.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>

#include <trace/events/sched.h>

#include "sched.h"
#include "tune.h"

#define MET_STUNE_DEBUG 1

#if MET_STUNE_DEBUG
#include <mt-plat/met_drv.h>
#endif

#ifdef CONFIG_CGROUP_SCHEDTUNE
bool schedtune_initialized = false;
#endif

unsigned int sysctl_sched_cfs_boost __read_mostly;

static int default_stune_threshold;
bool global_negative_flag;

/* A lock for set stune_task_threshold */
raw_spinlock_t stune_lock;

static struct target_cap schedtune_target_cap[16];
static int cpu_cluster_nr;

extern struct reciprocal_value schedtune_spc_rdiv;
struct target_nrg schedtune_target_nrg;

/* Performance Boost region (B) threshold params */
static int perf_boost_idx;

/* Performance Constraint region (C) threshold params */
static int perf_constrain_idx;

/**
 * Performance-Energy (P-E) Space thresholds constants
 */
struct threshold_params {
	int nrg_gain;
	int cap_gain;
};

/*
 * System specific P-E space thresholds constants
 */
static struct threshold_params
threshold_gains[] = {
	{ 0, 5 }, /*   < 10% */
	{ 1, 5 }, /*   < 20% */
	{ 2, 5 }, /*   < 30% */
	{ 3, 5 }, /*   < 40% */
	{ 4, 5 }, /*   < 50% */
	{ 5, 4 }, /*   < 60% */
	{ 5, 3 }, /*   < 70% */
	{ 5, 2 }, /*   < 80% */
	{ 5, 1 }, /*   < 90% */
	{ 5, 0 }  /* <= 100% */
};

static int
__schedtune_accept_deltas(int nrg_delta, int cap_delta,
			  int perf_boost_idx, int perf_constrain_idx)
{
	int payoff = -INT_MAX;
	int gain_idx = -1;

	/* Performance Boost (B) region */
	if (nrg_delta >= 0 && cap_delta > 0)
		gain_idx = perf_boost_idx;
	/* Performance Constraint (C) region */
	else if (nrg_delta < 0 && cap_delta <= 0)
		gain_idx = perf_constrain_idx;

	/* Default: reject schedule candidate */
	if (gain_idx == -1)
		return payoff;

	/*
	 * Evaluate "Performance Boost" vs "Energy Increase"
	 *
	 * - Performance Boost (B) region
	 *
	 *   Condition: nrg_delta > 0 && cap_delta > 0
	 *   Payoff criteria:
	 *     cap_gain / nrg_gain  < cap_delta / nrg_delta =
	 *     cap_gain * nrg_delta < cap_delta * nrg_gain
	 *   Note that since both nrg_gain and nrg_delta are positive, the
	 *   inequality does not change. Thus:
	 *
	 *     payoff = (cap_delta * nrg_gain) - (cap_gain * nrg_delta)
	 *
	 * - Performance Constraint (C) region
	 *
	 *   Condition: nrg_delta < 0 && cap_delta < 0
	 *   payoff criteria:
	 *     cap_gain / nrg_gain  > cap_delta / nrg_delta =
	 *     cap_gain * nrg_delta < cap_delta * nrg_gain
	 *   Note that since nrg_gain > 0 while nrg_delta < 0, the
	 *   inequality change. Thus:
	 *
	 *     payoff = (cap_delta * nrg_gain) - (cap_gain * nrg_delta)
	 *
	 * This means that, in case of same positive defined {cap,nrg}_gain
	 * for both the B and C regions, we can use the same payoff formula
	 * where a positive value represents the accept condition.
	 */
	payoff  = cap_delta * threshold_gains[gain_idx].nrg_gain;
	payoff -= nrg_delta * threshold_gains[gain_idx].cap_gain;

	return payoff;
}
#ifdef CONFIG_PROVE_LOCKING
inline int close_lockdep_if_cpu_offline(void)
{
	int result = 0;

	if (!cpu_online(raw_smp_processor_id())) {
		lockdep_off();
		result = 1;
	}

	return result;
}

inline void open_lockdep_if_need(int need)
{
	if (need)
		lockdep_on();
}
#endif

#ifdef CONFIG_CGROUP_SCHEDTUNE

/*
 * EAS scheduler tunables for task groups.
 *
 * When CGroup support is enabled, we have to synchronize two different
 * paths:
 *  - slow path: where CGroups are created/updated/removed
 *  - fast path: where tasks in a CGroups are accounted
 *
 * The slow path tracks (a limited number of) CGroups and maps each on a
 * "boost_group" index. The fastpath accounts tasks currently RUNNABLE on each
 * "boost_group".
 *
 * Once a new CGroup is created, a boost group idx is assigned and the
 * corresponding "boost_group" marked as valid on each CPU.
 * Once a CGroup is release, the corresponding "boost_group" is marked as
 * invalid on each CPU. The CPU boost value (boost_max) is aggregated by
 * considering only valid boost_groups with a non null tasks counter.
 *
 * .:: Locking strategy
 *
 * The fast path uses a spin lock for each CPU boost_group which protects the
 * tasks counter.
 *
 * The "valid" and "boost" values of each CPU boost_group is instead
 * protected by the RCU lock provided by the CGroups callbacks. Thus, only the
 * slow path can access and modify the boost_group attribtues of each CPU.
 * The fast path will catch up the most updated values at the next scheduling
 * event (i.e. enqueue/dequeue).
 *
 *                                                        |
 *                                             SLOW PATH  |   FAST PATH
 *                              CGroup add/update/remove  |   Scheduler enqueue/dequeue events
 *                                                        |
 *                                                        |
 *                                                        |     DEFINE_PER_CPU(struct boost_groups)
 *                                                        |     +--------------+----+---+----+----+
 *                                                        |     |  idle        |    |   |    |    |
 *                                                        |     |  boost_max   |    |   |    |    |
 *                                                        |  +---->lock        |    |   |    |    |
 *  struct schedtune                  allocated_groups    |  |  |  group[    ] |    |   |    |    |
 *  +------------------------------+         +-------+    |  |  +--+---------+-+----+---+----+----+
 *  | idx                          |         |       |    |  |     |  valid  |
 *  | boots / prefer_idle          |         |       |    |  |     |  boost  |
 *  | perf_{boost/constraints}_idx | <---------+(*)  |    |  |     |  tasks  | <------------+
 *  | css                          |         +-------+    |  |     +---------+              |
 *  +-+----------------------------+         |       |    |  |     |         |              |
 *    ^                                      |       |    |  |     |         |              |
 *    |                                      +-------+    |  |     +---------+              |
 *    |                                      |       |    |  |     |         |              |
 *    |                                      |       |    |  |     |         |              |
 *    |                                      +-------+    |  |     +---------+              |
 *    | zmalloc                              |       |    |  |     |         |              |
 *    |                                      |       |    |  |     |         |              |
 *    |                                      +-------+    |  |     +---------+              |
 *    +                              BOOSTGROUPS_COUNT    |  |     BOOSTGROUPS_COUNT        |
 *  schedtune_boostgroup_init()                           |  +                              |
 *                                                        |  schedtune_{en,de}queue_task()  |
 *                                                        |                                 +
 *                                                        |          schedtune_tasks_update()
 *                                                        |
 */

/* SchdTune tunables for a group of tasks */
struct schedtune {
	/* SchedTune CGroup subsystem */
	struct cgroup_subsys_state css;

	/* Boost group allocated ID */
	int idx;

	/* Boost value for tasks on that SchedTune CGroup */
	int boost;

	/* Performance Boost (B) region threshold params */
	int perf_boost_idx;

	/* Performance Constraint (C) region threshold params */
	int perf_constrain_idx;

	/* Hint to bias scheduling of tasks on that SchedTune CGroup
	 * towards idle CPUs */
	int prefer_idle;

	/* Add capacity_min for task floor setting */
	int capacity_min;

	/* Cpu util clamping */
	struct uclamp_se uclamp[UCLAMP_CNT];
};

static inline struct schedtune *css_st(struct cgroup_subsys_state *css)
{
	return css ? container_of(css, struct schedtune, css) : NULL;
}

static inline struct schedtune *task_schedtune(struct task_struct *tsk)
{
	return css_st(task_css(tsk, schedtune_cgrp_id));
}

static inline struct schedtune *parent_st(struct schedtune *st)
{
	return css_st(st->css.parent);
}

unsigned int uclamp_st_min(struct task_struct *tsk)
{
	unsigned int val;

	rcu_read_lock();
	val =  task_schedtune(tsk)->uclamp[UCLAMP_MIN].value;
	rcu_read_unlock();
	return val;
}

/*
 * SchedTune root control group
 * The root control group is used to defined a system-wide boosting tuning,
 * which is applied to all tasks in the system.
 * Task specific boost tuning could be specified by creating and
 * configuring a child control group under the root one.
 * By default, system-wide boosting is disabled, i.e. no boosting is applied
 * to tasks which are not into a child control group.
 */
static struct schedtune
root_schedtune = {
	.boost	= 0,
	.perf_boost_idx = 0,
	.perf_constrain_idx = 0,
	.prefer_idle = 0,
	.capacity_min = 0,
};

inline struct uclamp_se *root_schedtune_uclamp(int clamp_id)
{
	return &root_schedtune.uclamp[clamp_id];
}

struct uclamp_se *schedtune_uclamp(struct task_struct *tsk,
						int clamp_id)
{
	struct cgroup_subsys_state *css;
	struct uclamp_se *se;

	rcu_read_lock();
	css = task_css(tsk, schedtune_cgrp_id);
	se = &css_st(css)->uclamp[clamp_id];
	rcu_read_unlock();

	return se;
}

int
schedtune_accept_deltas(int nrg_delta, int cap_delta,
			struct task_struct *task)
{
	struct schedtune *ct;
	int perf_boost_idx;
	int perf_constrain_idx;

	/* Optimal (O) region */
	if (nrg_delta < 0 && cap_delta > 0) {
		trace_sched_tune_filter(nrg_delta, cap_delta, 0, 0, 1, 0);
		return INT_MAX;
	}

	/* Suboptimal (S) region */
	if (nrg_delta > 0 && cap_delta < 0) {
		trace_sched_tune_filter(nrg_delta, cap_delta, 0, 0, -1, 5);
		return -INT_MAX;
	}

	/* Get task specific perf Boost/Constraints indexes */
	rcu_read_lock();
	ct = task_schedtune(task);
	perf_boost_idx = ct->perf_boost_idx;
	perf_constrain_idx = ct->perf_constrain_idx;
	rcu_read_unlock();

	return __schedtune_accept_deltas(nrg_delta, cap_delta,
			perf_boost_idx, perf_constrain_idx);
}

/*
 * Maximum number of boost groups to support
 * When per-task boosting is used we still allow only limited number of
 * boost groups for two main reasons:
 * 1. on a real system we usually have only few classes of workloads which
 *    make sense to boost with different values (e.g. background vs foreground
 *    tasks, interactive vs low-priority tasks)
 * 2. a limited number allows for a simpler and more memory/time efficient
 *    implementation especially for the computation of the per-CPU boost
 *    value
 */
#ifdef CONFIG_MTK_IO_BOOST
#define BOOSTGROUPS_COUNT 7
#else
#define BOOSTGROUPS_COUNT 5
#endif

/* Array of configured boostgroups */
static struct schedtune *allocated_group[BOOSTGROUPS_COUNT] = {
	&root_schedtune,
	NULL,
};

/* SchedTune boost groups
 * Keep track of all the boost groups which impact on CPU, for example when a
 * CPU has two RUNNABLE tasks belonging to two different boost groups and thus
 * likely with different boost values.
 * Since on each system we expect only a limited number of boost groups, here
 * we use a simple array to keep track of the metrics required to compute the
 * maximum per-CPU boosting value.
 */
struct boost_groups {
	/* Maximum boost value for all RUNNABLE tasks on a CPU */
	int boost_max;
	/*
	 * Maximum capacity_min for all RUNNABLE tasks on a CPU,
	 * to fix floor capacity of CPU.
	 */
	int max_capacity_min;
	struct {
		/* True when this boost group maps an actual cgroup */
		bool valid;
		/* The boost for tasks on that boost group */
		int boost;
		/* Count of RUNNABLE tasks on that boost group */
		unsigned tasks;
		/* The capacity_min for tasks on that boost group */
		int capacity_min;
	} group[BOOSTGROUPS_COUNT];
	/* CPU's boost group locking */
	raw_spinlock_t lock;
};

/* Boost groups affecting each CPU in the system */
DEFINE_PER_CPU(struct boost_groups, cpu_boost_groups);

static void
schedtune_cpu_update(int cpu)
{
	struct boost_groups *bg;
	int boost_max;
	int idx;
	int max_capacity_min;

	bg = &per_cpu(cpu_boost_groups, cpu);

	/* The root boost group is always active */
	boost_max = bg->group[0].boost;
	max_capacity_min = bg->group[0].capacity_min;
	for (idx = 1; idx < BOOSTGROUPS_COUNT; ++idx) {

		/* Ignore non boostgroups not mapping a cgroup */
		if (!bg->group[idx].valid)
			continue;

		/*
		 * A boost group affects a CPU only if it has
		 * RUNNABLE tasks on that CPU
		 */
		if (bg->group[idx].tasks == 0)
			continue;

		boost_max = max(boost_max, bg->group[idx].boost);
		max_capacity_min =
			max(max_capacity_min, bg->group[idx].capacity_min);
	}

	/* Ensures boost_max is non-negative when all cgroup boost values
	 * are neagtive. Avoids under-accounting of cpu capacity which may cause
	 * task stacking and frequency spikes.*/
	/*
	 * mtk:
	 * If original path, max(boost_max, 0)
	 * If use mtk perfservice kernel API to update negative boost,
	 * when all group are neagtive, boost_max should lower than 0
	 * and it can decrease frequency.
	 */
	if (!global_negative_flag) {
		boost_max = max(boost_max, 0);
		max_capacity_min = max(max_capacity_min, 0);
	}

	bg->boost_max = boost_max;
	bg->max_capacity_min = max_capacity_min;
}

static int
schedtune_boostgroup_update(int idx, int boost)
{
	struct boost_groups *bg;
	int cur_boost_max;
	int old_boost;
	int cpu;

	/* Update per CPU boost groups */
	for_each_possible_cpu(cpu) {
		bg = &per_cpu(cpu_boost_groups, cpu);

		/* CGroups are never associated to non active cgroups */
		BUG_ON(!bg->group[idx].valid);

		/*
		 * Keep track of current boost values to compute the per CPU
		 * maximum only when it has been affected by the new value of
		 * the updated boost group
		 */
		cur_boost_max = bg->boost_max;
		old_boost = bg->group[idx].boost;

		/* Update the boost value of this boost group */
		bg->group[idx].boost = boost;

		/* Check if this update increase current max */
		if (boost > cur_boost_max && bg->group[idx].tasks) {
			bg->boost_max = boost;
			trace_sched_tune_boostgroup_update(cpu, 1, bg->boost_max);
			continue;
		}

		/* Check if this update has decreased current max */
		if (cur_boost_max == old_boost && old_boost > boost) {
			schedtune_cpu_update(cpu);
			trace_sched_tune_boostgroup_update(cpu, -1, bg->boost_max);
			continue;
		}

		trace_sched_tune_boostgroup_update(cpu, 0, bg->boost_max);
	}

	return 0;
}

static int
schedtune_boostgroup_update_capacity_min(int idx, int capacity_min)
{
	struct boost_groups *bg;
	int cur_max_capacity_min;
	int old_capacity_min;
	int cpu;

	/* Update per CPU boost groups */
	for_each_possible_cpu(cpu) {
		bg = &per_cpu(cpu_boost_groups, cpu);

		/*
		 * Keep track of current boost values to compute the per CPU
		 * maximum only when it has been affected by the new value of
		 * the updated boost group
		 */
		cur_max_capacity_min = bg->max_capacity_min;
		old_capacity_min = bg->group[idx].capacity_min;
		/* Update the boost value of this boost group */
		bg->group[idx].capacity_min = capacity_min;
		/* Check if this update increase current max */
		if (capacity_min > cur_max_capacity_min &&
			bg->group[idx].tasks) {
			bg->max_capacity_min = capacity_min;
			continue;
		}
		/* Check if this update has decreased current max */
		if (cur_max_capacity_min == old_capacity_min &&
			old_capacity_min > capacity_min) {
			schedtune_cpu_update(cpu);
			continue;
		}
	}
	return 0;
}

#include "tune_plus.c"

#define ENQUEUE_TASK  1
#define DEQUEUE_TASK -1

static inline void
schedtune_tasks_update(struct task_struct *p, int cpu, int idx, int task_count)
{
	struct boost_groups *bg = &per_cpu(cpu_boost_groups, cpu);
	int tasks = bg->group[idx].tasks + task_count;

	/* Update boosted tasks count while avoiding to make it negative */
	bg->group[idx].tasks = max(0, tasks);

	trace_sched_tune_tasks_update(p, cpu, tasks, idx,
			bg->group[idx].boost, bg->boost_max,
			bg->group[idx].capacity_min, bg->max_capacity_min);

	/* Boost group activation or deactivation on that RQ */
	if (tasks == 1 || tasks == 0)
		schedtune_cpu_update(cpu);
}

/*
 * NOTE: This function must be called while holding the lock on the CPU RQ
 */
void schedtune_enqueue_task(struct task_struct *p, int cpu)
{
	struct boost_groups *bg = &per_cpu(cpu_boost_groups, cpu);
	unsigned long irq_flags;
	struct schedtune *st;
	int idx;

	if (!unlikely(schedtune_initialized))
		return;

	/*
	 * When a task is marked PF_EXITING by do_exit() it's going to be
	 * dequeued and enqueued multiple times in the exit path.
	 * Thus we avoid any further update, since we do not want to change
	 * CPU boosting while the task is exiting.
	 */
	if (p->flags & PF_EXITING)
		return;

	/*
	 * Boost group accouting is protected by a per-cpu lock and requires
	 * interrupt to be disabled to avoid race conditions for example on
	 * do_exit()::cgroup_exit() and task migration.
	 */
	raw_spin_lock_irqsave(&bg->lock, irq_flags);
	rcu_read_lock();

	st = task_schedtune(p);
	idx = st->idx;

	schedtune_tasks_update(p, cpu, idx, ENQUEUE_TASK);

	rcu_read_unlock();
	raw_spin_unlock_irqrestore(&bg->lock, irq_flags);
}

int schedtune_can_attach(struct cgroup_taskset *tset)
{
	struct task_struct *task;
	struct cgroup_subsys_state *css;
	struct boost_groups *bg;
	struct rq_flags irq_flags;
	unsigned int cpu;
	struct rq *rq;
	int src_bg; /* Source boost group index */
	int dst_bg; /* Destination boost group index */
	int tasks;

	if (!unlikely(schedtune_initialized))
		return 0;


	cgroup_taskset_for_each(task, css, tset) {

		/*
		 * Lock the CPU's RQ the task is enqueued to avoid race
		 * conditions with migration code while the task is being
		 * accounted
		 */
		rq = lock_rq_of(task, &irq_flags);

		if (!task->on_rq) {
			unlock_rq_of(rq, task, &irq_flags);
			continue;
		}

		/*
		 * Boost group accouting is protected by a per-cpu lock and requires
		 * interrupt to be disabled to avoid race conditions on...
		 */
		cpu = cpu_of(rq);
		bg = &per_cpu(cpu_boost_groups, cpu);
		raw_spin_lock(&bg->lock);

		dst_bg = css_st(css)->idx;
		src_bg = task_schedtune(task)->idx;

		/*
		 * Current task is not changing boostgroup, which can
		 * happen when the new hierarchy is in use.
		 */
		if (unlikely(dst_bg == src_bg)) {
			raw_spin_unlock(&bg->lock);
			unlock_rq_of(rq, task, &irq_flags);
			continue;
		}

		/*
		 * This is the case of a RUNNABLE task which is switching its
		 * current boost group.
		 */

		/* Move task from src to dst boost group */
		tasks = bg->group[src_bg].tasks - 1;
		bg->group[src_bg].tasks = max(0, tasks);
		bg->group[dst_bg].tasks += 1;

		raw_spin_unlock(&bg->lock);
		unlock_rq_of(rq, task, &irq_flags);

		/* Update CPU boost group */
		if (bg->group[src_bg].tasks == 0 || bg->group[dst_bg].tasks == 1)
			schedtune_cpu_update(task_cpu(task));

	}

	return 0;
}

void schedtune_cancel_attach(struct cgroup_taskset *tset)
{
	/* This can happen only if SchedTune controller is mounted with
	 * other hierarchies ane one of them fails. Since usually SchedTune is
	 * mouted on its own hierarcy, for the time being we do not implement
	 * a proper rollback mechanism */
	WARN(1, "SchedTune cancel attach not implemented");
}

/*
 * NOTE: This function must be called while holding the lock on the CPU RQ
 */
void schedtune_dequeue_task(struct task_struct *p, int cpu)
{
	struct boost_groups *bg = &per_cpu(cpu_boost_groups, cpu);
	unsigned long irq_flags;
	struct schedtune *st;
	int idx;

	if (!unlikely(schedtune_initialized))
		return;

	/*
	 * When a task is marked PF_EXITING by do_exit() it's going to be
	 * dequeued and enqueued multiple times in the exit path.
	 * Thus we avoid any further update, since we do not want to change
	 * CPU boosting while the task is exiting.
	 * The last dequeue is already enforce by the do_exit() code path
	 * via schedtune_exit_task().
	 */
	if (p->flags & PF_EXITING)
		return;

	/*
	 * Boost group accouting is protected by a per-cpu lock and requires
	 * interrupt to be disabled to avoid race conditions on...
	 */
	raw_spin_lock_irqsave(&bg->lock, irq_flags);
	rcu_read_lock();

	st = task_schedtune(p);
	idx = st->idx;

	schedtune_tasks_update(p, cpu, idx, DEQUEUE_TASK);

	rcu_read_unlock();
	raw_spin_unlock_irqrestore(&bg->lock, irq_flags);
}

void schedtune_exit_task(struct task_struct *tsk)
{
	struct schedtune *st;
	struct rq_flags irq_flags;
	unsigned int cpu;
	struct rq *rq;
	int idx;

	if (!unlikely(schedtune_initialized))
		return;

	rq = lock_rq_of(tsk, &irq_flags);
	rcu_read_lock();

	cpu = cpu_of(rq);
	st = task_schedtune(tsk);
	idx = st->idx;
	schedtune_tasks_update(tsk, cpu, idx, DEQUEUE_TASK);

	rcu_read_unlock();
	unlock_rq_of(rq, tsk, &irq_flags);
}

int schedtune_cpu_boost(int cpu)
{
	struct boost_groups *bg;

	bg = &per_cpu(cpu_boost_groups, cpu);
	return bg->boost_max;
}

int schedtune_task_boost(struct task_struct *p)
{
	struct schedtune *st;
	int task_boost;
#ifdef CONFIG_PROVE_LOCKING
	int lockdep_off = 0;
#endif

	if (!unlikely(schedtune_initialized))
		return 0;

	/* Get task boost value */
#ifdef CONFIG_PROVE_LOCKING
	lockdep_off = close_lockdep_if_cpu_offline();
#endif
	rcu_read_lock();
	st = task_schedtune(p);
	task_boost = st->boost;
	rcu_read_unlock();
#ifdef CONFIG_PROVE_LOCKING
	open_lockdep_if_need(lockdep_off);
#endif

	return task_boost;
}

int schedtune_cpu_capacity_min(int cpu)
{
	struct boost_groups *bg;

	bg = &per_cpu(cpu_boost_groups, cpu);
	return bg->max_capacity_min;
}

int schedtune_task_capacity_min(struct task_struct *p)
{
	struct schedtune *st;
	int task_capacity_min;

	if (!unlikely(schedtune_initialized))
		return 0;

	/* Get task boost value */
	rcu_read_lock();
	st = task_schedtune(p);
	task_capacity_min = st->capacity_min;
	rcu_read_unlock();

	return task_capacity_min;
}

int schedtune_prefer_idle(struct task_struct *p)
{
	struct schedtune *st;
	int prefer_idle;
#ifdef CONFIG_PROVE_LOCKING
	int lockdep_off = 0;
#endif

	if (!unlikely(schedtune_initialized))
		return 0;

	/* Get prefer_idle value */
#ifdef CONFIG_PROVE_LOCKING
	lockdep_off = close_lockdep_if_cpu_offline();
#endif
	rcu_read_lock();
	st = task_schedtune(p);
	prefer_idle = st->prefer_idle;
	rcu_read_unlock();
#ifdef CONFIG_PROVE_LOCKING
	open_lockdep_if_need(lockdep_off);
#endif

	return prefer_idle;
}

static u64
prefer_idle_read(struct cgroup_subsys_state *css, struct cftype *cft)
{
	struct schedtune *st = css_st(css);

	return st->prefer_idle;
}

static int
prefer_idle_write(struct cgroup_subsys_state *css, struct cftype *cft,
	    u64 prefer_idle)
{
	struct schedtune *st = css_st(css);
	st->prefer_idle = prefer_idle;

#if MET_STUNE_DEBUG
	/* top-app */
	if (st->idx == 3)
		met_tag_oneshot(0, "sched_user_top_prefer_idle",
				st->prefer_idle);
#endif

	return 0;
}

static u64
capacity_min_read(struct cgroup_subsys_state *css, struct cftype *cft)
{
	struct schedtune *st = css_st(css);

	return st->capacity_min;
}

static int
capacity_min_write(struct cgroup_subsys_state *css, struct cftype *cft,
		u64 capacity_min)
{
	struct schedtune *st = css_st(css);

	if (capacity_min < 0 || capacity_min > 1024) {
		printk_deferred("warning: capacity_min should be 0~1024\n");
		return -EINVAL;
	}

#ifdef CONFIG_CPU_FREQ_GOV_SCHEDPLUS
	set_cap_min_freq(capacity_min);
#endif

	rcu_read_lock();
	st->capacity_min = capacity_min;
	/* Update CPU capacity_min */
	schedtune_boostgroup_update_capacity_min(st->idx, st->capacity_min);
	rcu_read_unlock();

#if MET_STUNE_DEBUG
	/* top-app */
	if (st->idx == 3)
		met_tag_oneshot(0, "sched_boost_user_top_capacity_min",
				st->capacity_min);
#endif

	return 0;
}

static s64
boost_read(struct cgroup_subsys_state *css, struct cftype *cft)
{
	struct schedtune *st = css_st(css);

	return st->boost;
}

static int
boost_write(struct cgroup_subsys_state *css, struct cftype *cft,
	    s64 boost)
{
	struct schedtune *st = css_st(css);
	unsigned threshold_idx;
	int boost_pct;
	bool dvfs_on_demand = false;
	int ctl_no = div64_s64(boost, 1000);
	int cluster;
#ifdef CONFIG_CPU_FREQ_GOV_SCHEDPLUS
	int floor = 0;
	int c0, c1, i;
#endif
	int cap_min = 0;

	switch (ctl_no) {
	case 4:
		/* min cpu capacity request */
		boost -= ctl_no * 1000;

		/* update capacity_min */
		if (boost < 0 || boost > 100) {
			printk_deferred("warning: boost for capacity_min should be 0~100\n");
			if (boost > 100)
				boost = 100;
			else if (boost < 0)
				boost = 0;
		}
		cap_min = div64_s64(boost * 1024, 100);

#ifdef CONFIG_CPU_FREQ_GOV_SCHEDPLUS
		set_cap_min_freq(cap_min);
#endif
		rcu_read_lock();
		st->capacity_min = cap_min;
		/* Update CPU capacity_min */
		schedtune_boostgroup_update_capacity_min(
					st->idx, st->capacity_min);
		rcu_read_unlock();

#if MET_STUNE_DEBUG
		/* top-app */
		if (st->idx == 3)
			met_tag_oneshot(0, "sched_boost_user_top_capacity_min",
					st->capacity_min);
#endif

		/* boost4xxx: no boost only capacity_min */
		boost = 0;

		break;
	case 3:
		/* a floor of cpu frequency */
		boost -= ctl_no * 1000;
		cluster = (int)boost / 100;
		boost = (int)boost % 100;
#ifdef CONFIG_CPU_FREQ_GOV_SCHEDPLUS
		if (cluster > 0 && cluster <= 0x2) { /* only two cluster */
			floor = 1;
			c0 = cluster & 0x1;
			c1 = cluster & 0x2;

			/* cluster 0 */
			if (c0)
				set_min_boost_freq(boost, 0);
			else
				min_boost_freq[0] = 0;

			/* cluster 1 */
			if (c1)
				set_min_boost_freq(boost, 1);
			else
				min_boost_freq[1] = 0;
		}
#endif
		break;
	case 2:
		/* dvfs short cut */
		boost -= 2000;
		dvfs_on_demand = true;
		break;
	case 1:
		/* boost all tasks */
		boost -= 1000;
		break;
	case 0:
		/* boost big tasks only */
		break;
	default:
		printk_deferred("warning: perf ctrl no should be 0~1\n");
		return -EINVAL;
	}

	if (boost < -100 || boost > 100) {
		printk_deferred("warning: perf boost value should be -100~100\n");
		return -EINVAL;
	}

#ifdef CONFIG_CPU_FREQ_GOV_SCHEDPLUS
	for (i = 0; i < cpu_cluster_nr; i++) {
		if (!floor)
			min_boost_freq[i] = 0;
		if (!cap_min)
			cap_min_freq[i] = 0;
#if MET_STUNE_DEBUG
		met_tag_oneshot(0, met_dvfs_info2[i], min_boost_freq[i]);
		met_tag_oneshot(0, met_dvfs_info3[i], cap_min_freq[i]);
#endif
	}
#endif /* CONFIG_CPU_FREQ_GOV_SCHEDPLUS */

	if (!cap_min) {
		rcu_read_lock();
		st->capacity_min = 0;

		/* Update CPU capacity_min */
		schedtune_boostgroup_update_capacity_min(
				st->idx, st->capacity_min);
		rcu_read_unlock();

#if MET_STUNE_DEBUG
		/* top-app */
		if (st->idx == 3)
			met_tag_oneshot(0, "sched_boost_user_top_capacity_min",
					st->capacity_min);
#endif
	}

	global_negative_flag = false;

	boost_pct = boost;

	/*
	 * Update threshold params for Performance Boost (B)
	 * and Performance Constraint (C) regions.
	 * The current implementatio uses the same cuts for both
	 * B and C regions.
	 */
	threshold_idx = clamp(boost_pct, 0, 99) / 10;
	st->perf_boost_idx = threshold_idx;
	st->perf_constrain_idx = threshold_idx;

	st->boost = boost;

	sys_boosted = boost;

	if (css == &root_schedtune.css) {
		sysctl_sched_cfs_boost = boost;
		perf_boost_idx  = threshold_idx;
		perf_constrain_idx  = threshold_idx;
	}

	/* Update CPU boost */
	schedtune_boostgroup_update(st->idx, st->boost);

#ifdef CONFIG_CPU_FREQ_GOV_SCHEDPLUS
	if (dvfs_on_demand)
		update_freq_fastpath();
#endif

	trace_sched_tune_config(st->boost);

#if MET_STUNE_DEBUG
	/* user: foreground */
	if (st->idx == 1)
		met_tag_oneshot(0, "sched_boost_user_fg", st->boost);
	/* user: top-app */
	if (st->idx == 3)
		met_tag_oneshot(0, "sched_boost_user_top", st->boost);
#endif

	return 0;
}

#ifdef CONFIG_UCLAMP_TASK_GROUP
static inline u64 cpu_uclamp_read(struct cgroup_subsys_state *css,
				  enum uclamp_id clamp_id)
{
	struct schedtune *st;
	u64 util_clamp;

	rcu_read_lock();
	st = css_st(css);
	util_clamp = st->uclamp[clamp_id].value;
	rcu_read_unlock();

	return scale_to_percent(util_clamp);
}

static u64 cpu_util_min_read_u64(struct cgroup_subsys_state *css,
				 struct cftype *cft)
{
	return cpu_uclamp_read(css, UCLAMP_MIN);
}

static int cpu_util_min_write_u64(struct cgroup_subsys_state *css,
				  struct cftype *cftype, u64 min_value)
{
	struct uclamp_se *uc_se;
	struct schedtune *st;
	int ret = -EINVAL;

	/* Check range and scale to internal representation */
	if (min_value > 100)
		return -ERANGE;

	min_value =  scale_from_percent(min_value);
#ifdef CONFIG_MTK_UNIFY_POWER
	min_value = search_opp_cappacity(min_value);
#endif

	mutex_lock(&uclamp_mutex);
	rcu_read_lock();

	st = css_st(css);
	if (st->uclamp[UCLAMP_MIN].value == min_value) {
		ret = 0;
		goto out;
	}

	/* Update TG's reference count */
	uc_se = &st->uclamp[UCLAMP_MIN];
	ret = uclamp_group_get(NULL, css, UCLAMP_MIN, uc_se, min_value);

out:
	rcu_read_unlock();
	mutex_unlock(&uclamp_mutex);

	return ret;
}

unsigned long uclamp_ts_min(struct task_struct *task)
{
	return task_schedtune(task)->uclamp[UCLAMP_MIN].value;
}
#else
unsigned long uclamp_ts_min(struct task_struct *task)
{
	return 0;
}

#endif

static struct cftype files[] = {
	{
		.name = "boost",
		.read_s64 = boost_read,
		.write_s64 = boost_write,
	},
	{
		.name = "prefer_idle",
		.read_u64 = prefer_idle_read,
		.write_u64 = prefer_idle_write,
	},
	{
		.name = "capacity_min",
		.read_u64 = capacity_min_read,
		.write_u64 = capacity_min_write,
	},
#ifdef CONFIG_UCLAMP_TASK_GROUP
	{
		.name = "uclamp_min",
		.read_u64 = cpu_util_min_read_u64,
		.write_u64 = cpu_util_min_write_u64,
	},
#endif
	{ }	/* terminate */
};

static void
schedtune_boostgroup_init(struct schedtune *st, int idx)
{
	struct boost_groups *bg;
	int cpu;

	/* Initialize per CPUs boost group support */
	for_each_possible_cpu(cpu) {
		bg = &per_cpu(cpu_boost_groups, cpu);
		bg->group[idx].boost = 0;
		bg->group[idx].valid = true;
	}

	/* Keep track of allocated boost groups */
	allocated_group[idx] = st;
	st->idx = idx;
}

#if defined(CONFIG_UCLAMP_TASK_GROUP)
/**
 * alloc_uclamp_sched_group: initialize a new TG's for utilization clamping
 * @st: the newly created schedtune
 * @parent: its parent schedtune
 *
 * A newly created schedtuen inherits its utilization clamp values, for all
 * clamp indexes, from its parent schedtune.
 * This ensures that its values are properly initialized and that the task
 * group is accounted in the same parent's group index.
 *
 * Return: !0 on error
 */
static inline int alloc_uclamp_sched_group(struct schedtune *st,
					   struct schedtune *parent)
{
	struct uclamp_se *uc_se;
	int clamp_id;
	int ret = 1;

	for (clamp_id = 0; clamp_id < UCLAMP_CNT; ++clamp_id) {
		uc_se = &st->uclamp[clamp_id];

		uc_se->value = parent->uclamp[clamp_id].value;
		uc_se->group_id = UCLAMP_NONE;

		if (uclamp_group_get(NULL, NULL, clamp_id, uc_se,
				     parent->uclamp[clamp_id].value)) {
			ret = 0;
			goto out;
		}
	}

out:
	return ret;
}

/**
 * release_uclamp_sched_group: release utilization clamp references of a TG
 * @st: the schedtune being removed
 *
 * An empty schedtune can be removed only when it has no more tasks or child
 * groups. This means that we can also safely release all the reference
 * counting to clamp groups.
 */
static inline void free_uclamp_sched_group(struct schedtune *st)
{
	struct uclamp_se *uc_se;
	int clamp_id;

	for (clamp_id = 0; clamp_id < UCLAMP_CNT; ++clamp_id) {
		uc_se = &st->uclamp[clamp_id];
		uclamp_group_put(clamp_id, uc_se->group_id);
	}
}
#else /* CONFIG_UCLAMP_TASK_GROUP */
static inline void free_uclamp_sched_group(struct schedtune *tg) { }
static inline int alloc_uclamp_sched_group(struct schedtune *st,
					   struct schedtune *parent)
{
	return 1;
}
#endif

static struct cgroup_subsys_state *
schedtune_css_alloc(struct cgroup_subsys_state *parent_css)
{
	struct schedtune *st;
	int idx;

	if (!parent_css)
		return &root_schedtune.css;

	/* Allow only single level hierachies */
	if (parent_css != &root_schedtune.css) {
		pr_err("Nested SchedTune boosting groups not allowed\n");
		return ERR_PTR(-ENOMEM);
	}

	/* Allow only a limited number of boosting groups */
	for (idx = 1; idx < BOOSTGROUPS_COUNT; ++idx)
		if (!allocated_group[idx])
			break;
	if (idx == BOOSTGROUPS_COUNT) {
		pr_err("Trying to create more than %d SchedTune boosting groups\n",
		       BOOSTGROUPS_COUNT);
		return ERR_PTR(-ENOSPC);
	}

	st = kzalloc(sizeof(*st), GFP_KERNEL);
	if (!st)
		goto out;

	/* Initialize per CPUs boost group support */
	schedtune_boostgroup_init(st, idx);

	if (!alloc_uclamp_sched_group(st, css_st(parent_css)))
		goto err;

	return &st->css;

err:
	kfree(st);

out:
	return ERR_PTR(-ENOMEM);
}

static void
schedtune_boostgroup_release(struct schedtune *st)
{
	struct boost_groups *bg;
	int cpu;

	/* Reset per CPUs boost group support */
	for_each_possible_cpu(cpu) {
		bg = &per_cpu(cpu_boost_groups, cpu);
		bg->group[st->idx].valid = false;
		bg->group[st->idx].boost = 0;
		bg->group[st->idx].capacity_min = 0;
	}

	/* Keep track of allocated boost groups */
	allocated_group[st->idx] = NULL;
}

static void
schedtune_css_free(struct cgroup_subsys_state *css)
{
	struct schedtune *st = css_st(css);

	free_uclamp_sched_group(st);
	/* Release per CPUs boost group support */
	schedtune_boostgroup_release(st);
	kfree(st);
}

struct cgroup_subsys schedtune_cgrp_subsys = {
	.css_alloc	= schedtune_css_alloc,
	.css_free	= schedtune_css_free,
	.can_attach     = schedtune_can_attach,
	.cancel_attach  = schedtune_cancel_attach,
	.legacy_cftypes	= files,
	.early_init	= 1,
};

static inline void
schedtune_init_cgroups(void)
{
	struct boost_groups *bg;
	int cpu;

	/* Initialize the per CPU boost groups */
	for_each_possible_cpu(cpu) {
		bg = &per_cpu(cpu_boost_groups, cpu);
		memset(bg, 0, sizeof(struct boost_groups));
		bg->group[0].valid = true;
		raw_spin_lock_init(&bg->lock);
	}

	pr_info("schedtune: configured to support %d boost groups\n",
		BOOSTGROUPS_COUNT);

	schedtune_initialized = true;
}

#else /* CONFIG_CGROUP_SCHEDTUNE */

int
schedtune_accept_deltas(int nrg_delta, int cap_delta,
			struct task_struct *task)
{
	/* Optimal (O) region */
	if (nrg_delta < 0 && cap_delta > 0) {
		trace_sched_tune_filter(nrg_delta, cap_delta, 0, 0, 1, 0);
		return INT_MAX;
	}

	/* Suboptimal (S) region */
	if (nrg_delta > 0 && cap_delta < 0) {
		trace_sched_tune_filter(nrg_delta, cap_delta, 0, 0, -1, 5);
		return -INT_MAX;
	}

	return __schedtune_accept_deltas(nrg_delta, cap_delta,
			perf_boost_idx, perf_constrain_idx);
}

#endif /* CONFIG_CGROUP_SCHEDTUNE */

int
sysctl_sched_cfs_boost_handler(struct ctl_table *table, int write,
			       void __user *buffer, size_t *lenp,
			       loff_t *ppos)
{
	int ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);
	unsigned threshold_idx;
	int boost_pct;

	if (ret || !write)
		return ret;

	if (sysctl_sched_cfs_boost < -100 || sysctl_sched_cfs_boost > 100)
		return -EINVAL;
	boost_pct = sysctl_sched_cfs_boost;

	/*
	 * Update threshold params for Performance Boost (B)
	 * and Performance Constraint (C) regions.
	 * The current implementatio uses the same cuts for both
	 * B and C regions.
	 */
	threshold_idx = clamp(boost_pct, 0, 99) / 10;
	perf_boost_idx = threshold_idx;
	perf_constrain_idx = threshold_idx;

	return 0;
}

#ifdef CONFIG_SCHED_DEBUG
static void
schedtune_test_nrg(unsigned long delta_pwr)
{
	unsigned long test_delta_pwr;
	unsigned long test_norm_pwr;
	int idx;

	/*
	 * Check normalization constants using some constant system
	 * energy values
	 */
	pr_info("schedtune: verify normalization constants...\n");
	for (idx = 0; idx < 6; ++idx) {
		test_delta_pwr = delta_pwr >> idx;

		/* Normalize on max energy for target platform */
		test_norm_pwr = reciprocal_divide(
					test_delta_pwr << SCHED_CAPACITY_SHIFT,
					schedtune_target_nrg.rdiv);

		pr_info("schedtune: max_pwr/2^%d: %4lu => norm_pwr: %5lu\n",
			idx, test_delta_pwr, test_norm_pwr);
	}
}
#else
#define schedtune_test_nrg(delta_pwr)
#endif

void show_ste_info(void)
{
	struct target_nrg *ste = &schedtune_target_nrg;
	int cluster_nr = arch_get_nr_clusters();
	int i;
	struct cpumask cluster_cpus;
	int cluster_first_cpu_ste = 0;
	const struct sched_group_energy *core_eng_ste;
	int core_nr_cap_ste = 0;

	printk_deferred("STE: min=%lu max=%lu\n",
			ste->min_power, ste->max_power);

	for (i = 0; i < cluster_nr; i++) {
		arch_get_cluster_cpus(&cluster_cpus, i);
		cluster_first_cpu_ste = cpumask_first(&cluster_cpus);
		core_eng_ste = cpu_core_energy(cluster_first_cpu_ste);
		core_nr_cap_ste = core_eng_ste->nr_cap_states;
#ifdef CONFIG_MTK_UNIFY_POWER
		ste->max_dyn_pwr[i] =
		core_eng_ste->cap_states[core_nr_cap_ste - 1].dyn_pwr;
		ste->max_stc_pwr[i] =
		core_eng_ste->cap_states[core_nr_cap_ste - 1].lkg_pwr[0];

		printk_deferred("STE: cid=%d max_dync=%lu max_stc=%lu\n",
				i, ste->max_dyn_pwr[i], ste->max_stc_pwr[i]);
#else
		ste->max_pwr[i] =
		core_eng_ste->cap_states[core_nr_cap_ste - 1].power;

		printk_deferred("STE: cid=%d max_pwr=%lu\n",
			i, ste->max_pwr[i]);
#endif
	}
}

void show_pwr_info(void)
{
	char str[32];
#ifdef CONFIG_MTK_UNIFY_POWER
	unsigned long dyn_pwr;
	unsigned long stc_pwr;
#else
	unsigned long pwr;
#endif
	const struct sched_group_energy *clu_eng, *core_eng;
	int cpu;
	int cluster_first_cpu = 0;
	int clu_nr_cap = 0, core_nr_cap = 0;
	struct hmp_domain *domain;

	/* Get num of all clusters */
	for_each_hmp_domain_L_first(domain) {
		cluster_first_cpu = cpumask_first(&domain->possible_cpus);

		snprintf(str, 32, "CLUSTER[%*pbl]",
				cpumask_pr_args(&domain->possible_cpus));

		/*
		 * Get Cluster energy using EM data of first CPU
		 * in this cluster
		 */
		clu_eng = cpu_cluster_energy(cluster_first_cpu);
		clu_nr_cap = clu_eng->nr_cap_states;
#ifdef CONFIG_MTK_UNIFY_POWER
		dyn_pwr =
		clu_eng->cap_states[clu_nr_cap - 1].dyn_pwr;
		stc_pwr =
		clu_eng->cap_states[clu_nr_cap - 1].lkg_pwr[0];
		pr_info("pwr_tlb: %-17s dyn_pwr: %5lu stc_pwr: %5lu\n",
				str, dyn_pwr, stc_pwr);
#else
		pwr = clu_eng->cap_states[clu_nr_cap - 1].power;
		pr_info("pwr_tlb: %-17s pwr: %5lu\n",
				str, pwr);
#endif
		/* Get CPU energy using EM data for each CPU in this cluster */
		for_each_cpu(cpu, &domain->possible_cpus) {
			core_eng = cpu_core_energy(cpu);
			core_nr_cap = core_eng->nr_cap_states;
			snprintf(str, 32, "CPU[%d]", cpu);
#ifdef CONFIG_MTK_UNIFY_POWER
			dyn_pwr =
			core_eng->cap_states[core_nr_cap - 1].dyn_pwr;
			stc_pwr =
			core_eng->cap_states[core_nr_cap - 1].lkg_pwr[0];

			pr_info("pwr_tlb: %-17s dyn_pwr: %5lu stc_pwr: %5lu\n",
					str, dyn_pwr, stc_pwr);
#else
			pwr = core_eng->cap_states[core_nr_cap - 1].power;
			pr_info("pwr_tlb: %-17s pwr: %5lu\n",
					str, pwr);
#endif
		}
	}
}

#ifdef CONFIG_HOTPLUG_CPU
/*
 * mtk: Because system only eight cores online when init, we compute
 * the min/max power consumption of all possible clusters and CPUs.
 */
static void
schedtune_add_cluster_nrg_hotplug(struct target_nrg *ste)
{
	char str[32];
	unsigned long min_pwr;
	unsigned long max_pwr;
	const struct sched_group_energy *clu_eng, *core_eng;
	int cpu;
	int cluster_first_cpu = 0;
	int clu_nr_cap = 0, core_nr_cap = 0;
	int clu_nr_idle = 0, core_nr_idle = 0;
	struct hmp_domain *domain;

	/* Get num of all clusters */
	for_each_hmp_domain_L_first(domain) {
		cluster_first_cpu = cpumask_first(&domain->possible_cpus);

		snprintf(str, 32, "CLUSTER[%*pbl]",
			cpumask_pr_args(&domain->possible_cpus));

		/*
		 * Get Cluster energy using EM data of first CPU
		 * in this cluster.
		 */
		clu_eng = cpu_cluster_energy(cluster_first_cpu);
		clu_nr_cap = clu_eng->nr_cap_states;
		clu_nr_idle = clu_eng->nr_idle_states;
		min_pwr =
		clu_eng->idle_states[clu_nr_idle - 1].power;
#ifdef CONFIG_MTK_UNIFY_POWER
		max_pwr =
		(clu_eng->cap_states[clu_nr_cap - 1].dyn_pwr +
		   clu_eng->cap_states[clu_nr_cap - 1].lkg_pwr[0]);
#else
		max_pwr = clu_eng->cap_states[clu_nr_cap - 1].power;
#endif
		pr_info("schedtune: %-17s min_pwr: %5lu max_pwr: %5lu\n",
			str, min_pwr, max_pwr);

		ste->min_power += min_pwr;
		ste->max_power += max_pwr;

		/* Get CPU energy using EM data for each CPU in this cluster */
		for_each_cpu(cpu, &domain->possible_cpus) {
			core_eng = cpu_core_energy(cpu);
			core_nr_cap = core_eng->nr_cap_states;
			core_nr_idle = core_eng->nr_idle_states;
			min_pwr =
			core_eng->idle_states[core_nr_idle - 1].power;
#ifdef CONFIG_MTK_UNIFY_POWER
			max_pwr =
			(core_eng->cap_states[core_nr_cap - 1].dyn_pwr +
			core_eng->cap_states[core_nr_cap - 1].lkg_pwr[0]);
#else
			max_pwr =
			core_eng->cap_states[core_nr_cap - 1].power;
#endif
			ste->min_power += min_pwr;
			ste->max_power += max_pwr;

			snprintf(str, 32, "CPU[%d]", cpu);
			pr_info("schedtune: %-17s min_pwr: %5lu max_pwr: %5lu\n",
				str, min_pwr, max_pwr);
		}
	}
}
#else
/*
 * Compute the min/max power consumption of a cluster and all its CPUs
 */
static void
schedtune_add_cluster_nrg(
		struct sched_domain *sd,
		struct sched_group *sg,
		struct target_nrg *ste)
{
	struct sched_domain *sd2;
	struct sched_group *sg2;

	struct cpumask *cluster_cpus;
	char str[32];

	unsigned long min_pwr;
	unsigned long max_pwr;
	int cpu;

	/* Get Cluster energy using EM data for the first CPU */
	cluster_cpus = sched_group_cpus(sg);
	snprintf(str, 32, "CLUSTER[%*pbl]",
		 cpumask_pr_args(cluster_cpus));

	min_pwr = sg->sge->idle_states[sg->sge->nr_idle_states - 1].power;
#ifdef CONFIG_MTK_UNIFY_POWER
	max_pwr = sg->sge->cap_states[sg->sge->nr_cap_states - 1].dyn_pwr +
		sg->sge->cap_states[sg->sge->nr_cap_states - 1].lkg_pwr[0];
#else
	max_pwr = sg->sge->cap_states[sg->sge->nr_cap_states - 1].power;
#endif
	pr_info("schedtune: %-17s min_pwr: %5lu max_pwr: %5lu\n",
		str, min_pwr, max_pwr);

	/*
	 * Keep track of this cluster's energy in the computation of the
	 * overall system energy
	 */
	ste->min_power += min_pwr;
	ste->max_power += max_pwr;

	/* Get CPU energy using EM data for each CPU in the group */
	for_each_cpu(cpu, cluster_cpus) {
		/* Get a SD view for the specific CPU */
		for_each_domain(cpu, sd2) {
			/* Get the CPU group */
			sg2 = sd2->groups;
			min_pwr = sg2->sge->idle_states[sg2->sge->nr_idle_states - 1].power;
#ifdef CONFIG_MTK_UNIFY_POWER
			max_pwr = sg2->sge->cap_states[sg2->sge->nr_cap_states - 1].dyn_pwr +
				sg2->sge->cap_states[sg2->sge->nr_cap_states - 1].lkg_pwr[0];
#else
			max_pwr = sg2->sge->cap_states[sg2->sge->nr_cap_states - 1].power;
#endif

			ste->min_power += min_pwr;
			ste->max_power += max_pwr;

			snprintf(str, 32, "CPU[%d]", cpu);
			pr_info("schedtune: %-17s min_pwr: %5lu max_pwr: %5lu\n",
				str, min_pwr, max_pwr);

			/*
			 * Assume we have EM data only at the CPU and
			 * the upper CLUSTER level
			 */
			BUG_ON(!cpumask_equal(
				sched_group_cpus(sg),
				sched_group_cpus(sd2->parent->groups)
				));
			break;
		}
	}
}
#endif /* define CONFIG_HOTPLUG_CPU */

/*
 * Initialize the constants required to compute normalized energy.
 * The values of these constants depends on the EM data for the specific
 * target system and topology.
 * Thus, this function is expected to be called by the code
 * that bind the EM to the topology information.
 */
static int
schedtune_init(void)
{
	struct target_nrg *ste = &schedtune_target_nrg;
	unsigned long delta_pwr = 0;
#ifdef CONFIG_HOTPLUG_CPU
	const struct sched_group_energy *sge_core;
	struct hmp_domain *domain;
	int cluster_first_cpu = 0;
#else
	struct sched_domain *sd;
	struct sched_group *sg;
#endif
	int i;

	pr_info("schedtune: init normalization constants...\n");
	ste->max_power = 0;
	ste->min_power = 0;
#ifdef CONFIG_MTK_UNIFY_POWER
	memset(ste->max_dyn_pwr, 0, sizeof(ste->max_dyn_pwr));
	memset(ste->max_stc_pwr, 0, sizeof(ste->max_stc_pwr));
#else
	memset(ste->max_pwr, 0, sizeof(ste->max_pwr));
#endif
	raw_spin_lock_init(&stune_lock);

	rcu_read_lock();

	/*
	 * When EAS is in use, we always have a pointer to the highest SD
	 * which provides EM data.
	 */
#ifdef CONFIG_HOTPLUG_CPU
	for_each_hmp_domain_L_first(domain) {
		cluster_first_cpu = cpumask_first(&domain->possible_cpus);
		/* lowest capacity CPU in system */
		sge_core = cpu_core_energy(cluster_first_cpu);
		if (!sge_core) {
			pr_info("schedtune: no energy model data\n");
			goto nodata;
		}
		default_stune_threshold = sge_core->cap_states[0].cap;

		if (default_stune_threshold) {
			set_stune_task_threshold(-1);
			break;
		}
	}
#else
	sd = rcu_dereference(per_cpu(sd_ea, cpumask_first(cpu_online_mask)));

	if (!sd) {
		pr_info("schedtune: no energy model data\n");
		goto nodata;
	}

	sg = sd->groups;

	default_stune_threshold = sg->sge->cap_states[0].cap;
#endif

#ifdef CONFIG_HOTPLUG_CPU
	/*
	 * mtk: compute max_power & min_power of all possible cores,
	 * not only online cores.
	 */
	schedtune_add_cluster_nrg_hotplug(ste);
#else
	do {
		schedtune_add_cluster_nrg(sd, sg, ste);
	} while (sg = sg->next, sg != sd->groups);
#endif
	rcu_read_unlock();

	pr_info("schedtune: %-17s min_pwr: %5lu max_pwr: %5lu\n",
		"SYSTEM", ste->min_power, ste->max_power);

	/* Get capacity & freq information */
	cpu_cluster_nr = arch_get_nr_clusters();

	for (i = 0; i < cpu_cluster_nr ; i++) {
		struct cpumask cluster_cpus;
		int first_cpu;
		const struct sched_group_energy *pwr_tlb;

		arch_get_cluster_cpus(&cluster_cpus, i);
		first_cpu = cpumask_first(&cluster_cpus);
		pwr_tlb = cpu_core_energy(first_cpu);

		schedtune_target_cap[i].cap =
			pwr_tlb->cap_states[pwr_tlb->nr_cap_states - 1].cap;
#ifdef CONFIG_CPU_FREQ_GOV_SCHEDPLUS
		schedtune_target_cap[i].freq = mt_cpufreq_get_freq_by_idx(i, 0);
#endif
	}

	/* Compute normalization constants */
	delta_pwr = ste->max_power - ste->min_power;
	if (delta_pwr > 0)
		ste->rdiv = reciprocal_value(delta_pwr);
	else {
		ste->rdiv.m = 0;
		ste->rdiv.sh1 = 0;
		ste->rdiv.sh2 = 0;
	}
	pr_info("schedtune: using normalization constants mul: %u sh1: %u sh2: %u\n",
		ste->rdiv.m, ste->rdiv.sh1, ste->rdiv.sh2);

	schedtune_test_nrg(delta_pwr);

#ifdef CONFIG_CGROUP_SCHEDTUNE
	schedtune_init_cgroups();
#else
	pr_info("schedtune: configured to support global boosting only\n");
#endif

	schedtune_spc_rdiv = reciprocal_value(100);

	return 0;

nodata:
	pr_warning("schedtune: disabled!\n");
	rcu_read_unlock();
	return -EINVAL;
}
late_initcall_sync(schedtune_init);
