// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifdef CONFIG_MTK_AEE_IPANIC
/* for turn on/off debug.c's log */
#define DEBUG 0

#include <linux/sched.h>
#include "mboot_params.h" /* aee's lib */
#include "sched.h"

/* sched: aee for sched/debug */
#define TRYLOCK_NUM 10
#include <linux/delay.h>
#include <linux/sched/signal.h>

/*
 * from kernel/sched/debug.c
 * Ease the printing of nsec fields:
 */
static long long nsec_high(unsigned long long nsec)
{
	if ((long long)nsec < 0) {
		nsec = -nsec;
		do_div(nsec, 1000000);
		return -nsec;
	}
	do_div(nsec, 1000000);

	return nsec;
}

static unsigned long nsec_low(unsigned long long nsec)
{
	if ((long long)nsec < 0)
		nsec = -nsec;

	return do_div(nsec, 1000000);
}

#define SPLIT_NS(x) (nsec_high(x), nsec_low(x))

#ifdef CONFIG_CGROUP_SCHED
static char group_path[PATH_MAX];

static char *task_group_path(struct task_group *tg)
{
	if (autogroup_path(tg, group_path, PATH_MAX))
		return group_path;

	cgroup_path(tg->css.cgroup, group_path, PATH_MAX);

	return group_path;
}
#endif

static DEFINE_SPINLOCK(sched_debug_lock);

static const char * const sched_tunable_scaling_names[] = {
	"none",
	"logaritmic",
	"linear"
}; /* kernel/sched/debug.c */

char print_at_AEE_buffer[160];

#define SEQ_printf_at_AEE(m, x...)		\
do {						\
	snprintf(print_at_AEE_buffer, sizeof(print_at_AEE_buffer), x);	\
	aee_sram_fiq_log(print_at_AEE_buffer);	\
} while (0)

static void
print_task_at_AEE(struct seq_file *m, struct rq *rq, struct task_struct *p)
{
#ifdef CONFIG_SCHEDSTATS
	if (rq->curr == p) {
#ifdef CONFIG_CGROUP_SCHED
		SEQ_printf_at_AEE(m, "R %15s %5d %9lld.%06ld %9lld ",
			p->comm,
			task_pid_nr(p),
			SPLIT_NS(p->se.vruntime),
			(long long)(p->nvcsw + p->nivcsw));

		SEQ_printf_at_AEE(m, "%5d ", p->prio);

		SEQ_printf_at_AEE(m, "%9lld.%06ld %9lld.%06ld %9lld.%06ld ",
			SPLIT_NS(p->se.statistics.wait_sum),
			SPLIT_NS(p->se.sum_exec_runtime),
			SPLIT_NS(p->se.statistics.sum_sleep_runtime));

		SEQ_printf_at_AEE(m, "%s\n ", task_group_path(task_group(p)));

#else
		SEQ_printf_at_AEE(m, "R %15s %5d %9lld.%06ld %9lld ",
			p->comm,
			task_pid_nr(p),
			SPLIT_NS(p->se.vruntime),
			(long long)(p->nvcsw + p->nivcsw));

		SEQ_printf_at_AEE(m, "%5d ", p->prio);

		SEQ_printf_at_AEE(m, "%9lld.%06ld %9lld.%06ld %9lld.%06ld ",
			SPLIT_NS(p->se.statistics.wait_sum),
			SPLIT_NS(p->se.sum_exec_runtime),
			SPLIT_NS(p->se.statistics.sum_sleep_runtime));
#endif
#ifdef CONFIG_NUMA_BALANCING
	SEQ_printf_at_AEE(m, " %d %d", task_node(p), task_numa_group_id(p));
#endif
	} else {
#ifdef CONFIG_CGROUP_SCHED
		SEQ_printf_at_AEE(m, "R %15s %5d %9lld.%06ld %9lld ",
			p->comm,
			task_pid_nr(p),
			SPLIT_NS(p->se.vruntime),
			(long long)(p->nvcsw + p->nivcsw));

		SEQ_printf_at_AEE(m, "%5d ", p->prio);

		SEQ_printf_at_AEE(m, "%9lld.%06ld %9lld.%06ld %9lld.%06ld ",
			SPLIT_NS(p->se.statistics.wait_sum),
			SPLIT_NS(p->se.sum_exec_runtime),
			SPLIT_NS(p->se.statistics.sum_sleep_runtime));

		SEQ_printf_at_AEE(m, "%s\n ", task_group_path(task_group(p)));
#else
		SEQ_printf_at_AEE(m, "R %15s %5d %9lld.%06ld %9lld ",
			p->comm,
			task_pid_nr(p),
			SPLIT_NS(p->se.vruntime),
			(long long)(p->nvcsw + p->nivcsw));
		SEQ_printf_at_AEE(m, "%5d ", p->prio);

		SEQ_printf_at_AEE(m, "%9lld.%06ld %9lld.%06ld %9lld.%06ld ",
			SPLIT_NS(p->se.statistics.wait_sum),
			SPLIT_NS(p->se.sum_exec_runtime),
			SPLIT_NS(p->se.statistics.sum_sleep_runtime));
#endif
#ifdef CONFIG_NUMA_BALANCING
	SEQ_printf_at_AEE(m, " %d %d", task_node(p), task_numa_group_id(p));
#endif
	}
#else
	SEQ_printf_at_AEE(m, "%9lld.%06ld %9lu %9lld.%06ld",
		0LL, 0L,
		SPLIT_NS(p->se.sum_exec_runtime),
		0LL, 0L);
#endif
}

/* sched: add aee log */
#define read_trylock_irqsave(lock, flags) \
	({ \
	 typecheck(unsigned long, flags); \
	 local_irq_save(flags); \
	 read_trylock(lock) ? \
	 1 : ({ local_irq_restore(flags); 0; }); \
	 })

int read_trylock_n_irqsave(rwlock_t *lock,
		unsigned long *flags, struct seq_file *m, char *msg)
{
	int locked, trylock_cnt = 0;

	do {
		locked = read_trylock_irqsave(lock, *flags);
		trylock_cnt++;
		mdelay(10);
	} while ((!locked) && (trylock_cnt < TRYLOCK_NUM));

	if (!locked) {
#ifdef CONFIG_DEBUG_SPINLOCK
		struct task_struct *owner = NULL;
#endif
		SEQ_printf_at_AEE(m, "Warning: fail to get lock in %s\n", msg);
#ifdef CONFIG_DEBUG_SPINLOCK
		if (lock->owner && lock->owner != SPINLOCK_OWNER_INIT)
			owner = lock->owner;
#ifdef CONFIG_SMP
		SEQ_printf_at_AEE(m, " lock: %p, .magic: %08x, .owner: %s/%d",
				lock, lock->magic,
				owner ? owner->comm : "<<none>>",
				owner ? task_pid_nr(owner) : -1);
#ifdef CONFIG_ARM64
		SEQ_printf_at_AEE(m, ".owner_cpu: %d, value: %d\n",
			lock->owner_cpu, lock->raw_lock.wait_lock.locked);
#else
		SEQ_printf_at_AEE(m, ".owner_cpu: %d, value: %d\n",
			   lock->owner_cpu, lock->lock);
#endif

#else
		SEQ_printf_at_AEE(m, " lock: %p, .magic: %08x, .owner: %s/%d",
			   lock, lock->magic,
			   owner ? owner->comm : "<<none>>",
			   owner ? task_pid_nr(owner) : -1);
		SEQ_printf_at_AEE(m, ".owner_cpu: %d\n", lock->owner_cpu);
#endif
#endif
	}

	return locked;
}

int raw_spin_trylock_n_irqsave(raw_spinlock_t *lock,
		unsigned long *flags, struct seq_file *m, char *msg)
{
	int locked, trylock_cnt = 0;

	do {
		locked = raw_spin_trylock_irqsave(lock, *flags);
		trylock_cnt++;
		mdelay(10);
	} while ((!locked) && (trylock_cnt < TRYLOCK_NUM));

	if (!locked) {
#ifdef CONFIG_DEBUG_SPINLOCK
		struct task_struct *owner = NULL;
#endif
		SEQ_printf_at_AEE(m, "Warning: fail to get lock in %s\n", msg);
#ifdef CONFIG_DEBUG_SPINLOCK
		if (lock->owner && lock->owner != SPINLOCK_OWNER_INIT)
			owner = lock->owner;
#ifdef CONFIG_ARM64
#ifdef CONFIG_SMP
		SEQ_printf_at_AEE(m, " lock: %lx, .magic: %08x, .owner: %s/%d",
			   (long)lock, lock->magic,
			   owner ? owner->comm : "<<none>>",
			   owner ? task_pid_nr(owner) : -1);
		SEQ_printf_at_AEE(m, ".owner_cpu: %d",
			   lock->owner_cpu);
#else
		SEQ_printf_at_AEE(m, " lock: %lx, .magic: %08x, .owner: %s/%d",
			   (long)lock, lock->magic,
			   owner ? owner->comm : "<<none>>",
			   owner ? task_pid_nr(owner) : -1);
		SEQ_printf_at_AEE(m, ".owner_cpu: %d, value: %d\n",
			   lock->owner_cpu, lock->raw_lock.slock);
#endif
#else
		SEQ_printf_at_AEE(m, " lock: %x, .magic: %08x, .owner: %s/%d",
			   (int)lock, lock->magic,
				owner ? owner->comm : "<<none>>",
				owner ? task_pid_nr(owner) : -1);
		SEQ_printf_at_AEE(m, ".owner_cpu: %d, value: %d\n",
				lock->owner_cpu, lock->raw_lock.slock);
#endif
#endif
	}

	return locked;
}

int spin_trylock_n_irqsave(spinlock_t *lock,
		unsigned long *flags, struct seq_file *m, char *msg)
{
	int locked, trylock_cnt = 0;

	do {
		locked = spin_trylock_irqsave(lock, *flags);
		trylock_cnt++;
		mdelay(10);

	} while ((!locked) && (trylock_cnt < TRYLOCK_NUM));

	if (!locked) {
#ifdef CONFIG_DEBUG_SPINLOCK
		raw_spinlock_t rlock = lock->rlock;
		struct task_struct *owner = NULL;
#endif
		SEQ_printf_at_AEE(m, "Warning: fail to get lock in %s\n", msg);
#ifdef CONFIG_DEBUG_SPINLOCK
		if (rlock.owner && rlock.owner != SPINLOCK_OWNER_INIT)
			owner = rlock.owner;
#ifdef CONFIG_ARM64
#ifdef CONFIG_SMP
		SEQ_printf_at_AEE(m, " lock: %lx, .magic: %08x, .owner: %s/%d",
			   (long)&rlock, rlock.magic,
			   owner ? owner->comm : "<<none>>",
			   owner ? task_pid_nr(owner) : -1);
		SEQ_printf_at_AEE(m, " .owner_cpu: %d, pending: %u",
			   rlock.owner_cpu,
			   rlock.raw_lock.pending);
#else
		SEQ_printf_at_AEE(m, " lock: %lx, .magic: %08x, .owner: %s/%d",
			   (long)&rlock, rlock.magic,
			   owner ? owner->comm : "<<none>>",
			   owner ? task_pid_nr(owner) : -1);
		SEQ_printf_at_AEE(m, ".owner_cpu: %d, value: %d\n",
			   rlock.owner_cpu, rlock.raw_lock.slock);
#endif
#else
		SEQ_printf_at_AEE(m, " lock: %x, .magic: %08x, .owner: %s/%d",
			   (int)&rlock, rlock.magic,
			   owner ? owner->comm : "<<none>>",
			   owner ? task_pid_nr(owner) : -1);
		SEQ_printf_at_AEE(m, ".owner_cpu: %d, value: %d\n",
			    rlock.owner_cpu, rlock.raw_lock.slock);
#endif
#endif
	}

	return locked;
}
static void print_rq_at_AEE(struct seq_file *m, struct rq *rq, int rq_cpu)
{
	struct task_struct *g, *p;

	SEQ_printf_at_AEE(m, "\n");
	SEQ_printf_at_AEE(m, "runnable tasks:\n");
	SEQ_printf_at_AEE(m,
	"            task   PID         tree-key  switches  prio     wait-time             sum-exec        sum-sleep\n");
	SEQ_printf_at_AEE(m, "---------------------------------------------------\n");

	rcu_read_lock();
	for_each_process_thread(g, p) {
		/*
		 * if (task_cpu(p) != rq_cpu)
		 * sched: only output the runnable tasks,
		 * rather than ALL tasks in runqueues
		 */
		if (!p->on_rq || task_cpu(p) != rq_cpu)
			continue;

		print_task_at_AEE(m, rq, p);
	}
	rcu_read_unlock();
}

#ifdef CONFIG_FAIR_GROUP_SCHED
static void print_cfs_group_stats_at_AEE(struct seq_file *m,
		int cpu, struct task_group *tg)
{
	struct sched_entity *se = tg->se[cpu];

#define P(F)		SEQ_printf_at_AEE(m, "  .%-30s: %lld\n",	#F, (long long)F)
#define P_SCHEDSTAT(F)	SEQ_printf_at_AEE(m, "  .%-30s: %lld\n",	#F, (long long)schedstat_val(F))
#define PN(F)		SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", #F, SPLIT_NS((long long)F))
#define PN_SCHEDSTAT(F)	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", #F, SPLIT_NS((long long)schedstat_val(F)))

	if (!se)
		return;

	PN(se->exec_start);
	PN(se->vruntime);
	PN(se->sum_exec_runtime);
	if (schedstat_enabled()) {
		PN_SCHEDSTAT(se->statistics.wait_start);
		PN_SCHEDSTAT(se->statistics.sleep_start);
		PN_SCHEDSTAT(se->statistics.block_start);
		PN_SCHEDSTAT(se->statistics.sleep_max);
		PN_SCHEDSTAT(se->statistics.block_max);
		PN_SCHEDSTAT(se->statistics.exec_max);
		PN_SCHEDSTAT(se->statistics.slice_max);
		PN_SCHEDSTAT(se->statistics.wait_max);
		PN_SCHEDSTAT(se->statistics.wait_sum);
		P_SCHEDSTAT(se->statistics.wait_count);
	}
	P(se->load.weight);
	P(se->runnable_weight);
#ifdef CONFIG_SMP
	P(se->avg.load_avg);
	P(se->avg.util_avg);
	P(se->avg.runnable_load_avg);
#endif
#undef PN
#undef P
}
#endif

void print_cfs_rq_at_AEE(struct seq_file *m, int cpu, struct cfs_rq *cfs_rq)
{
	s64 MIN_vruntime = -1, min_vruntime, max_vruntime = -1,
		spread, rq0_min_vruntime, spread0;
	struct rq *rq = cpu_rq(cpu);
	struct sched_entity *last;
	unsigned long flags;
	int locked;
#ifdef CONFIG_FAIR_GROUP_SCHED
	SEQ_printf_at_AEE(m, "\n");
	SEQ_printf_at_AEE(m, "cfs_rq[%d]:%s\n", cpu, task_group_path(cfs_rq->tg));
#else
	SEQ_printf_at_AEE(m, "\n");
	SEQ_printf_at_AEE(m, "cfs_rq[%d]:\n", cpu);
#endif
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", "exec_clock",
			SPLIT_NS(cfs_rq->exec_clock));

	locked = raw_spin_trylock_n_irqsave(&rq->lock,
			&flags, m, "print_cfs_rq_at_AEE");
	if (rb_first_cached(&cfs_rq->tasks_timeline))
		MIN_vruntime = (__pick_first_entity(cfs_rq))->vruntime;
	last = __pick_last_entity(cfs_rq);
	if (last)
		max_vruntime = last->vruntime;
	min_vruntime = cfs_rq->min_vruntime;
	rq0_min_vruntime = cpu_rq(0)->cfs.min_vruntime;
	if (locked)
		raw_spin_unlock_irqrestore(&rq->lock, flags);
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", "MIN_vruntime",
			SPLIT_NS(MIN_vruntime));
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", "min_vruntime",
			SPLIT_NS(min_vruntime));
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", "max_vruntime",
			SPLIT_NS(max_vruntime));

	spread = max_vruntime - MIN_vruntime;
#ifdef DEBUG
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", "spread",
			SPLIT_NS(spread));
#endif
	spread0 = min_vruntime - rq0_min_vruntime;
#ifdef DEBUG
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", "spread0",
		SPLIT_NS(spread0));
	SEQ_printf_at_AEE(m, "  .%-30s: %d\n", "nr_spread_over",
		cfs_rq->nr_spread_over);
#endif

	SEQ_printf_at_AEE(m, "  .%-30s: %d\n",
			"nr_running", cfs_rq->nr_running);
	SEQ_printf_at_AEE(m, "  .%-30s: %ld\n", "load", cfs_rq->load.weight);
#ifdef CONFIG_SMP
	SEQ_printf_at_AEE(m, "  .%-30s: %ld\n", "runnable_weight", cfs_rq->runnable_weight);

	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", "load_avg",
			cfs_rq->avg.load_avg);
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", "runnable_load_avg",
			cfs_rq->avg.runnable_load_avg);
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", "util_avg",
			cfs_rq->avg.util_avg);
	SEQ_printf_at_AEE(m, "  .%-30s: %u\n", "util_est_enqueued",
			cfs_rq->avg.util_est.enqueued);
	SEQ_printf_at_AEE(m, "  .%-30s: %ld\n", "removed.load_avg",
			cfs_rq->removed.load_avg);
	SEQ_printf_at_AEE(m, "  .%-30s: %ld\n", "removed.util_avg",
			cfs_rq->removed.util_avg);
	SEQ_printf_at_AEE(m, "  .%-30s: %ld\n", "removed.runnable_sum",
			cfs_rq->removed.runnable_sum);

#ifdef CONFIG_FAIR_GROUP_SCHED
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", "tg_load_avg_contrib",
			cfs_rq->tg_load_avg_contrib);
	SEQ_printf_at_AEE(m, "  .%-30s: %ld\n", "tg_load_avg",
			atomic_long_read(&cfs_rq->tg->load_avg));
#endif
#endif
#ifdef CONFIG_CFS_BANDWIDTH
	SEQ_printf_at_AEE(m, "  .%-30s: %d\n", "throttled",
			cfs_rq->throttled);
	SEQ_printf_at_AEE(m, "  .%-30s: %d\n", "throttle_count",
			cfs_rq->throttle_count);
#endif

#ifdef CONFIG_FAIR_GROUP_SCHED
	print_cfs_group_stats_at_AEE(m, cpu, cfs_rq->tg);
#endif
}

#define for_each_leaf_cfs_rq(rq, cfs_rq) \
	list_for_each_entry_rcu(cfs_rq, &rq->leaf_cfs_rq_list, leaf_cfs_rq_list)


void print_cfs_stats_at_AEE(struct seq_file *m, int cpu)
{
	struct cfs_rq *cfs_rq;

	rcu_read_lock();
	cfs_rq = &cpu_rq(cpu)->cfs;
	/* sched: only output / cgroup schedule info */
	print_cfs_rq_at_AEE(m, cpu, cfs_rq);
	rcu_read_unlock();
}

void print_rt_rq_at_AEE(struct seq_file *m, int cpu, struct rt_rq *rt_rq)
{
#ifdef CONFIG_RT_GROUP_SCHED
	SEQ_printf_at_AEE(m, "\n");
	SEQ_printf_at_AEE(m, "rt_rq[%d]:%s\n", cpu, task_group_path(rt_rq->tg));
#else
	SEQ_printf_at_AEE(m, "\n");
	SEQ_printf_at_AEE(m, "rt_rq[%d]:\n", cpu);
#endif


#define P(x) \
	SEQ_printf_at_AEE(m, "  .%-30s: %lld\n", #x, (long long)(rt_rq->x))
#define PU(x) \
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", #x, (unsigned long)(rt_rq->x))
#define PN(x) \
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", #x, SPLIT_NS(rt_rq->x))

	P(rt_nr_running);
#ifdef CONFIG_SMP
	PU(rt_nr_migratory);
#endif

	P(rt_throttled);
	PN(rt_time);
	PN(rt_runtime);

#undef PN
#undef PU
#undef P
}


#ifdef CONFIG_RT_GROUP_SCHED

static inline struct task_group *next_task_group(struct task_group *tg)
{
	do {
		tg = list_entry_rcu(tg->list.next,
			typeof(struct task_group), list);
	} while (&tg->list != &task_groups && task_group_is_autogroup(tg));

	if (&tg->list == &task_groups)
		tg = NULL;

	return tg;
}

#define for_each_rt_rq(rt_rq, iter, rq)					\
	for (iter = container_of(&task_groups, typeof(*iter), list);	\
		(iter = next_task_group(iter)) &&			\
		(rt_rq = iter->rt_rq[cpu_of(rq)]);)

#else /* !CONFIG_RT_GROUP_SCHED */

#define for_each_rt_rq(rt_rq, iter, rq) \
	for ((void) iter, rt_rq = &rq->rt; rt_rq; rt_rq = NULL)

#endif

void print_rt_stats_at_AEE(struct seq_file *m, int cpu)
{
	struct rt_rq *rt_rq;

	rt_rq = &cpu_rq(cpu)->rt;

	rcu_read_lock();
	/* sched: only output / cgroup schedule info */
	print_rt_rq_at_AEE(m, cpu, rt_rq);
	rcu_read_unlock();
}

void print_dl_rq_at_AEE(struct seq_file *m, int cpu, struct dl_rq *dl_rq)
{
	struct dl_bw *dl_bw;

	SEQ_printf_at_AEE(m, "\n");
	SEQ_printf_at_AEE(m, "dl_rq[%d]:\n", cpu);
	SEQ_printf_at_AEE(m, "  .%-30s: %ld\n",
		"dl_nr_running", dl_rq->dl_nr_running);

#define PU(x) \
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", #x, (unsigned long)(dl_rq->x))

	PU(dl_nr_running);
#ifdef CONFIG_SMP
	PU(dl_nr_migratory);
	dl_bw = &cpu_rq(cpu)->rd->dl_bw;
#else
	dl_bw = &dl_rq->dl_bw;
#endif
	SEQ_printf_at_AEE(m, "  .%-30s: %lld\n", "dl_bw->bw", dl_bw->bw);
	SEQ_printf_at_AEE(m, "  .%-30s: %lld\n", "dl_bw->total_bw", dl_bw->total_bw);

#undef PU
}

void print_dl_stats_at_AEE(struct seq_file *m, int cpu)
{
	print_dl_rq_at_AEE(m, cpu, &cpu_rq(cpu)->dl);
}

static void print_cpu_at_AEE(struct seq_file *m, int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long flags;
	int locked;

#ifdef CONFIG_X86
	{
		unsigned int freq = cpu_khz ? : 1;

		SEQ_printf_at_AEE(m, "cpu#%d, %u.%03u MHz\n",
			   cpu, freq / 1000, (freq % 1000));
	}
#else
	/* sched: add cpu info */
	SEQ_printf_at_AEE(m, "cpu#%d: %s\n", cpu,
			cpu_is_offline(cpu) ? "Offline" : "Online");
#endif

#define P(x) \
do { \
	if (sizeof(rq->x) == 4) \
		SEQ_printf_at_AEE(m, "  .%-30s: %ld\n", \
		#x, (long)(rq->x)); \
	else \
		SEQ_printf_at_AEE(m, "  .%-30s: %lld\n", \
		#x, (long long)(rq->x)); \
} while (0)

#define PN(x) \
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", #x, SPLIT_NS(rq->x))

	P(nr_running);
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", "load",
		   rq->load.weight);
#ifdef DEBUG
	P(nr_switches);
#endif
	P(nr_load_updates);
	P(nr_uninterruptible);
	PN(next_balance);
	SEQ_printf_at_AEE(m, "  .%-30s: %ld\n",
			"curr->pid", (long)(task_pid_nr(rq->curr)));
	PN(clock);
	SEQ_printf_at_AEE(m, "  .%-30s: %ld %ld %ld %ld %ld\n", "cpu_load",
			(long)(rq->cpu_load[0]),
			(long)(rq->cpu_load[1]),
			(long)(rq->cpu_load[2]),
			(long)(rq->cpu_load[3]),
			(long)(rq->cpu_load[4]));

#undef P
#undef PN

#ifdef CONFIG_SMP
#define P64(n) SEQ_printf_at_AEE(m, "  .%-30s: %lld\n", #n, rq->n)
	P64(avg_idle);
	P64(max_idle_balance_cost);
#undef P64
#endif

#define P(n) SEQ_printf_at_AEE(m, "  .%-30s: %d\n", #n, schedstat_val(rq->n))
	if (schedstat_enabled()) {
		P(yld_count);
		P(sched_count);
		P(sched_goidle);
		P(ttwu_count);
		P(ttwu_local);
	}
#undef P

	locked = spin_trylock_n_irqsave(&sched_debug_lock,
			&flags, m, "print_cpu_at_AEE");
	print_cfs_stats_at_AEE(m, cpu);
	print_rt_stats_at_AEE(m, cpu);
	print_dl_stats_at_AEE(m, cpu);

	rcu_read_lock();
	print_rq_at_AEE(m, rq, cpu);
	SEQ_printf_at_AEE(m, "============================================\n");
	rcu_read_unlock();

	if (locked)
		spin_unlock_irqrestore(&sched_debug_lock, flags);
}

static void sched_debug_header_at_AEE(struct seq_file *m)
{
	u64 sched_clk, cpu_clk;
	unsigned long flags;

	local_irq_save(flags);
	sched_clk = sched_clock();
	cpu_clk = local_clock();
	local_irq_restore(flags);

	SEQ_printf_at_AEE(m, "Sched Debug Version: v0.11, %s %.*s\n",
		init_utsname()->release,
		(int)strcspn(init_utsname()->version, " "),
		init_utsname()->version);

#define P(x) \
	SEQ_printf_at_AEE(m, "%-40s: %lld\n", #x, (long long)(x))
#define PN(x) \
	SEQ_printf_at_AEE(m, "%-40s: %lu\n", #x, SPLIT_NS(x))
	PN(sched_clk);
	PN(cpu_clk);
	P(jiffies);
#ifdef CONFIG_HAVE_UNSTABLE_SCHED_CLOCK
	P(sched_clock_stable());
#endif
#undef PN
#undef P

	SEQ_printf_at_AEE(m, "\n");
	SEQ_printf_at_AEE(m, "sysctl_sched\n");

#define P(x) \
	SEQ_printf_at_AEE(m, "  .%-40s: %lld\n", #x, (long long)(x))
#define PN(x) \
	SEQ_printf_at_AEE(m, "  .%-40s: %lu\n", #x, SPLIT_NS(x))
	PN(sysctl_sched_latency);
	PN(sysctl_sched_min_granularity);
	PN(sysctl_sched_wakeup_granularity);
	P(sysctl_sched_child_runs_first);
	P(sysctl_sched_features);
#undef PN
#undef P

	SEQ_printf_at_AEE(m, "  .%-40s: %d (%s)\n",
		"sysctl_sched_tunable_scaling",
		sysctl_sched_tunable_scaling,
		sched_tunable_scaling_names[sysctl_sched_tunable_scaling]);
	SEQ_printf_at_AEE(m, "\n");
}

void sysrq_sched_debug_show_at_AEE(void)
{
	int cpu;
	unsigned long flags;
	int locked;

	sched_debug_header_at_AEE(NULL);
	locked = read_trylock_n_irqsave(&tasklist_lock,
			&flags, NULL, "sched_debug_show_at_AEE");

	for_each_possible_cpu(cpu) {
		print_cpu_at_AEE(NULL, cpu);
	}
	if (locked)
		read_unlock_irqrestore(&tasklist_lock, flags);
}

#endif /* end CONFIG_MTK_AEE_IPANIC */
