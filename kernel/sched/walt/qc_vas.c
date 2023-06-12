// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */
#include <linux/irq.h>
#include <linux/delay.h>
#include <trace/events/sched.h>

#include "qc_vas.h"

#ifdef CONFIG_SCHED_WALT
/* 1ms default for 20ms window size scaled to 1024 */
unsigned int sysctl_sched_min_task_util_for_boost = 51;
/* 0.68ms default for 20ms window size scaled to 1024 */
unsigned int sysctl_sched_min_task_util_for_colocation = 35;

int
kick_active_balance(struct rq *rq, struct task_struct *p, int new_cpu)
{
	unsigned long flags;
	int rc = 0;

	/* Invoke active balance to force migrate currently running task */
	raw_spin_lock_irqsave(&rq->lock, flags);
	if (!rq->active_balance) {
		rq->active_balance = 1;
		rq->push_cpu = new_cpu;
		get_task_struct(p);
		rq->wrq.push_task = p;
		rc = 1;
	}
	raw_spin_unlock_irqrestore(&rq->lock, flags);

	return rc;
}

struct walt_rotate_work {
	struct work_struct w;
	struct task_struct *src_task;
	struct task_struct *dst_task;
	int src_cpu;
	int dst_cpu;
};

DEFINE_PER_CPU(struct walt_rotate_work, walt_rotate_works);

void walt_rotate_work_func(struct work_struct *work)
{
	struct walt_rotate_work *wr = container_of(work,
					struct walt_rotate_work, w);
	struct rq *src_rq = cpu_rq(wr->src_cpu), *dst_rq = cpu_rq(wr->dst_cpu);
	unsigned long flags;

	migrate_swap(wr->src_task, wr->dst_task, wr->dst_cpu, wr->src_cpu);

	put_task_struct(wr->src_task);
	put_task_struct(wr->dst_task);

	local_irq_save(flags);
	double_rq_lock(src_rq, dst_rq);

	dst_rq->active_balance = 0;
	src_rq->active_balance = 0;

	double_rq_unlock(src_rq, dst_rq);
	local_irq_restore(flags);

	clear_reserved(wr->src_cpu);
	clear_reserved(wr->dst_cpu);
}

void walt_rotate_work_init(void)
{
	int i;

	for_each_possible_cpu(i) {
		struct walt_rotate_work *wr = &per_cpu(walt_rotate_works, i);

		INIT_WORK(&wr->w, walt_rotate_work_func);
	}
}

#define WALT_ROTATION_THRESHOLD_NS      16000000
void walt_check_for_rotation(struct rq *src_rq)
{
	u64 wc, wait, max_wait = 0, run, max_run = 0;
	int deserved_cpu = nr_cpu_ids, dst_cpu = nr_cpu_ids;
	int i, src_cpu = cpu_of(src_rq);
	struct rq *dst_rq;
	struct walt_rotate_work *wr = NULL;

	if (!walt_rotation_enabled)
		return;

	if (!is_min_capacity_cpu(src_cpu))
		return;

	wc = sched_ktime_clock();
	for_each_possible_cpu(i) {
		struct rq *rq = cpu_rq(i);

		if (!is_min_capacity_cpu(i))
			break;

		if (is_reserved(i))
			continue;

		if (!rq->misfit_task_load || rq->curr->sched_class !=
							&fair_sched_class)
			continue;

		wait = wc - rq->curr->wts.last_enqueued_ts;
		if (wait > max_wait) {
			max_wait = wait;
			deserved_cpu = i;
		}
	}

	if (deserved_cpu != src_cpu)
		return;

	for_each_possible_cpu(i) {
		struct rq *rq = cpu_rq(i);

		if (is_min_capacity_cpu(i))
			continue;

		if (is_reserved(i))
			continue;

		if (rq->curr->sched_class != &fair_sched_class)
			continue;

		if (rq->nr_running > 1)
			continue;

		run = wc - rq->curr->wts.last_enqueued_ts;

		if (run < WALT_ROTATION_THRESHOLD_NS)
			continue;

		if (run > max_run) {
			max_run = run;
			dst_cpu = i;
		}
	}

	if (dst_cpu == nr_cpu_ids)
		return;

	dst_rq = cpu_rq(dst_cpu);

	double_rq_lock(src_rq, dst_rq);
	if (dst_rq->curr->sched_class == &fair_sched_class &&
		!src_rq->active_balance && !dst_rq->active_balance) {
		get_task_struct(src_rq->curr);
		get_task_struct(dst_rq->curr);

		mark_reserved(src_cpu);
		mark_reserved(dst_cpu);
		wr = &per_cpu(walt_rotate_works, src_cpu);

		wr->src_task = src_rq->curr;
		wr->dst_task = dst_rq->curr;

		wr->src_cpu = src_cpu;
		wr->dst_cpu = dst_cpu;
		dst_rq->active_balance = 1;
		src_rq->active_balance = 1;
	}

	double_rq_unlock(src_rq, dst_rq);

	if (wr)
		queue_work_on(src_cpu, system_highpri_wq, &wr->w);
}

DEFINE_RAW_SPINLOCK(migration_lock);
void check_for_migration(struct rq *rq, struct task_struct *p)
{
	int active_balance;
	int new_cpu = -1;
	int prev_cpu = task_cpu(p);
	int ret;

	if (rq->misfit_task_load) {
		if (rq->curr->state != TASK_RUNNING ||
		    rq->curr->nr_cpus_allowed == 1)
			return;

		if (walt_rotation_enabled) {
			raw_spin_lock(&migration_lock);
			walt_check_for_rotation(rq);
			raw_spin_unlock(&migration_lock);
			return;
		}

		raw_spin_lock(&migration_lock);
		rcu_read_lock();
		new_cpu = find_energy_efficient_cpu(p, prev_cpu, 0, 1);
		rcu_read_unlock();
		if ((new_cpu >= 0) && (new_cpu != prev_cpu) &&
		    (capacity_orig_of(new_cpu) > capacity_orig_of(prev_cpu))) {
			active_balance = kick_active_balance(rq, p, new_cpu);
			if (active_balance) {
				mark_reserved(new_cpu);
				raw_spin_unlock(&migration_lock);
				ret = stop_one_cpu_nowait(prev_cpu,
					active_load_balance_cpu_stop, rq,
					&rq->active_balance_work);
				if (!ret)
					clear_reserved(new_cpu);
				else
					wake_up_if_idle(new_cpu);
				return;
			}
		}
		raw_spin_unlock(&migration_lock);
	}
}

int sched_init_task_load_show(struct seq_file *m, void *v)
{
	struct inode *inode = m->private;
	struct task_struct *p;

	p = get_proc_task(inode);
	if (!p)
		return -ESRCH;

	seq_printf(m, "%d\n", sched_get_init_task_load(p));

	put_task_struct(p);

	return 0;
}

ssize_t
sched_init_task_load_write(struct file *file, const char __user *buf,
	    size_t count, loff_t *offset)
{
	struct inode *inode = file_inode(file);
	struct task_struct *p;
	char buffer[PROC_NUMBUF];
	int init_task_load, err;

	memset(buffer, 0, sizeof(buffer));
	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;
	if (copy_from_user(buffer, buf, count)) {
		err = -EFAULT;
		goto out;
	}

	err = kstrtoint(strstrip(buffer), 0, &init_task_load);
	if (err)
		goto out;

	p = get_proc_task(inode);
	if (!p)
		return -ESRCH;

	err = sched_set_init_task_load(p, init_task_load);

	put_task_struct(p);

out:
	return err < 0 ? err : count;
}

int sched_init_task_load_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, sched_init_task_load_show, inode);
}

int sched_group_id_show(struct seq_file *m, void *v)
{
	struct inode *inode = m->private;
	struct task_struct *p;

	p = get_proc_task(inode);
	if (!p)
		return -ESRCH;

	seq_printf(m, "%d\n", sched_get_group_id(p));

	put_task_struct(p);

	return 0;
}

ssize_t
sched_group_id_write(struct file *file, const char __user *buf,
	    size_t count, loff_t *offset)
{
	struct inode *inode = file_inode(file);
	struct task_struct *p;
	char buffer[PROC_NUMBUF];
	int group_id, err;

	memset(buffer, 0, sizeof(buffer));
	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;
	if (copy_from_user(buffer, buf, count)) {
		err = -EFAULT;
		goto out;
	}

	err = kstrtoint(strstrip(buffer), 0, &group_id);
	if (err)
		goto out;

	p = get_proc_task(inode);
	if (!p)
		return -ESRCH;

	err = sched_set_group_id(p, group_id);

	put_task_struct(p);

out:
	return err < 0 ? err : count;
}

int sched_group_id_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, sched_group_id_show, inode);
}

#ifdef CONFIG_SMP
/*
 * Print out various scheduling related per-task fields:
 */
int sched_wake_up_idle_show(struct seq_file *m, void *v)
{
	struct inode *inode = m->private;
	struct task_struct *p;

	p = get_proc_task(inode);
	if (!p)
		return -ESRCH;

	seq_printf(m, "%d\n", sched_get_wake_up_idle(p));

	put_task_struct(p);

	return 0;
}

ssize_t
sched_wake_up_idle_write(struct file *file, const char __user *buf,
	    size_t count, loff_t *offset)
{
	struct inode *inode = file_inode(file);
	struct task_struct *p;
	char buffer[PROC_NUMBUF];
	int wake_up_idle, err;

	memset(buffer, 0, sizeof(buffer));
	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;
	if (copy_from_user(buffer, buf, count)) {
		err = -EFAULT;
		goto out;
	}

	err = kstrtoint(strstrip(buffer), 0, &wake_up_idle);
	if (err)
		goto out;

	p = get_proc_task(inode);
	if (!p)
		return -ESRCH;

	err = sched_set_wake_up_idle(p, wake_up_idle);

	put_task_struct(p);

out:
	return err < 0 ? err : count;
}

int sched_wake_up_idle_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, sched_wake_up_idle_show, inode);
}

int group_balance_cpu_not_isolated(struct sched_group *sg)
{
	cpumask_t cpus;

	cpumask_and(&cpus, sched_group_span(sg), group_balance_mask(sg));
	cpumask_andnot(&cpus, &cpus, cpu_isolated_mask);
	return cpumask_first(&cpus);
}
#endif /* CONFIG_SMP */

#ifdef CONFIG_PROC_SYSCTL
static void sched_update_updown_migrate_values(bool up)
{
	int i = 0, cpu;
	struct walt_sched_cluster *cluster;
	int cap_margin_levels = num_sched_clusters - 1;

	if (cap_margin_levels > 1) {
		/*
		 * No need to worry about CPUs in last cluster
		 * if there are more than 2 clusters in the system
		 */
		for_each_sched_cluster(cluster) {
			for_each_cpu(cpu, &cluster->cpus) {
				if (up)
					sched_capacity_margin_up[cpu] =
					sysctl_sched_capacity_margin_up[i];
				else
					sched_capacity_margin_down[cpu] =
					sysctl_sched_capacity_margin_down[i];
			}

			if (++i >= cap_margin_levels)
				break;
		}
	} else {
		for_each_possible_cpu(cpu) {
			if (up)
				sched_capacity_margin_up[cpu] =
				sysctl_sched_capacity_margin_up[0];
			else
				sched_capacity_margin_down[cpu] =
				sysctl_sched_capacity_margin_down[0];
		}
	}
}

int sched_updown_migrate_handler(struct ctl_table *table, int write,
				void __user *buffer, size_t *lenp,
				loff_t *ppos)
{
	int ret, i;
	unsigned int *data = (unsigned int *)table->data;
	unsigned int *old_val;
	static DEFINE_MUTEX(mutex);
	int cap_margin_levels = num_sched_clusters ? num_sched_clusters - 1 : 0;

	if (cap_margin_levels <= 0)
		return -EINVAL;

	mutex_lock(&mutex);

	if (table->maxlen != (sizeof(unsigned int) * cap_margin_levels))
		table->maxlen = sizeof(unsigned int) * cap_margin_levels;

	if (!write) {
		ret = proc_douintvec_capacity(table, write, buffer, lenp, ppos);
		goto unlock_mutex;
	}

	/*
	 * Cache the old values so that they can be restored
	 * if either the write fails (for example out of range values)
	 * or the downmigrate and upmigrate are not in sync.
	 */
	old_val = kzalloc(table->maxlen, GFP_KERNEL);
	if (!old_val) {
		ret = -ENOMEM;
		goto unlock_mutex;
	}

	memcpy(old_val, data, table->maxlen);

	ret = proc_douintvec_capacity(table, write, buffer, lenp, ppos);

	if (ret) {
		memcpy(data, old_val, table->maxlen);
		goto free_old_val;
	}

	for (i = 0; i < cap_margin_levels; i++) {
		if (sysctl_sched_capacity_margin_up[i] >
				sysctl_sched_capacity_margin_down[i]) {
			memcpy(data, old_val, table->maxlen);
			ret = -EINVAL;
			goto free_old_val;
		}
	}

	sched_update_updown_migrate_values(data ==
					&sysctl_sched_capacity_margin_up[0]);

free_old_val:
	kfree(old_val);
unlock_mutex:
	mutex_unlock(&mutex);

	return ret;
}
#endif /* CONFIG_PROC_SYSCTL */

int sched_isolate_count(const cpumask_t *mask, bool include_offline)
{
	cpumask_t count_mask = CPU_MASK_NONE;

	if (include_offline) {
		cpumask_complement(&count_mask, cpu_online_mask);
		cpumask_or(&count_mask, &count_mask, cpu_isolated_mask);
		cpumask_and(&count_mask, &count_mask, mask);
	} else {
		cpumask_and(&count_mask, mask, cpu_isolated_mask);
	}

	return cpumask_weight(&count_mask);
}

#ifdef CONFIG_HOTPLUG_CPU
static int do_isolation_work_cpu_stop(void *data)
{
	unsigned int cpu = smp_processor_id();
	struct rq *rq = cpu_rq(cpu);
	struct rq_flags rf;

	local_irq_disable();

	irq_migrate_all_off_this_cpu();

	sched_ttwu_pending();

	/* Update our root-domain */
	rq_lock(rq, &rf);

	/*
	 * Temporarily mark the rq as offline. This will allow us to
	 * move tasks off the CPU.
	 */
	if (rq->rd) {
		BUG_ON(!cpumask_test_cpu(cpu, rq->rd->span));
		set_rq_offline(rq);
	}

	migrate_tasks(rq, &rf, false);

	if (rq->rd)
		set_rq_online(rq);
	rq_unlock(rq, &rf);

	clear_walt_request(cpu);
	local_irq_enable();
	return 0;
}

static int do_unisolation_work_cpu_stop(void *data)
{
	watchdog_enable(smp_processor_id());
	return 0;
}

static void sched_update_group_capacities(int cpu)
{
	struct sched_domain *sd;

	mutex_lock(&sched_domains_mutex);
	rcu_read_lock();

	for_each_domain(cpu, sd) {
		int balance_cpu = group_balance_cpu(sd->groups);

		init_sched_groups_capacity(cpu, sd);
		/*
		 * Need to ensure this is also called with balancing
		 * cpu.
		 */
		if (cpu != balance_cpu)
			init_sched_groups_capacity(balance_cpu, sd);
	}

	rcu_read_unlock();
	mutex_unlock(&sched_domains_mutex);
}

static unsigned int cpu_isolation_vote[NR_CPUS];

/*
 * 1) CPU is isolated and cpu is offlined:
 *	Unisolate the core.
 * 2) CPU is not isolated and CPU is offlined:
 *	No action taken.
 * 3) CPU is offline and request to isolate
 *	Request ignored.
 * 4) CPU is offline and isolated:
 *	Not a possible state.
 * 5) CPU is online and request to isolate
 *	Normal case: Isolate the CPU
 * 6) CPU is not isolated and comes back online
 *	Nothing to do
 *
 * Note: The client calling sched_isolate_cpu() is repsonsible for ONLY
 * calling sched_unisolate_cpu() on a CPU that the client previously isolated.
 * Client is also responsible for unisolating when a core goes offline
 * (after CPU is marked offline).
 */
int sched_isolate_cpu(int cpu)
{
	struct rq *rq;
	cpumask_t avail_cpus;
	int ret_code = 0;
	u64 start_time = 0;

	if (trace_sched_isolate_enabled())
		start_time = sched_clock();

	cpu_maps_update_begin();

	cpumask_andnot(&avail_cpus, cpu_online_mask, cpu_isolated_mask);

	if (cpu < 0 || cpu >= nr_cpu_ids || !cpu_possible(cpu) ||
				!cpu_online(cpu) || cpu >= NR_CPUS) {
		ret_code = -EINVAL;
		goto out;
	}

	rq = cpu_rq(cpu);

	if (++cpu_isolation_vote[cpu] > 1)
		goto out;

	/* We cannot isolate ALL cpus in the system */
	if (cpumask_weight(&avail_cpus) == 1) {
		--cpu_isolation_vote[cpu];
		ret_code = -EINVAL;
		goto out;
	}

	/*
	 * There is a race between watchdog being enabled by hotplug and
	 * core isolation disabling the watchdog. When a CPU is hotplugged in
	 * and the hotplug lock has been released the watchdog thread might
	 * not have run yet to enable the watchdog.
	 * We have to wait for the watchdog to be enabled before proceeding.
	 */
	if (!watchdog_configured(cpu)) {
		msleep(20);
		if (!watchdog_configured(cpu)) {
			--cpu_isolation_vote[cpu];
			ret_code = -EBUSY;
			goto out;
		}
	}

	set_cpu_isolated(cpu, true);
	cpumask_clear_cpu(cpu, &avail_cpus);

	/* Migrate timers */
	smp_call_function_any(&avail_cpus, hrtimer_quiesce_cpu, &cpu, 1);
	smp_call_function_any(&avail_cpus, timer_quiesce_cpu, &cpu, 1);

	watchdog_disable(cpu);
	irq_lock_sparse();
	stop_cpus(cpumask_of(cpu), do_isolation_work_cpu_stop, 0);
	irq_unlock_sparse();

	calc_load_migrate(rq);
	update_max_interval();
	sched_update_group_capacities(cpu);

out:
	cpu_maps_update_done();
	trace_sched_isolate(cpu, cpumask_bits(cpu_isolated_mask)[0],
			    start_time, 1);
	return ret_code;
}

/*
 * Note: The client calling sched_isolate_cpu() is repsonsible for ONLY
 * calling sched_unisolate_cpu() on a CPU that the client previously isolated.
 * Client is also responsible for unisolating when a core goes offline
 * (after CPU is marked offline).
 */
int sched_unisolate_cpu_unlocked(int cpu)
{
	int ret_code = 0;
	u64 start_time = 0;

	if (cpu < 0 || cpu >= nr_cpu_ids || !cpu_possible(cpu)
						|| cpu >= NR_CPUS) {
		ret_code = -EINVAL;
		goto out;
	}

	if (trace_sched_isolate_enabled())
		start_time = sched_clock();

	if (!cpu_isolation_vote[cpu]) {
		ret_code = -EINVAL;
		goto out;
	}

	if (--cpu_isolation_vote[cpu])
		goto out;

	set_cpu_isolated(cpu, false);
	update_max_interval();
	sched_update_group_capacities(cpu);

	if (cpu_online(cpu)) {
		stop_cpus(cpumask_of(cpu), do_unisolation_work_cpu_stop, 0);

		/* Kick CPU to immediately do load balancing */
		if (!atomic_fetch_or(NOHZ_KICK_MASK, nohz_flags(cpu)))
			smp_send_reschedule(cpu);
	}

out:
	trace_sched_isolate(cpu, cpumask_bits(cpu_isolated_mask)[0],
			    start_time, 0);
	return ret_code;
}

int sched_unisolate_cpu(int cpu)
{
	int ret_code;

	cpu_maps_update_begin();
	ret_code = sched_unisolate_cpu_unlocked(cpu);
	cpu_maps_update_done();
	return ret_code;
}

/*
 * Remove a task from the runqueue and pretend that it's migrating. This
 * should prevent migrations for the detached task and disallow further
 * changes to tsk_cpus_allowed.
 */
void
detach_one_task_core(struct task_struct *p, struct rq *rq,
						struct list_head *tasks)
{
	lockdep_assert_held(&rq->lock);

	p->on_rq = TASK_ON_RQ_MIGRATING;
	deactivate_task(rq, p, 0);
	list_add(&p->se.group_node, tasks);
}

void attach_tasks_core(struct list_head *tasks, struct rq *rq)
{
	struct task_struct *p;

	lockdep_assert_held(&rq->lock);

	while (!list_empty(tasks)) {
		p = list_first_entry(tasks, struct task_struct, se.group_node);
		list_del_init(&p->se.group_node);

		BUG_ON(task_rq(p) != rq);
		activate_task(rq, p, 0);
		p->on_rq = TASK_ON_RQ_QUEUED;
	}
}
#endif /* CONFIG_HOTPLUG_CPU */
#endif /* CONFIG_SCHED_WALT */
