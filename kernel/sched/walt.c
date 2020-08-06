// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#include "sched.h"
#include "walt.h"

int __weak sched_wake_up_idle_show(struct seq_file *m, void *v)
{
	return -EPERM;
}

ssize_t __weak sched_wake_up_idle_write(struct file *file,
		const char __user *buf, size_t count, loff_t *offset)
{
	return -EPERM;
}

int __weak sched_wake_up_idle_open(struct inode *inode,	struct file *filp)
{
	return -EPERM;
}

int __weak sched_init_task_load_show(struct seq_file *m, void *v)
{
	return -EPERM;
}

ssize_t __weak
sched_init_task_load_write(struct file *file, const char __user *buf,
					size_t count, loff_t *offset)
{
	return -EPERM;
}

int __weak sched_init_task_load_open(struct inode *inode, struct file *filp)
{
	return -EPERM;
}

int __weak sched_group_id_show(struct seq_file *m, void *v)
{
	return -EPERM;
}

ssize_t __weak sched_group_id_write(struct file *file, const char __user *buf,
					size_t count, loff_t *offset)
{
	return -EPERM;
}

int __weak sched_group_id_open(struct inode *inode, struct file *filp)
{
	return -EPERM;
}

int __weak sched_isolate_cpu(int cpu) { return 0; }

int __weak sched_unisolate_cpu(int cpu) { return 0; }

int __weak sched_unisolate_cpu_unlocked(int cpu) { return 0; }

int __weak register_cpu_cycle_counter_cb(struct cpu_cycle_counter_cb *cb)
{
	return 0;
}

void __weak sched_update_cpu_freq_min_max(const cpumask_t *cpus, u32 fmin,
							u32 fmax) { }

void __weak free_task_load_ptrs(struct task_struct *p) { }

int __weak core_ctl_set_boost(bool boost) { return 0; }

void __weak core_ctl_notifier_register(struct notifier_block *n) { }

void __weak core_ctl_notifier_unregister(struct notifier_block *n) { }

void __weak sched_update_nr_prod(int cpu, long delta, bool inc) { }

unsigned int __weak sched_get_cpu_util(int cpu) { return 0; }

void __weak sched_update_hyst_times(void) { }

u64 __weak sched_lpm_disallowed_time(int cpu) { return 0; }

int __weak
walt_proc_group_thresholds_handler(struct ctl_table *table, int write,
			void __user *buffer, size_t *lenp, loff_t *ppos)
{
	return -ENOSYS;
}

int __weak
walt_proc_user_hint_handler(struct ctl_table *table, int write,
			void __user *buffer, size_t *lenp, loff_t *ppos)
{
	return -ENOSYS;
}

int __weak
sched_updown_migrate_handler(struct ctl_table *table, int write,
			void __user *buffer, size_t *lenp, loff_t *ppos)
{
	return -ENOSYS;
}

int __weak
sched_ravg_window_handler(struct ctl_table *table, int write,
			void __user *buffer, size_t *lenp, loff_t *ppos)
{
	return -ENOSYS;
}

int __weak sched_boost_handler(struct ctl_table *table, int write,
			void __user *buffer, size_t *lenp, loff_t *ppos)
{
	return -ENOSYS;
}

int __weak sched_busy_hyst_handler(struct ctl_table *table, int write,
			void __user *buffer, size_t *lenp, loff_t *ppos)
{
	return -ENOSYS;
}

u64 __weak sched_ktime_clock(void) { return 0; }

unsigned long __weak
cpu_util_freq_walt(int cpu, struct walt_cpu_load *walt_load)
{
	return cpu_util(cpu);
}

int __weak update_preferred_cluster(struct walt_related_thread_group *grp,
			struct task_struct *p, u32 old_load, bool from_tick)
{
	return 0;
}

void __weak set_preferred_cluster(struct walt_related_thread_group *grp) { }

void __weak add_new_task_to_grp(struct task_struct *new) { }

int __weak
preferred_cluster(struct walt_sched_cluster *cluster, struct task_struct *p)
{
	return -1;
}

int __weak sync_cgroup_colocation(struct task_struct *p, bool insert)
{
	return 0;
}

int __weak alloc_related_thread_groups(void) { return 0; }

void __weak check_for_migration(struct rq *rq, struct task_struct *p) { }

unsigned long __weak thermal_cap(int cpu)
{
	return cpu_rq(cpu)->cpu_capacity_orig;
}

void __weak clear_walt_request(int cpu) { }

void __weak clear_ed_task(struct task_struct *p, struct rq *rq) { }

bool __weak early_detection_notify(struct rq *rq, u64 wallclock)
{
	return 0;
}

void __weak note_task_waking(struct task_struct *p, u64 wallclock) { }

int __weak group_balance_cpu_not_isolated(struct sched_group *sg)
{
	return group_balance_cpu(sg);
}

void __weak detach_one_task_core(struct task_struct *p, struct rq *rq,
					struct list_head *tasks) { }

void __weak attach_tasks_core(struct list_head *tasks, struct rq *rq) { }

void __weak walt_update_task_ravg(struct task_struct *p, struct rq *rq,
				int event, u64 wallclock, u64 irqtime) { }

void __weak fixup_busy_time(struct task_struct *p, int new_cpu) { }

void __weak init_new_task_load(struct task_struct *p) { }

void __weak mark_task_starting(struct task_struct *p) { }

void __weak set_window_start(struct rq *rq) { }

bool __weak do_pl_notif(struct rq *rq) { return false; }

void __weak walt_sched_account_irqstart(int cpu, struct task_struct *curr) { }
void __weak walt_sched_account_irqend(int cpu, struct task_struct *curr,
				      u64 delta)
{
}

void __weak update_cluster_topology(void) { }

void __weak init_clusters(void) { }

void __weak walt_sched_init_rq(struct rq *rq) { }

void __weak walt_update_cluster_topology(void) { }

void __weak walt_task_dead(struct task_struct *p) { }

#if defined(CONFIG_UCLAMP_TASK_GROUP)
void __weak walt_init_sched_boost(struct task_group *tg) { }
#endif
