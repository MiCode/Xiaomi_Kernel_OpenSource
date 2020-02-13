/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SCHED_STAT_H
#define _LINUX_SCHED_STAT_H

#include <linux/percpu.h>

/*
 * Various counters maintained by the scheduler and fork(),
 * exposed via /proc, sys.c or used by drivers via these APIs.
 *
 * ( Note that all these values are acquired without locking,
 *   so they can only be relied on in narrow circumstances. )
 */

extern unsigned long total_forks;
extern int nr_threads;
DECLARE_PER_CPU(unsigned long, process_counts);
extern int nr_processes(void);
extern unsigned long nr_running(void);
extern bool single_task_running(void);
extern unsigned long nr_iowait(void);
extern unsigned long nr_iowait_cpu(int cpu);

#ifdef CONFIG_SCHED_WALT
extern void sched_update_nr_prod(int cpu, long delta, bool inc);
extern unsigned int sched_get_cpu_util(int cpu);
extern void sched_update_hyst_times(void);
extern u64 sched_lpm_disallowed_time(int cpu);

extern int sched_wake_up_idle_show(struct seq_file *m, void *v);
extern ssize_t sched_wake_up_idle_write(struct file *file,
		const char __user *buf, size_t count, loff_t *offset);
extern int sched_wake_up_idle_open(struct inode *inode,
						struct file *filp);

extern int sched_init_task_load_show(struct seq_file *m, void *v);
extern ssize_t
sched_init_task_load_write(struct file *file, const char __user *buf,
					size_t count, loff_t *offset);
extern int
sched_init_task_load_open(struct inode *inode, struct file *filp);

extern int sched_group_id_show(struct seq_file *m, void *v);
extern ssize_t
sched_group_id_write(struct file *file, const char __user *buf,
					size_t count, loff_t *offset);
extern int sched_group_id_open(struct inode *inode, struct file *filp);
#else
static inline void sched_update_nr_prod(int cpu, long delta, bool inc) {}
static inline unsigned int sched_get_cpu_util(int cpu)
{
	return 0;
}
static inline void sched_update_hyst_times(void) {}
static inline u64 sched_lpm_disallowed_time(int cpu)
{
	return 0;
}
#endif

static inline int sched_info_on(void)
{
#ifdef CONFIG_SCHEDSTATS
	return 1;
#elif defined(CONFIG_TASK_DELAY_ACCT)
	extern int delayacct_on;
	return delayacct_on;
#else
	return 0;
#endif
}

#ifdef CONFIG_SCHEDSTATS
void force_schedstat_enabled(void);
#endif

#endif /* _LINUX_SCHED_STAT_H */
