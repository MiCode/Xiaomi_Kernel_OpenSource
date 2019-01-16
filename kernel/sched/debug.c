/*
 * kernel/sched/debug.c
 *
 * Print the CFS rbtree
 *
 * Copyright(C) 2007, Red Hat, Inc., Ingo Molnar
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <linux/utsname.h>
#ifdef CONFIG_KGDB_KDB
#include <linux/kdb.h>
#endif

#include "sched.h"

//#define TEST_SCHED_DEBUG_ENHANCEMENT
//#define MTK_SCHED_CMP_PRINT
#define TRYLOCK_NUM 10
#include <linux/delay.h>
static DEFINE_SPINLOCK(sched_debug_lock);

DECLARE_PER_CPU(u64, exec_delta_time);
DECLARE_PER_CPU(u64, clock_task);
DECLARE_PER_CPU(u64, exec_start);
DECLARE_PER_CPU(struct task_struct, exec_task);

/*
 * This allows printing both to /proc/sched_debug and
 * to the console
 */
#ifndef CONFIG_KGDB_KDB
#define SEQ_printf(m, x...)			\
 do {						\
	if (m)					\
		seq_printf(m, x);		\
	else					\
		printk(x);			\
 } while (0)
#else
#define SEQ_printf(m, x...)			\
 do {						\
	if (m)					\
		seq_printf(m, x);		\
	else if (__get_cpu_var(kdb_in_use) == 1)		\
		kdb_printf(x);			\
	else						\
		printk(x);				\
 } while (0)
#endif
/*
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

#define SPLIT_NS(x) nsec_high(x), nsec_low(x)

#ifdef CONFIG_FAIR_GROUP_SCHED
static void print_cfs_group_stats(struct seq_file *m, int cpu, struct task_group *tg)
{
	struct sched_entity *se = tg->se[cpu];

#define P(F) \
	SEQ_printf(m, "  .%-30s: %lld\n", #F, (long long)F)
#define PN(F) \
	SEQ_printf(m, "  .%-30s: %lld.%06ld\n", #F, SPLIT_NS((long long)F))

	if (!se) {
		struct sched_avg *avg = &cpu_rq(cpu)->avg;
		P(avg->runnable_avg_sum);
		P(avg->runnable_avg_period);
#ifdef MTK_SCHED_CMP_PRINT
# ifdef CONFIG_MTK_SCHED_CMP
		/* usage_avg_sum & load_avg_ratio are based on Linaro 12.11 */
		P(avg->usage_avg_sum);
		P(avg->load_avg_ratio);
# endif
		P(avg->last_runnable_update);
#endif
		return;
	}


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
	P(se->avg.runnable_avg_sum);
	P(se->avg.runnable_avg_period);
	P(se->avg.usage_avg_sum);
	P(se->avg.load_avg_contrib);
	P(se->avg.decay_count);

# ifdef MTK_SCHED_CMP_PRINT
#  ifdef CONFIG_MTK_SCHED_CMP
	/* usage_avg_sum & load_avg_ratio are based on Linaro 12.11 */
	P(se->avg.usage_avg_sum);
	P(se->avg.load_avg_ratio);
#  endif
	P(se->avg.last_runnable_update);
# endif
#endif
#undef PN
#undef P
}
#endif

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

static void
print_task(struct seq_file *m, struct rq *rq, struct task_struct *p)
{
	if (rq->curr == p)
		SEQ_printf(m, "R");
	else
		SEQ_printf(m, " ");

	SEQ_printf(m, "%15s %5d %9Ld.%06ld %9Ld %5d ",
		p->comm, p->pid,
		SPLIT_NS(p->se.vruntime),
		(long long)(p->nvcsw + p->nivcsw),
		p->prio);
#ifdef CONFIG_SCHEDSTATS
	SEQ_printf(m, "%9Ld.%06ld %9Ld.%06ld %9Ld.%06ld",
		SPLIT_NS(p->se.vruntime),
		SPLIT_NS(p->se.sum_exec_runtime),
		SPLIT_NS(p->se.statistics.sum_sleep_runtime));
#else
	SEQ_printf(m, "%15Ld %15Ld %15Ld.%06ld %15Ld.%06ld %15Ld.%06ld",
		0LL, 0LL, 0LL, 0L, 0LL, 0L, 0LL, 0L);
#endif
#ifdef CONFIG_CGROUP_SCHED
	SEQ_printf(m, " %s", task_group_path(task_group(p)));
#endif

	SEQ_printf(m, "\n");
}

static void print_rq(struct seq_file *m, struct rq *rq, int rq_cpu)
{
	struct task_struct *g, *p;
	unsigned long flags;

	SEQ_printf(m,
	"\nrunnable tasks:\n"
	"            task   PID         tree-key  switches  prio"
	"     exec-runtime         sum-exec        sum-sleep\n"
	"------------------------------------------------------"
	"----------------------------------------------------\n");

	read_lock_irqsave(&tasklist_lock, flags);

	do_each_thread(g, p) {
		if (!p->on_rq || task_cpu(p) != rq_cpu)
			continue;

		print_task(m, rq, p);
	} while_each_thread(g, p);

	read_unlock_irqrestore(&tasklist_lock, flags);
}

void print_cfs_rq(struct seq_file *m, int cpu, struct cfs_rq *cfs_rq)
{
	s64 MIN_vruntime = -1, min_vruntime, max_vruntime = -1,
		spread, rq0_min_vruntime, spread0;
	struct rq *rq = cpu_rq(cpu);
	struct sched_entity *last;
	unsigned long flags;

#ifdef CONFIG_FAIR_GROUP_SCHED
	SEQ_printf(m, "\ncfs_rq[%d]:%s\n", cpu, task_group_path(cfs_rq->tg));
#else
	SEQ_printf(m, "\ncfs_rq[%d]:\n", cpu);
#endif
	SEQ_printf(m, "  .%-30s: %Ld.%06ld\n", "exec_clock",
			SPLIT_NS(cfs_rq->exec_clock));

	raw_spin_lock_irqsave(&rq->lock, flags);
	if (cfs_rq->rb_leftmost)
		MIN_vruntime = (__pick_first_entity(cfs_rq))->vruntime;
	last = __pick_last_entity(cfs_rq);
	if (last)
		max_vruntime = last->vruntime;
	min_vruntime = cfs_rq->min_vruntime;
	rq0_min_vruntime = cpu_rq(0)->cfs.min_vruntime;
	raw_spin_unlock_irqrestore(&rq->lock, flags);
	SEQ_printf(m, "  .%-30s: %Ld.%06ld\n", "MIN_vruntime",
			SPLIT_NS(MIN_vruntime));
	SEQ_printf(m, "  .%-30s: %Ld.%06ld\n", "min_vruntime",
			SPLIT_NS(min_vruntime));
	SEQ_printf(m, "  .%-30s: %Ld.%06ld\n", "max_vruntime",
			SPLIT_NS(max_vruntime));
	spread = max_vruntime - MIN_vruntime;
	SEQ_printf(m, "  .%-30s: %Ld.%06ld\n", "spread",
			SPLIT_NS(spread));
	spread0 = min_vruntime - rq0_min_vruntime;
	SEQ_printf(m, "  .%-30s: %Ld.%06ld\n", "spread0",
			SPLIT_NS(spread0));
	SEQ_printf(m, "  .%-30s: %d\n", "nr_spread_over",
			cfs_rq->nr_spread_over);
	SEQ_printf(m, "  .%-30s: %d\n", "nr_running", cfs_rq->nr_running);
	SEQ_printf(m, "  .%-30s: %ld\n", "load", cfs_rq->load.weight);
#ifdef CONFIG_SMP
	SEQ_printf(m, "  .%-30s: %ld\n", "runnable_load_avg",
			cfs_rq->runnable_load_avg);
	SEQ_printf(m, "  .%-30s: %ld\n", "blocked_load_avg",
			cfs_rq->blocked_load_avg);
#ifdef CONFIG_FAIR_GROUP_SCHED
	SEQ_printf(m, "  .%-30s: %ld\n", "tg_load_contrib",
			cfs_rq->tg_load_contrib);
	SEQ_printf(m, "  .%-30s: %d\n", "tg_runnable_contrib",
			cfs_rq->tg_runnable_contrib);
	SEQ_printf(m, "  .%-30s: %ld\n", "tg->load_avg",
			atomic_long_read(&cfs_rq->tg->load_avg));
	SEQ_printf(m, "  .%-30s: %d\n", "tg->runnable_avg",
			atomic_read(&cfs_rq->tg->runnable_avg));
	SEQ_printf(m, "  .%-30s: %d\n", "tg->usage_avg",
			atomic_read(&cfs_rq->tg->usage_avg));
#endif
#ifdef CONFIG_CFS_BANDWIDTH
	SEQ_printf(m, "  .%-30s: %d\n", "tg->cfs_bandwidth.timer_active",
			cfs_rq->tg->cfs_bandwidth.timer_active);
	SEQ_printf(m, "  .%-30s: %d\n", "throttled",
			cfs_rq->throttled);
	SEQ_printf(m, "  .%-30s: %d\n", "throttle_count",
			cfs_rq->throttle_count);
#endif

#ifdef CONFIG_FAIR_GROUP_SCHED
	print_cfs_group_stats(m, cpu, cfs_rq->tg);
#endif
#endif
}

void print_rt_rq(struct seq_file *m, int cpu, struct rt_rq *rt_rq)
{
#ifdef CONFIG_RT_GROUP_SCHED
	SEQ_printf(m, "\nrt_rq[%d]:%s\n", cpu, task_group_path(rt_rq->tg));
#else
	SEQ_printf(m, "\nrt_rq[%d]:\n", cpu);
#endif

#define P(x) \
	SEQ_printf(m, "  .%-30s: %Ld\n", #x, (long long)(rt_rq->x))
#define PN(x) \
	SEQ_printf(m, "  .%-30s: %Ld.%06ld\n", #x, SPLIT_NS(rt_rq->x))

	P(rt_nr_running);
	P(rt_throttled);
	PN(rt_time);
	PN(rt_runtime);

#undef PN
#undef P
}

extern __read_mostly int sched_clock_running;

static void print_cpu(struct seq_file *m, int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long flags;

#ifdef CONFIG_X86
	{
		unsigned int freq = cpu_khz ? : 1;

		SEQ_printf(m, "cpu#%d, %u.%03u MHz\n",
			   cpu, freq / 1000, (freq % 1000));
	}
#else
	SEQ_printf(m, "cpu#%d: %s\n", cpu, cpu_is_offline(cpu)?"Offline":"Online");
#endif

#define P(x)								\
do {									\
	if (sizeof(rq->x) == 4)						\
		SEQ_printf(m, "  .%-30s: %ld\n", #x, (long)(rq->x));	\
	else								\
		SEQ_printf(m, "  .%-30s: %Ld\n", #x, (long long)(rq->x));\
} while (0)

#define PN(x) \
	SEQ_printf(m, "  .%-30s: %Ld.%06ld\n", #x, SPLIT_NS(rq->x))

	P(nr_running);
	SEQ_printf(m, "  .%-30s: %lu\n", "load",
		   rq->load.weight);
	P(nr_switches);
	P(nr_load_updates);
	P(nr_uninterruptible);
	PN(next_balance);
	P(curr->pid);
	PN(clock);
	P(cpu_load[0]);
	P(cpu_load[1]);
	P(cpu_load[2]);
	P(cpu_load[3]);
	P(cpu_load[4]);
#undef P
#undef PN

#ifdef CONFIG_SCHEDSTATS
#define P(n) SEQ_printf(m, "  .%-30s: %d\n", #n, rq->n);
#define P64(n) SEQ_printf(m, "  .%-30s: %Ld\n", #n, rq->n);

	P(yld_count);

	P(sched_count);
	P(sched_goidle);
#ifdef CONFIG_SMP
	P64(avg_idle);
#endif

	P(ttwu_count);
	P(ttwu_local);

#undef P
#undef P64
#endif
	spin_lock_irqsave(&sched_debug_lock, flags);
	print_cfs_stats(m, cpu);
	print_rt_stats(m, cpu);

	rcu_read_lock();
	print_rq(m, rq, cpu);
	rcu_read_unlock();
	spin_unlock_irqrestore(&sched_debug_lock, flags);
	SEQ_printf(m, "\n");
}

static const char *sched_tunable_scaling_names[] = {
	"none",
	"logaritmic",
	"linear"
};

#ifdef TEST_SCHED_DEBUG_ENHANCEMENT
extern void lock_timekeeper(void);		
#endif
static void sched_debug_header(struct seq_file *m)
{
	u64 ktime, sched_clk, cpu_clk;
	unsigned long flags;

#ifdef TEST_SCHED_DEBUG_ENHANCEMENT
	static int i=0;
	i++;
	if(i==10){
		struct rq *rq = cpu_rq(0);
		//lock_timekeeper();	
		raw_spin_lock_irq(&rq->lock);
		spin_lock_irqsave(&sched_debug_lock, flags);
		write_lock_irqsave(&tasklist_lock, flags);
		BUG_ON(1);
	}
#endif

	local_irq_save(flags);
	ktime = ktime_to_ns(ktime_get());
	sched_clk = sched_clock();
	cpu_clk = local_clock();
	local_irq_restore(flags);

	SEQ_printf(m, "Sched Debug Version: v0.10, %s %.*s\n",
		init_utsname()->release,
		(int)strcspn(init_utsname()->version, " "),
		init_utsname()->version);

#define P(x) \
	SEQ_printf(m, "%-40s: %Ld\n", #x, (long long)(x))
#define PN(x) \
	SEQ_printf(m, "%-40s: %Ld.%06ld\n", #x, SPLIT_NS(x))
	PN(ktime);
	PN(sched_clk);
	PN(cpu_clk);
	P(jiffies);
#ifdef CONFIG_HAVE_UNSTABLE_SCHED_CLOCK
	P(sched_clock_stable);
#endif
#undef PN
#undef P

	SEQ_printf(m, "\n");
	SEQ_printf(m, "sysctl_sched\n");

#define P(x) \
	SEQ_printf(m, "  .%-40s: %Ld\n", #x, (long long)(x))
#define PN(x) \
	SEQ_printf(m, "  .%-40s: %Ld.%06ld\n", #x, SPLIT_NS(x))
	PN(sysctl_sched_latency);
	PN(sysctl_sched_min_granularity);
	PN(sysctl_sched_wakeup_granularity);
	P(sysctl_sched_child_runs_first);
	P(sysctl_sched_features);
#undef PN
#undef P

	SEQ_printf(m, "  .%-40s: %d (%s)\n",
		"sysctl_sched_tunable_scaling",
		sysctl_sched_tunable_scaling,
		sched_tunable_scaling_names[sysctl_sched_tunable_scaling]);
	SEQ_printf(m, "\n");
}

static int sched_debug_show(struct seq_file *m, void *v)
{
	int cpu = (unsigned long)(v - 2);
	unsigned long flags;

	if (cpu != -1) {
		read_lock_irqsave(&tasklist_lock, flags);
		print_cpu(m, cpu);
		read_unlock_irqrestore(&tasklist_lock, flags);
		SEQ_printf(m, "\n");
	} else
		sched_debug_header(m);

	return 0;
}

void sysrq_sched_debug_show(void)
{
	int cpu;
	unsigned long flags;

	sched_debug_header(NULL);
	read_lock_irqsave(&tasklist_lock, flags);
	//for_each_online_cpu(cpu)
	for_each_possible_cpu(cpu)
		print_cpu(NULL, cpu);
	read_unlock_irqrestore(&tasklist_lock, flags);

}

/*
 * This itererator needs some explanation.
 * It returns 1 for the header position.
 * This means 2 is cpu 0.
 * In a hotplugged system some cpus, including cpu 0, may be missing so we have
 * to use cpumask_* to iterate over the cpus.
 */
static void *sched_debug_start(struct seq_file *file, loff_t *offset)
{
	unsigned long n = *offset;

	if (n == 0)
		return (void *) 1;

	n--;

	if (n > 0)
		n = cpumask_next(n - 1, cpu_online_mask);
	else
		n = cpumask_first(cpu_online_mask);

	*offset = n + 1;

	if (n < nr_cpu_ids)
		return (void *)(unsigned long)(n + 2);
	return NULL;
}

static void *sched_debug_next(struct seq_file *file, void *data, loff_t *offset)
{
	(*offset)++;
	return sched_debug_start(file, offset);
}

static void sched_debug_stop(struct seq_file *file, void *data)
{
}

static const struct seq_operations sched_debug_sops = {
	.start = sched_debug_start,
	.next = sched_debug_next,
	.stop = sched_debug_stop,
	.show = sched_debug_show,
};

static int sched_debug_release(struct inode *inode, struct file *file)
{
	seq_release(inode, file);

	return 0;
}

static int sched_debug_open(struct inode *inode, struct file *filp)
{
	int ret = 0;

	ret = seq_open(filp, &sched_debug_sops);

	return ret;
}

static const struct file_operations sched_debug_fops = {
	.open		= sched_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= sched_debug_release,
};

static int __init init_sched_debug_procfs(void)
{
	struct proc_dir_entry *pe;

	pe = proc_create("sched_debug", 0444, NULL, &sched_debug_fops);
	if (!pe)
		return -ENOMEM;
	return 0;
}

__initcall(init_sched_debug_procfs);

void proc_sched_show_task(struct task_struct *p, struct seq_file *m)
{
	unsigned long nr_switches;

	SEQ_printf(m, "%s (%d, #threads: %d)\n", p->comm, p->pid,
						get_nr_threads(p));
	SEQ_printf(m,
		"---------------------------------------------------------\n");
#define __P(F) \
	SEQ_printf(m, "%-35s:%21Ld\n", #F, (long long)F)
#define P(F) \
	SEQ_printf(m, "%-35s:%21Ld\n", #F, (long long)p->F)
#define __PN(F) \
	SEQ_printf(m, "%-35s:%14Ld.%06ld\n", #F, SPLIT_NS((long long)F))
#define PN(F) \
	SEQ_printf(m, "%-35s:%14Ld.%06ld\n", #F, SPLIT_NS((long long)p->F))

	PN(se.exec_start);
	PN(se.vruntime);
	PN(se.sum_exec_runtime);

	nr_switches = p->nvcsw + p->nivcsw;

#ifdef CONFIG_SCHEDSTATS
	PN(se.statistics.wait_start);
	PN(se.statistics.sleep_start);
	PN(se.statistics.block_start);
	PN(se.statistics.sleep_max);
	PN(se.statistics.block_max);
	PN(se.statistics.exec_max);
	PN(se.statistics.slice_max);
	PN(se.statistics.wait_max);
	PN(se.statistics.wait_sum);
	P(se.statistics.wait_count);
	PN(se.statistics.iowait_sum);
	P(se.statistics.iowait_count);
	P(se.nr_migrations);
	P(se.statistics.nr_migrations_cold);
	P(se.statistics.nr_failed_migrations_affine);
	P(se.statistics.nr_failed_migrations_running);
	P(se.statistics.nr_failed_migrations_hot);
	P(se.statistics.nr_forced_migrations);
	P(se.statistics.nr_wakeups);
	P(se.statistics.nr_wakeups_sync);
	P(se.statistics.nr_wakeups_migrate);
	P(se.statistics.nr_wakeups_local);
	P(se.statistics.nr_wakeups_remote);
	P(se.statistics.nr_wakeups_affine);
	P(se.statistics.nr_wakeups_affine_attempts);
	P(se.statistics.nr_wakeups_passive);
	P(se.statistics.nr_wakeups_idle);

	{
		u64 avg_atom, avg_per_cpu;

		avg_atom = p->se.sum_exec_runtime;
		if (nr_switches)
			avg_atom = div64_ul(avg_atom, nr_switches);
		else
			avg_atom = -1LL;

		avg_per_cpu = p->se.sum_exec_runtime;
		if (p->se.nr_migrations) {
			avg_per_cpu = div64_u64(avg_per_cpu,
						p->se.nr_migrations);
		} else {
			avg_per_cpu = -1LL;
		}

		__PN(avg_atom);
		__PN(avg_per_cpu);
	}
#endif
	__P(nr_switches);
	SEQ_printf(m, "%-35s:%21Ld\n",
		   "nr_voluntary_switches", (long long)p->nvcsw);
	SEQ_printf(m, "%-35s:%21Ld\n",
		   "nr_involuntary_switches", (long long)p->nivcsw);

	P(se.load.weight);
#ifdef CONFIG_SMP
	P(se.avg.runnable_avg_sum);
	P(se.avg.runnable_avg_period);
	P(se.avg.load_avg_contrib);
	P(se.avg.decay_count);

# ifdef MTK_SCHED_CMP_PRINT
#  ifdef CONFIG_MTK_SCHED_CMP
	/* usage_avg_sum & load_avg_ratio are based on Linaro 12.11 */
	P(se.avg.usage_avg_sum);
	P(se.avg.load_avg_ratio);
#  endif
	P(se.avg.last_runnable_update);
# endif
#endif
	P(policy);
	P(prio);
#undef PN
#undef __PN
#undef P
#undef __P

	{
		unsigned int this_cpu = raw_smp_processor_id();
		u64 t0, t1;

		t0 = cpu_clock(this_cpu);
		t1 = cpu_clock(this_cpu);
		SEQ_printf(m, "%-35s:%21Ld\n",
			   "clock-delta", (long long)(t1-t0));
	}
}

void proc_sched_set_task(struct task_struct *p)
{
#ifdef CONFIG_SCHEDSTATS
	memset(&p->se.statistics, 0, sizeof(p->se.statistics));
#endif
}

#define read_trylock_irqsave(lock, flags)		\
	({						\
		typecheck(unsigned long, flags);	\
		local_irq_save(flags);			\
		read_trylock(lock)?			\
		1 : ({ local_irq_restore(flags); 0; }); \
	})

int read_trylock_n_irqsave(rwlock_t *lock, unsigned long *flags, struct seq_file *m, char *msg){
	int locked, trylock_cnt=0;

	do{
		locked = read_trylock_irqsave(lock, *flags);
		trylock_cnt++;
		mdelay(10);
	}while((!locked) && (trylock_cnt < TRYLOCK_NUM));

	if (!locked){
#ifdef CONFIG_DEBUG_SPINLOCK		
		struct task_struct *owner = NULL;
#endif		
		SEQ_printf(m, "Warning: fail to get lock in %s\n", msg);
#ifdef CONFIG_DEBUG_SPINLOCK
		if (lock->owner && lock->owner != SPINLOCK_OWNER_INIT )
			owner = lock->owner;
		SEQ_printf(m, " lock: %p, .magic: %08x, .owner: %s/%d, "
				".owner_cpu: %d, value: %d\n", 
			lock, lock->magic, 
			owner ? owner-> comm: "<<none>>", 	
			owner ? task_pid_nr(owner): -1, 
			lock->owner_cpu, lock->raw_lock.lock);
#endif
	}

	return locked;
}

int raw_spin_trylock_n_irqsave(raw_spinlock_t *lock, unsigned long *flags, struct seq_file *m, char *msg){
	int locked, trylock_cnt=0;

	do{
		locked = raw_spin_trylock_irqsave(lock, *flags);
		trylock_cnt++;
		mdelay(10);
	}while((!locked) && (trylock_cnt < TRYLOCK_NUM));

	if (!locked){
#ifdef CONFIG_DEBUG_SPINLOCK		
		struct task_struct *owner = NULL;
#endif		
		SEQ_printf(m, "Warning: fail to get lock in %s\n", msg);
#ifdef CONFIG_DEBUG_SPINLOCK
		if (lock->owner && lock->owner != SPINLOCK_OWNER_INIT )
			owner = lock->owner;
# ifdef CONFIG_ARM64
		SEQ_printf(m, " lock: %lx, .magic: %08x, .owner: %s/%d, "
				".owner_cpu: %d, value: %d\n", 
			(long)lock, lock->magic, 
			owner ? owner-> comm: "<<none>>", 	
			owner ? task_pid_nr(owner): -1, 
			lock->owner_cpu, lock->raw_lock.lock);
# else
		SEQ_printf(m, " lock: %x, .magic: %08x, .owner: %s/%d, "
				".owner_cpu: %d, value: %d\n", 
			(int)lock, lock->magic, 
			owner ? owner-> comm: "<<none>>", 	
			owner ? task_pid_nr(owner): -1, 
			lock->owner_cpu, lock->raw_lock.slock);
# endif
#endif
	}

	return locked;
}

int spin_trylock_n_irqsave(spinlock_t *lock, unsigned long *flags, struct seq_file *m, char *msg){
	int locked, trylock_cnt=0;

	do{
		locked = spin_trylock_irqsave(lock, *flags);
		trylock_cnt++;
		mdelay(10);
		
	}while((!locked) && (trylock_cnt < TRYLOCK_NUM));

	if (!locked){
#ifdef CONFIG_DEBUG_SPINLOCK		
		raw_spinlock_t rlock = lock->rlock;
		struct task_struct *owner = NULL;
#endif				
		SEQ_printf(m, "Warning: fail to get lock in %s\n", msg);
#ifdef CONFIG_DEBUG_SPINLOCK
		if (rlock.owner && rlock.owner != SPINLOCK_OWNER_INIT )
			owner = rlock.owner;
# ifdef CONFIG_ARM64
		SEQ_printf(m, " lock: %lx, .magic: %08x, .owner: %s/%d, "
				".owner_cpu: %d, value: %d\n", 
			(long) &rlock, rlock.magic, 
			owner ? owner-> comm: "<<none>>", 	
			owner ? task_pid_nr(owner): -1, 
			rlock.owner_cpu, rlock.raw_lock.lock);
# else
		SEQ_printf(m, " lock: %x, .magic: %08x, .owner: %s/%d, "
				".owner_cpu: %d, value: %d\n", 
			(int) &rlock, rlock.magic, 
			owner ? owner-> comm: "<<none>>", 	
			owner ? task_pid_nr(owner): -1, 
			rlock.owner_cpu, rlock.raw_lock.slock);
# endif
#endif
	}

	return locked;
}

void print_rq_at_KE(struct seq_file *m, struct rq *rq, int rq_cpu)
{
	struct task_struct *g, *p;
	unsigned long flags;
	int locked;

	SEQ_printf(m,
	"runnable tasks:\n"
	"            task   PID         tree-key  switches  prio"
	"     exec-runtime         sum-exec        sum-sleep\n"
	"------------------------------------------------------"
	"----------------------------------------------------\n");

	//read_lock_irqsave(&tasklist_lock, flags);
	locked = read_trylock_n_irqsave(&tasklist_lock, &flags, m, "print_rq_at_KE");

	do_each_thread(g, p) {
		if (!p->on_rq || task_cpu(p) != rq_cpu)
			continue;

		print_task(m, rq, p);
	} while_each_thread(g, p);

	if (locked)
		read_unlock_irqrestore(&tasklist_lock, flags);
}

#ifdef CONFIG_FAIR_GROUP_SCHED
static void print_cfs_group_stats_at_KE(struct seq_file *m, int cpu, struct task_group *tg)
{
	struct sched_entity *se = tg->se[cpu];

#define P(F) \
	SEQ_printf(m, "  .%-22s: %lld\n", #F, (long long)F)
#define PN(F) \
	SEQ_printf(m, "  .%-22s: %lld.%06ld\n", #F, SPLIT_NS((long long)F))

	if (!se) {
		struct sched_avg *avg = &cpu_rq(cpu)->avg;
		P(avg->runnable_avg_sum);
		P(avg->runnable_avg_period);
#ifdef MTK_SCHED_CMP_PRINT
# ifdef CONFIG_MTK_SCHED_CMP
		/* usage_avg_sum & load_avg_ratio are based on Linaro 12.11 */
		P(avg->usage_avg_sum);
		P(avg->load_avg_ratio);
# endif
		P(avg->last_runnable_update);
#endif
		return;
	}


	PN(se->exec_start);
	PN(se->vruntime);
	PN(se->sum_exec_runtime);
	P(se->load.weight);
#ifdef CONFIG_SMP
	P(se->avg.runnable_avg_sum);
	P(se->avg.runnable_avg_period);
	P(se->avg.usage_avg_sum);
	P(se->avg.load_avg_contrib);
	P(se->avg.decay_count);

# ifdef MTK_SCHED_CMP_PRINT
#  ifdef CONFIG_MTK_SCHED_CMP
	/* usage_avg_sum & load_avg_ratio are based on Linaro 12.11 */
	P(se->avg.usage_avg_sum);
	P(se->avg.load_avg_ratio);
#  endif
	P(se->avg.last_runnable_update);
# endif
#endif
#undef PN
#undef P
}
#endif

void print_cfs_rq_at_KE(struct seq_file *m, int cpu, struct cfs_rq *cfs_rq)
{
	s64 MIN_vruntime = -1, min_vruntime, max_vruntime = -1,
		spread, rq0_min_vruntime, spread0;
	struct rq *rq = cpu_rq(cpu);
	struct sched_entity *last;
	unsigned long flags;
	int locked;

#ifdef CONFIG_FAIR_GROUP_SCHED
	SEQ_printf(m, "cfs_rq[%d]:%s\n", cpu, task_group_path(cfs_rq->tg));
#else
	SEQ_printf(m, "cfs_rq[%d]:\n", cpu);
#endif
	SEQ_printf(m, "  .%-22s: %Ld.%06ld\n", "exec_clock",
			SPLIT_NS(cfs_rq->exec_clock));

	//raw_spin_lock_irqsave(&rq->lock, flags);
	locked = raw_spin_trylock_n_irqsave(&rq->lock, &flags, m, "print_cfs_rq_at_KE");
	if (cfs_rq->rb_leftmost)
		MIN_vruntime = (__pick_first_entity(cfs_rq))->vruntime;
	last = __pick_last_entity(cfs_rq);
	if (last)
		max_vruntime = last->vruntime;
	min_vruntime = cfs_rq->min_vruntime;
	rq0_min_vruntime = cpu_rq(0)->cfs.min_vruntime;
	if(locked)
		raw_spin_unlock_irqrestore(&rq->lock, flags);
	SEQ_printf(m, "  .%-22s: %Ld.%06ld\n", "MIN_vruntime",
			SPLIT_NS(MIN_vruntime));
	SEQ_printf(m, "  .%-22s: %Ld.%06ld\n", "min_vruntime",
			SPLIT_NS(min_vruntime));
	SEQ_printf(m, "  .%-22s: %Ld.%06ld\n", "max_vruntime",
			SPLIT_NS(max_vruntime));
	spread = max_vruntime - MIN_vruntime;
	SEQ_printf(m, "  .%-22s: %Ld.%06ld\n", "spread",
			SPLIT_NS(spread));
	spread0 = min_vruntime - rq0_min_vruntime;
	SEQ_printf(m, "  .%-22s: %Ld.%06ld\n", "spread0",
			SPLIT_NS(spread0));
	SEQ_printf(m, "  .%-22s: %d\n", "nr_spread_over",
			cfs_rq->nr_spread_over);
	SEQ_printf(m, "  .%-22s: %d\n", "nr_running", cfs_rq->nr_running);
	SEQ_printf(m, "  .%-22s: %ld\n", "load", cfs_rq->load.weight);
#ifdef CONFIG_SMP
	SEQ_printf(m, "  .%-22s: %ld\n", "runnable_load_avg",
			cfs_rq->runnable_load_avg);
	SEQ_printf(m, "  .%-22s: %ld\n", "blocked_load_avg",
			cfs_rq->blocked_load_avg);
#  ifdef CONFIG_FAIR_GROUP_SCHED
	SEQ_printf(m, "  .%-22s: %ld\n", "tg_load_contrib",
			cfs_rq->tg_load_contrib);
	SEQ_printf(m, "  .%-22s: %d\n", "tg_runnable_contrib",
			cfs_rq->tg_runnable_contrib);
	SEQ_printf(m, "  .%-22s: %ld\n", "tg->load_avg",
			atomic_long_read(&cfs_rq->tg->load_avg));
	SEQ_printf(m, "  .%-22s: %d\n", "tg->runnable_avg",
			atomic_read(&cfs_rq->tg->runnable_avg));
# endif
#endif

#ifdef CONFIG_FAIR_GROUP_SCHED
	print_cfs_group_stats_at_KE(m, cpu, cfs_rq->tg);
#endif
}

#define for_each_leaf_cfs_rq(rq, cfs_rq) \
	list_for_each_entry_rcu(cfs_rq, &rq->leaf_cfs_rq_list, leaf_cfs_rq_list)

void print_cfs_stats_at_KE(struct seq_file *m, int cpu)
{
	struct cfs_rq *cfs_rq;

	rcu_read_lock();
	for_each_leaf_cfs_rq(cpu_rq(cpu), cfs_rq)
		print_cfs_rq_at_KE(m, cpu, cfs_rq);
	rcu_read_unlock();
}

void print_rt_rq_at_KE(struct seq_file *m, int cpu, struct rt_rq *rt_rq)
{
#ifdef CONFIG_RT_GROUP_SCHED
	SEQ_printf(m, "rt_rq[%d]:%s\n", cpu, task_group_path(rt_rq->tg));
#else
	SEQ_printf(m, "rt_rq[%d]:\n", cpu);
#endif

#define P(x) \
	SEQ_printf(m, "  .%-22s: %Ld\n", #x, (long long)(rt_rq->x))
#define PN(x) \
	SEQ_printf(m, "  .%-22s: %Ld.%06ld\n", #x, SPLIT_NS(rt_rq->x))

	P(rt_nr_running);
	P(rt_throttled);
	SEQ_printf(m, "  exec_task[%d:%s], prio=%d exec_delta_time[%llu]"
                             ", clock_task[%llu], exec_start[%llu]\n",
			     per_cpu(exec_task, cpu).pid,
			     per_cpu(exec_task, cpu).comm,
			     per_cpu(exec_task, cpu).prio,
                             per_cpu(exec_delta_time, cpu),
                             per_cpu(clock_task, cpu),
                             per_cpu(exec_start, cpu));
	PN(rt_time);
	PN(rt_runtime);

#undef PN
#undef P
}

#ifdef CONFIG_RT_GROUP_SCHED
typedef struct task_group *rt_rq_iter_t;

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

typedef struct rt_rq *rt_rq_iter_t;

#define for_each_rt_rq(rt_rq, iter, rq) \
	for ((void) iter, rt_rq = &rq->rt; rt_rq; rt_rq = NULL)

#endif

void print_rt_stats_at_KE(struct seq_file *m, int cpu)
{
        rt_rq_iter_t iter;
        struct rt_rq *rt_rq;

        rcu_read_lock();
        for_each_rt_rq(rt_rq, iter, cpu_rq(cpu))
                print_rt_rq_at_KE(m, cpu, rt_rq);
        rcu_read_unlock();
}

static void print_cpu_at_KE(struct seq_file *m, int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long flags;
	int locked;

#ifdef CONFIG_X86
	{
		unsigned int freq = cpu_khz ? : 1;

		SEQ_printf(m, "\ncpu#%d, %u.%03u MHz\n",
			   cpu, freq / 1000, (freq % 1000));
	}
#else
	SEQ_printf(m, "cpu#%d: %s\n", cpu, cpu_is_offline(cpu)?"Offline":"Online");
#endif

#define P(x)								\
do {									\
	if (sizeof(rq->x) == 4)						\
		SEQ_printf(m, "  .%-22s: %ld\n", #x, (long)(rq->x));	\
	else								\
		SEQ_printf(m, "  .%-22s: %Ld\n", #x, (long long)(rq->x));\
} while (0)

#define PN(x) \
	SEQ_printf(m, "  .%-22s: %Ld.%06ld\n", #x, SPLIT_NS(rq->x))

	P(nr_running);
	SEQ_printf(m, "  .%-22s: %lu\n", "load",
		   rq->load.weight);
	P(nr_switches);
	P(nr_load_updates);
	P(nr_uninterruptible);
	PN(next_balance);
	P(curr->pid);
	PN(clock);
	P(cpu_load[0]);
	P(cpu_load[1]);
	P(cpu_load[2]);
	P(cpu_load[3]);
	P(cpu_load[4]);
#undef P
#undef PN

#ifdef CONFIG_SCHEDSTATS
#define P(n) SEQ_printf(m, "  .%-22s: %d\n", #n, rq->n);
#define P64(n) SEQ_printf(m, "  .%-22s: %Ld\n", #n, rq->n);

	P(yld_count);

	P(sched_count);
	P(sched_goidle);
#ifdef CONFIG_SMP
	P64(avg_idle);
#endif

	P(ttwu_count);
	P(ttwu_local);

#undef P
#undef P64
#endif
	//spin_lock_irqsave(&sched_debug_lock, flags);
	locked = spin_trylock_n_irqsave( &sched_debug_lock, &flags, m, "print_cpu_at_KE");
	print_cfs_stats_at_KE(m, cpu);
	print_rt_stats_at_KE(m, cpu);

	rcu_read_lock();
	print_rq_at_KE(m, rq, cpu);
	SEQ_printf(m,
	"======================================================"
	"====================================================\n");
	rcu_read_unlock();
	if (locked)
		spin_unlock_irqrestore(&sched_debug_lock, flags);
}

static void sched_debug_header_at_KE(struct seq_file *m)
{
	u64 ktime=0, sched_clk, cpu_clk;
	unsigned long flags;

	local_irq_save(flags);
	// ktime = ktime_to_ns(ktime_get());
	sched_clk = sched_clock();
	cpu_clk = local_clock();
	local_irq_restore(flags);

	SEQ_printf(m, "Sched Debug Version: v0.10, %s %.*s\n",
		init_utsname()->release,
		(int)strcspn(init_utsname()->version, " "),
		init_utsname()->version);

#define P(x) \
	SEQ_printf(m, "%-22s: %Ld\n", #x, (long long)(x))
#define PN(x) \
	SEQ_printf(m, "%-22s: %Ld.%06ld\n", #x, SPLIT_NS(x))
	PN(ktime);
	PN(sched_clk);
	PN(cpu_clk);
	P(jiffies);
#ifdef CONFIG_HAVE_UNSTABLE_SCHED_CLOCK
	P(sched_clock_stable);
#endif
#undef PN
#undef P

	//SEQ_printf(m, "\n");
	SEQ_printf(m, "sysctl_sched\n");

#define P(x) \
	SEQ_printf(m, "  .%-35s: %Ld\n", #x, (long long)(x))
#define PN(x) \
	SEQ_printf(m, "  .%-35s: %Ld.%06ld\n", #x, SPLIT_NS(x))
	PN(sysctl_sched_latency);
	PN(sysctl_sched_min_granularity);
	PN(sysctl_sched_wakeup_granularity);
	P(sysctl_sched_child_runs_first);
	P(sysctl_sched_features);
#undef PN
#undef P

	SEQ_printf(m, "  .%-35s: %d (%s)\n",
		"sysctl_sched_tunable_scaling",
		sysctl_sched_tunable_scaling,
		sched_tunable_scaling_names[sysctl_sched_tunable_scaling]);
	SEQ_printf(m, "\n");
}

void sysrq_sched_debug_show_at_KE(void)
{
	int cpu;
	unsigned long flags;
	int locked;

	sched_debug_header_at_KE(NULL);
	//read_lock_irqsave(&tasklist_lock, flags);
	locked = read_trylock_n_irqsave(&tasklist_lock, &flags, NULL, "sched_debug_show_at_KE");
	//for_each_online_cpu(cpu)
	for_each_possible_cpu(cpu)
		print_cpu_at_KE(NULL, cpu);
	if (locked)
		read_unlock_irqrestore(&tasklist_lock, flags);

}

#ifdef CONFIG_MET_SCHED_HMP
/* MET */
#include <linux/export.h>
#include <linux/met_drv.h>

static char header[] = 
"met-info [000] 0.0: ms_ud_sys_header: TaskTh,B->th,L->th,d,d\n"
"met-info [000] 0.0: ms_ud_sys_header: HmpStat,force_up,force_down,d,d\n"
"met-info [000] 0.0: ms_ud_sys_header: HmpLoad,big_load_avg,little_load_avg,d,d\n"
"met-info [000] 0.0: ms_ud_sys_header: RqLen,rq0,rq1,rq2,rq3,d,d,d,d\n"
"met-info [000] 0.0: ms_ud_sys_header: CfsLen,cfs_rq0,cfs_rq1,cfs_rq2,cfs_rq3,d,d,d,d\n"
"met-info [000] 0.0: ms_ud_sys_header: RtLen,rt_rq0,rt_rq1,rt_rq2,rt_rq3,d,d,d,d\n";

static char help[] = "  --met_hmp_cfs                              monitor hmp_cfs\n";
static int sample_print_help(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, help);
}

static int sample_print_header(char *buf, int len)
{
	return snprintf(buf, PAGE_SIZE, header);
}

unsigned int mt_cfs_dbg=0;
static void sample_start(void)
{
	mt_cfs_dbg=1;
	return;
}

static void sample_stop(void)
{
	mt_cfs_dbg=0;
	return;
}

struct metdevice met_hmp_cfs = {
	.name = "hmp_cfs",
	.owner = THIS_MODULE,
	.type = MET_TYPE_BUS,
	.start = sample_start,
	.stop = sample_stop,
	.print_help = sample_print_help,
	.print_header = sample_print_header,
};
EXPORT_SYMBOL(met_hmp_cfs);

void TaskTh(unsigned int B_th,unsigned int L_th){	
if(mt_cfs_dbg)	
trace_printk("%d,%d\n",B_th,L_th);
}

void HmpStat(struct hmp_statisic *hmp_stats){
if(mt_cfs_dbg)	
trace_printk("%d,%d\n",hmp_stats->nr_force_up,hmp_stats->nr_force_down);
}

void HmpLoad(int big_load_avg, int little_load_avg){
if(mt_cfs_dbg)
trace_printk("%d,%d\n",big_load_avg,little_load_avg);
}

static DEFINE_PER_CPU(unsigned int, cfsrqCnt);
static DEFINE_PER_CPU(unsigned int, rtrqCnt);
static DEFINE_PER_CPU(unsigned int, rqCnt);

void RqLen(int cpu, int length){
if(mt_cfs_dbg){
        per_cpu(rqCnt, cpu) = length;
	#if NR_CPUS == 4  
	trace_printk("%d,%d,%d,%d\n",per_cpu(rqCnt,0),per_cpu(rqCnt,1),per_cpu(rqCnt,2),per_cpu(rqCnt,3));
	#endif
	}
}

void CfsLen(int cpu, int length){
if(mt_cfs_dbg){
        per_cpu(cfsrqCnt, cpu) = length;
	#if NR_CPUS == 4  
	trace_printk("%d,%d,%d,%d\n",per_cpu(cfsrqCnt,0),per_cpu(cfsrqCnt,1),per_cpu(cfsrqCnt,2),per_cpu(cfsrqCnt,3));
	#endif
	}
}

void RtLen(int cpu, int length){
if(mt_cfs_dbg){
        per_cpu(rtrqCnt, cpu) = length;        
	#if NR_CPUS == 4  
	trace_printk("%d,%d,%d,%d\n",per_cpu(rtrqCnt,0),per_cpu(rtrqCnt,1),per_cpu(rtrqCnt,2),per_cpu(rtrqCnt,3));
	#endif
	}
}
#endif
