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
unsigned long long big_cpu_eff_tp = 1024;
int tiny_thresh;

#ifndef cpu_isolated
#define cpu_isolated(cpu) 0
#endif

#ifndef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
int l_plus_cpu = -1;
#endif

static void
update_system_overutilized(struct lb_env *env)
{
	unsigned long group_util;
	bool intra_overutil = false;
	unsigned long max_capacity;
	struct sched_group *group = env->sd->groups;
	int i;

	if (!sched_feat(SCHED_MTK_EAS))
		return;


	do {
		int this_cpu;

		group_util = 0;

		this_cpu = smp_processor_id();
		max_capacity = cpu_rq(this_cpu)->rd->max_cpu_capacity.val;

		for_each_cpu_and(i, sched_group_cpus(group), env->cpus) {

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

	if (!lb_sd_parent(env->sd)) {
		/* Update system-wide over-utilization indicator */
		if (system_overutil != intra_overutil) {
			system_overutil = intra_overutil;
			trace_sched_system_overutilized(system_overutil);
		}
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
__select_idle_sibling(struct task_struct *p, int prev_cpu, int new_cpu)
{
	if (sched_feat(SCHED_MTK_EAS)) {
#ifdef CONFIG_CGROUP_SCHEDTUNE
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
	struct cpumask *tsk_cpus_allow = tsk_cpus_allowed(p);

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
	struct cpumask *tsk_cpus_allow = tsk_cpus_allowed(p);
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

/*
 * Add a system-wide over-utilization indicator which
 * is updated in load-balance.
 */
static bool system_overutil;

static inline bool __system_overutilized(int cpu)
{
	if (sched_feat(SCHED_MTK_EAS))
		return system_overutil;
	else
		return cpu_rq(cpu)->rd->overutilized;
}

bool system_overutilized(int cpu)
{
	return __system_overutilized(cpu);
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

void set_sched_turn_point_cap(void)
{
#ifdef CONFIG_MTK_UNIFY_POWER
	int turn_point_idx;
	struct hmp_domain *domain;
	int cpu;
	const struct sched_group_energy *sge_core;

	domain = list_entry(hmp_domains.prev, struct hmp_domain, hmp_domains);
	cpu = cpumask_first(&domain->possible_cpus);
	sge_core = cpu_core_energy(cpu);

	turn_point_idx = max(upower_get_turn_point() - 1, 0);
	cpu_eff_tp = sge_core->cap_states[turn_point_idx].cap;
#endif
}

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
	if (!cpumask_test_cpu(env->dst_cpu, tsk_cpus_allowed(p))) {
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
			raw_spin_unlock_irqrestore(&target->lock, flags);
		}
	}

	return force;
}
#endif

#ifdef CONFIG_MTK_IDLE_BALANCE_ENHANCEMENT
int __weak schedtune_task_capacity_min(struct task_struct *tsk)
{
	return 0;
}

bool idle_lb_enhance(struct task_struct *p, int cpu)
{
	int target_capacity;

	target_capacity = capacity_orig_of(cpu);

	if (schedtune_task_capacity_min(p) >= target_capacity)
		return 1;

	if (task_uclamped_min(p) >= target_capacity)
		return 1;

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
				       tsk_cpus_allowed(task_of(se)))) {
			struct task_struct *p;

			p = task_of(se);

#ifdef CONFIG_MTK_SCHED_BOOST
			if (!task_prefer_match_on_cpu(p, cpu, target_cpu))
				return se;
#endif

			if (check_min_cap &&
			  (schedtune_task_capacity_min(p) >= src_capacity ||
			  task_uclamped_min(p) >= src_capacity))
				return se;

			if (schedtune_prefer_idle(task_of(se)) &&
					!idle_cpu(cpu)) {
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
					     tsk_cpus_allowed(task_of(se)))) {
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

static int check_freq_turning(void)
{
	struct root_domain *rd = cpu_rq(smp_processor_id())->rd;
	unsigned long capacity_curr_little, capacity_curr_big;

	if (rd->min_cap_orig_cpu < 0 || rd->max_cap_orig_cpu < 0)
		return false;

	capacity_curr_little = capacity_curr_of(rd->min_cap_orig_cpu);
	capacity_curr_big = capacity_curr_of(rd->max_cap_orig_cpu);

	if ((capacity_curr_little > cpu_eff_tp) &&
			(capacity_curr_big <=  big_cpu_eff_tp))
		return true;

	return false;
}

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
	int turning;

	hmp_domain = hmp_cpu_domain(this_cpu);

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
					     tsk_cpus_allowed(task_of(se)))) {
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
					     tsk_cpus_allowed(task_of(se)))) {
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
	 * 3. turning = true, pick a runnable task from slower domain
	 */
	turning = check_freq_turning();
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
			if ((se && entity_is_task(se) &&
			     (schedtune_task_capacity_min(task_of(se)) >=
							 target_capacity ||
				task_uclamped_min(task_of(se)) >=
				target_capacity)) &&
			     cpumask_test_cpu(this_cpu,
					      tsk_cpus_allowed(task_of(se)))) {
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

			if (turning && !backup_task) {
				const struct cpumask *hmp_target_mask = NULL;
				struct cfs_rq *cfs_rq;
				struct sched_entity *se;

				raw_spin_lock_irqsave(&rq->lock, flags);

				hmp_target_mask = cpumask_of(this_cpu);
				cfs_rq = &rq->cfs;
				se = __pick_first_entity(cfs_rq);
				if (se && entity_is_task(se) &&
					    cpumask_intersects(hmp_target_mask,
					       tsk_cpus_allowed(task_of(se)))) {
					backup_cpu = cpu;
					/* get task and selection inside
					 * rq lock
					 */
					backup_task = task_of(se);
					get_task_struct(backup_task);
				}
				raw_spin_unlock_irqrestore(&rq->lock, flags);
			}
		}

		if (list_is_last(pos, &hmp_domains))
			break;
	}

	if (backup_task) {
		*p = backup_task;
		*target = cpu_rq(backup_cpu);
		return;
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
	if (hmp_cpu_is_slowest(this_cpu)) {
		hmp_slowest_idle_prefer_pull(this_cpu, &p, &target);
		if (p) {
			trace_sched_hmp_migrate(p, this_cpu, 0x10);
			moved = migrate_runnable_task(p, this_cpu, target);
			if (moved)
				goto done;
		}
	} else {
		hmp_fastest_idle_prefer_pull(this_cpu, &p, &target);
		if (p) {
			trace_sched_hmp_migrate(p, this_cpu, 0x10);
			moved = migrate_runnable_task(p, this_cpu, target);
			if (moved)
				goto done;

			moved = migrate_running_task(this_cpu, p, target);
		}
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

#ifdef CONFIG_MTK_SCHED_BOOST
static void select_task_prefer_cpu_fair(struct task_struct *p, int *result)
{
	int task_prefer;
	int cpu, new_cpu;

	task_prefer = cpu_prefer(p);

	cpu = (*result & LB_CPU_MASK);

	if (task_prefer_match(p, cpu))
		return;

	new_cpu = select_task_prefer_cpu(p, cpu);

	if ((new_cpu >= 0)  && (new_cpu != cpu))
		*result = new_cpu | LB_HINT;
}

void check_for_hint_migration(struct rq *rq, struct task_struct *p)
{
	int new_cpu;
	int active_balance;
	int cpu = task_cpu(p);

	if (rq->curr->state != TASK_RUNNING ||
		rq->curr->nr_cpus_allowed == 1)
		return;

	if (task_prefer_match(p, cpu))
		return;

	new_cpu = select_task_prefer_cpu(p, cpu);

	if (new_cpu != cpu) {

		active_balance = kick_active_balance(rq, p, new_cpu);
		if (active_balance) {
			stop_one_cpu_nowait(cpu,
					active_load_balance_cpu_stop,
					rq, &rq->active_balance_work);
			trace_sched_hmp_migrate(p, new_cpu, 7);
		}
	}
}

#else

static void select_task_prefer_cpu_fair(struct task_struct *p, int *result)
{
}

void check_for_hint_migration(struct rq *rq, struct task_struct *p)
{
}
#endif

/*
 * Trigger for active balance migration
 */
void migration_kick(int cpu)
{
	struct rq *rq = cpu_rq(cpu);

	if (!test_and_set_bit(MIGRATION_KICK, &rq->rotat_flags))
		smp_send_reschedule(cpu);
}

void migration_kick_cpus(void)
{
	int i;
	struct cpumask kick_mask;

	cpumask_andnot(&kick_mask, cpu_online_mask, cpu_isolated_mask);

	for_each_cpu(i, &kick_mask) {
		if (!is_max_capacity_cpu(i))
			migration_kick(i);
	}
}

int got_migration_kick(void)
{
	int cpu = smp_processor_id();
	struct rq *rq = cpu_rq(cpu);

	return test_bit(MIGRATION_KICK, &rq->rotat_flags);
}

void clear_migration_kick(int cpu)
{
	struct rq *rq = cpu_rq(cpu);

	clear_bit(MIGRATION_KICK, &rq->rotat_flags);
}

struct task_rotate_work {
	struct work_struct w;
	struct task_struct *src_task;
	struct task_struct *dst_task;
	int src_cpu;
	int dst_cpu;
};

static DEFINE_PER_CPU(struct task_rotate_work, task_rotate_works);
unsigned int sysctl_sched_rotation_enable;

void set_sched_rotation_enable(bool enable)
{
	if (enable)
		sysctl_sched_rotation_enable = true;
	else
		sysctl_sched_rotation_enable = false;
}

static void task_rotate_work_func(struct work_struct *work)
{
	struct task_rotate_work *wr = container_of(work,
				struct task_rotate_work, w);

	migrate_swap(wr->src_task, wr->dst_task);

	put_task_struct(wr->src_task);
	put_task_struct(wr->dst_task);

	clear_reserved(wr->src_cpu);
	clear_reserved(wr->dst_cpu);
}

void task_rotate_work_init(void)
{
	int i;

	for_each_possible_cpu(i) {
		struct task_rotate_work *wr = &per_cpu(task_rotate_works, i);

		INIT_WORK(&wr->w, task_rotate_work_func);
	}
}

#define TASK_ROTATION_THRESHOLD_NS	6000000
#define HEAVY_TASK_NUM	4
void task_check_for_rotation(struct rq *src_rq)
{
	u64 wc, wait, max_wait = 0, run, max_run = 0;
	int deserved_cpu = nr_cpu_ids, dst_cpu = nr_cpu_ids;
	int i, src_cpu = cpu_of(src_rq);
	struct rq *dst_rq;
	struct task_rotate_work *wr = NULL;
	int heavy_task = 0;

	if (!sysctl_sched_rotation_enable)
		return;

	if (got_migration_kick())
		return;

	if (is_max_capacity_cpu(src_cpu))
		return;

	for_each_possible_cpu(i) {
		struct rq *rq = cpu_rq(i);
		struct task_struct *curr_task = rq->curr;

		if (curr_task && !task_fits_max(curr_task, i))
			heavy_task += 1;
	}

	if (heavy_task < HEAVY_TASK_NUM)
		return;

	wc = ktime_get_ns();
	for_each_possible_cpu(i) {
		struct rq *rq = cpu_rq(i);

		if (is_max_capacity_cpu(i))
			break;

		if (is_reserved(i))
			continue;

		if (!rq->misfit_task || rq->curr->sched_class !=
						&fair_sched_class)
			continue;

		wait = wc - rq->curr->last_enqueued_ts;
		if (wait > max_wait) {
			max_wait = wait;
			deserved_cpu = i;
		}
	}

	if (deserved_cpu != src_cpu)
		return;

	for_each_possible_cpu(i) {
		struct rq *rq = cpu_rq(i);

		if (!is_max_capacity_cpu(i))
			continue;

		if (is_reserved(i))
			continue;

		if (rq->curr->sched_class != &fair_sched_class)
			continue;

		if (rq->nr_running > 1)
			continue;

		run = wc - rq->curr->last_enqueued_ts;

		if (run < TASK_ROTATION_THRESHOLD_NS)
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
	if (dst_rq->curr->sched_class == &fair_sched_class) {
		get_task_struct(src_rq->curr);
		get_task_struct(dst_rq->curr);

		mark_reserved(src_cpu);
		mark_reserved(dst_cpu);
		wr = &per_cpu(task_rotate_works, src_cpu);

		wr->src_task = src_rq->curr;
		wr->dst_task = dst_rq->curr;

		wr->src_cpu = src_cpu;
		wr->dst_cpu = dst_cpu;
	}
	double_rq_unlock(src_rq, dst_rq);

	if (wr) {
		queue_work_on(src_cpu, system_highpri_wq, &wr->w);
		trace_sched_big_task_rotation(src_cpu, dst_cpu,
					src_rq->curr->pid, dst_rq->curr->pid);
	}
}

