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

bool schedtune_initialized = false;
extern struct reciprocal_value schedtune_spc_rdiv;

/* We hold schedtune boost in effect for at least this long */
#define SCHEDTUNE_BOOST_HOLD_NS 50000000ULL

/*
 * EAS scheduler tunables for task groups.
 */

/* SchdTune tunables for a group of tasks */
struct schedtune {
	/* SchedTune CGroup subsystem */
	struct cgroup_subsys_state css;

	/* Boost group allocated ID */
	int idx;

	/* Boost value for tasks on that SchedTune CGroup */
	int boost;

	/* Hint to bias scheduling of tasks on that SchedTune CGroup
	 * towards idle CPUs */
	int prefer_idle;

#ifdef CONFIG_UCLAMP_TASK_GROUP
	/* Task utilization clamping */
	struct			uclamp_se uclamp[UCLAMP_CNT];
#endif
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

#ifdef CONFIG_UCLAMP_TASK_GROUP
struct uclamp_se *task_schedtune_uclamp(struct task_struct *tsk, int clamp_id)
{
	struct schedtune *st;

	rcu_read_lock();
	st = task_schedtune(tsk);
	rcu_read_unlock();

	return &st->uclamp[clamp_id];
}
#endif

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
	.prefer_idle = 0,
};

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
#define BOOSTGROUPS_COUNT 10

/* Array of configured boostgroups */
static struct schedtune *allocated_group[BOOSTGROUPS_COUNT] = {
	&root_schedtune,
	NULL,
};

static inline bool is_group_idx_valid(int idx)
{
	return idx >= 0 && idx < BOOSTGROUPS_COUNT;
}

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
	bool idle;
	int boost_max;
	u64 boost_ts;
	struct {
		/* The boost for tasks on that boost group */
		int boost;
		/* Count of RUNNABLE tasks on that boost group */
		unsigned tasks;
		/* Timestamp of boost activation */
		u64 ts;
	} group[BOOSTGROUPS_COUNT];
	/* CPU's boost group locking */
	raw_spinlock_t lock;
};

/* Boost groups affecting each CPU in the system */
DEFINE_PER_CPU(struct boost_groups, cpu_boost_groups);

static inline bool schedtune_boost_timeout(u64 now, u64 ts)
{
	return ((now - ts) > SCHEDTUNE_BOOST_HOLD_NS);
}

static inline bool
schedtune_boost_group_active(int idx, struct boost_groups* bg, u64 now)
{
	if (bg->group[idx].tasks)
		return true;

	return !schedtune_boost_timeout(now, bg->group[idx].ts);
}

static void
schedtune_cpu_update(int cpu, u64 now)
{
	struct boost_groups *bg = &per_cpu(cpu_boost_groups, cpu);
	int boost_max;
	u64 boost_ts;
	int idx;

	/* The root boost group is always active */
	boost_max = bg->group[0].boost;
	boost_ts = now;
	for (idx = 1; idx < BOOSTGROUPS_COUNT; ++idx) {
		/*
		 * A boost group affects a CPU only if it has
		 * RUNNABLE tasks on that CPU or it has hold
		 * in effect from a previous task.
		 */
		if (!schedtune_boost_group_active(idx, bg, now))
			continue;

		/* This boost group is active */
		if (boost_max > bg->group[idx].boost)
			continue;

		boost_max = bg->group[idx].boost;
		boost_ts =  bg->group[idx].ts;
	}
	/* Ensures boost_max is non-negative when all cgroup boost values
	 * are neagtive. Avoids under-accounting of cpu capacity which may cause
	 * task stacking and frequency spikes.*/
	boost_max = max(boost_max, 0);
	bg->boost_max = boost_max;
	bg->boost_ts = boost_ts;
}

static int
schedtune_boostgroup_update(int idx, int boost)
{
	struct boost_groups *bg;
	int cur_boost_max;
	int old_boost;
	int cpu;
	u64 now;

	/* Update per CPU boost groups */
	for_each_possible_cpu(cpu) {
		bg = &per_cpu(cpu_boost_groups, cpu);

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
		now = sched_clock_cpu(cpu);
		if (boost > cur_boost_max &&
			schedtune_boost_group_active(idx, bg, now)) {
			bg->boost_max = boost;
			bg->boost_ts = bg->group[idx].ts;

			trace_sched_tune_boostgroup_update(cpu, 1, bg->boost_max);
			continue;
		}

		/* Check if this update has decreased current max */
		if (cur_boost_max == old_boost && old_boost > boost) {
			schedtune_cpu_update(cpu, now);
			trace_sched_tune_boostgroup_update(cpu, -1, bg->boost_max);
			continue;
		}

		trace_sched_tune_boostgroup_update(cpu, 0, bg->boost_max);
	}

	return 0;
}

#ifdef CONFIG_UCLAMP_TASK_GROUP
/**
 * cpu_util_update: update effective clamp
 * @css: the task group to update
 * @clamp_id: the clamp index to update
 * @group_id: the group index mapping the new task clamp value
 * @value: the new task group clamp value
 *
 * The effective clamp for a TG is expected to track the most restrictive
 * value between the ST's clamp value and it's parent effective clamp value.
 * This method achieve that:
 * 1. updating the current TG effective value
 * 2. walking all the descendant task group that needs an update
 *
 * A ST's effective clamp needs to be updated when its current value is not
 * matching the ST's clamp value. In this case indeed either:
 * a) the parent has got a more relaxed clamp value
 *    thus potentially we can relax the effective value for this group
 * b) the parent has got a more strict clamp value
 *    thus potentially we have to restrict the effective value of this group
 *
 * Restriction and relaxation of current ST's effective clamp values needs to
 * be propagated down to all the descendants. When a subgroup is found which
 * has already its effective clamp value matching its clamp value, then we can
 * safely skip all its descendants which are granted to be already in sync.
 *
 * The ST's group_id is also updated to ensure it tracks the effective clamp
 * value.
 */
static void cpu_util_update(struct cgroup_subsys_state *css,
				 unsigned int clamp_id, unsigned int group_id,
				 unsigned int value)
{
	struct uclamp_se *uc_se;

	uc_se = &css_st(css)->uclamp[clamp_id];
	uc_se->effective.value = value;
	uc_se->effective.group_id = group_id;
}
/*
 * free_uclamp_sched_group: release utilization clamp references of a TG
 * @st: the schetune being removed
 *
 * An empty task group can be removed only when it has no more tasks or child
 * groups. This means that we can also safely release all the reference
 * counting to clamp groups.
 */
static inline void free_uclamp_sched_group(struct schedtune *st)
{
	int clamp_id;

	for (clamp_id = 0; clamp_id < UCLAMP_CNT; ++clamp_id)
		uclamp_group_put(clamp_id, st->uclamp[clamp_id].group_id);
}

/**
 * alloc_uclamp_sched_group: initialize a new ST's for utilization clamping
 * @st: the newly created schedtune
 *
 * A newly created schedtune inherits its utilization clamp values, for all
 * clamp indexes, from its parent task group.
 * This ensures that its values are properly initialized and that the task
 * group is accounted in the same parent's group index.
 *
 * Return: 0 on error
 */
static inline int alloc_uclamp_sched_group(struct schedtune *st)
{
	struct uclamp_se *uc_se;
	int clamp_id;

	for (clamp_id = 0; clamp_id < UCLAMP_CNT; ++clamp_id) {
		uc_se = &st->uclamp[clamp_id];
		uclamp_group_get(NULL, NULL, &st->uclamp[clamp_id],
				 clamp_id, uclamp_none(clamp_id));
		uc_se->effective.value = uc_se->value;
		uc_se->effective.group_id = uc_se->group_id;
	}

	return 1;
}

static int cpu_util_min_write_u64(struct cgroup_subsys_state *css,
				  struct cftype *cftype, u64 min_value)
{
	struct schedtune *st;
	int ret = 0;

	if (min_value > SCHED_CAPACITY_SCALE)
		return -ERANGE;

	if (!opp_capacity_tbl_ready)
		init_opp_capacity_tbl();

	min_value = find_fit_capacity(min_value);

	mutex_lock(&uclamp_mutex);
	rcu_read_lock();

	st = css_st(css);
	if (st->uclamp[UCLAMP_MIN].value == min_value)
		goto out;
	if (st->uclamp[UCLAMP_MAX].value < min_value) {
		ret = -EINVAL;
		goto out;
	}

	/* Update ST's reference count */
	uclamp_group_get(NULL, css, &st->uclamp[UCLAMP_MIN],
			 UCLAMP_MIN, min_value);

	/* Update effective clamps to track the most restrictive value */
	cpu_util_update(css, UCLAMP_MIN, st->uclamp[UCLAMP_MIN].group_id,
			     min_value);
out:
	rcu_read_unlock();
	mutex_unlock(&uclamp_mutex);

	return ret;
}

static int cpu_util_min_pct_write_u64(struct cgroup_subsys_state *css,
				  struct cftype *cftype, u64 pct)
{
	u64 min_value;

	if (pct < 0 || pct > 100)
		return -ERANGE;

	min_value = scale_from_percent(pct);
	return cpu_util_min_write_u64(css, cftype, min_value);
}

static int cpu_util_max_write_u64(struct cgroup_subsys_state *css,
				  struct cftype *cftype, u64 max_value)
{
	struct schedtune *st;
	int ret = 0;

	if (max_value > SCHED_CAPACITY_SCALE)
		return -ERANGE;

	if (!opp_capacity_tbl_ready)
		init_opp_capacity_tbl();

	max_value = find_fit_capacity(max_value);

	mutex_lock(&uclamp_mutex);
	rcu_read_lock();

	st = css_st(css);
	if (st->uclamp[UCLAMP_MAX].value == max_value)
		goto out;
	if (st->uclamp[UCLAMP_MIN].value > max_value) {
		ret = -EINVAL;
		goto out;
	}

	/* Update ST's reference count */
	uclamp_group_get(NULL, css, &st->uclamp[UCLAMP_MAX],
			 UCLAMP_MAX, max_value);

	/* Update effective clamps to track the most restrictive value */
	cpu_util_update(css, UCLAMP_MAX, st->uclamp[UCLAMP_MAX].group_id,
			     max_value);
out:
	rcu_read_unlock();
	mutex_unlock(&uclamp_mutex);

	return ret;
}

static int cpu_util_max_pct_write_u64(struct cgroup_subsys_state *css,
				  struct cftype *cftype, u64 pct)
{
	u64 max_value;

	if (pct < 0 || pct > 100)
		return -ERANGE;

	max_value = scale_from_percent(pct);
	return cpu_util_max_write_u64(css, cftype, max_value);
}

static inline u64 cpu_uclamp_read(struct cgroup_subsys_state *css,
				  enum uclamp_id clamp_id,
				  bool effective)
{
	struct schedtune *st;
	u64 util_clamp;

	rcu_read_lock();
	st = css_st(css);
	util_clamp = effective
		? st->uclamp[clamp_id].effective.value
		: st->uclamp[clamp_id].value;
	rcu_read_unlock();

	return util_clamp;
}

static u64 cpu_util_min_read_u64(struct cgroup_subsys_state *css,
				 struct cftype *cft)
{
	return cpu_uclamp_read(css, UCLAMP_MIN, false);
}

static u64 cpu_util_max_read_u64(struct cgroup_subsys_state *css,
				 struct cftype *cft)
{
	return cpu_uclamp_read(css, UCLAMP_MAX, false);
}

static u64 cpu_util_min_effective_read_u64(struct cgroup_subsys_state *css,
					   struct cftype *cft)
{
	return cpu_uclamp_read(css, UCLAMP_MIN, true);
}

static u64 cpu_util_max_effective_read_u64(struct cgroup_subsys_state *css,
					   struct cftype *cft)
{
	return cpu_uclamp_read(css, UCLAMP_MAX, true);
}
#else
static inline void free_uclamp_sched_group(struct schedtune *st) {}
static inline int alloc_uclamp_sched_group(struct schedtune *st)
{
	return 1;
}
#endif /* CONFIG_UCLAMP_TASK_GROUP */

#include "tune_plus.c"

#define ENQUEUE_TASK  1
#define DEQUEUE_TASK -1

static inline bool
schedtune_update_timestamp(struct task_struct *p)
{
	if (sched_feat(SCHEDTUNE_BOOST_HOLD_ALL))
		return true;

	return task_has_rt_policy(p);
}

static inline void
schedtune_tasks_update(struct task_struct *p, int cpu, int idx, int task_count)
{
	struct boost_groups *bg = &per_cpu(cpu_boost_groups, cpu);
	int tasks = bg->group[idx].tasks + task_count;

	/* Update boosted tasks count while avoiding to make it negative */
	bg->group[idx].tasks = max(0, tasks);

	/* Update timeout on enqueue */
	if (task_count > 0) {
		u64 now = sched_clock_cpu(cpu);

		if (schedtune_update_timestamp(p))
			bg->group[idx].ts = now;

		/* Boost group activation or deactivation on that RQ */
		if (bg->group[idx].tasks == 1)
			schedtune_cpu_update(cpu, now);
	}

	trace_sched_tune_tasks_update(p, cpu, tasks, idx,
			bg->group[idx].boost, bg->boost_max,
			bg->group[idx].ts);
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

	if (unlikely(!schedtune_initialized))
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
	struct rq_flags rq_flags;
	unsigned int cpu;
	struct rq *rq;
	int src_bg; /* Source boost group index */
	int dst_bg; /* Destination boost group index */
	int tasks;
	u64 now;

	if (unlikely(!schedtune_initialized))
		return 0;


	cgroup_taskset_for_each(task, css, tset) {

		/*
		 * Lock the CPU's RQ the task is enqueued to avoid race
		 * conditions with migration code while the task is being
		 * accounted
		 */
		rq = task_rq_lock(task, &rq_flags);

		if (!task->on_rq) {
			task_rq_unlock(rq, task, &rq_flags);
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
			task_rq_unlock(rq, task, &rq_flags);
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

		/* Update boost hold start for this group */
		now = sched_clock_cpu(cpu);
		bg->group[dst_bg].ts = now;

		/* Force boost group re-evaluation at next boost check */
		bg->boost_ts = now - SCHEDTUNE_BOOST_HOLD_NS;

		raw_spin_unlock(&bg->lock);
		task_rq_unlock(rq, task, &rq_flags);
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

	if (unlikely(!schedtune_initialized))
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

int schedtune_cpu_boost(int cpu)
{
	struct boost_groups *bg;
	u64 now;

	bg = &per_cpu(cpu_boost_groups, cpu);
	now = sched_clock_cpu(cpu);

	/* Check to see if we have a hold in effect */
	if (schedtune_boost_timeout(now, bg->boost_ts))
		schedtune_cpu_update(cpu, now);

	return bg->boost_max;
}

int schedtune_task_boost(struct task_struct *p)
{
	struct schedtune *st;
	int task_boost;

	if (unlikely(!schedtune_initialized))
		return 0;

	/* Get task boost value */
	rcu_read_lock();
	st = task_schedtune(p);
	task_boost = st->boost;
	rcu_read_unlock();

	return task_boost;
}

int schedtune_prefer_idle(struct task_struct *p)
{
	struct schedtune *st;
	int prefer_idle;

	if (unlikely(!schedtune_initialized))
		return 0;

	/* Get prefer_idle value */
	rcu_read_lock();
	st = task_schedtune(p);
	prefer_idle = st->prefer_idle;
	rcu_read_unlock();

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
	st->prefer_idle = !!prefer_idle;

#if MET_STUNE_DEBUG
	/* user: foreground */
	if (st->idx == 1)
		met_tag_oneshot(0, "sched_user_prefer_idle_fg",
				st->prefer_idle);
	/* user: top-app */
	if (st->idx == 3)
		met_tag_oneshot(0, "sched_user_prefer_idle_top",
				st->prefer_idle);
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

	if (boost < 0 || boost > 100)
		return -EINVAL;

	st->boost = boost;

	/* Update CPU boost */
	schedtune_boostgroup_update(st->idx, st->boost);

#if MET_STUNE_DEBUG
	/* user: foreground */
	if (st->idx == 1)
		met_tag_oneshot(0, "sched_user_boost_fg", st->boost);
	/* user: top-app */
	if (st->idx == 3)
		met_tag_oneshot(0, "sched_user_boost_top", st->boost);
#endif

	return 0;
}

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
#if defined(CONFIG_UCLAMP_TASK_GROUP)
	{
		.name = "util.min",
		.read_u64 = cpu_util_min_read_u64,
		.write_u64 = cpu_util_min_write_u64,
	},
	{
		.name = "util.min.pct",
		.write_u64 = cpu_util_min_pct_write_u64,
	},
	{
		.name = "util.min.effective",
		.read_u64 = cpu_util_min_effective_read_u64,
	},
	{
		.name = "util.max",
		.read_u64 = cpu_util_max_read_u64,
		.write_u64 = cpu_util_max_write_u64,
	},
	{
		.name = "util.max.pct",
		.write_u64 = cpu_util_max_pct_write_u64,
	},
	{
		.name = "util.max.effective",
		.read_u64 = cpu_util_max_effective_read_u64,
	},
#endif
	{ }	/* terminate */
};

static int
schedtune_boostgroup_init(struct schedtune *st)
{
	struct boost_groups *bg;
	int cpu;

	/* Keep track of allocated boost groups */
	allocated_group[st->idx] = st;

	/* Initialize the per CPU boost groups */
	for_each_possible_cpu(cpu) {
		bg = &per_cpu(cpu_boost_groups, cpu);
		bg->group[st->idx].boost = 0;
		bg->group[st->idx].tasks = 0;
		bg->group[st->idx].ts = 0;
	}

	return 0;
}

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
	st->idx = idx;
	if (schedtune_boostgroup_init(st))
		goto release;

	if (!alloc_uclamp_sched_group(st))
		goto release;

	return &st->css;

release:
	kfree(st);
out:
	return ERR_PTR(-ENOMEM);
}

static void
schedtune_boostgroup_release(struct schedtune *st)
{
	/* Reset this boost group */
	schedtune_boostgroup_update(st->idx, 0);

	/* Keep track of allocated boost groups */
	allocated_group[st->idx] = NULL;
}

static void
schedtune_css_free(struct cgroup_subsys_state *css)
{
	struct schedtune *st = css_st(css);

	schedtune_boostgroup_release(st);
	free_uclamp_sched_group(st);
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

#ifdef CONFIG_UCLAMP_TASK_GROUP
void schedtune_init_uclamp(void)
{
	struct uclamp_se *uc_se;
	unsigned int clamp_id;

	for (clamp_id = 0; clamp_id < UCLAMP_CNT; ++clamp_id) {
		/* Init root ST's clamp group */
		uc_se = &root_schedtune.uclamp[clamp_id];
		uclamp_group_get(NULL, NULL, uc_se, clamp_id,
				 uclamp_none(clamp_id));
		uc_se->effective.group_id = uc_se->group_id;
		uc_se->effective.value = uc_se->value;
	}

}
#endif

static inline void
schedtune_init_cgroups(void)
{
	struct boost_groups *bg;
	int cpu;

	/* Initialize the per CPU boost groups */
	for_each_possible_cpu(cpu) {
		bg = &per_cpu(cpu_boost_groups, cpu);
		memset(bg, 0, sizeof(struct boost_groups));
		raw_spin_lock_init(&bg->lock);
	}

	pr_info("schedtune: configured to support %d boost groups\n",
		BOOSTGROUPS_COUNT);

	schedtune_initialized = true;
}

/*
 * Initialize the cgroup structures
 */
static int
schedtune_init(void)
{
	/* set default threshold */
	calculate_default_stune_threshold();

	schedtune_spc_rdiv = reciprocal_value(100);
	schedtune_init_cgroups();

	return 0;
}
postcore_initcall(schedtune_init);
