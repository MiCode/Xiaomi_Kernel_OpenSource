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
/* cpu7 is L+ */
int l_plus_cpu = 7;
#else
int l_plus_cpu = -1;
#endif


#ifdef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT

#if defined(CONFIG_MACH_MT6763) || defined(CONFIG_MACH_MT6758)
/* MT6763: 2 gears. cluster 0 & 1 is buck shared. */
static int share_buck[3] = {1, 0, 2};
#elif defined(CONFIG_MACH_MT6799)
/* MT6799: 3 gears. cluster 0 & 2 is buck shared. */
static int share_buck[3] = {2, 1, 0};
#elif defined(CONFIG_MACH_MT6765) || defined(CONFIG_MACH_MT6762)
static int share_buck[3] = {1, 0, 2};
#elif defined(CONFIG_MACH_MT6779)
static int share_buck[2] = {2, 1};
#define ARM_V8_2
int l_plus_cpu = -1;
#elif defined(CONFIG_MACH_MT6893) || \
	(defined(CONFIG_MACH_MT6885) && defined(CONFIG_MTK_SCHED_MULTI_GEARS))
static int share_buck[3] = {0, 2, 1};
#else
/* no buck shared */
static int share_buck[3] = {0, 1, 2};
#endif

#endif

#define CCI_ID (arch_get_nr_clusters())


static void
update_system_overutilized(struct lb_env *env)
{
	unsigned long group_util;
	bool intra_overutil = false;
	unsigned long min_capacity;
	struct sched_group *group = env->sd->groups;
	int this_cpu;
	int min_cap_orig_cpu;
	bool overutilized =  sd_overutilized(env->sd);
	int i;

	if (!sched_feat(SCHED_MTK_EAS))
		return;

	this_cpu = smp_processor_id();
	min_cap_orig_cpu = cpu_rq(this_cpu)->rd->min_cap_orig_cpu;
	if (min_cap_orig_cpu > -1)
		min_capacity = capacity_orig_of(min_cap_orig_cpu);
	else
		return;

	do {

		group_util = 0;

		for_each_cpu_and(i, sched_group_span(group), env->cpus) {

			if (cpu_isolated(i))
				continue;

			group_util += cpu_util(i);
			if (cpu_overutilized(i)) {
				if (capacity_orig_of(i) == min_capacity) {
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
					group_util * capacity_margin)) {
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
	int i;
	int best_idle_cpu = -1;
	struct cpumask *tsk_cpus_allow = &p->cpus_allowed;
	struct hmp_domain *domain;
	int domain_order = 0;
	int prefer_big = prefer_idle && (task_util(p) > stune_task_threshold);

	for_each_hmp_domain_L_first(domain) {
		for_each_cpu(i, &domain->possible_cpus) {

			/* tsk with prefer idle to find bigger idle cpu */
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
				if (!prefer_big) {
					goto find_idle_cpu;
				} else {
#ifdef CONFIG_MTK_SCHED_BL_FIRST
					if (domain_order == 1)
						goto find_idle_cpu;
#endif
				}
			}
		}

		domain_order++;
	}

find_idle_cpu:

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

unsigned long cluster_max_capacity(void)
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
	return util * capacity_margin / SCHED_CAPACITY_SCALE;
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

	if (!raw_spin_trylock(&p->pi_lock))
		return moved;

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

#ifdef CONFIG_UCLAMP_TASK
static __always_inline
unsigned long uclamp_rq_util_with(struct rq *rq, unsigned long util,
					struct task_struct *p)
{
	unsigned long min_util = rq->uclamp.value[UCLAMP_MIN];
	unsigned long max_util = rq->uclamp.value[UCLAMP_MAX];

	if (p) {
		min_util = max_t(unsigned long, min_util,
		  (unsigned long)uclamp_task_effective_util(p, UCLAMP_MIN));
		max_util = max_t(unsigned long, max_util,
		  (unsigned long)uclamp_task_effective_util(p, UCLAMP_MAX));
	}

	/*
	 * Since CPU's {min,max}_util clamps are MAX aggregated considering
	 * RUNNABLE tasks with_different_ clamps, we can end up with an
	 * inversion. Fix it now when the clamps are applied.
	 */
	if (unlikely(min_util >= max_util))
		return min_util;

	return clamp(util, min_util, max_util);
}
#endif

#ifdef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
#define fits_capacity(cap, max) ((cap) * capacity_margin < (max) * 1024)


static unsigned long __cpu_norm_sumutil(unsigned long util,
					unsigned long capacity)
{
	return  (util << SCHED_CAPACITY_SHIFT)/capacity;
}

struct sg_state {
	int cid;
	int cap_idx;
	unsigned long cap;
	unsigned long volt;
	unsigned long max_util;
	unsigned long sum_util;
};

/*
 * compute_energy(): Estimates the energy that @pd would consume if @p was
 * migrated to @dst_cpu. compute_energy() predicts what will be the utilization
 * landscape of @pd's CPUs after the task migration, and uses the Energy Model
 * to compute what would be the energy if we decided to actually migrate that
 * task.
 */
static int
update_sg_util(struct task_struct *p, int dst_cpu,
		const struct cpumask *sg_mask, struct sg_state *sg_env)
{
	int cpu = cpumask_first(sg_mask);
	struct sched_domain *sd;
	const struct sched_group *sg;
	const struct sched_group_energy *sge;
	unsigned long new_util;
	int idx, max_idx;

	sg_env->sum_util = 0;
	sg_env->max_util = 0;

	sge = cpu_core_energy(cpu); /* for CPU */
	/*
	 * The capacity state of CPUs of the current rd can be driven by CPUs
	 * of another rd if they belong to the same pd. So, account for the
	 * utilization of these CPUs too by masking pd with cpu_online_mask
	 * instead of the rd span.
	 *
	 * If an entire pd is outside of the current rd, it will not appear in
	 * its pd list and will not be accounted by compute_energy().
	 */
	for_each_cpu_and(cpu, sg_mask, cpu_online_mask) {
		unsigned long cpu_util, cpu_boosted_util;
		struct task_struct *tsk = cpu == dst_cpu ? p : NULL;

		cpu_util = cpu_util_without(cpu, p);
		cpu_boosted_util = uclamp_rq_util_with(cpu_rq(cpu), cpu_util, p);

		if (tsk)
			cpu_util += task_util_est(p);

		sg_env->sum_util += cpu_util;
		sg_env->max_util = max(sg_env->max_util, cpu_boosted_util);
	}

	/* default is max_cap if we don't find a match */
	max_idx = sge->nr_cap_states - 1;
	sg_env->cap_idx = max_idx;
	sg_env->cap = sge->cap_states[max_idx].cap;

	new_util = sg_env->max_util * capacity_margin >>  SCHED_CAPACITY_SHIFT;
	new_util = min_t(unsigned long, new_util,
		(unsigned long) sge->cap_states[sge->nr_cap_states-1].cap);

	for (idx = 0; idx < sge->nr_cap_states; idx++) {
		if (sge->cap_states[idx].cap >= new_util) {
			/* Keep track of SG's capacity */
			sg_env->cap_idx	= idx;
			sg_env->cap = sge->cap_states[idx].cap;
			sg_env->volt = sge->cap_states[idx].volt;
			break;
		}
	}

	mt_sched_printf(sched_eas_energy_calc,
		"dst_cpu=%d mask=0x%lx sum_util=%lu max_util=%lu new_util=%lu (idx=%d cap=%ld volt=%ld)",
		dst_cpu, sg_mask->bits[0], sg_env->sum_util, sg_env->max_util,
		new_util, sg_env->cap_idx, sg_env->cap, sg_env->volt);

	return 1;
}

unsigned int share_buck_lkg_idx(const struct sched_group_energy *_sge,
				int cpu_idx, unsigned long v_max)
{
	int co_buck_lkg_idx = _sge->nr_cap_states - 1;
	int idx;

	for (idx = cpu_idx; idx < _sge->nr_cap_states; idx++) {
		if (_sge->cap_states[idx].volt >= v_max) {
			co_buck_lkg_idx = idx;
			break;
		}
	}

	return co_buck_lkg_idx;
}

#define VOLT_SCALE 10
void calc_pwr(int sd_level, const struct sched_group_energy *_sge,
		int cap_idx, unsigned long volt, unsigned long co_volt,
		unsigned long *dyn_pwr, unsigned long *lkg_pwr)
{
	unsigned long int volt_factor = 1;

	if (co_volt > volt) {
		/*
		 * calculated power with share-buck impact
		 *
		 * dynamic power = F*V^2
		 *
		 * dyn_pwr  = current_power * (v_max/v_min)^2
		 * lkg_pwr = tlb[idx of v_max].leak;
		 */
		unsigned long v_max = co_volt;
		unsigned long v_min = volt;
		int lkg_idx = _sge->lkg_idx;
		int co_buck_lkg_idx;

		volt_factor = ((v_max*v_max) << VOLT_SCALE) /
				(v_min*v_min);
		*dyn_pwr = (_sge->cap_states[cap_idx].dyn_pwr *
				volt_factor) >> VOLT_SCALE;
		co_buck_lkg_idx = share_buck_lkg_idx(_sge, cap_idx, v_max);
		*lkg_pwr = _sge->cap_states[co_buck_lkg_idx].lkg_pwr[lkg_idx];

		trace_sched_busy_power(sd_level, cap_idx,
				_sge->cap_states[cap_idx].dyn_pwr, volt_factor,
				*dyn_pwr, co_buck_lkg_idx, *lkg_pwr,
				*dyn_pwr + *lkg_pwr);
	} else {
		/* No share buck impact */
		int lkg_idx = _sge->lkg_idx;

		*dyn_pwr = _sge->cap_states[cap_idx].dyn_pwr;
		*lkg_pwr = _sge->cap_states[cap_idx].lkg_pwr[lkg_idx];

		trace_sched_busy_power(sd_level, cap_idx, *dyn_pwr,
					volt_factor, *dyn_pwr, cap_idx,
					*lkg_pwr, *dyn_pwr + *lkg_pwr);

	}
}

/**
 * em_sg_energy() - Estimates the energy consumed by the CPUs of a perf. domain
 * @sd		: performance domain for which energy has to be estimated
 * @max_util	: highest utilization among CPUs of the domain
 * @sum_util	: sum of the utilization of all CPUs in the domain
 *
 * Return: the sum of the energy consumed by the CPUs of the domain assuming
 * a capacity state satisfying the max utilization of the domain.
 */
static inline unsigned long compute_energy_sg(const struct cpumask *sg_cpus,
			struct sg_state *sg_env, struct sg_state *share_env)
{
	int cpu;
	const struct sched_group_energy *_sge;
	unsigned long dyn_pwr, lkg_pwr;
	unsigned long dyn_egy, lkg_egy;
	unsigned long total_energy;
	unsigned long sg_util;

	cpu = cpumask_first(sg_cpus);
	_sge = cpu_core_energy(cpu); /* for CPU */
	calc_pwr(0, _sge,
		sg_env->cap_idx, sg_env->volt, share_env->volt,
		&dyn_pwr, &lkg_pwr);

	sg_util = __cpu_norm_sumutil(sg_env->sum_util, sg_env->cap);
	dyn_egy = sg_util * dyn_pwr;
	lkg_egy = SCHED_CAPACITY_SCALE * lkg_pwr;
	total_energy = dyn_egy + lkg_egy;

	mt_sched_printf(sched_eas_energy_calc,
			"sg_util=%lu dyn_egy=%d lkg_egy=%d (cost=%d) mask=0x%lx",
			sg_util,
			(int)dyn_egy, (int)lkg_egy, (int)total_energy,
			sg_cpus->bits[0]);

	return total_energy;
}

bool is_share_buck(int cid, int *co_buck_cid)
{
	bool ret = false;

	if (share_buck[cid] != cid) {
		*co_buck_cid = share_buck[cid];
		ret = true;
	}

	return ret;
}

static long
compute_energy_enhanced(struct task_struct *p, int dst_cpu,
				struct sched_group *sg)
{
	int cid, share_cid, cpu;
	struct sg_state sg_env, share_env;
	const struct cpumask *sg_cpus;
	struct cpumask share_cpus;
	unsigned long total_energy = 0;

	share_env.volt = 0;
	sg_cpus = sched_group_span(sg);
	cpu = cpumask_first(sg_cpus);
#ifdef CONFIG_ARM64
	cid = cpu_topology[cpu].cluster_id;
#else
	cid = cpu_topology[cpu].socket_id;
#endif
	if (!update_sg_util(p, dst_cpu, sg_cpus, &sg_env))
		return 0;

	if (is_share_buck(cid, &share_cid)) {
		arch_get_cluster_cpus(&share_cpus, share_cid);
		if (!update_sg_util(p, dst_cpu, &share_cpus, &share_env))
			return 0;

		total_energy += compute_energy_sg(&share_cpus, &share_env,
							&sg_env);
	}

	total_energy += compute_energy_sg(sg_cpus, &sg_env, &share_env);

	return total_energy;
}

static int find_energy_efficient_cpu_enhanced(struct task_struct *p,
					int this_cpu, int prev_cpu, int sync)
{
	unsigned long prev_energy = 0;
	unsigned long prev_delta = ULONG_MAX, best_delta = ULONG_MAX;
	int max_spare_cap_cpu_ls = prev_cpu;
	unsigned long max_spare_cap_ls = 0, target_cap;
	unsigned long sys_max_spare_cap = 0;
	unsigned long cpu_cap, util, wake_util;
	bool boosted, prefer_idle = false;
	unsigned int min_exit_lat = UINT_MAX;
	int sys_max_spare_cap_cpu = -1;
	int best_energy_cpu = prev_cpu;
	struct cpuidle_state *idle;
	struct sched_domain *sd;
	struct sched_group *sg;

	if (sysctl_sched_sync_hint_enable && sync) {
		if (cpumask_test_cpu(this_cpu, &p->cpus_allowed) &&
			!cpu_isolated(this_cpu)) {
			return this_cpu;
		}
	}

	sd = rcu_dereference(per_cpu(sd_ea, this_cpu));
	if (!sd)
		return -1;

	if (!boosted_task_util(p))
		return -1;

	prefer_idle = schedtune_prefer_idle(p);
	boosted = (schedtune_task_boost(p) > 0) || (uclamp_task_effective_util(p, UCLAMP_MIN) > 0);
	target_cap = boosted ? 0 : ULONG_MAX;

	sg = sd->groups;
	do {
		unsigned long cur_energy = 0, cur_delta = 0;
		unsigned long spare_cap, max_spare_cap = 0;
		unsigned long base_energy_sg;
		int max_spare_cap_cpu = -1, best_idle_cpu = -1;
		int cpu;

		/* compute the ''base' energy of the sg, without @p*/
		base_energy_sg = compute_energy_enhanced(p, -1, sg);
		for_each_cpu_and(cpu, &p->cpus_allowed, sched_group_span(sg)) {

			if (cpu_isolated(cpu))
				continue;
#ifdef CONFIG_MTK_SCHED_INTEROP
			if (cpu_rq(cpu)->rt.rt_nr_running &&
				likely(!is_rt_throttle(cpu)))
				continue;
#endif

			/* Skip CPUs that will be overutilized. */
			wake_util = cpu_util_without(cpu, p);
			util = wake_util + task_util_est(p);
			cpu_cap = capacity_of(cpu);
			spare_cap = cpu_cap - util;
			if (spare_cap > sys_max_spare_cap) {
				sys_max_spare_cap = spare_cap;
				sys_max_spare_cap_cpu = cpu;
			}

			/*
			 * Skip CPUs that cannot satisfy the capacity request.
			 * IOW, placing the task there would make the CPU
			 * overutilized. Take uclamp into account to see how
			 * much capacity we can get out of the CPU; this is
			 * aligned with schedutil_cpu_util().
			 */
			util = uclamp_rq_util_with(cpu_rq(cpu), util, p);
			if (!fits_capacity(util, cpu_cap))
				continue;

			/* Always use prev_cpu as a candidate. */
			if (cpu == prev_cpu &&
			    (!prefer_idle || (prefer_idle && idle_cpu(cpu)))) {
				prev_energy = compute_energy_enhanced(p,
								prev_cpu, sg);
				prev_delta = prev_energy - base_energy_sg;
				best_delta = min(best_delta, prev_delta);
			}

			/*
			 * Find the CPU with the maximum spare capacity in
			 * the performance domain
			 */
			spare_cap = cpu_cap - util;
			if (spare_cap > max_spare_cap) {
				max_spare_cap = spare_cap;
				max_spare_cap_cpu = cpu;
			}

			if (!prefer_idle)
				continue;

			if (idle_cpu(cpu)) {
				cpu_cap = capacity_orig_of(cpu);
				if (!boosted && cpu_cap > target_cap)
					continue;
				idle = idle_get_state(cpu_rq(cpu));
				if (idle && idle->exit_latency > min_exit_lat &&
						cpu_cap == target_cap)
					continue;

				if (idle)
					min_exit_lat = idle->exit_latency;
				target_cap = cpu_cap;
				best_idle_cpu = cpu;

			} else if (spare_cap > max_spare_cap_ls) {
				max_spare_cap_ls = spare_cap;
				max_spare_cap_cpu_ls = cpu;
			}
		}

		if (!prefer_idle && max_spare_cap_cpu >= 0 &&
					max_spare_cap_cpu != prev_cpu) {
			cur_energy = compute_energy_enhanced(p,
							max_spare_cap_cpu, sg);
			cur_delta = cur_energy - base_energy_sg;
			if (cur_delta < best_delta) {
				best_delta = cur_delta;
				best_energy_cpu = max_spare_cap_cpu;
			}
		}

		if (prefer_idle && best_idle_cpu >= 0 &&
					best_idle_cpu != prev_cpu) {
			cur_energy = compute_energy_enhanced(p,
							best_idle_cpu, sg);
			cur_delta = cur_energy - base_energy_sg;
			if (cur_delta < best_delta) {
				best_delta = cur_delta;
				best_energy_cpu = best_idle_cpu;
			}
		}

		mt_sched_printf(sched_eas_energy_calc,
		    "prev_cpu=%d base_energy=%lu prev_energy=%lu prev_delta=%d",
		    prev_cpu, base_energy_sg, prev_energy, (int)prev_delta);

		mt_sched_printf(sched_eas_energy_calc,
		    "max_spare_cap_cpu=%d best_idle_cpu=%d cur_energy=%lu cur_delta=%d",
			max_spare_cap_cpu, best_idle_cpu, cur_energy, (int)cur_delta);

	} while (sg = sg->next, sg != sd->groups);

	/*
	 * Pick the best CPU if prev_cpu cannot be used, or it it saves energy
	 * used by prev_cpu.
	 */
	if (prev_delta == ULONG_MAX) {
		/* All cpu failed on !fit_capacity, use sys_max_spare_cap_cpu */
		if (best_energy_cpu == prev_cpu)
			return sys_max_spare_cap_cpu;
		else
			return best_energy_cpu;
	}

	if ((prev_delta - best_delta) > 0)
		return best_energy_cpu;

	return prev_cpu;
}

static int __find_energy_efficient_cpu(struct sched_domain *sd,
				     struct task_struct *p,
				     int cpu, int prev_cpu,
				     int sync)
{
	int num_cluster = arch_get_nr_clusters();

	if (num_cluster <= 2)
		return find_energy_efficient_cpu(sd, p, cpu, prev_cpu, sync);
	else
		return find_energy_efficient_cpu_enhanced(p, cpu, prev_cpu, sync);
}

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
	new_capacity = util * capacity_margin >> SCHED_CAPACITY_SHIFT;
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
		"cpu_idx=%d dst_cpu=%d cid=%d max_cpu=%d (util=%ld new=%ld) max_opp=%d (cap=%d)",
		cpu_idx, eenv->cpu[cpu_idx].cpu_id,
		cid, cpu, util, new_capacity,
		eenv->cpu[cpu_idx].cap_idx[cid],
		eenv->cpu[cpu_idx].cap[cid]);
}

#if defined(ARM_V8_2) && defined(CONFIG_MTK_UNIFY_POWER)
struct sched_group_energy cci_tbl;
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
void get_cci_volt(struct sg_state *cci)
{
	const struct sched_group_energy *_sge;
	static int CCI_nr_cap_stats;

	_sge = cci_energy();

	if (CCI_nr_cap_stats == 0) {
		CCI_nr_cap_stats = _sge->nr_cap_states;
	}

	cci->cap_idx = CCI_nr_cap_stats - mt_cpufreq_get_cur_cci_freq_idx();
	cci->volt = _sge->cap_states[cci->cap_idx].volt;
}
#else
void get_cci_volt(struct sg_state *cci)
{
}
#endif

void share_buck_volt(struct energy_env *eenv, int cpu_idx, int cid,
			struct sg_state *co_buck)
{
	if (is_share_buck(cid, &(co_buck->cid))) {
		int num_cluster = arch_get_nr_clusters();
		int cap_idx = eenv->cpu[cpu_idx].cap_idx[cid];

		if (co_buck->cid < num_cluster) {
			struct cpumask cls_cpus;
			const struct sched_group_energy *sge_core;
			int cpu;

			arch_get_cluster_cpus(&cls_cpus, co_buck->cid);
			cpu = cpumask_first(&cls_cpus);
			sge_core = cpu_core_energy(cpu);
			co_buck->cap_idx =
				eenv->cpu[cpu_idx].cap_idx[co_buck->cid];
			co_buck->volt =
				sge_core->cap_states[co_buck->cap_idx].volt;
#if defined(ARM_V8_2) && defined(CONFIG_MTK_UNIFY_POWER)
		} else if (co_buck->cid ==  CCI_ID) {    /* CCI + DSU */
			get_cci_volt(co_buck);
#endif
		}

		trace_sched_share_buck(cpu_idx, cid, cap_idx, co_buck->cid,
				co_buck->cap_idx, co_buck->volt);
	}
}

int
mtk_idle_power(int cpu_idx, int idle_state, int cpu, void *argu, int sd_level)
{
	struct energy_env *eenv = (struct energy_env *)argu;
	const struct sched_group_energy *_sge, *sge_core, *sge_clus;
	struct sched_domain *sd;
	unsigned long volt;
	int energy_cost = 0;
#ifdef CONFIG_ARM64
	int cid = cpu_topology[cpu].cluster_id;
#else
	int cid = cpu_topology[cpu].socket_id;
#endif
	int cap_idx = eenv->cpu[cpu_idx].cap_idx[cid];
	struct sg_state co_buck =  {-1, -1, 0};

	sd = rcu_dereference_check_sched_domain(cpu_rq(cpu)->sd);
	/* [FIXME] racing with hotplug */
	if (!sd)
		return 0;

	/* [FIXME] racing with hotplug */
	if (cap_idx == -1)
		return 0;

	_sge = cpu_core_energy(cpu);
	volt =  _sge->cap_states[cap_idx].volt;
	share_buck_volt(eenv, cpu_idx, cid, &co_buck);

	if (co_buck.volt > volt)
		cap_idx = share_buck_lkg_idx(_sge, cap_idx, co_buck.volt);

	_sge = sge_core = sge_clus = NULL;
	/* To handle only 1 CPU in cluster by HPS */
	if (unlikely(!sd->child &&
	   (rcu_dereference(per_cpu(sd_scs, cpu)) == NULL))) {
		struct upower_tbl_row *cpu_pwr_tbl, *clu_pwr_tbl;
		sge_core = cpu_core_energy(cpu);
		sge_clus = cpu_cluster_energy(cpu);

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

		if (sd_level == 0)
			_sge = cpu_core_energy(cpu); /* for cpu */
		else
			_sge = cpu_cluster_energy(cpu); /* for cluster */

		pwr_tbl =  &_sge->cap_states[cap_idx];
		lkg_pwr = pwr_tbl->lkg_pwr[_sge->lkg_idx];
		energy_cost = lkg_pwr;

		trace_sched_idle_power(sd_level, cap_idx, lkg_pwr, energy_cost);
	}

	idle_state = 0;

#if defined(ARM_V8_2) && defined(CONFIG_MTK_UNIFY_POWER)
	if ((sd_level != 0) && (co_buck.cid == CCI_ID)) {
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
				unsigned long co_volt, int sd_level)
{
	unsigned long dyn_pwr, lkg_pwr;
	unsigned long volt;

	volt = _sge->cap_states[cap_idx].volt;
	calc_pwr(sd_level, _sge, cap_idx, volt, co_volt, &dyn_pwr, &lkg_pwr);

	return dyn_pwr + lkg_pwr;
}

int mtk_busy_power(int cpu_idx, int cpu, void *argu, int sd_level)
{
	struct energy_env *eenv = (struct energy_env *)argu;
	const struct sched_group_energy *_sge;
	struct sched_domain *sd;
	int energy_cost = 0;
#ifdef CONFIG_ARM64
	int cid = cpu_topology[cpu].cluster_id;
#else
	int cid = cpu_topology[cpu].socket_id;
#endif
	int cap_idx = eenv->cpu[cpu_idx].cap_idx[cid];
	struct sg_state co_buck = {-1, -1, 0};

	sd = rcu_dereference_check_sched_domain(cpu_rq(cpu)->sd);
	/* [FIXME] racing with hotplug */
	if (!sd)
		return 0;

	/* [FIXME] racing with hotplug */
	if (cap_idx == -1)
		return 0;

	share_buck_volt(eenv, cpu_idx, cid, &co_buck);
	/* To handle only 1 CPU in cluster by HPS */
	if (unlikely(!sd->child &&
		(rcu_dereference(per_cpu(sd_scs, cpu)) == NULL))) {
		/* fix HPS defeats: only one CPU in this cluster */

		_sge = cpu_core_energy(cpu); /* for CPU */
		energy_cost = calc_busy_power(_sge, cap_idx, co_buck.volt,
							0);
		_sge = cpu_cluster_energy(cpu); /* for cluster */
		energy_cost += calc_busy_power(_sge, cap_idx, co_buck.volt,
							1);
	} else {
		if (sd_level == 0)
			_sge = cpu_core_energy(cpu); /* for CPU */
		else
			_sge = cpu_cluster_energy(cpu); /* for cluster */

		energy_cost = calc_busy_power(_sge, cap_idx, co_buck.volt,
							sd_level);
	}

#if defined(ARM_V8_2) && defined(CONFIG_MTK_UNIFY_POWER)
	if ((sd_level != 0) && (co_buck.cid == CCI_ID)) {
		/* CCI + DSU */
		unsigned long volt;

		_sge = cpu_core_energy(cpu); /* for CPU */
		volt =  _sge->cap_states[cap_idx].volt;

		_sge = cci_energy();
		energy_cost += calc_busy_power(_sge, co_buck.cap_idx, volt,
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

static int __find_energy_efficient_cpu(struct sched_domain *sd,
				     struct task_struct *p,
				     int cpu, int prev_cpu,
				     int sync)
{
	return find_energy_efficient_cpu(sd, p, cpu, prev_cpu, sync);
}
#endif

#ifdef CONFIG_MTK_SCHED_BOOST
static void select_task_prefer_cpu_fair(struct task_struct *p, int *result)
{
	int task_prefer;
	int cpu, new_cpu;

	task_prefer = cpu_prefer(p);

	cpu = (*result & LB_CPU_MASK);

	new_cpu = select_task_prefer_cpu(p, cpu);

	if ((new_cpu >= 0)  && (new_cpu != cpu)) {
		if (task_prefer_match(p, cpu))
			*result = new_cpu | LB_THERMAL;
		else
			*result = new_cpu | LB_HINT;
	}
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

#ifdef CONFIG_MTK_SCHED_BOOST
	if (task_prefer_match(p, src_cpu))
		return 0;

	target_tsk = rq->curr;
	if (task_prefer_fit(target_tsk, target_cpu))
		return 0;
#endif

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

struct task_rotate_work {
	struct work_struct w;
	struct task_struct *src_task;
	struct task_struct *dst_task;
	int src_cpu;
	int dst_cpu;
};

static DEFINE_PER_CPU(struct task_rotate_work, task_rotate_works);
struct task_rotate_reset_uclamp_work task_rotate_reset_uclamp_works;
unsigned int sysctl_sched_rotation_enable;
bool set_uclamp;

void set_sched_rotation_enable(bool enable)
{
	sysctl_sched_rotation_enable = enable;
}

bool is_min_capacity_cpu(int cpu)
{
	struct root_domain *rd = cpu_rq(smp_processor_id())->rd;

	if (rd->min_cap_orig_cpu < 0)
		return false;

	if (capacity_orig_of(cpu) == capacity_orig_of(rd->min_cap_orig_cpu))
		return true;

	return false;
}

static void task_rotate_work_func(struct work_struct *work)
{
	struct task_rotate_work *wr = container_of(work,
				struct task_rotate_work, w);
	int ret = -1;
	struct rq *src_rq, *dst_rq;

	ret = migrate_swap(wr->src_task, wr->dst_task);

	if (ret == 0) {
		update_eas_uclamp_min(EAS_UCLAMP_KIR_BIG_TASK, CGROUP_TA,
				scale_to_percent(SCHED_CAPACITY_SCALE));
		set_uclamp = true;
		trace_sched_big_task_rotation(wr->src_cpu, wr->dst_cpu,
						wr->src_task->pid,
						wr->dst_task->pid,
						true, set_uclamp);
	}

	put_task_struct(wr->src_task);
	put_task_struct(wr->dst_task);

	src_rq = cpu_rq(wr->src_cpu);
	dst_rq = cpu_rq(wr->dst_cpu);

	local_irq_disable();
	double_rq_lock(src_rq, dst_rq);
	src_rq->active_balance = 0;
	dst_rq->active_balance = 0;
	double_rq_unlock(src_rq, dst_rq);
	local_irq_enable();
}

static void task_rotate_reset_uclamp_work_func(struct work_struct *work)
{
	update_eas_uclamp_min(EAS_UCLAMP_KIR_BIG_TASK, CGROUP_TA, 0);
	set_uclamp = false;
	trace_sched_big_task_rotation_reset(set_uclamp);
}

void task_rotate_work_init(void)
{
	int i;

	for_each_possible_cpu(i) {
		struct task_rotate_work *wr = &per_cpu(task_rotate_works, i);

		INIT_WORK(&wr->w, task_rotate_work_func);
	}

	INIT_WORK(&task_rotate_reset_uclamp_works.w,
			task_rotate_reset_uclamp_work_func);
}

void task_check_for_rotation(struct rq *src_rq)
{
	u64 wc, wait, max_wait = 0, run, max_run = 0;
	int deserved_cpu = nr_cpu_ids, dst_cpu = nr_cpu_ids;
	int i, src_cpu = cpu_of(src_rq);
	struct rq *dst_rq;
	struct task_rotate_work *wr = NULL;
	int heavy_task = 0;
	int force = 0;

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

		if (!is_min_capacity_cpu(i))
			continue;

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

		if (capacity_orig_of(i) <= capacity_orig_of(src_cpu))
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

		if (!cpumask_test_cpu(dst_cpu,
					&(src_rq->curr)->cpus_allowed) ||
			!cpumask_test_cpu(src_cpu,
					&(dst_rq->curr)->cpus_allowed)) {
			double_rq_unlock(src_rq, dst_rq);
			return;
		}

		if (!src_rq->active_balance && !dst_rq->active_balance) {
			src_rq->active_balance = MIGR_ROTATION;
			dst_rq->active_balance = MIGR_ROTATION;

			get_task_struct(src_rq->curr);
			get_task_struct(dst_rq->curr);

			wr = &per_cpu(task_rotate_works, src_cpu);

			wr->src_task = src_rq->curr;
			wr->dst_task = dst_rq->curr;

			wr->src_cpu = src_rq->cpu;
			wr->dst_cpu = dst_rq->cpu;
			force = 1;
		}
	}
	double_rq_unlock(src_rq, dst_rq);

	if (force) {
		queue_work_on(src_cpu, system_highpri_wq, &wr->w);
		trace_sched_big_task_rotation(wr->src_cpu, wr->dst_cpu,
					wr->src_task->pid, wr->dst_task->pid,
					false, set_uclamp);
	}
}
