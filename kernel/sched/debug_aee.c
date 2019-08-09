/*
 * Copyright (C) 2016 MediaTek Inc.
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

#include <linux/sched.h>
#include "mtk_ram_console.h"
#ifdef CONFIG_MTK_RT_THROTTLE_MON
#include "mtk_rt_mon.h"
#endif

/* sched: aee for sched/debug */
/* #define TEST_SCHED_DEBUG_ENHANCEMENT */
#define TRYLOCK_NUM 10
#include <linux/delay.h>

char print_at_AEE_buffer[160];
/* sched: add rt_exec_task info */
DECLARE_PER_CPU(u64, rt_throttling_start);
DECLARE_PER_CPU(u64, exec_delta_time);
DECLARE_PER_CPU(u64, clock_task);
DECLARE_PER_CPU(u64, exec_start);
DECLARE_PER_CPU(struct task_struct, exec_task);
DECLARE_PER_CPU(u64, old_rt_time);
DECLARE_PER_CPU(u64, init_rt_time);
DECLARE_PER_CPU(u64, rt_period_time);

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
		SEQ_printf_at_AEE(m, "R %15s %5d %9lld.%06ld %9lld %5d %9lld.%06ld %9lld.%06ld %9lld.%06ld %s\n",
			p->comm,
			task_pid_nr(p),
			SPLIT_NS(p->se.vruntime),
			(long long)(p->nvcsw + p->nivcsw),
			p->prio,
			SPLIT_NS(p->se.statistics.wait_sum),
			SPLIT_NS(p->se.sum_exec_runtime),
			SPLIT_NS(p->se.statistics.sum_sleep_runtime),
			task_group_path(task_group(p)));
#else
		SEQ_printf_at_AEE(m, "R %15s %5d %9lld.%06ld %9lld %5d %9lld.%06ld %9lld.%06ld %9lld.%06ld\n",
			p->comm,
			task_pid_nr(p),
			SPLIT_NS(p->se.vruntime),
			(long long)(p->nvcsw + p->nivcsw),
			p->prio,
			SPLIT_NS(p->se.statistics.wait_sum),
			SPLIT_NS(p->se.sum_exec_runtime),
			SPLIT_NS(p->se.statistics.sum_sleep_runtime));
#endif
	} else {
#ifdef CONFIG_CGROUP_SCHED
		SEQ_printf_at_AEE(m, "  %15s %5d %9lld.%06ld %9lld %5d %9lld.%06ld %9lld.%06ld %9lld.%06ld %s\n",
			p->comm,
			task_pid_nr(p),
			SPLIT_NS(p->se.vruntime),
			(long long)(p->nvcsw + p->nivcsw),
			p->prio,
			SPLIT_NS(p->se.statistics.wait_sum),
			SPLIT_NS(p->se.sum_exec_runtime),
			SPLIT_NS(p->se.statistics.sum_sleep_runtime),
			task_group_path(task_group(p)));
#else
		SEQ_printf_at_AEE(m, "  %15s %5d %9lld.%06ld %9lld %5d %9lld.%06ld %9lld.%06ld %9lld.%06ld\n",
			p->comm,
			task_pid_nr(p),
			SPLIT_NS(p->se.vruntime),
			(long long)(p->nvcsw + p->nivcsw),
			p->prio,
			SPLIT_NS(p->se.statistics.wait_sum),
			SPLIT_NS(p->se.sum_exec_runtime),
			SPLIT_NS(p->se.statistics.sum_sleep_runtime));
#endif
	}
#else
	SEQ_printf_at_AEE(m, "%9lld.%06ld %9lld.%06ld %9lld.%06ld",
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
		SEQ_printf_at_AEE(m, ".owner_cpu: %d, value: %d\n",
			   lock->owner_cpu, lock->raw_lock.lock);
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
		SEQ_printf_at_AEE(m, ".owner_cpu: %d, owner: %hu, next: %hu\n",
			   lock->owner_cpu,
			   lock->raw_lock.owner, lock->raw_lock.next);
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
		SEQ_printf_at_AEE(m, ".owner_cpu: %d, owner: %hu, next: %hu\n",
			   rlock.owner_cpu,
			   rlock.raw_lock.owner, rlock.raw_lock.next);
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

	SEQ_printf_at_AEE(m, "\nrunnable tasks:\n");
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

#define P(F) \
	SEQ_printf_at_AEE(m, "  .%-30s: %lld\n", #F, (long long)F)
#define PN(F) \
	SEQ_printf_at_AEE(m, "  .%-30s: %lld.%06ld\n", \
	#F, SPLIT_NS((long long)F))

	if (!se)
		return;

	PN(se->exec_start);
	PN(se->vruntime);
	PN(se->sum_exec_runtime);
#ifdef CONFIG_SCHEDSTATS
	PN(se->statistics.wait_start);
	PN(se->statistics.sleep_start);
	PN(se->statistics.block_start);
	PN(se->statistics.sleep_max);
	PN(se->statistics.block_max);
	PN(se->statistics.exec_max);
	PN(se->statistics.slice_max);
	PN(se->statistics.wait_max);
	PN(se->statistics.wait_sum);
	P(se->statistics.wait_count);
#endif
	P(se->load.weight);
#ifdef CONFIG_SMP
	P(se->avg.load_avg);
	P(se->avg.util_avg);
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
	SEQ_printf_at_AEE(m, "\ncfs_rq[%d]:%s\n",
			cpu, task_group_path(cfs_rq->tg));
#else
	SEQ_printf_at_AEE(m, "\ncfs_rq[%d]:\n", cpu);
#endif
	SEQ_printf_at_AEE(m, "  .%-30s: %lld.%06ld\n", "exec_clock",
			SPLIT_NS(cfs_rq->exec_clock));

	/*raw_spin_lock_irqsave(&rq->lock, flags);*/
	locked = raw_spin_trylock_n_irqsave(&rq->lock,
			&flags, m, "print_cfs_rq_at_AEE");
	if (cfs_rq->rb_leftmost)
		MIN_vruntime = (__pick_first_entity(cfs_rq))->vruntime;
	last = __pick_last_entity(cfs_rq);
	if (last)
		max_vruntime = last->vruntime;
	min_vruntime = cfs_rq->min_vruntime;
	rq0_min_vruntime = cpu_rq(0)->cfs.min_vruntime;
	if (locked)
		raw_spin_unlock_irqrestore(&rq->lock, flags);
	SEQ_printf_at_AEE(m, "  .%-30s: %lld.%06ld\n", "MIN_vruntime",
			SPLIT_NS(MIN_vruntime));
	SEQ_printf_at_AEE(m, "  .%-30s: %lld.%06ld\n", "min_vruntime",
			SPLIT_NS(min_vruntime));
	SEQ_printf_at_AEE(m, "  .%-30s: %lld.%06ld\n", "max_vruntime",
			SPLIT_NS(max_vruntime));
	spread = max_vruntime - MIN_vruntime;
	/*
	 * SEQ_printf_at_AEE(m, "  .%-30s: %Ld.%06ld\n", "spread",
	 *		SPLIT_NS(spread));
	 */
	spread0 = min_vruntime - rq0_min_vruntime;
	/*
	 * SEQ_printf_at_AEE(m, "  .%-30s: %Ld.%06ld\n", "spread0",
	 *		SPLIT_NS(spread0));
	 * SEQ_printf_at_AEE(m, "  .%-30s: %d\n", "nr_spread_over",
	 *		cfs_rq->nr_spread_over);
	 */
	SEQ_printf_at_AEE(m, "  .%-30s: %d\n",
			"nr_running", cfs_rq->nr_running);
	SEQ_printf_at_AEE(m, "  .%-30s: %ld\n", "load", cfs_rq->load.weight);
#ifdef CONFIG_SMP
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", "load_avg",
			cfs_rq->avg.load_avg);
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", "runnable_load_avg",
			cfs_rq->runnable_load_avg);
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", "util_avg",
			cfs_rq->avg.util_avg);
	SEQ_printf_at_AEE(m, "  .%-30s: %ld\n", "removed_load_avg",
			atomic_long_read(&cfs_rq->removed_load_avg));
	SEQ_printf_at_AEE(m, "  .%-30s: %ld\n", "removed_util_avg",
			atomic_long_read(&cfs_rq->removed_util_avg));
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
	/*sched: only output / cgroup schedule info*/
	print_cfs_rq_at_AEE(m, cpu, cfs_rq);
	rcu_read_unlock();
}

void print_rt_rq_at_AEE(struct seq_file *m, int cpu, struct rt_rq *rt_rq)
{
#ifdef CONFIG_RT_GROUP_SCHED
	int cpu_rq_throttle = rq_cpu(rt_rq->rq);
	SEQ_printf_at_AEE(m, "\nrt_rq[%d]:%s\n",
			cpu, task_group_path(rt_rq->tg));
#else
	SEQ_printf_at_AEE(m, "\nrt_rq[%d]:\n", cpu);
#endif

#define P(x) \
	SEQ_printf_at_AEE(m, "  .%-30s: %lld\n", #x, (long long)(rt_rq->x))
#define PN(x) \
	SEQ_printf_at_AEE(m, "  .%-30s: %lld.%06ld\n", #x, SPLIT_NS(rt_rq->x))

	P(rt_nr_running);
	P(rt_throttled);

	SEQ_printf_at_AEE(m, "  exec_task[%d:%s], prio=%d\n",
			per_cpu(exec_task, cpu).pid,
			per_cpu(exec_task, cpu).comm,
			per_cpu(exec_task, cpu).prio);
#ifdef CONFIG_RT_GROUP_SCHED
	SEQ_printf_at_AEE(m, "  .rt_throttling_start   : [%llu]\n",
			per_cpu(rt_throttling_start, cpu_rq_throttle));
#endif

	PN(rt_time);
	PN(rt_runtime);

#undef PN
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
	/*sched: only output / cgroup schedule info*/
	print_rt_rq_at_AEE(m, cpu, rt_rq);
	rcu_read_unlock();
}

void print_dl_rq_at_AEE(struct seq_file *m, int cpu, struct dl_rq *dl_rq)
{
	SEQ_printf_at_AEE(m, "\ndl_rq[%d]:\n", cpu);
	SEQ_printf_at_AEE(m, "  .%-30s: %ld\n",
			"dl_nr_running", dl_rq->dl_nr_running);
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
	SEQ_printf_at_AEE(m, "  .%-30s: %lld.%06ld\n", #x, SPLIT_NS(rq->x))

	P(nr_running);
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", "load",
		   rq->load.weight);
	/*P(nr_switches);*/
	P(nr_load_updates);
	P(nr_uninterruptible);
	PN(next_balance);
	SEQ_printf_at_AEE(m, "  .%-30s: %ld\n",
			"curr->pid", (long)(task_pid_nr(rq->curr)));
	PN(clock);
	PN(clock_task);
	SEQ_printf_at_AEE(m, "  .%-30s: %ld %ld %ld %ld %ld\n", "cpu_load",
			(long)(rq->cpu_load[0]),
			(long)(rq->cpu_load[1]),
			(long)(rq->cpu_load[2]),
			(long)(rq->cpu_load[3]),
			(long)(rq->cpu_load[4]));
	/*
	 * P(cpu_load[0]);
	 * P(cpu_load[1]);
	 * P(cpu_load[2]);
	 * P(cpu_load[3]);
	 * P(cpu_load[4]);
	 */
#undef P
#undef PN

#ifdef CONFIG_SCHEDSTATS
#define P(n) SEQ_printf_at_AEE(m, "  .%-30s: %d\n", #n, rq->n)
#define P64(n) SEQ_printf_at_AEE(m, "  .%-30s: %lld\n", #n, rq->n)
	/*
	 * P(yld_count);
	 * P(sched_count);
	 * P(sched_goidle);
	 */
#ifdef CONFIG_SMP
	P64(avg_idle);
	P64(max_idle_balance_cost);
#endif
	/*
	 * P(ttwu_count);
	 * P(ttwu_local);
	 */
#undef P
#undef P64
#endif
	/*spin_lock_irqsave_lock_irqsave(&sched_debug_lock, flags);*/
	locked = spin_trylock_n_irqsave(&sched_debug_lock,
			&flags, m, "print_cpu_at_AEE");
	print_cfs_stats_at_AEE(m, cpu);
	print_rt_stats_at_AEE(m, cpu);
	print_dl_stats_at_AEE(m, cpu);

	rcu_read_lock();
	print_rq_at_AEE(m, rq, cpu);
	SEQ_printf_at_AEE(m, "============================================\n");
	rcu_read_unlock();
	/*spin_unlock_irqrestore(&sched_debug_lock, flags);*/
	if (locked)
		spin_unlock_irqrestore(&sched_debug_lock, flags);
}

static void sched_debug_header_at_AEE(struct seq_file *m)
{
	u64 sched_clk, cpu_clk;
	unsigned long flags;

#ifdef TEST_SCHED_DEBUG_ENHANCEMENT
	struct rq *rq = cpu_rq(0);
	/* lock_timekeeper(); */
	raw_spin_lock_irq(&rq->lock);
	spin_lock_irqsave(&sched_debug_lock, flags);
	write_lock_irqsave(&tasklist_lock, flags);
#endif

	local_irq_save(flags);
	/*ktime = ktime_to_ns(ktime_get());*/
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
	SEQ_printf_at_AEE(m, "%-40s: %lld.%06ld\n", #x, SPLIT_NS(x))
	/*PN(ktime);*/
	PN(sched_clk);
	PN(cpu_clk);
	P(jiffies);
#ifdef CONFIG_HAVE_UNSTABLE_SCHED_CLOCK
	P(sched_clock_stable());
#endif
#undef PN
#undef P

	/*SEQ_printf_at_AEE(m, "\n");*/
	SEQ_printf_at_AEE(m, "sysctl_sched\n");

#define P(x) \
	SEQ_printf_at_AEE(m, "  .%-40s: %lld\n", #x, (long long)(x))
#define PN(x) \
	SEQ_printf_at_AEE(m, "  .%-40s: %lld.%06ld\n", #x, SPLIT_NS(x))
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
	/* read_lock_irqsave(&tasklist_lock, flags); */
	locked = read_trylock_n_irqsave(&tasklist_lock,
			&flags, NULL, "sched_debug_show_at_AEE");

	/* for_each_online_cpu(cpu) */
	for_each_possible_cpu(cpu) {
		print_cpu_at_AEE(NULL, cpu);
	}
	if (locked)
		read_unlock_irqrestore(&tasklist_lock, flags);

#ifdef CONFIG_MTK_RT_THROTTLE_MON
	/* sched:rt throttle monitor */
	mt_rt_mon_print_task_from_buffer();
#endif
}
