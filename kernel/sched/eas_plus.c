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
static inline unsigned long task_util(struct task_struct *p);
static int select_max_spare_capacity(struct task_struct *p, int target);
int cpu_eff_tp = 1024;
int tiny_thresh;

#ifndef cpu_isolated
#define cpu_isolated(cpu) 0
#endif

#if defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6758)
 #ifdef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
/* MT6763: 2 gears. cluster 0 & 1 is buck shared. */
static int share_buck[3] = {1, 0, 2};
/* cpu7 is L+ */
 #endif
int l_plus_cpu = 7;
#elif defined(CONFIG_MACH_MT6799)
 #ifdef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
/* MT6799: 3 gears. cluster 0 & 2 is buck shared. */
static int share_buck[3] = {2, 1, 0};
 #endif
/* No L+ */
int l_plus_cpu = -1;
#elif defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6762)
 #ifdef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
static int share_buck[3] = {1, 0, 2};
 #endif
int l_plus_cpu = -1;
#elif defined(CONFIG_MACH_MT6779)
 #ifdef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
static int share_buck[2] = {2, 1};
 #endif
int l_plus_cpu = -1;
#else
 #ifdef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
/* no buck shared */
static int share_buck[3] = {0, 1, 2};
 #endif
int l_plus_cpu = -1;
#endif

#define CCI_ID (arch_get_nr_clusters())


static void
update_system_overutilized(struct lb_env *env)
{
	unsigned long group_util;
	bool intra_overutil = false;
	unsigned long max_capacity;
	struct sched_group *group = env->sd->groups;
	int this_cpu;
	bool overutilized =  sd_overutilized(env->sd);
	int i;

	if (!sched_feat(SCHED_MTK_EAS))
		return;

	this_cpu = smp_processor_id();
	max_capacity = cpu_rq(this_cpu)->rd->max_cpu_capacity.val;

	do {

		group_util = 0;

		for_each_cpu_and(i, sched_group_span(group), env->cpus) {

			if (cpu_isolated(i))
				continue;

			group_util += cpu_util(i);
			if (cpu_overutilized(i)) {
				if (capacity_orig_of(i) < max_capacity) {
					intra_overutil = true;
					break;
				}
			}
		}

		/*
		 * A capacity base hint for over-utilization.
		 * Not to trigger system overutiled if heavy tasks
		 * in Big.cluster, so
		 * add the free room(20%) of Big.cluster is impacted which means
		 * system-wide over-utilization,
		 * that considers whole cluster not single cpu
		 */
		if (group->group_weight > 1 && (group->sgc->capacity * 1024 <
						group_util * 1280))  {
			intra_overutil = true;
			break;
		}

		group = group->next;

	} while (group != env->sd->groups && !intra_overutil);

	if (overutilized != intra_overutil) {
		if (overutilized == true)
			set_sd_overutilized(env->sd);
		else
			clear_sd_overutilized(env->sd);
	}
}

bool is_intra_domain(int prev, int target)
{
#ifdef CONFIG_ARM64
	return (cpu_topology[prev].cluster_id ==
			cpu_topology[target].cluster_id);
#else
	return (cpu_topology[prev].socket_id ==
			cpu_topology[target].socket_id);
#endif
}

static int is_tiny_task(struct task_struct *p)
{
	if (task_util(p) < tiny_thresh)
		return 1;

	return 0;
}

static int
___select_idle_sibling(struct task_struct *p, int prev_cpu, int new_cpu)
{
	if (sched_feat(SCHED_MTK_EAS)) {
#ifdef CONFIG_SCHED_TUNE
		bool prefer_idle = schedtune_prefer_idle(p) > 0;
#else
		bool prefer_idle = true;
#endif
		int idle_cpu;

		idle_cpu = find_best_idle_cpu(p, prefer_idle);
		if (idle_cpu >= 0)
			new_cpu = idle_cpu;
		else
			new_cpu = select_max_spare_capacity(p, new_cpu);
	} else
		new_cpu = select_idle_sibling(p, prev_cpu, new_cpu);

	return new_cpu;
}

/* To find a CPU with max spare capacity in the same cluster with target */
static
int select_max_spare_capacity(struct task_struct *p, int target)
{
	unsigned long int max_spare_capacity = 0;
	int max_spare_cpu = -1;
	struct cpumask cls_cpus;
	int cid = arch_get_cluster_id(target); /* cid of target CPU */
	int cpu = task_cpu(p);
	struct cpumask *tsk_cpus_allow = &p->cpus_allowed;

	/* If the prevous cpu is cache affine and idle, choose it first. */
	if (cpu != l_plus_cpu && cpu != target &&
		cpus_share_cache(cpu, target) &&
		idle_cpu(cpu) && !cpu_isolated(cpu))
		return cpu;

	arch_get_cluster_cpus(&cls_cpus, cid);

	/* Otherwise, find a CPU with max spare-capacity in cluster */
	for_each_cpu_and(cpu, tsk_cpus_allow, &cls_cpus) {
		unsigned long int new_usage;
		unsigned long int spare_cap;

		if (!cpu_online(cpu))
			continue;

		if (cpu_isolated(cpu))
			continue;

#ifdef CONFIG_MTK_SCHED_INTEROP
		if (cpu_rq(cpu)->rt.rt_nr_running &&
			likely(!is_rt_throttle(cpu)))
			continue;
#endif

#ifdef CONFIG_SCHED_WALT
		if (walt_cpu_high_irqload(cpu))
			continue;
#endif

		if (idle_cpu(cpu))
			return cpu;

		new_usage = cpu_util(cpu) + task_util(p);

		if (new_usage >= capacity_of(cpu))
			spare_cap = 0;
		else    /* consider RT/IRQ capacity reduction */
			spare_cap = (capacity_of(cpu) - new_usage);

		/* update CPU with max spare capacity */
		if ((long int)spare_cap > (long int)max_spare_capacity) {
			max_spare_cpu = cpu;
			max_spare_capacity = spare_cap;
		}
	}

	/* if max_spare_cpu exist, choose it. */
	if (max_spare_cpu > -1)
		return max_spare_cpu;
	else
		return task_cpu(p);
}

/*
 * @p: the task want to be located at.
 *
 * Return:
 *
 * cpu id or
 * -1 if target CPU is not found
 */
int find_best_idle_cpu(struct task_struct *p, bool prefer_idle)
{
	int iter_cpu;
	int best_idle_cpu = -1;
	struct cpumask *tsk_cpus_allow = &p->cpus_allowed;
	struct hmp_domain *domain;

	for_each_hmp_domain_L_first(domain) {
		for_each_cpu(iter_cpu, &domain->possible_cpus) {

			/* tsk with prefer idle to find bigger idle cpu */
			int i = ((prefer_idle &&
				(task_util(p) > stune_task_threshold)))
				?  nr_cpu_ids-iter_cpu-1 : iter_cpu;

			if (!cpu_online(i) || cpu_isolated(i) ||
					!cpumask_test_cpu(i, tsk_cpus_allow))
				continue;

#ifdef CONFIG_MTK_SCHED_INTEROP
			if (cpu_rq(i)->rt.rt_nr_running &&
					likely(!is_rt_throttle(i)))
				continue;
#endif

			/* favoring tasks that prefer idle cpus
			 * to improve latency.
			 */
			if (idle_cpu(i)) {
				best_idle_cpu = i;
				break;
			}
		}
	}

	return best_idle_cpu;
}

static int init_cpu_info(void)
{
	int i;

	for (i = 0; i < nr_cpu_ids; i++) {
		unsigned long capacity = SCHED_CAPACITY_SCALE;

		if (cpu_core_energy(i)) {
			int idx = cpu_core_energy(i)->nr_cap_states - 1;

			capacity = cpu_core_energy(i)->cap_states[idx].cap;
		}

		cpu_rq(i)->cpu_capacity_hw = capacity;
	}

	return 0;
}
late_initcall_sync(init_cpu_info)

static int __init parse_dt_eas(void)
{
	struct device_node *cn;
	int ret = 0;
	const u32 *tp;

	cn = of_find_node_by_path("/eas");
	if (!cn) {
		pr_info("No eas information found in DT\n");
		return 0;
	}

	/* turning point */
	tp = of_get_property(cn, "eff_turn_point", &ret);
	if (!tp)
		pr_info("%s missing turning point property\n",
			cn->full_name);
	else
		cpu_eff_tp = be32_to_cpup(tp);

	/* tiny task */
	tp = of_get_property(cn, "tiny", &ret);
	if (!tp)
		pr_info("%s missing turning point property\n",
			cn->full_name);
	else
		tiny_thresh = be32_to_cpup(tp);

	pr_info("eas: turning point=<%d> tiny=<%d>\n", cpu_eff_tp, tiny_thresh);

	of_node_put(cn);
	return ret;
}
core_initcall(parse_dt_eas)

#if defined(CONFIG_SCHED_HMP) || defined(CONFIG_MTK_IDLE_BALANCE_ENHANCEMENT)
static int hmp_can_migrate_task(struct task_struct *p, struct lb_env *env)
{
	int tsk_cache_hot = 0;

	/*
	 * We do not migrate tasks that are:
	 * 1) running (obviously), or
	 * 2) cannot be migrated to this CPU due to cpus_allowed
	 */
	if (!cpumask_test_cpu(env->dst_cpu, &p->cpus_allowed)) {
		schedstat_inc(p->se.statistics.nr_failed_migrations_affine);
		return 0;
	}
	env->flags &= ~LBF_ALL_PINNED;

	if (task_running(env->src_rq, p)) {
		schedstat_inc(p->se.statistics.nr_failed_migrations_running);
		return 0;
	}

	if (idle_lb_enhance(p, env->src_cpu))
		return 1;

	/*
	 * Aggressive migration if:
	 * 1) task is cache cold, or
	 * 2) too many balance attempts have failed.
	 */

	tsk_cache_hot = task_hot(p, env);

#ifdef CONFIG_SCHEDSTATS
	if (env->sd->nr_balance_failed > env->sd->cache_nice_tries
			&& tsk_cache_hot) {
		schedstat_inc(env->sd->lb_hot_gained[env->idle]);
		schedstat_inc(p->se.statistics.nr_forced_migrations);
	}
#endif

	return 1;
}


extern void move_task(struct task_struct *p, struct lb_env *env);
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
		schedstat_inc(env->sd->lb_gained[env->idle]);
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
	if (!cpu_online(busiest_cpu) || !cpu_online(target_cpu) ||
		cpu_isolated(busiest_cpu) || cpu_isolated(target_cpu))
		goto out_unlock;
	/* Task has migrated meanwhile, abort forced migration */
	if ((!p) || (task_rq(p) != busiest_rq))
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

		schedstat_inc(sd->alb_count);

		if (move_specific_task(&env, p))
			schedstat_inc(sd->alb_pushed);
		else
			schedstat_inc(sd->alb_failed);
	}
	rcu_read_unlock();
	double_unlock_balance(busiest_rq, target_rq);
out_unlock:
	busiest_rq->active_balance = 0;
	raw_spin_unlock_irq(&busiest_rq->lock);

	put_task_struct(p);
	return 0;
}

/*
 * Heterogenous Multi-Processor (HMP) Global Load Balance
 */
static DEFINE_SPINLOCK(hmp_force_migration);

/*
 * For debugging purpose,
 * to depart functions of cpu_stop to make call_stack clear.
 */
static int hmp_idle_pull_cpu_stop(void *data)
{
	return hmp_active_task_migration_cpu_stop(data);
}

static int
migrate_running_task(int this_cpu, struct task_struct *p, struct rq *target)
{
	unsigned long flags;
	unsigned int force = 0;

	/* now we have a candidate */
	raw_spin_lock_irqsave(&target->lock, flags);
	if (!target->active_balance &&
			(task_rq(p) == target) && !cpu_park(cpu_of(target))) {
		if (p->state != TASK_DEAD) {
			get_task_struct(p);
			target->push_cpu = this_cpu;
			target->migrate_task = p;
			trace_sched_hmp_migrate(p, target->push_cpu, 4);
#ifdef CONFIG_SCHED_HMP
			hmp_next_up_delay(&p->se, target->push_cpu);
#endif
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

	return force;
}
#endif

#ifdef CONFIG_MTK_IDLE_BALANCE_ENHANCEMENT
bool idle_lb_enhance(struct task_struct *p, int cpu)
{
	if (schedtune_prefer_idle(p))
		return 1;

	return 0;
}

/* must hold runqueue lock for queue se is currently on */
static const int idle_prefer_max_tasks = 5;
static struct sched_entity
*get_idle_prefer_task(int cpu, int target_cpu, int check_min_cap,
		struct task_struct **backup_task, int *backup_cpu)
{
	int num_tasks = idle_prefer_max_tasks;
	const struct cpumask *hmp_target_mask = NULL;
	int target_capacity, src_capacity;
	struct cfs_rq *cfs_rq;
	struct sched_entity *se;

	if (target_cpu >= 0)
		hmp_target_mask = cpumask_of(target_cpu);
	else
		return NULL;

	/* The currently running task is not on the runqueue
	 *	a. idle prefer
	 *	b. task_capacity > belonged CPU
	 */
	src_capacity = capacity_orig_of(cpu);
	target_capacity = capacity_orig_of(target_cpu);
	cfs_rq = &cpu_rq(cpu)->cfs;
	se = __pick_first_entity(cfs_rq);
	while (num_tasks && se) {
		if (entity_is_task(se) &&
		    cpumask_intersects(hmp_target_mask,
				       &(task_of(se)->cpus_allowed))) {
			struct task_struct *p;

			p = task_of(se);

#ifdef CONFIG_MTK_SCHED_BOOST
			if (!task_prefer_match_on_cpu(p, cpu, target_cpu))
				return se;
#endif

			if (schedtune_prefer_idle(task_of(se))) {
				if (!check_min_cap)
					return se;

				if (backup_task && !*backup_task) {
					*backup_cpu = cpu;
					/* get task and selection inside
					 * rq lock
					 */
					*backup_task = task_of(se);
					get_task_struct(*backup_task);
				}
			}
		}
		se = __pick_next_entity(se);
		num_tasks--;
	}

	return NULL;
}

static void
hmp_slowest_idle_prefer_pull(int this_cpu, struct task_struct **p,
			     struct rq **target)
{
	int cpu, backup_cpu;
	struct sched_entity *se = NULL;
	struct task_struct  *backup_task = NULL;
	struct hmp_domain *domain;
	struct list_head *pos;
	int selected = 0;
	struct rq *rq;
	unsigned long flags;
	int check_min_cap;

	/* 1. select a runnable task
	 *     idle prefer
	 *
	 *     order: fast to slow hmp domain
	 */
	check_min_cap = 0;
	list_for_each(pos, &hmp_domains) {
		domain = list_entry(pos, struct hmp_domain, hmp_domains);

		for_each_cpu(cpu, &domain->cpus) {
			if (cpu == this_cpu)
				continue;

			rq = cpu_rq(cpu);
			raw_spin_lock_irqsave(&rq->lock, flags);

			se = get_idle_prefer_task(cpu, this_cpu,
				check_min_cap, &backup_task, &backup_cpu);
			if (se && entity_is_task(se) &&
			    cpumask_test_cpu(this_cpu,
					     &(task_of(se))->cpus_allowed)) {
				selected = 1;
				/* get task and selection inside rq lock  */
				*p = task_of(se);
				get_task_struct(*p);

				*target = rq;
			}

			raw_spin_unlock_irqrestore(&rq->lock, flags);

			if (selected) {
				/* To put task out of rq lock */
				if (backup_task)
					put_task_struct(backup_task);
				return;
			}
		}
	}

	if (backup_task) {
		*target = cpu_rq(backup_cpu);
		return;
	}
}

DECLARE_PER_CPU(struct hmp_domain *, hmp_cpu_domain);
static void
hmp_fastest_idle_prefer_pull(int this_cpu, struct task_struct **p,
						struct rq **target)
{
	int cpu, backup_cpu;
	struct sched_entity *se = NULL;
	struct task_struct  *backup_task = NULL;
	struct hmp_domain *hmp_domain = NULL, *domain;
	struct list_head *pos;
	int selected = 0;
	struct rq *rq;
	unsigned long flags;
	int target_capacity;
	int check_min_cap;

	hmp_domain = per_cpu(hmp_cpu_domain, this_cpu);

	/* 1. select a runnable task
	 *
	 * first candidate:
	 *     capacity_min in slow domain
	 *
	 *     order: target->next to slow hmp domain
	 */
	check_min_cap = 1;
	list_for_each(pos, &hmp_domain->hmp_domains) {
		domain = list_entry(pos, struct hmp_domain, hmp_domains);

		for_each_cpu(cpu, &domain->cpus) {
			if (cpu == this_cpu)
				continue;

			rq = cpu_rq(cpu);
			raw_spin_lock_irqsave(&rq->lock, flags);

			se = get_idle_prefer_task(cpu, this_cpu,
				check_min_cap, &backup_task, &backup_cpu);
			if (se && entity_is_task(se) &&
			    cpumask_test_cpu(this_cpu,
					     &(task_of(se))->cpus_allowed)) {
				selected = 1;
				/* get task and selection inside rq lock  */
				*p = task_of(se);
				get_task_struct(*p);

				*target = rq;
			}

			raw_spin_unlock_irqrestore(&rq->lock, flags);

			if (selected) {
				/* To put task out of rq lock */
				if (backup_task)
					put_task_struct(backup_task);
				return;
			}
		}

		if (list_is_last(pos, &hmp_domains))
			break;
	}

	/* backup candidate:
	 *     idle prefer
	 *
	 *     order: fastest to target hmp domain
	 */
	check_min_cap = 0;
	list_for_each(pos, &hmp_domains) {
		domain = list_entry(pos, struct hmp_domain, hmp_domains);

		for_each_cpu(cpu, &domain->cpus) {
			if (cpu == this_cpu)
				continue;

			rq = cpu_rq(cpu);
			raw_spin_lock_irqsave(&rq->lock, flags);

			se = get_idle_prefer_task(cpu, this_cpu,
				check_min_cap, &backup_task, &backup_cpu);

			if (se && entity_is_task(se) &&
			    cpumask_test_cpu(this_cpu,
					     &(task_of(se)->cpus_allowed))) {
				selected = 1;
				/* get task and selection inside rq lock  */
				*p = task_of(se);
				get_task_struct(*p);

				*target = rq;
			}

			raw_spin_unlock_irqrestore(&rq->lock, flags);

			if (selected) {
				/* To put task out of rq lock */
				if (backup_task)
					put_task_struct(backup_task);
				return;
			}
		}

		if (cpumask_test_cpu(this_cpu, &domain->cpus))
			break;
	}

	if (backup_task) {
		*p = backup_task;
		*target = cpu_rq(backup_cpu);
		return;
	}

	/* 2. select a running task
	 *     order: target->next to slow hmp domain
	 */
	list_for_each(pos, &hmp_domain->hmp_domains) {
		domain = list_entry(pos, struct hmp_domain, hmp_domains);

		for_each_cpu(cpu, &domain->cpus) {
			if (cpu == this_cpu)
				continue;

			rq = cpu_rq(cpu);
			raw_spin_lock_irqsave(&rq->lock, flags);

			se = rq->cfs.curr;
			if (!se) {
				raw_spin_unlock_irqrestore(&rq->lock, flags);
				continue;
			}
			if (!entity_is_task(se)) {
				struct cfs_rq *cfs_rq;

				cfs_rq = group_cfs_rq(se);
				while (cfs_rq) {
					se = cfs_rq->curr;
					if (!entity_is_task(se))
						cfs_rq = group_cfs_rq(se);
					else
						cfs_rq = NULL;
				}
			}

			target_capacity = capacity_orig_of(cpu);
			if (se && entity_is_task(se) &&
			     cpumask_test_cpu(this_cpu,
					      &((task_of(se))->cpus_allowed))) {
				selected = 1;
				/* get task and selection inside rq lock  */
				*p = task_of(se);
				get_task_struct(*p);

				*target = rq;
			}

			raw_spin_unlock_irqrestore(&rq->lock, flags);

			if (selected)
				return;
		}

		if (list_is_last(pos, &hmp_domains))
			break;
	}
}

static int
migrate_runnable_task(struct task_struct *p, int target_cpu,
					struct rq *busiest_rq)
{
	int busiest_cpu = cpu_of(busiest_rq);
	struct rq *target_rq = cpu_rq(target_cpu);
	struct sched_domain *sd;
	int moved = 0;
	unsigned long flags;

	raw_spin_lock_irqsave(&busiest_rq->lock, flags);
	/* Is there any task to move? */
	if (busiest_rq->nr_running <= 1)
		goto out_unlock;
	/* Are both target and busiest cpu online */
	if (!cpu_online(busiest_cpu) || !cpu_online(target_cpu) ||
		cpu_isolated(busiest_cpu) || cpu_isolated(target_cpu))
		goto out_unlock;
	/* Task has migrated meanwhile, abort forced migration */
	if ((!p) || (task_rq(p) != busiest_rq))
		goto out_unlock;
	/*
	 * This condition is "impossible", if it occurs
	 * we need to fix it. Originally reported by
	 * Bjorn Helgaas on a 128-cpu setup.
	 */
	WARN_ON(busiest_rq == target_rq);

	if (task_running(busiest_rq, p))
		goto out_unlock;

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

		schedstat_inc(sd->alb_count);

		moved = move_specific_task(&env, p);
		trace_sched_hmp_migrate(p, env.dst_cpu, 3);
	}
	rcu_read_unlock();
	double_unlock_balance(busiest_rq, target_rq);
out_unlock:
	raw_spin_unlock_irqrestore(&busiest_rq->lock, flags);

	return moved;
}

static unsigned int aggressive_idle_pull(int this_cpu)
{
	int moved = 0;
	struct rq *target = NULL;
	struct task_struct *p = NULL;

	if (!spin_trylock(&hmp_force_migration))
		return 0;

	/*
	 * aggressive idle balance for min_cap/idle_prefer
	 */
	if (hmp_cpu_is_slowest(this_cpu))
		hmp_slowest_idle_prefer_pull(this_cpu, &p, &target);
	else
		hmp_fastest_idle_prefer_pull(this_cpu, &p, &target);

	if (p) {
		moved = migrate_runnable_task(p, this_cpu, target);
		if (moved)
			goto done;

		moved = migrate_running_task(this_cpu, p, target);
	}

done:
	spin_unlock(&hmp_force_migration);
	if (p)
		put_task_struct(p);

	return moved;
}

#else
bool idle_lb_enhance(struct task_struct *p, int cpu)
{
	return 0;
}

static unsigned int aggressive_idle_pull(int this_cpu)
{
	return 0;
}
#endif

#ifdef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
/*
 * group_norm_util() returns the approximated group util relative to it's
 * current capacity (busy ratio) in the range [0..SCHED_CAPACITY_SCALE] for use
 * in energy calculations. Since task executions may or may not overlap in time
 * in the group the true normalized util is between max(cpu_norm_util(i)) and
 * sum(cpu_norm_util(i)) when iterating over all cpus in the group, i. The
 * latter is used as the estimate as it leads to a more pessimistic energy
 * estimate (more busy).
 */
static unsigned
long group_norm_util(struct energy_env *eenv, int cpu_idx)
{
	struct sched_group *sg = eenv->sg;
	int cpu_id = group_first_cpu(sg);
#ifdef CONFIG_ARM64
	int cid = cpu_topology[cpu_id].cluster_id;
#else
	int cid = cpu_topology[cpu_id].socket_id;
#endif
	unsigned long capacity = eenv->cpu[cpu_idx].cap[cid];
	unsigned long util, util_sum = 0;
	int cpu;

	for_each_cpu(cpu, sched_group_span(eenv->sg)) {
		util = cpu_util_wake(cpu, eenv->p);

		/*
		 * If we are looking at the target CPU specified by the eenv,
		 * then we should add the (estimated) utilization of the task
		 * assuming we will wake it up on that CPU.
		 */
		if (unlikely(cpu == eenv->cpu[cpu_idx].cpu_id))
			util += eenv->util_delta;

		util_sum += __cpu_norm_util(util, capacity);

		mt_sched_printf(sched_eas_energy_calc,
			"%s: cpu=%d util_sum=%lu norm_util=%d delta=%d util=%lu capacity=%d",
			__func__, cpu, util_sum,
			(int)__cpu_norm_util(util, capacity),
			eenv->util_delta, util, (int)capacity);
	}

	if (util_sum > SCHED_CAPACITY_SCALE)
		return SCHED_CAPACITY_SCALE;
	return util_sum;
}
#endif


#ifdef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
static unsigned long
mtk_cluster_max_usage(int cid, struct energy_env *eenv, int cpu_idx,
			int *max_cpu)
{
	unsigned long util, max_util = 0;
	int cpu = -1;
	struct cpumask cls_cpus;

	*max_cpu = -1;

	arch_get_cluster_cpus(&cls_cpus, cid);

	for_each_cpu(cpu, &cls_cpus) {

		if (!cpu_online(cpu))
			continue;

		util = cpu_util_wake(cpu, eenv->p);

		/*
		 * If we are looking at the target CPU specified by the eenv,
		 * then we should add the (estimated) utilization of the task
		 * assuming we will wake it up on that CPU.
		 */
		if (unlikely(cpu == eenv->cpu[cpu_idx].cpu_id))
			util += eenv->util_delta;

		if (util >= max_util) {
			max_util = util;
			*max_cpu = cpu;
		}
	}

	return max_util;
}

void mtk_cluster_capacity_idx(int cid, struct energy_env *eenv, int cpu_idx)
{
	int cpu;
	unsigned long util = mtk_cluster_max_usage(cid, eenv, cpu_idx, &cpu);
	unsigned long new_capacity = util;
	struct sched_domain *sd;
	struct sched_group *sg;
	const struct sched_group_energy *sge;
	int idx, max_idx;

	if (cpu == -1) { /* maybe no online CPU */
		printk_deferred("sched: %s no online CPU", __func__);
		return;
	}

	sd = rcu_dereference_check_sched_domain(cpu_rq(cpu)->sd);
	if (sd) {
		sg = sd->groups;
		sge = sg->sge;
	} else{
		printk_deferred("sched: %s no sd", __func__);
		return;
	}

	max_idx = sge->nr_cap_states - 1;

	/* default is max_cap if we don't find a match */
	eenv->cpu[cpu_idx].cap_idx[cid] = max_idx;
	eenv->cpu[cpu_idx].cap[cid] = sge->cap_states[max_idx].cap;

	/* OPP idx to refer capacity margin */
	new_capacity = util * capacity_margin_dvfs >> SCHED_CAPACITY_SHIFT;
	new_capacity = min(new_capacity,
		(unsigned long) sge->cap_states[sge->nr_cap_states-1].cap);

	for (idx = 0; idx < sge->nr_cap_states; idx++) {
		if (sge->cap_states[idx].cap >= new_capacity) {
			/* Keep track of SG's capacity */
			eenv->cpu[cpu_idx].cap_idx[cid] = idx;
			eenv->cpu[cpu_idx].cap[cid] = sge->cap_states[idx].cap;
			break;
		}
	}

	mt_sched_printf(sched_eas_energy_calc,
		"cpu_idx=%d cid=%d max_cpu=%d (util=%ld new=%ld) max_opp=%d (cap=%d)",
		cpu_idx, cid, cpu, util, new_capacity,
		eenv->cpu[cpu_idx].cap_idx[cid],
		eenv->cpu[cpu_idx].cap[cid]);
}

#define VOLT_SCALE 10

bool is_share_buck(int cid, int *co_buck_cid)
{
	bool ret = false;

	if (share_buck[cid] != cid) {
		*co_buck_cid = share_buck[cid];
		ret = true;
	}

	return ret;
}

extern unsigned int mt_cpufreq_get_cur_cci_freq_idx(void);
int get_cci_cap_idx(void)
{
	const struct sched_group_energy *_sge;
	static int CCI_nr_cap_stats;

	if (CCI_nr_cap_stats == 0) {
#ifdef CONFIG_MTK_UNIFY_POWER
		_sge = cci_energy();
		CCI_nr_cap_stats = _sge->nr_cap_states;
#endif
	}

	return CCI_nr_cap_stats - mt_cpufreq_get_cur_cci_freq_idx();
}

int share_buck_cap_idx(struct energy_env *eenv, int cpu_idx,
			int cid, int *co_buck_cid)
{
	int cap_idx = eenv->cpu[cpu_idx].cap_idx[cid];
	int co_buck_cap_idx = -1;

	if (is_share_buck(cid, co_buck_cid)) {
		int num_cluster = arch_get_nr_clusters();

		if (*co_buck_cid < num_cluster)
			co_buck_cap_idx =
				eenv->cpu[cpu_idx].cap_idx[*co_buck_cid];
		else if (*co_buck_cid ==  CCI_ID)    /* CCI + DSU */
			co_buck_cap_idx = get_cci_cap_idx();

		trace_sched_share_buck(cpu_idx, cid, cap_idx, *co_buck_cid,
					co_buck_cap_idx);
	}

	return co_buck_cap_idx;
}

int
mtk_idle_power(int cpu_idx, int idle_state, int cpu, void *argu, int sd_level)
{
	int energy_cost = 0;
	struct sched_domain *sd;
	const struct sched_group_energy *_sge, *sge_core, *sge_clus;
#ifdef CONFIG_ARM64
	int cid = cpu_topology[cpu].cluster_id;
#else
	int cid = cpu_topology[cpu].socket_id;
#endif
	struct energy_env *eenv = (struct energy_env *)argu;
	int cap_idx = eenv->cpu[cpu_idx].cap_idx[cid];
	int co_buck_cid = -1, co_buck_cap_idx;
	int only_lv1 = 0;

	sd = rcu_dereference_check_sched_domain(cpu_rq(cpu)->sd);

	/* [FIXME] racing with hotplug */
	if (!sd)
		return 0;

	/* [FIXME] racing with hotplug */
	if (cap_idx == -1)
		return 0;

	co_buck_cap_idx = share_buck_cap_idx(eenv, cpu_idx, cid, &co_buck_cid);
	cap_idx = max(cap_idx, co_buck_cap_idx);

	_sge = sge_core = sge_clus = NULL;

	/* To handle only 1 CPU in cluster by HPS */
	if (unlikely(!sd->child &&
	   (rcu_dereference(per_cpu(sd_scs, cpu)) == NULL))) {
		sge_core = cpu_core_energy(cpu);
		sge_clus = cpu_cluster_energy(cpu);

		only_lv1 = 1;
	} else {
		if (sd_level == 0)
			_sge = cpu_core_energy(cpu); /* for cpu */
		else
			_sge = cpu_cluster_energy(cpu); /* for cluster */
	}

	idle_state = 0;

	/* active idle: WFI */
	if (only_lv1) {
		struct upower_tbl_row *cpu_pwr_tbl, *clu_pwr_tbl;

		cpu_pwr_tbl = &sge_core->cap_states[cap_idx];
		clu_pwr_tbl = &sge_clus->cap_states[cap_idx];

		/* idle: core->leask_power + cluster->lkg_pwr */
		energy_cost = cpu_pwr_tbl->lkg_pwr[sge_core->lkg_idx] +
				clu_pwr_tbl->lkg_pwr[sge_clus->lkg_idx];

		mt_sched_printf(sched_eas_energy_calc,
			"%s: %s lv=%d tlb_cpu[%d].leak=%d tlb_clu[%d].leak=%d total=%d",
			__func__, "WFI", sd_level,
			cap_idx,
			cpu_pwr_tbl->lkg_pwr[sge_core->lkg_idx],
			cap_idx,
			clu_pwr_tbl->lkg_pwr[sge_clus->lkg_idx],
			energy_cost);
	} else {
		struct upower_tbl_row *pwr_tbl;
		unsigned long lkg_pwr;

		pwr_tbl =  &_sge->cap_states[cap_idx];
		lkg_pwr = pwr_tbl->lkg_pwr[_sge->lkg_idx];
		energy_cost = lkg_pwr;

		trace_sched_idle_power(sd_level, cap_idx, lkg_pwr, energy_cost);
	}

	if ((sd_level != 0) && (co_buck_cid == CCI_ID)) {
		struct upower_tbl_row *CCI_pwr_tbl;
		unsigned long lkg_pwr;

		_sge = cci_energy();

		CCI_pwr_tbl = &_sge->cap_states[cap_idx];
		lkg_pwr = CCI_pwr_tbl->lkg_pwr[_sge->lkg_idx];
		energy_cost += lkg_pwr;

		trace_sched_idle_power(sd_level, cap_idx, lkg_pwr, energy_cost);
	}

	return energy_cost;
}

int calc_busy_power(const struct sched_group_energy *_sge, int cap_idx,
				int co_buck_cap_idx, int sd_level)
{
	int energy_cost;
	unsigned long int volt_factor = 1;

	if (co_buck_cap_idx > cap_idx) {
		/*
		 * calculated power with share-buck impact
		 *
		 * dynamic power = F*V^2
		 *
		 * dyn_pwr  = current_power * (v_max/v_min)^2
		 * lkg_pwr = tlb[idx of v_max].leak;
		 */
		unsigned long v_max = _sge->cap_states[co_buck_cap_idx].volt;
		unsigned long v_min = _sge->cap_states[cap_idx].volt;
		unsigned long dyn_pwr;
		unsigned long lkg_pwr;
		int lkg_idx = _sge->lkg_idx;

		volt_factor = ((v_max*v_max) << VOLT_SCALE) /
				(v_min*v_min);

		dyn_pwr = (_sge->cap_states[cap_idx].dyn_pwr *
				volt_factor) >> VOLT_SCALE;
		lkg_pwr = _sge->cap_states[co_buck_cap_idx].lkg_pwr[lkg_idx];
		energy_cost = dyn_pwr + lkg_pwr;

		trace_sched_busy_power(sd_level, cap_idx,
				_sge->cap_states[cap_idx].dyn_pwr, volt_factor,
				dyn_pwr, co_buck_cap_idx, lkg_pwr,
				energy_cost);

	} else {
		/* No share buck impact */
		unsigned long dyn_pwr;
		unsigned long lkg_pwr;
		int lkg_idx = _sge->lkg_idx;

		dyn_pwr = _sge->cap_states[cap_idx].dyn_pwr;
		lkg_pwr = _sge->cap_states[cap_idx].lkg_pwr[lkg_idx];
		energy_cost = dyn_pwr + lkg_pwr;

		trace_sched_busy_power(sd_level, cap_idx, dyn_pwr,
					volt_factor, dyn_pwr, cap_idx, lkg_pwr,
					energy_cost);

	}

	return energy_cost;
}

int mtk_busy_power(int cpu_idx, int cpu, void *argu, int sd_level)
{
	struct energy_env *eenv = (struct energy_env *)argu;
	struct sched_domain *sd;
	int energy_cost = 0;
#ifdef CONFIG_ARM64
	int cid = cpu_topology[cpu].cluster_id;
#else
	int cid = cpu_topology[cpu].socket_id;
#endif
	int cap_idx = eenv->cpu[cpu_idx].cap_idx[cid];
	int co_cap_idx = -1;
	int co_buck_cid = -1;
	unsigned long int volt_factor = 1;

	sd = rcu_dereference_check_sched_domain(cpu_rq(cpu)->sd);
	/* [FIXME] racing with hotplug */
	if (!sd)
		return 0;

	/* [FIXME] racing with hotplug */
	if (cap_idx == -1)
		return 0;

	co_cap_idx = share_buck_cap_idx(eenv, cpu_idx, cid, &co_buck_cid);

	/* To handle only 1 CPU in cluster by HPS */
	if (unlikely(!sd->child &&
		(rcu_dereference(per_cpu(sd_scs, cpu)) == NULL))) {
		/* fix HPS defeats: only one CPU in this cluster */
		const struct sched_group_energy *sge_core;
		const struct sched_group_energy *sge_clus;

		sge_core = cpu_core_energy(cpu);
		sge_clus = cpu_cluster_energy(cpu);

		if (co_cap_idx > cap_idx) {
			unsigned long v_max;
			unsigned long v_min;
			unsigned long clu_dyn_pwr, cpu_dyn_pwr;
			unsigned long clu_lkg_pwr, cpu_lkg_pwr;
			struct upower_tbl_row *cpu_pwr_tbl, *clu_pwr_tbl;

			v_max = sge_core->cap_states[co_cap_idx].volt;
			v_min = sge_core->cap_states[cap_idx].volt;

			/*
			 * dynamic power = F*V^2
			 *
			 * dyn_pwr  = current_power * (v_max/v_min)^2
			 * lkg_pwr = tlb[idx of v_max].leak;
			 *
			 */
			volt_factor = ((v_max*v_max) << VOLT_SCALE)
						/ (v_min*v_min);

			cpu_dyn_pwr = sge_core->cap_states[cap_idx].dyn_pwr;
			clu_dyn_pwr = sge_clus->cap_states[cap_idx].dyn_pwr;

			energy_cost = ((cpu_dyn_pwr+clu_dyn_pwr)*volt_factor)
						>> VOLT_SCALE;

			/* + leak power of co_buck_cid's opp */
			cpu_pwr_tbl = &sge_core->cap_states[co_cap_idx];
			clu_pwr_tbl = &sge_clus->cap_states[co_cap_idx];
			cpu_lkg_pwr = cpu_pwr_tbl->lkg_pwr[sge_core->lkg_idx];
			clu_lkg_pwr = clu_pwr_tbl->lkg_pwr[sge_clus->lkg_idx];
			energy_cost += (cpu_lkg_pwr + clu_lkg_pwr);

			mt_sched_printf(sched_eas_energy_calc,
				"%s: %s lv=%d tlb[%d].dyn_pwr=(cpu:%d,clu:%d) tlb[%d].leak=(cpu:%d,clu:%d) vlt_f=%ld",
				__func__, "share_buck/only1CPU", sd_level,
				cap_idx,
				sge_core->cap_states[cap_idx].dyn_pwr,
				sge_clus->cap_states[cap_idx].dyn_pwr,
				co_cap_idx,
				cpu_pwr_tbl->lkg_pwr[sge_core->lkg_idx],
				clu_pwr_tbl->lkg_pwr[sge_clus->lkg_idx],
				volt_factor);
			mt_sched_printf(sched_eas_energy_calc,
				"%s: %s total=%d",
				__func__, "share_buck/only1CPU", energy_cost);
		} else {
			struct upower_tbl_row *cpu_pwr_tbl, *clu_pwr_tbl;

			cpu_pwr_tbl = &sge_core->cap_states[cap_idx];
			clu_pwr_tbl = &sge_clus->cap_states[cap_idx];

			energy_cost = cpu_pwr_tbl->dyn_pwr +
					cpu_pwr_tbl->lkg_pwr[sge_core->lkg_idx];

			energy_cost += clu_pwr_tbl->dyn_pwr +
					clu_pwr_tbl->lkg_pwr[sge_clus->lkg_idx];

			mt_sched_printf(sched_eas_energy_calc,
				"%s: %s lv=%d tlb_core[%d].dyn_pwr=(%d,%d) tlb_clu[%d]=(%d,%d) total=%d",
				__func__, "only1CPU", sd_level,
				cap_idx,
				cpu_pwr_tbl->dyn_pwr,
				cpu_pwr_tbl->lkg_pwr[sge_core->lkg_idx],
				cap_idx,
				clu_pwr_tbl->dyn_pwr,
				clu_pwr_tbl->lkg_pwr[sge_clus->lkg_idx],
				energy_cost);
		}
	} else {
		const struct sched_group_energy *_sge;

		if (sd_level == 0)
			_sge = cpu_core_energy(cpu); /* for CPU */
		else
			_sge = cpu_cluster_energy(cpu); /* for cluster */

		energy_cost = calc_busy_power(_sge, cap_idx, co_cap_idx,
							sd_level);

	}

	if ((sd_level != 0) && (co_buck_cid == CCI_ID)) {
		/* CCI + DSU */
		const struct sched_group_energy *_sge;

		_sge = cci_energy();
		energy_cost += calc_busy_power(_sge, co_cap_idx, cap_idx,
							sd_level);
	}

	return energy_cost;
}
#endif

#ifdef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
void mtk_update_new_capacity(struct energy_env *eenv)
{
	int i, cpu_idx;
	int last_cpu_idx = eenv->max_cpu_count - 1;

	/* To get max opp index of every cluster for power estimation of
	 * share buck
	 */
	for (cpu_idx = EAS_CPU_PRV; cpu_idx < last_cpu_idx; ++cpu_idx) {
		if (eenv->cpu[cpu_idx].cpu_id == -1)
			continue;

		for (i = 0; i < arch_get_nr_clusters(); i++)
			mtk_cluster_capacity_idx(i, eenv, cpu_idx);
	}

}
#else
void mtk_update_new_capacity(struct energy_env *eenv)
{
}
#endif
