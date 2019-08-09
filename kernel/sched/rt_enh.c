/*
 * Copyright (C) 2017 MediaTek Inc.
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

bool __weak sched_rt_boost(void)
{
	return 0;
}

#ifdef CONFIG_MTK_RT_ENHANCEMENT
static int find_lowest_rq_in_domain(struct cpumask *domain_cpu_mask,
	struct task_struct *task, struct cpumask *lowest_mask)
{
	int cpu, lowest_prio = 0;
	struct rq *rq = NULL;

	cpumask_clear(lowest_mask);

	for_each_cpu(cpu, domain_cpu_mask) {
		int prio;

		if (!cpu_online(cpu))
			continue;

		if (cpu_isolated(cpu))
			continue;

		rq = cpu_rq(cpu);
		prio = rq->rt.highest_prio.curr;

		/* If the highest priority of CPU is higher than lowest_prio
		 * or higher than the task, then bypass
		 */
		if ((prio < lowest_prio) || (prio <= task->prio))
			continue;

		if (!cpumask_test_cpu(cpu, tsk_cpus_allowed(task)))
			continue;

		/* If the priority lower than lowest_prio */
		if (prio > lowest_prio) {
			lowest_prio = prio;
			cpumask_clear(lowest_mask);
		}

		cpumask_set_cpu(cpu, lowest_mask);
	}

	if (cpumask_empty(lowest_mask)) {
		mt_sched_printf(sched_rt_info, "%s not find", __func__);
		return 0;
	}

	mt_sched_printf(sched_rt_info, "%s find %d:%s:%d %d:%lu",
		__func__, task->pid, task->comm, task->prio,
		lowest_prio, (unsigned long)lowest_mask->bits[0]);
	return 1;
}

int find_lowest_rq_in_hmp(struct task_struct *task, struct cpumask *lowest_mask)
{
	struct list_head *pos;

	/* order: fast to slow hmp_domain */
	list_for_each(pos, &hmp_domains) {
		struct hmp_domain *domain;

		domain = list_entry(pos, struct hmp_domain, hmp_domains);
		if (!cpumask_empty(&domain->cpus)) {
			/* return if find*/
			if (find_lowest_rq_in_domain(&domain->possible_cpus,
					task, lowest_mask))
				return 1;
		}
	}

	return 0;
}

int select_task_rq_rt_boost(struct task_struct *p, int cpu)
{
	struct task_struct *curr;
	struct rq *rq;

	rq = cpu_rq(cpu);
	rcu_read_lock();
	curr = READ_ONCE(rq->curr); /* unlocked access */

	if ((p->nr_cpus_allowed > 1) || cpu_isolated(cpu)) {
		int target = find_lowest_rq(p);

		/*
		 * Don't bother moving it if the destination CPU is
		 * not running a lower priority task.
		 */
		if (target != -1 &&
		    p->prio < cpu_rq(target)->rt.highest_prio.curr)
			cpu = target;
	}
	rcu_read_unlock();

	return cpu;
}

/*
 * rt_active_task_migration_cpu_stop is run by cpu stopper and used to
 * migrate a specific task from one runqueue to another.
 * Based on active_load_balance_stop_cpu and can potentially be merged.
 */
int rt_active_task_migration_cpu_stop(void *data)
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
	if ((!p) || (task_rq(p) != busiest_rq) || p->on_rq != TASK_ON_RQ_QUEUED)
		goto out_unlock;
	/*
	 * This condition is "impossible", if it occurs
	 * we need to fix it. Originally reported by
	 * Bjorn Helgaas on a 128-cpu setup.
	 */
	WARN_ON(busiest_rq == target_rq);

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
			.sd             = sd,
			.dst_cpu        = target_cpu,
			.dst_rq         = target_rq,
			.src_cpu        = busiest_rq->cpu,
			.src_rq         = busiest_rq,
			.idle           = CPU_IDLE,
		};

		move_task(p, &env);
	}
	rcu_read_unlock();
	double_unlock_balance(busiest_rq, target_rq);
out_unlock:
	busiest_rq->active_balance = 0;
	raw_spin_unlock_irq(&busiest_rq->lock);

	put_task_struct(p);
	return 0;
}

static int rt_force_migrate_cpu_stop(void *data)
{
	return rt_active_task_migration_cpu_stop(data);
}

static int find_highest_prio_in_cluster(struct cpumask *domain_cpu_mask,
		struct rq *this_rq)
{
	int cpu, prio, this_cpu = this_rq->cpu, highest_prio;
	struct rq *rq = NULL, *target;
	struct cpumask *lowest_mask = this_cpu_cpumask_var_ptr(local_cpu_mask);
	unsigned int force = 0;
	struct task_struct *p;
	unsigned long flags;

	highest_prio = MAX_RT_PRIO;
	cpumask_clear(lowest_mask);

	raw_spin_lock_irqsave(&this_rq->lock, flags);

	for_each_cpu(cpu, domain_cpu_mask) {
		if (!cpu_online(cpu))
			continue;

		rq = cpu_rq(cpu);
		if (rq->rt.rt_nr_running == 0)
			continue;

		prio = rq->rt.highest_prio.curr;

		/* If the highest priority of LITTLE CPU is smaller and equal
		 * than current, then bypass
		 */
		if (prio >= this_rq->rt.highest_prio.curr)
			continue;

		/* If the prority of LITTLE CPU is smaller than
		 * highest_prio of LITTLE CPUs
		 */
		if (prio > highest_prio)
			continue;

		/* check the affinity */
		if (!cpumask_test_cpu(this_rq->cpu, tsk_cpus_allowed(rq->curr)))
			continue;

		if (prio < highest_prio) {

			highest_prio = prio;
			cpumask_clear(lowest_mask);
		}

		cpumask_set_cpu(cpu, lowest_mask);
	}

	if (cpumask_empty(lowest_mask)) {
		mt_sched_printf(sched_rt_info, "%s not found", __func__);
		raw_spin_unlock_irqrestore(&this_rq->lock, flags);
		return 0;
	}

	raw_spin_unlock_irqrestore(&this_rq->lock, flags);
	cpu = cpumask_any(lowest_mask);
	target = cpu_rq(cpu);
	raw_spin_lock_irqsave(&target->lock, flags);

	p = target->curr;
	if (!target->active_balance && p->prio == highest_prio &&
		!cpu_park(cpu_of(target))) {
		if (p->state != TASK_DEAD) {
			get_task_struct(p);
			target->active_balance = 1; /* force up */
			target->push_cpu = this_cpu;
			target->migrate_task = p;
			force = 1;
			mt_sched_printf(sched_rt_info,
				"%s src_cpu=%d %d:%s push_cpu=%d",
				__func__, cpu, p->pid, p->comm, this_cpu);
		}
	}
	raw_spin_unlock_irqrestore(&target->lock, flags);

	if (force) {
		if (stop_one_cpu_dispatch(cpu_of(target),
				rt_force_migrate_cpu_stop,
				target, &target->active_balance_work)) {
			put_task_struct(p);
			raw_spin_lock_irqsave(&target->lock, flags);
			target->active_balance = 0;
			raw_spin_unlock_irqrestore(&target->lock, flags);
		}
	}

	return 1;
}

static int find_highest_prio_in_slower(struct rq *this_rq)
{
	int this_cpu = this_rq->cpu;
	struct list_head *pos;

	/* from slower */
	list_for_each(pos, &hmp_cpu_domain(this_cpu)->hmp_domains) {
		struct hmp_domain *domain;
		struct cpumask *domain_cpumask;

		domain = list_entry(pos, struct hmp_domain, hmp_domains);
		domain_cpumask = &domain->cpus;

		if (find_highest_prio_in_cluster(domain_cpumask, this_rq))
			return 1;

		if (list_is_last(pos, &hmp_domains))
			break;
	}

	return 0;
}

int mt_post_schedule(struct rq *rq)
{
	int this_cpu = rq->cpu, ret = 0;

	if (!hmp_cpu_is_slowest(this_cpu))
		ret = find_highest_prio_in_slower(rq);

	return ret;
}

#else
inline int select_task_rq_rt_boost(struct task_struct *p, int cpu)
{
	return cpu;
}

inline int
find_lowest_rq_in_hmp(struct task_struct *task, struct cpumask *lowest_mask)
{
	return 0;
}

inline int mt_post_schedule(struct rq *rq)
{
	return 0;
}

#endif
