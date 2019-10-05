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
#include <linux/stop_machine.h>
static inline unsigned long task_util(struct task_struct *p);
static int select_max_spare_capacity(struct task_struct *p, int target);
int cpu_eff_tp = 1024;
unsigned long long big_cpu_eff_tp = 1024;

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
#define ARM_V8_2
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
	int max_cap_orig_cpu;
	bool overutilized =  sd_overutilized(env->sd);
	int i;

	if (!sched_feat(SCHED_MTK_EAS))
		return;

	this_cpu = smp_processor_id();
	max_cap_orig_cpu = cpu_rq(this_cpu)->rd->max_cap_orig_cpu;
	if (max_cap_orig_cpu > -1)
		max_capacity = capacity_orig_of(max_cap_orig_cpu);
	else
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
		if (intra_overutil == true)
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
	}

	return 0;
}
late_initcall_sync(init_cpu_info)

#ifdef CONFIG_MTK_UNIFY_POWER
void set_sched_turn_point_cap(void)
{
	int turn_point_idx;
	struct hmp_domain *domain;
	int cpu;
	const struct sched_group_energy *sge_core;

	domain = list_entry(hmp_domains.prev, struct hmp_domain, hmp_domains);
	cpu = cpumask_first(&domain->possible_cpus);
	sge_core = cpu_core_energy(cpu);

	turn_point_idx = max(upower_get_turn_point() - 1, 0);
	cpu_eff_tp = sge_core->cap_states[turn_point_idx].cap;
}
#else
void set_sched_turn_point_cap(void)
{
	return;
}
#endif

#if defined(CONFIG_SCHED_HMP) || defined(CONFIG_MTK_IDLE_BALANCE_ENHANCEMENT)

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
	int ret;
	struct task_struct *p = ((struct rq *)data)->migrate_task;

	ret = active_load_balance_cpu_stop(data);
	put_task_struct(p);
	return ret;
}

static int
migrate_running_task(int this_cpu, struct task_struct *p, struct rq *target)
{
	unsigned long flags;
	unsigned int force = 0;

	/* now we have a candidate */
	raw_spin_lock_irqsave(&target->lock, flags);
	if (!target->active_balance &&
			(task_rq(p) == target) && !cpu_park(cpu_of(target)) &&
			p->state != TASK_DEAD) {
		get_task_struct(p);
		target->push_cpu = this_cpu;
		target->migrate_task = p;
		trace_sched_hmp_migrate(p, target->push_cpu, MIGR_IDLE_RUNNING);
#ifdef CONFIG_SCHED_HMP
		hmp_next_up_delay(&p->se, target->push_cpu);
#endif
		target->active_balance = MIGR_IDLE_RUNNING; /* idle pull */
		force = 1;
	}
	raw_spin_unlock_irqrestore(&target->lock, flags);
	if (force) {
		if (!stop_one_cpu_nowait(cpu_of(target),
					hmp_idle_pull_cpu_stop,
					target, &target->active_balance_work)) {
			put_task_struct(p); /* out of rq->lock */
			raw_spin_lock_irqsave(&target->lock, flags);
			target->active_balance = 0;
			target->migrate_task = NULL;
			force = 0;
			raw_spin_unlock_irqrestore(&target->lock, flags);
		}
	}

	return force;
}
#endif

inline unsigned long cluster_max_capacity(void)
{
	struct hmp_domain *domain;
	unsigned int max_capacity = 0;

	for_each_hmp_domain_L_first(domain) {
		int cpu;
		unsigned long capacity;

		cpu = cpumask_first(&domain->possible_cpus);
		capacity = capacity_of(cpu);

		if (capacity > max_capacity)
			max_capacity = capacity;
	}

	return max_capacity;
}

inline unsigned long task_uclamped_min_w_ceiling(struct task_struct *p)
{
	unsigned long max_capacity = cluster_max_capacity();

	return min_t(unsigned int, uclamp_task_effective_util(p, UCLAMP_MIN),
			max_capacity);
}

/* Calculte util with DVFS margin */
inline unsigned int freq_util(unsigned long util)
{
	return util * 10 >> 3;
}

#ifdef CONFIG_MTK_IDLE_BALANCE_ENHANCEMENT
bool idle_lb_enhance(struct task_struct *p, int cpu)
{
	int target_capacity = capacity_orig_of(cpu);

	if (schedtune_prefer_idle(p))
		return 1;

	if (uclamp_task_effective_util(p, UCLAMP_MIN) > target_capacity)
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
	int src_capacity;
	unsigned int util_min;
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
	cfs_rq = &cpu_rq(cpu)->cfs;
	se = __pick_first_entity(cfs_rq);
	while (num_tasks && se) {
		if (entity_is_task(se) &&
		    cpumask_intersects(hmp_target_mask,
				       &(task_of(se)->cpus_allowed))) {
			struct task_struct *p;

			p = task_of(se);
			util_min = uclamp_task_effective_util(p, UCLAMP_MIN);

#ifdef CONFIG_MTK_SCHED_BOOST
			if (!task_prefer_match_on_cpu(p, cpu, target_cpu))
				return se;
#endif

			if (check_min_cap && util_min >= src_capacity)
				return se;

			if (schedtune_prefer_idle(task_of(se)) &&
					cpu_rq(cpu)->nr_running > 1) {
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
	int turning;

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
			if (se && entity_is_task(se) &&
			     (uclamp_task_effective_util(task_of(se),
				UCLAMP_MIN) >= target_capacity) &&
			     cpumask_test_cpu(this_cpu,
					      &((task_of(se))->cpus_allowed))) {
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
						&(task_of(se)->cpus_allowed))) {
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

/*
 * rq: src rq
 */
static int
migrate_runnable_task(struct task_struct *p, int dst_cpu,
					struct rq *rq)
{
	struct rq_flags rf;
	int moved = 0;
	int src_cpu = cpu_of(rq);

	raw_spin_lock(&p->pi_lock);
	rq_lock(rq, &rf);

	/* Are both target and busiest cpu online */
	if (!cpu_online(src_cpu) || !cpu_online(dst_cpu) ||
		cpu_isolated(src_cpu) || cpu_isolated(dst_cpu))
		goto out_unlock;

	/* Task has migrated meanwhile, abort forced migration */
	/* can't migrate running task */
	if (task_running(rq, p))
		goto out_unlock;

	/*
	 * If task_rq(p) != rq, it cannot be migrated here, because we're
	 * holding rq->lock, if p->on_rq == 0 it cannot get enqueued because
	 * we're holding p->pi_lock.
	 */
	if (task_rq(p) == rq) {
		if (task_on_rq_queued(p)) {
			rq = __migrate_task(rq, &rf, p, dst_cpu);
			moved = 1;
		}
	}

out_unlock:
	rq_unlock(rq, &rf);
	raw_spin_unlock(&p->pi_lock);

	return moved;
}

static unsigned int aggressive_idle_pull(int this_cpu)
{
	int moved = 0;
	struct rq *target = NULL;
	struct task_struct *p = NULL;

	if (!sched_smp_initialized)
		return 0;

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
		util = cpu_util_without(cpu, eenv->p);

		/*
		 * If we are looking at the target CPU specified by the eenv,
		 * then we should add the (estimated) utilization of the task
		 * assuming we will wake it up on that CPU.
		 */
		if (unlikely(cpu == eenv->cpu[cpu_idx].cpu_id))
			util += eenv->util_delta;

		util_sum += __cpu_norm_util(util, capacity);

		trace_group_norm_util(cpu_idx, cpu, cid, util_sum,
			__cpu_norm_util(util, capacity), eenv->util_delta,
			util, capacity);
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

		util = cpu_util_without(cpu, eenv->p);

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

unsigned int capacity_margin_dvfs = 1280;
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

#ifdef ARM_V8_2
const struct sched_group_energy * const cci_energy(void)
{
	struct sched_group_energy *sge = &cci_tbl;
	struct upower_tbl_info **addr_ptr_tbl_info;
	struct upower_tbl_info *ptr_tbl_info;
	struct upower_tbl *ptr_tbl;

	addr_ptr_tbl_info = upower_get_tbl();
	ptr_tbl_info = *addr_ptr_tbl_info;
	ptr_tbl = ptr_tbl_info[UPOWER_BANK_CCI].p_upower_tbl;

	sge->nr_cap_states = ptr_tbl->row_num;
	sge->cap_states = ptr_tbl->row;
	sge->lkg_idx = ptr_tbl->lkg_idx;
	return sge;
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
#endif

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
#ifdef ARM_V8_2
		else if (*co_buck_cid ==  CCI_ID)    /* CCI + DSU */
			co_buck_cap_idx = get_cci_cap_idx();
#endif
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

#ifdef ARM_V8_2
	if ((sd_level != 0) && (co_buck_cid == CCI_ID)) {
		struct upower_tbl_row *CCI_pwr_tbl;
		unsigned long lkg_pwr;

		_sge = cci_energy();

		CCI_pwr_tbl = &_sge->cap_states[cap_idx];
		lkg_pwr = CCI_pwr_tbl->lkg_pwr[_sge->lkg_idx];
		energy_cost += lkg_pwr;

		trace_sched_idle_power(sd_level, cap_idx, lkg_pwr, energy_cost);
	}
#endif

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

#ifdef ARM_V8_2
	if ((sd_level != 0) && (co_buck_cid == CCI_ID)) {
		/* CCI + DSU */
		const struct sched_group_energy *_sge;

		_sge = cci_energy();
		energy_cost += calc_busy_power(_sge, co_cap_idx, cap_idx,
							sd_level);
	}
#endif

	return energy_cost;
}
#endif

#ifdef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
void mtk_update_new_capacity(struct energy_env *eenv)
{
	int i, cpu_idx;

	/* To get max opp index of every cluster for power estimation of
	 * share buck
	 */
	for (cpu_idx = EAS_CPU_PRV; cpu_idx < eenv->max_cpu_count ; ++cpu_idx) {
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

#else

static void select_task_prefer_cpu_fair(struct task_struct *p, int *result)
{
}

#endif

inline int
task_match_on_dst_cpu(struct task_struct *p, int src_cpu, int target_cpu)
{
	struct task_struct *target_tsk;
	struct rq *rq = cpu_rq(target_cpu);

	if (task_prefer_match(p, src_cpu))
		return 0;

	target_tsk = rq->curr;
	if (task_prefer_fit(target_tsk, target_cpu))
		return 0;

	return 1;
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

static int collect_cluster_info(int cpu, int *total_nr_running, int *cpu_count)
{
	struct sched_domain *sd;
	struct sched_group *sg;
	int i;

	/* Find SD for the start CPU */
	sd = rcu_dereference(per_cpu(sd_ea, cpu));
	if (!sd)
		return 0;

	*total_nr_running = 0;
	/* Scan CPUs in all SDs */
	sg = sd->groups;

	*cpu_count = cpumask_weight(sched_group_span(sg));
	for_each_cpu(i, sched_group_span(sg)) {
		struct rq *rq = cpu_rq(i);
		*total_nr_running += rq->nr_running;
	}

	return 1;
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
	sysctl_sched_rotation_enable = enable;
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

	if (is_max_capacity_cpu(src_cpu))
		return;

	for_each_possible_cpu(i) {
		struct rq *rq = cpu_rq(i);
		struct task_struct *curr_task = rq->curr;

		if (curr_task &&
			!task_fits_capacity(curr_task, capacity_of(i)))
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

		if (!rq->misfit_task_load || rq->curr->sched_class !=
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
