// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include "sched.h"
#include <trace/events/sched.h>

DEFINE_PER_CPU(struct task_struct*, migrate_task);
static int idle_pull_cpu_stop(void *data)
{
	int ret;
	int src_cpu = cpu_of((struct rq *)data);
	struct task_struct *p;

	p = per_cpu(migrate_task, src_cpu);
	ret = active_load_balance_cpu_stop(data);
	put_task_struct(p);
	return ret;
}

int
migrate_running_task(int dst_cpu, struct task_struct *p, struct rq *src_rq)
{
	unsigned long flags;
	unsigned int force = 0;
	int src_cpu = cpu_of(src_rq);

	/* now we have a candidate */
	raw_spin_lock_irqsave(&src_rq->lock, flags);
	if (!src_rq->active_balance &&
			(task_rq(p) == src_rq) && p->state != TASK_DEAD) {
		get_task_struct(p);
		src_rq->push_cpu = dst_cpu;
		per_cpu(migrate_task, src_cpu) = p;
		trace_sched_migrate(p, cpu_of(src_rq), src_rq->push_cpu,
				MIGR_IDLE_RUNNING);
		src_rq->active_balance = MIGR_IDLE_RUNNING; /* idle pull */
		force = 1;
	}
	raw_spin_unlock_irqrestore(&src_rq->lock, flags);
	if (force) {
		if (!stop_one_cpu_nowait(cpu_of(src_rq),
					idle_pull_cpu_stop,
					src_rq, &src_rq->active_balance_work)) {
			put_task_struct(p); /* out of rq->lock */
			raw_spin_lock_irqsave(&src_rq->lock, flags);
			src_rq->active_balance = 0;
			per_cpu(migrate_task, src_cpu) = NULL;
			force = 0;
			raw_spin_unlock_irqrestore(&src_rq->lock, flags);
		}
	}

	return force;
}

#if defined(CONFIG_ENERGY_MODEL) && defined(CONFIG_CPU_FREQ_GOV_SCHEDUTIL)
/*
 * The cpu types are distinguished using a list of perf_order_domains
 * which each represent one cpu type using a cpumask.
 * The list is assumed ordered by compute capacity with the
 * fastest domain first.
 */

/* Perf order domain common utils */
LIST_HEAD(perf_order_domains);
DEFINE_PER_CPU(struct perf_order_domain *, perf_order_cpu_domain);
static bool pod_ready;

bool pod_is_ready(void)
{
	return pod_ready;
}

/* Initialize perf_order_cpu_domains */
void perf_order_cpu_mask_setup(void)
{
	struct perf_order_domain *domain;
	struct list_head *pos;
	int cpu;

	list_for_each(pos, &perf_order_domains) {
		domain = list_entry(pos, struct perf_order_domain,
					perf_order_domains);

		for_each_cpu(cpu, &domain->possible_cpus)
			per_cpu(perf_order_cpu_domain, cpu) = domain;
	}
}

/*
 *Perf domain capacity compare function
 * Only inspect lowest id of cpus in same domain.
 * Assume CPUs in same domain has same capacity.
 */
struct cluster_info {
	struct perf_order_domain *pod;
	unsigned long cpu_perf;
	int cpu;
};

static inline void fillin_cluster(struct cluster_info *cinfo,
		struct perf_order_domain *pod)
{
	int cpu;
	unsigned long cpu_perf;

	cinfo->pod = pod;
	cinfo->cpu = cpumask_any(&cinfo->pod->possible_cpus);

	for_each_cpu(cpu, &pod->possible_cpus) {
		cpu_perf = arch_scale_cpu_capacity(NULL, cpu);
		pr_info("cpu=%d, cpu_perf=%lu\n", cpu, cpu_perf);
		if (cpu_perf > 0)
			break;
	}
	cinfo->cpu_perf = cpu_perf;

	if (cpu_perf == 0)
		pr_info("Uninitialized CPU performance (CPU mask: %lx)",
				cpumask_bits(&pod->possible_cpus)[0]);
}

/*
 * Negative, if @a should sort before @b
 * Positive, if @a should sort after @b.
 * Return 0, if ordering is to be preserved
 */
int perf_domain_compare(void *priv, struct list_head *a, struct list_head *b)
{
	struct cluster_info ca;
	struct cluster_info cb;

	fillin_cluster(&ca, list_entry(a, struct perf_order_domain,
				perf_order_domains));
	fillin_cluster(&cb, list_entry(b, struct perf_order_domain,
				perf_order_domains));

	return (ca.cpu_perf > cb.cpu_perf) ? -1 : 1;
}

void init_perf_order_domains(struct perf_domain *pd)
{
	struct perf_order_domain *domain;

	pr_info("Initializing perf order domain:\n");

	if (!pd) {
		pr_info("Perf domain is not ready!\n");
		return;
	}

	for (; pd; pd = pd->next) {
		domain = (struct perf_order_domain *)
			kmalloc(sizeof(struct perf_order_domain), GFP_KERNEL);
		if (domain) {
			cpumask_copy(&domain->possible_cpus,
					perf_domain_span(pd));
			cpumask_and(&domain->cpus, cpu_online_mask,
				&domain->possible_cpus);
			list_add(&domain->perf_order_domains,
					&perf_order_domains);
		}
	}

	if (list_empty(&perf_order_domains)) {
		pr_info("Perf order domain list is empty!\n");
		return;
	}

	/*
	 * Sorting Perf domain by CPU capacity
	 */
	list_sort(NULL, &perf_order_domains, &perf_domain_compare);
	pr_info("Sort perf_domains from little to big:\n");
	for_each_perf_domain_ascending(domain) {
		pr_info("    cpumask: 0x%02lx\n",
				*cpumask_bits(&domain->possible_cpus));
	}

	/* Initialize perf_order_cpu_domains */
	perf_order_cpu_mask_setup();

	pod_ready = true;

	pr_info("Initializing perf order domain done\n");
}
EXPORT_SYMBOL(init_perf_order_domains);

/* Check if cpu is in fastest perf_order_domain */
inline unsigned int cpu_is_fastest(int cpu)
{
	struct list_head *pos;

	if (!pod_is_ready()) {
		pr_info("Perf order domain is not ready!\n");
		return -1;
	}

	pos = &perf_order_cpu_domain(cpu)->perf_order_domains;
	return pos == perf_order_domains.next;
}
EXPORT_SYMBOL(cpu_is_fastest);

/* Check if cpu is in slowest perf_order_domain */
inline unsigned int cpu_is_slowest(int cpu)
{
	struct list_head *pos;

	if (!pod_is_ready()) {
		pr_info("Perf order domain is not ready!\n");
		return -1;
	}

	pos = &perf_order_cpu_domain(cpu)->perf_order_domains;
	return list_is_last(pos, &perf_order_domains);
}
EXPORT_SYMBOL(cpu_is_slowest);

bool is_intra_domain(int prev, int target)
{
	struct perf_order_domain *perf_domain = NULL;

	if (!pod_is_ready()) {
		pr_info("Perf order domain is not ready!\n");
		return 0;
	}

	perf_domain = per_cpu(perf_order_cpu_domain, prev);

	return cpumask_test_cpu(target, &perf_domain->cpus);
}
#endif

#ifdef CONFIG_MTK_SCHED_CPU_PREFER

int task_prefer_little(struct task_struct *p)
{
	if (cpu_prefer(p) == SCHED_PREFER_LITTLE)
		return 1;

	return 0;
}

int task_prefer_big(struct task_struct *p)
{
	if (cpu_prefer(p) == SCHED_PREFER_BIG)
		return 1;

	return 0;
}

int task_prefer_match(struct task_struct *p, int cpu)
{
	if (cpu_prefer(p) == SCHED_PREFER_NONE)
		return 1;

	if (task_prefer_little(p) && cpu_is_slowest(cpu))
		return 1;

	if (task_prefer_big(p) && cpu_is_fastest(cpu))
		return 1;

	return 0;
}

inline int hinted_cpu_prefer(int task_prefer)
{
	if (task_prefer <= SCHED_PREFER_NONE || task_prefer >= SCHED_PREFER_END)
		return 0;

	return 1;
}

int select_task_prefer_cpu(struct task_struct *p, int new_cpu)
{
	int task_prefer;
	struct perf_order_domain *domain;
	struct perf_order_domain *tmp_domain[5] = {0, 0, 0, 0, 0};
	int i, iter_domain, domain_cnt = 0;
	int iter_cpu;
	struct cpumask *tsk_cpus_allow = &p->cpus_allowed;

	task_prefer = cpu_prefer(p);

	if (!hinted_cpu_prefer(task_prefer) && !cpu_isolated(new_cpu))
		return new_cpu;

	for_each_perf_domain_ascending(domain) {
		tmp_domain[domain_cnt] = domain;
		domain_cnt++;
	}

	for (i = 0; i < domain_cnt; i++) {
		iter_domain = (task_prefer == SCHED_PREFER_BIG) ?
				domain_cnt-i-1 : i;
		domain = tmp_domain[iter_domain];

		if (cpumask_test_cpu(new_cpu, &domain->possible_cpus)
			&& !cpu_isolated(new_cpu))
			return new_cpu;

		for_each_cpu(iter_cpu, &domain->possible_cpus) {

			/* tsk with prefer idle to find bigger idle cpu */
			if (!cpu_online(iter_cpu) ||
				!cpumask_test_cpu(iter_cpu, tsk_cpus_allow) ||
				cpu_isolated(iter_cpu))
				continue;

			/* favoring tasks that prefer idle cpus
			 * to improve latency.
			 */
			if (idle_cpu(iter_cpu))
				return iter_cpu;

		}
	}

	return new_cpu;
}

void select_task_prefer_cpu_fair(struct task_struct *p, int *result)
{
	int task_prefer;
	int cpu, new_cpu;

	task_prefer = cpu_prefer(p);

	cpu = (*result & LB_CPU_MASK);

	if (task_prefer_match(p, cpu))
		return;

	new_cpu = select_task_prefer_cpu(p, cpu);

	if ((new_cpu >= 0)  && (new_cpu != cpu))
		*result = new_cpu | LB_CPU_PREFER;
}

int task_prefer_fit(struct task_struct *p, int cpu)
{
	if (cpu_prefer(p) == SCHED_PREFER_NONE)
		return 0;

	if (task_prefer_little(p) && cpu_is_slowest(cpu))
		return 1;

	if (task_prefer_big(p) && cpu_is_fastest(cpu))
		return 1;

	return 0;
}

int
task_prefer_match_on_cpu(struct task_struct *p, int src_cpu, int target_cpu)
{
	/* No need to migrate*/
	if (is_intra_domain(src_cpu, target_cpu))
		return 1;

	if (cpu_prefer(p) == SCHED_PREFER_NONE)
		return 1;

	if (task_prefer_little(p) && cpu_is_slowest(src_cpu))
		return 1;

	if (task_prefer_big(p) && cpu_is_fastest(src_cpu))
		return 1;

	return 0;
}

inline int valid_cpu_prefer(int task_prefer)
{
	if (task_prefer < SCHED_PREFER_NONE || task_prefer >= SCHED_PREFER_END)
		return 0;

	return 1;
}


#endif

#ifdef CONFIG_MTK_IDLE_BALANCE_ENHANCEMENT
static inline struct task_struct *task_of(struct sched_entity *se)
{
	return container_of(se, struct task_struct, se);
}

static struct sched_entity *__pick_next_entity(struct sched_entity *se)
{
	struct rb_node *next = rb_next(&se->run_node);

	if (!next)
		return NULL;

	return rb_entry(next, struct sched_entity, run_node);

}

/* runqueue "owned" by this group */
static inline struct cfs_rq *group_cfs_rq(struct sched_entity *grp)
{
	return grp->my_q;

}

/* must hold runqueue lock for queue se is currently on */
static const int idle_prefer_max_tasks = 5;
static struct sched_entity
*get_idle_prefer_task(int cpu, int target_cpu, int check_min_cap,
		struct task_struct **backup_task, int *backup_cpu)
{
	int num_tasks = idle_prefer_max_tasks;
	const struct cpumask *target_mask = NULL;
	int src_capacity;
	unsigned int util_min = 0;
	struct cfs_rq *cfs_rq;
	struct sched_entity *se;

	if (target_cpu >= 0)
		target_mask = cpumask_of(target_cpu);
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
		    cpumask_intersects(target_mask,
				       &(task_of(se)->cpus_allowed))) {
			struct task_struct *p;

			p = task_of(se);
#ifdef CONFIG_UCLAMP_TASK
			util_min = uclamp_task(p);
#endif

#ifdef CONFIG_MTK_SCHED_CPU_PREFER
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
slowest_domain_idle_prefer_pull(int this_cpu, struct task_struct **p,
			     struct rq **target)
{
	int cpu, backup_cpu;
	struct sched_entity *se = NULL;
	struct task_struct  *backup_task = NULL;
	struct perf_order_domain *domain;
	struct list_head *pos;
	int selected = 0;
	struct rq *rq;
	unsigned long flags;
	int check_min_cap;

	/* 1. select a runnable task
	 *     idle prefer
	 *
	 *     order: fast to slow perf domain
	 */

	if (cpu_isolated(this_cpu))
		return;
	check_min_cap = 0;
	list_for_each(pos, &perf_order_domains) {
		domain = list_entry(pos, struct perf_order_domain,
				perf_order_domains);

		for_each_cpu(cpu, &domain->cpus) {
			if (cpu == this_cpu)
				continue;
			if (cpu_isolated(cpu))
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

static void
fastest_domain_idle_prefer_pull(int this_cpu, struct task_struct **p,
						struct rq **target)
{
	int cpu, backup_cpu;
	struct sched_entity *se = NULL;
	struct task_struct  *backup_task = NULL;
	struct perf_order_domain *perf_domain = NULL, *domain;
	struct list_head *pos;
	int selected = 0;
	struct rq *rq;
	unsigned long flags;
	int check_min_cap;
#ifdef CONFIG_UCLAMP_TASK
	int target_capacity;
#endif

	perf_domain = per_cpu(perf_order_cpu_domain, this_cpu);

	/* 1. select a runnable task
	 *
	 * first candidate:
	 *     capacity_min in slow domain
	 *
	 *     order: target->next to slow perf domain
	 */
	if (cpu_isolated(this_cpu))
		return;
	check_min_cap = 1;
	list_for_each(pos, &perf_domain->perf_order_domains) {
		domain = list_entry(pos, struct perf_order_domain,
				perf_order_domains);

		for_each_cpu(cpu, &domain->cpus) {
			if (cpu == this_cpu)
				continue;
			if (cpu_isolated(cpu))
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

		if (list_is_last(pos, &perf_order_domains))
			break;
	}

	/* backup candidate:
	 *     idle prefer
	 *
	 *     order: fastest to target perf domain
	 */
	check_min_cap = 0;
	list_for_each(pos, &perf_order_domains) {
		domain = list_entry(pos, struct perf_order_domain,
				perf_order_domains);

		for_each_cpu(cpu, &domain->cpus) {
			if (cpu == this_cpu)
				continue;
			if (cpu_isolated(cpu))
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
	 *     order: target->next to slow perf domain
	 * 3. turning = true, pick a runnable task from slower domain
	 */
	list_for_each(pos, &perf_domain->perf_order_domains) {
		domain = list_entry(pos, struct perf_order_domain,
				perf_order_domains);

		for_each_cpu(cpu, &domain->cpus) {
			if (cpu == this_cpu)
				continue;
			if (cpu_isolated(cpu))
				continue;

			rq = cpu_rq(cpu);
#ifdef CONFIG_UCLAMP_TASK
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
			     (uclamp_task(task_of(se)) >= target_capacity) &&
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
#endif

		}

		if (list_is_last(pos, &perf_order_domains))
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
	if (!cpu_online(src_cpu) || !cpu_online(dst_cpu))
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

static DEFINE_SPINLOCK(force_migration);

unsigned int aggressive_idle_pull(int this_cpu)
{
	int moved = 0;
	struct rq *src_rq = NULL;
	struct task_struct *p = NULL;

	if (!pod_is_ready())
		return 0;

	if (!spin_trylock(&force_migration))
		return 0;

	/*
	 * aggressive idle balance for min_cap/idle_prefer
	 */
	if (cpu_is_slowest(this_cpu)) {
		slowest_domain_idle_prefer_pull(this_cpu, &p, &src_rq);
		if (p) {
			trace_sched_migrate(p, this_cpu, cpu_of(src_rq),
							MIGR_IDLE_BALANCE);
			moved = migrate_runnable_task(p, this_cpu, src_rq);
			if (moved)
				goto done;
		}
	} else {
		fastest_domain_idle_prefer_pull(this_cpu, &p, &src_rq);
		if (p) {
			trace_sched_migrate(p, this_cpu, cpu_of(src_rq),
							MIGR_IDLE_BALANCE);
			moved = migrate_runnable_task(p, this_cpu, src_rq);
			if (moved)
				goto done;

			moved = migrate_running_task(this_cpu, p, src_rq);
		}
	}

done:
	spin_unlock(&force_migration);
	if (p)
		put_task_struct(p);

	return moved;
}

#endif

#ifdef CONFIG_MTK_SCHED_BIG_TASK_MIGRATE
DEFINE_PER_CPU(struct task_rotate_work, task_rotate_works);
DEFINE_PER_CPU(unsigned long, rotate_flags);
struct task_rotate_reset_uclamp_work task_rotate_reset_uclamp_works;
bool big_task_rotation_enable;
bool set_uclamp;

unsigned int scale_to_percent(unsigned int value)
{
	WARN_ON(value > SCHED_CAPACITY_SCALE);

	return (value * 100 / SCHED_CAPACITY_SCALE);
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

	ret = migrate_swap(wr->src_task, wr->dst_task,
			task_cpu(wr->dst_task), task_cpu(wr->src_task));

	put_task_struct(wr->src_task);
	put_task_struct(wr->dst_task);

	clear_reserved(wr->src_cpu);
	clear_reserved(wr->dst_cpu);

	if (ret == 0) {
		update_eas_uclamp_min(EAS_UCLAMP_KIR_BIG_TASK, CGROUP_TA,
				scale_to_percent(SCHED_CAPACITY_SCALE));
		set_uclamp = true;
		trace_sched_big_task_rotation(wr->src_cpu, wr->dst_cpu,
						wr->src_task->pid,
						wr->dst_task->pid,
						true, set_uclamp);
	}
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
#endif /* CONFIG_MTK_SCHED_BIG_TASK_MIGRATE */

/**
 *for isolation
 */
void init_cpu_isolated(const struct cpumask *src)
{
	cpumask_copy(&__cpu_isolated_mask, src);
}

DEFINE_MUTEX(sched_isolation_mutex);
struct cpumask cpu_all_masks;
/*
 * Remove a task from the runqueue and pretend that it's migrating. This
 * should prevent migrations for the detached task and disallow further
 * changes to tsk_cpus_allowed.
 */
void
iso_detach_one_task(struct task_struct *p, struct rq *rq,
			struct list_head *tasks)
{
	lockdep_assert_held(&rq->lock);

	p->on_rq = TASK_ON_RQ_MIGRATING;
	deactivate_task(rq, p, 0);
	list_add(&p->se.group_node, tasks);
}

void iso_attach_tasks(struct list_head *tasks, struct rq *rq)
{
	struct task_struct *p;

	lockdep_assert_held(&rq->lock);

	while (!list_empty(tasks)) {
		p = list_first_entry(tasks, struct task_struct, se.group_node);
		list_del_init(&p->se.group_node);

		WARN_ON(task_rq(p) != rq);
		activate_task(rq, p, 0);
		p->on_rq = TASK_ON_RQ_QUEUED;
	}
}

static inline bool iso_is_per_cpu_kthread(struct task_struct *p)
{
	if (!(p->flags & PF_KTHREAD))
		return false;

	if (p->nr_cpus_allowed != 1)
		return false;

	return true;
}

/*
 * Per-CPU kthreads are allowed to run on !actie && online CPUs, see
 * __set_cpus_allowed_ptr() and select_fallback_rq().
 */
static inline bool iso_is_cpu_allowed(struct task_struct *p, int cpu)
{
	if (!cpumask_test_cpu(cpu, &p->cpus_allowed))
		return false;

	if (iso_is_per_cpu_kthread(p))
		return cpu_online(cpu);

	return cpu_active(cpu);
}

int do_isolation_work_cpu_stop(void *data)
{
	unsigned int cpu = smp_processor_id();
	struct rq *rq = cpu_rq(cpu);
	struct rq_flags rf;

	local_irq_disable();

	sched_ttwu_pending();

	rq_lock_irqsave(rq, &rf);

	/*
	 * Temporarily mark the rq as offline. This will allow us to
	 * move tasks off the CPU.
	 */
	if (rq->rd) {
		WARN_ON(!cpumask_test_cpu(cpu, rq->rd->span));
		set_rq_offline(rq);
	}


	migrate_tasks(rq, &rf, false);


	if (rq->rd)
		set_rq_online(rq);

	rq_unlock_irqrestore(rq, &rf);

	/*
	 * We might have been in tickless state. Clear NOHZ flags to avoid
	 * us being kicked for helping out with balancing
	 */
	nohz_balance_clear_nohz_mask(cpu);

	local_irq_enable();
	return 0;
}

static void sched_update_group_capacities(int cpu)
{
	struct sched_domain *sd;

	mutex_lock(&sched_domains_mutex);
	rcu_read_lock();

	for_each_domain(cpu, sd) {
		int balance_cpu = group_balance_cpu(sd->groups);

		iso_init_sched_groups_capacity(cpu, sd);
		/*
		 * Need to ensure this is also called with balancing
		 * cpu.
		 */
		if (cpu != balance_cpu)
			iso_init_sched_groups_capacity(balance_cpu, sd);
	}

	rcu_read_unlock();
	mutex_unlock(&sched_domains_mutex);
}

static unsigned int cpu_isolation_vote[NR_CPUS];



static inline void
set_cpumask_isolated(unsigned int cpu, bool isolated)
{
	if (isolated)
		cpumask_set_cpu(cpu, &__cpu_isolated_mask);
	else
		cpumask_clear_cpu(cpu, &__cpu_isolated_mask);
}

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
 * calling sched_deisolate_cpu() on a CPU that the client previously isolated.
 * Client is also responsible for deisolating when a core goes offline
 * (after CPU is marked offline).
 */
int _sched_isolate_cpu(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	cpumask_t avail_cpus;
	int ret_code = 0;
	u64 start_time = 0;

	if (trace_sched_isolate_enabled())
		start_time = sched_clock();

	cpu_maps_update_begin();

	cpumask_andnot(&avail_cpus, cpu_online_mask, cpu_isolated_mask);

	/* We cannot isolate ALL cpus in the system */
	if (cpumask_weight(&avail_cpus) == 1) {
		ret_code = -EINVAL;
		goto out;
	}

	if (!cpu_online(cpu)) {
		ret_code = -EINVAL;
		goto out;
	}

	if (++cpu_isolation_vote[cpu] > 1)
		goto out;

	set_cpumask_isolated(cpu, true);
	/*wait mcdi api*/
	//mcdi_cpu_iso_mask(cpu_isolated_mask->bits[0]);
	cpumask_clear_cpu(cpu, &avail_cpus);
	/* Migrate timers */
	/*wait hrtimer_quiesce_cpu api ready*/
	//smp_call_function_any(&avail_cpus, hrtimer_quiesce_cpu, &cpu, 1);
	//smp_call_function_any(&avail_cpus, timer_quiesce_cpu, &cpu, 1);

	stop_cpus(cpumask_of(cpu), do_isolation_work_cpu_stop, 0);

	iso_calc_load_migrate(rq);
	update_max_interval();
	sched_update_group_capacities(cpu);

out:
	cpu_maps_update_done();
	trace_sched_isolate(cpu, cpumask_bits(cpu_isolated_mask)[0],
			    start_time, 1);
	printk_deferred("%s:cpu=%d, isolation_cpus=0x%lx\n",
			__func__, cpu, cpu_isolated_mask->bits[0]);
	return ret_code;
}

/*
 * Note: The client calling sched_isolate_cpu() is repsonsible for ONLY
 * calling sched_deisolate_cpu() on a CPU that the client previously isolated.
 * Client is also responsible for deisolating when a core goes offline
 * (after CPU is marked offline).
 */
int __sched_deisolate_cpu_unlocked(int cpu)
{
	int ret_code = 0;
	u64 start_time = 0;

	if (trace_sched_isolate_enabled())
		start_time = sched_clock();

	if (!cpu_isolation_vote[cpu]) {
		ret_code = -EINVAL;
		goto out;
	}

	if (--cpu_isolation_vote[cpu])
		goto out;

	set_cpumask_isolated(cpu, false);

	/*wait mcdi api*/
	//mcdi_cpu_iso_mask(cpu_isolated_mask->bits[0]);

	update_max_interval();
	sched_update_group_capacities(cpu);

	if (cpu_online(cpu)) {
		/* Kick CPU to immediately do load balancing */
		if ((atomic_fetch_or(NOHZ_BALANCE_KICK, nohz_flags(cpu)) &
			NOHZ_BALANCE_KICK) != NOHZ_BALANCE_KICK)
			smp_send_reschedule(cpu);
	}

out:
	trace_sched_isolate(cpu, cpumask_bits(cpu_isolated_mask)[0],
			    start_time, 0);

	printk_deferred("%s:cpu=%d, isolation_cpus=0x%lx\n",
			__func__, cpu, cpu_isolated_mask->bits[0]);
	return ret_code;
}

int _sched_deisolate_cpu(int cpu)
{
	int ret_code;

	cpu_maps_update_begin();
	ret_code = __sched_deisolate_cpu_unlocked(cpu);
	cpu_maps_update_done();
	return ret_code;
}

/*called when sched_init for get all cpu masks*/
void iso_cpumask_init(void)
{
	cpumask_copy(&cpu_all_masks, cpu_possible_mask);
}
