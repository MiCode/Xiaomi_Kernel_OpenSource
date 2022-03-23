// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#undef pr_fmt
#define pr_fmt(fmt) "Task-Turbo: " fmt

#include <linux/sched.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <uapi/linux/sched/types.h>
#include <uapi/linux/prctl.h>
#include <linux/futex.h>
#include <linux/plist.h>
#include <linux/percpu-defs.h>

#include <trace/hooks/vendor_hooks.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/dtask.h>
#include <trace/hooks/binder.h>
#include <trace/hooks/rwsem.h>
#include <trace/hooks/futex.h>
#include <trace/hooks/fpsimd.h>
#include <trace/hooks/topology.h>
#include <trace/hooks/debug.h>
#include <trace/hooks/wqlockup.h>
#include <trace/hooks/cgroup.h>
#include <trace/hooks/sys.h>

#include <task_turbo.h>

#define CREATE_TRACE_POINTS
#include <trace_task_turbo.h>

LIST_HEAD(hmp_domains);

#define SCHED_FEAT(name, enabled)	\
	[__SCHED_FEAT_##name] = {0},

struct static_key sched_feat_keys[__SCHED_FEAT_NR] = {
#include "features.h"
};

#undef SCHED_FEAT

/*TODO: find the magic bias number */
#define TOP_APP_GROUP_ID	(4-1)*9
#define TURBO_PID_COUNT		8
#define RENDER_THREAD_NAME	"RenderThread"
#define TAG			"Task-Turbo"
#define TURBO_ENABLE		1
#define TURBO_DISABLE		0
#define INHERIT_THRESHOLD	4
#define type_offset(type)		 (type * 4)
#define task_turbo_nice(nice) (nice == 0xbeef || nice == 0xbeee)
#define task_restore_nice(nice) (nice == 0xbeee)
#define type_number(type)		 (1U << type_offset(type))
#define get_value_with_type(value, type)				\
	(value & ((unsigned int)(0x0000000f) << (type_offset(type))))
#define for_each_hmp_domain_L_first(hmpd) \
		list_for_each_entry_reverse(hmpd, &hmp_domains, hmp_domains)
#define hmp_cpu_domain(cpu)     (per_cpu(hmp_cpu_domain, (cpu)))

/*
 * Unsigned subtract and clamp on underflow.
 *
 * Explicitly do a load-store to ensure the intermediate value never hits
 * memory. This allows lockless observations without ever seeing the negative
 * values.
 */
#define sub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	typeof(*ptr) val = (_val);				\
	typeof(*ptr) res, var = READ_ONCE(*ptr);		\
	res = var - val;					\
	if (res > var)						\
		res = 0;					\
	WRITE_ONCE(*ptr, res);					\
} while (0)

/*
 * Remove and clamp on negative, from a local variable.
 *
 * A variant of sub_positive(), which does not use explicit load-store
 * and is thus optimized for local variable updates.
 */
#define lsub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	*ptr -= min_t(typeof(*ptr), *ptr, _val);		\
} while (0)

#define RWSEM_READER_OWNED	(1UL << 0)
#define RWSEM_RD_NONSPINNABLE	(1UL << 1)
#define RWSEM_WR_NONSPINNABLE	(1UL << 2)
#define RWSEM_NONSPINNABLE	(RWSEM_RD_NONSPINNABLE | RWSEM_WR_NONSPINNABLE)
#define RWSEM_OWNER_FLAGS_MASK	(RWSEM_READER_OWNED | RWSEM_NONSPINNABLE)
#define RWSEM_WRITER_LOCKED	(1UL << 0)
#define RWSEM_WRITER_MASK	RWSEM_WRITER_LOCKED

DEFINE_PER_CPU(struct hmp_domain *, hmp_cpu_domain);
DEFINE_PER_CPU(unsigned long, cpu_scale) = SCHED_CAPACITY_SCALE;

static uint32_t latency_turbo = SUB_FEAT_LOCK | SUB_FEAT_BINDER |
				SUB_FEAT_SCHED;
static uint32_t launch_turbo =  SUB_FEAT_LOCK | SUB_FEAT_BINDER |
				SUB_FEAT_SCHED | SUB_FEAT_FLAVOR_BIGCORE;
static DEFINE_SPINLOCK(TURBO_SPIN_LOCK);
static pid_t turbo_pid[TURBO_PID_COUNT] = {0};
static unsigned int task_turbo_feats;

static bool is_turbo_task(struct task_struct *p);
static void set_load_weight(struct task_struct *p, bool update_load);
static void rwsem_stop_turbo_inherit(struct rw_semaphore *sem);
static void rwsem_list_add(struct task_struct *task, struct list_head *entry,
				struct list_head *head);
static bool binder_start_turbo_inherit(struct task_struct *from,
					struct task_struct *to);
static void binder_stop_turbo_inherit(struct task_struct *p);
static inline struct task_struct *rwsem_owner(struct rw_semaphore *sem);
static inline bool rwsem_test_oflags(struct rw_semaphore *sem, long flags);
static inline bool is_rwsem_reader_owned(struct rw_semaphore *sem);
static void rwsem_start_turbo_inherit(struct rw_semaphore *sem);
static bool sub_feat_enable(int type);
static bool start_turbo_inherit(struct task_struct *task, int type, int cnt);
static bool stop_turbo_inherit(struct task_struct *task, int type);
static inline bool should_set_inherit_turbo(struct task_struct *task);
static inline void add_inherit_types(struct task_struct *task, int type);
static inline void sub_inherit_types(struct task_struct *task, int type);
static inline void set_scheduler_tuning(struct task_struct *task);
static inline void unset_scheduler_tuning(struct task_struct *task);
static bool is_inherit_turbo(struct task_struct *task, int type);
static bool test_turbo_cnt(struct task_struct *task);
static int select_turbo_cpu(struct task_struct *p);
static int find_best_turbo_cpu(struct task_struct *p);
static void init_hmp_domains(void);
static void hmp_cpu_mask_setup(void);
static int arch_get_nr_clusters(void);
static void arch_get_cluster_cpus(struct cpumask *cpus, int package_id);
static int hmp_compare(void *priv, struct list_head *a, struct list_head *b);
static inline void fillin_cluster(struct cluster_info *cinfo,
		struct hmp_domain *hmpd);
static void sys_set_turbo_task(struct task_struct *p);
static unsigned long capacity_of(int cpu);
static void init_turbo_attr(struct task_struct *p);
static inline unsigned long cpu_util(int cpu);
static inline unsigned long task_util(struct task_struct *p);
static inline unsigned long _task_util_est(struct task_struct *p);


static void probe_android_rvh_prepare_prio_fork(void *ignore, struct task_struct *p)
{
	struct task_turbo_t *turbo_data;

	init_turbo_attr(p);
	if (unlikely(is_turbo_task(current))) {
		turbo_data = get_task_turbo_t(current);
		if (task_has_dl_policy(p) || task_has_rt_policy(p))
			p->static_prio = NICE_TO_PRIO(turbo_data->nice_backup);
		else {
			p->static_prio = NICE_TO_PRIO(turbo_data->nice_backup);
			p->prio = p->normal_prio = p->static_prio;
			set_load_weight(p, false);
		}
		trace_turbo_prepare_prio_fork(turbo_data, p);
	}
}

static void probe_android_rvh_finish_prio_fork(void *ignore, struct task_struct *p)
{
	if (!dl_prio(p->prio) && !rt_prio(p->prio)) {
		struct task_turbo_t *turbo_data;

		turbo_data = get_task_turbo_t(p);
		/* prio and backup should be aligned */
		turbo_data->nice_backup = PRIO_TO_NICE(p->prio);
	}
}

static void probe_android_rvh_rtmutex_prepare_setprio(void *ignore, struct task_struct *p,
						struct task_struct *pi_task)
{
	int queued, running;
	struct rq_flags rf;
	struct rq *rq;

	/* if rt boost, recover prio with backup */
	if (unlikely(is_turbo_task(p))) {
		if (!dl_prio(p->prio) && !rt_prio(p->prio)) {
			struct task_turbo_t *turbo_data;
			int backup;

			turbo_data = get_task_turbo_t(p);
			backup = turbo_data->nice_backup;

			if (backup >= MIN_NICE && backup <= MAX_NICE) {
				rq = __task_rq_lock(p, &rf);
				update_rq_clock(rq);

				queued = task_on_rq_queued(p);
				running = task_current(rq, p);
				if (queued)
					deactivate_task(rq, p,
						DEQUEUE_SAVE | DEQUEUE_NOCLOCK);
				if (running)
					put_prev_task(rq, p);

				p->static_prio = NICE_TO_PRIO(backup);
				p->prio = p->normal_prio = p->static_prio;
				set_load_weight(p, false);

				if (queued)
					activate_task(rq, p,
						ENQUEUE_RESTORE | ENQUEUE_NOCLOCK);
				if (running)
					set_next_task(rq, p);

				trace_turbo_rtmutex_prepare_setprio(turbo_data, p);
				__task_rq_unlock(rq, &rf);
			}
		}
	}
}

static void probe_android_rvh_set_user_nice(void *ignore, struct task_struct *p, long *nice, bool *allowed)
{
	struct task_turbo_t *turbo_data;

	if ((*nice < MIN_NICE || *nice > MAX_NICE) && !task_turbo_nice(*nice)) {
                *allowed = false;
		return;
	} else
                *allowed = true;


	turbo_data = get_task_turbo_t(p);
	/* for general use, backup it */
	if (!task_turbo_nice(*nice))
		turbo_data->nice_backup = *nice;

	if (is_turbo_task(p) && !task_restore_nice(*nice)) {
		*nice = rlimit_to_nice(task_rlimit(p, RLIMIT_NICE));
		if (unlikely(*nice > MAX_NICE)) {
			pr_warn("%s: pid=%d RLIMIT_NICE=%ld is not set\n",
				TAG, p->pid, *nice);
			*nice = turbo_data->nice_backup;
		}
	} else
		*nice = turbo_data->nice_backup;

	trace_sched_set_user_nice(p, *nice, is_turbo_task(p));
}

static void probe_android_rvh_setscheduler(void *ignore, struct task_struct *p)
{
	struct task_turbo_t *turbo_data;

	if (!dl_prio(p->prio) && !rt_prio(p->prio)) {
		turbo_data = get_task_turbo_t(p);
		turbo_data->nice_backup = PRIO_TO_NICE(p->prio);
	}
}

static void probe_android_vh_rwsem_write_finished(void *ignore, struct rw_semaphore *sem)
{
	rwsem_stop_turbo_inherit(sem);
}

static void probe_android_vh_rwsem_init(void *ignore, struct rw_semaphore *sem)
{
	sem->android_vendor_data1 = 0;
}

static void probe_android_vh_alter_rwsem_list_add(void *ignore, struct rwsem_waiter *waiter,
							struct rw_semaphore *sem,
							bool *already_on_list)
{
	rwsem_list_add(waiter->task, &waiter->list, &sem->wait_list);
	*already_on_list = true;
}

static void probe_android_vh_rwsem_wake(void *ignore, struct rw_semaphore *sem)
{
	rwsem_start_turbo_inherit(sem);
}

static void probe_android_vh_binder_transaction_init(void *ignore, struct binder_transaction *t)
{
	t->android_vendor_data1 = 0;
}

static void probe_android_vh_binder_set_priority(void *ignore, struct binder_transaction *t,
							struct task_struct *task)
{
	if (binder_start_turbo_inherit(t->from ?
			t->from->task : NULL, task)) {
		t->android_vendor_data1 = (u64)task;
	}
}

static void probe_android_vh_binder_restore_priority(void *ignore,
		struct binder_transaction *in_reply_to, struct task_struct *cur)
{
	struct task_struct *inherit_task;

	if (in_reply_to) {
		inherit_task = get_inherit_task(in_reply_to);
		if (cur && cur == inherit_task) {
			binder_stop_turbo_inherit(cur);
			in_reply_to->android_vendor_data1 = 0;
		}
	} else
		binder_stop_turbo_inherit(cur);
}

static void probe_android_vh_alter_futex_plist_add(void *ignore, struct plist_node *q_list,
						struct plist_head *hb_chain, bool *already_on_hb)
{
	struct futex_q *this, *next;
	struct plist_node *current_node = q_list;
	struct plist_node *this_node;
	int prev_pid = 0;
	bool prev_turbo = 1;

	if (!sub_feat_enable(SUB_FEAT_LOCK) &&
	    !is_turbo_task(current)) {
		*already_on_hb = false;
		return;
	}

	plist_for_each_entry_safe(this, next, hb_chain, list) {
		if ((!this->pi_state || !this->rt_waiter) && !is_turbo_task(this->task)) {
			this_node = &this->list;
			trace_turbo_futex_plist_add(prev_pid, prev_turbo,
					this->task->pid, is_turbo_task(this->task));
			list_add(&current_node->node_list,
				 this_node->node_list.prev);
			*already_on_hb = true;
			return;
		}
		prev_pid = this->task->pid;
		prev_turbo = is_turbo_task(this->task);
	}

	*already_on_hb = false;
}

static void probe_android_rvh_select_task_rq_fair(void *ignore, struct task_struct *p,
							int prev_cpu, int sd_flag,
							int wake_flags, int *target_cpu)
{
	*target_cpu = select_turbo_cpu(p);
}

static inline struct task_struct *rwsem_owner(struct rw_semaphore *sem)
{
	return (struct task_struct *)
		(atomic_long_read(&sem->owner) & ~RWSEM_OWNER_FLAGS_MASK);
}

static inline bool rwsem_test_oflags(struct rw_semaphore *sem, long flags)
{
	return atomic_long_read(&sem->owner) & flags;
}

static inline bool is_rwsem_reader_owned(struct rw_semaphore *sem)
{
#if IS_ENABLED(CONFIG_DEBUG_RWSEMS)
	/*
	 * Check the count to see if it is write-locked.
	 */
	long count = atomic_long_read(&sem->count);

	if (count & RWSEM_WRITER_MASK)
		return false;
#endif
	return rwsem_test_oflags(sem, RWSEM_READER_OWNED);
}

static unsigned long capacity_of(int cpu)
{
	return cpu_rq(cpu)->cpu_capacity;
}

/*
 * cpu_util_without: compute cpu utilization without any contributions from *p
 * @cpu: the CPU which utilization is requested
 * @p: the task which utilization should be discounted
 *
 * The utilization of a CPU is defined by the utilization of tasks currently
 * enqueued on that CPU as well as tasks which are currently sleeping after an
 * execution on that CPU.
 *
 * This method returns the utilization of the specified CPU by discounting the
 * utilization of the specified task, whenever the task is currently
 * contributing to the CPU utilization.
 */
static unsigned long cpu_util_without(int cpu, struct task_struct *p)
{
	struct cfs_rq *cfs_rq;
	unsigned int util;

	/* Task has no contribution or is new */
	if (cpu != task_cpu(p) || !READ_ONCE(p->se.avg.last_update_time))
		return cpu_util(cpu);

	cfs_rq = &cpu_rq(cpu)->cfs;
	util = READ_ONCE(cfs_rq->avg.util_avg);

	/* Discount task's util from CPU's util */
	lsub_positive(&util, task_util(p));

	/*
	 * Covered cases:
	 *
	 * a) if *p is the only task sleeping on this CPU, then:
	 *      cpu_util (== task_util) > util_est (== 0)
	 *    and thus we return:
	 *      cpu_util_without = (cpu_util - task_util) = 0
	 *
	 * b) if other tasks are SLEEPING on this CPU, which is now exiting
	 *    IDLE, then:
	 *      cpu_util >= task_util
	 *      cpu_util > util_est (== 0)
	 *    and thus we discount *p's blocked utilization to return:
	 *      cpu_util_without = (cpu_util - task_util) >= 0
	 *
	 * c) if other tasks are RUNNABLE on that CPU and
	 *      util_est > cpu_util
	 *    then we use util_est since it returns a more restrictive
	 *    estimation of the spare capacity on that CPU, by just
	 *    considering the expected utilization of tasks already
	 *    runnable on that CPU.
	 *
	 * Cases a) and b) are covered by the above code, while case c) is
	 * covered by the following code when estimated utilization is
	 * enabled.
	 */
	if (sched_feat(UTIL_EST)) {
		unsigned int estimated =
			READ_ONCE(cfs_rq->avg.util_est.enqueued);

		/*
		 * Despite the following checks we still have a small window
		 * for a possible race, when an execl's select_task_rq_fair()
		 * races with LB's detach_task():
		 *
		 *   detach_task()
		 *     p->on_rq = TASK_ON_RQ_MIGRATING;
		 *     ---------------------------------- A
		 *     deactivate_task()                   \
		 *       dequeue_task()                     + RaceTime
		 *         util_est_dequeue()              /
		 *     ---------------------------------- B
		 *
		 * The additional check on "current == p" it's required to
		 * properly fix the execl regression and it helps in further
		 * reducing the chances for the above race.
		 */
		if (unlikely(task_on_rq_queued(p) || current == p))
			lsub_positive(&estimated, _task_util_est(p));

		util = max(util, estimated);
	}

	/*
	 * Utilization (estimated) can exceed the CPU capacity, thus let's
	 * clamp to the maximum CPU capacity to ensure consistency with
	 * the cpu_util call.
	 */
	return min_t(unsigned long, util, capacity_orig_of(cpu));
}

static inline unsigned long cpu_util(int cpu)
{
	struct cfs_rq *cfs_rq;
	unsigned int util;

	cfs_rq = &cpu_rq(cpu)->cfs;
	util = READ_ONCE(cfs_rq->avg.util_avg);

	if (sched_feat(UTIL_EST))
		util = max(util, READ_ONCE(cfs_rq->avg.util_est.enqueued));

	return min_t(unsigned long, util, capacity_orig_of(cpu));
}

static inline unsigned long task_util(struct task_struct *p)
{
	return READ_ONCE(p->se.avg.util_avg);
}

static inline unsigned long _task_util_est(struct task_struct *p)
{
	struct util_est ue = READ_ONCE(p->se.avg.util_est);

	return max(ue.ewma, (ue.enqueued & ~UTIL_AVG_UNCHANGED));
}

int find_best_turbo_cpu(struct task_struct *p)
{
	struct hmp_domain *domain;
	struct hmp_domain *tmp_domain[5] = {0, 0, 0, 0, 0};
	int i, domain_cnt = 0;
	int iter_cpu;
	unsigned long spare_cap, max_spare_cap = 0;
	const struct cpumask *tsk_cpus_ptr = p->cpus_ptr;
	int max_spare_cpu = -1;
	int new_cpu = -1;

	/* The order is B, BL, LL cluster */
	for_each_hmp_domain_L_first(domain) {
		tmp_domain[domain_cnt] = domain;
		domain_cnt++;
	}

	for (i = 0; i < domain_cnt; i++) {
		domain = tmp_domain[i];
		/* check fastest domain for turbo task */
		if (i != 0)
			break;
		for_each_cpu(iter_cpu, &domain->possible_cpus) {

			if (!cpu_online(iter_cpu) ||
			    !cpumask_test_cpu(iter_cpu, tsk_cpus_ptr) ||
			    !cpu_active(iter_cpu))
				continue;

			/*
			 * favor tasks that prefer idle cpus
			 * to improve latency
			 */
			if (idle_cpu(iter_cpu)) {
				new_cpu = iter_cpu;
				goto out;
			}

			spare_cap =
			     max_t(long, capacity_of(iter_cpu) - cpu_util_without(iter_cpu, p), 0);
			/*
			 * trace_printk("util:%d cpu_cap: %d  spare_cap: %d max_spare_cap: %d \n",
			 *		util, cpu_cap, spare_cap, max_spare_cap);
			 */
			if (spare_cap > max_spare_cap) {
				max_spare_cap = spare_cap;
				max_spare_cpu = iter_cpu;
			}
		}
	}
	if (max_spare_cpu > 0)
		new_cpu = max_spare_cpu;
out:
	trace_select_turbo_cpu(new_cpu, p, max_spare_cap, max_spare_cpu);
	return new_cpu;
}

int select_turbo_cpu(struct task_struct *p)
{
	int target_cpu = -1;

	if (!is_turbo_task(p))
		return -1;

	if (!sub_feat_enable(SUB_FEAT_FLAVOR_BIGCORE))
		return -1;

	target_cpu = find_best_turbo_cpu(p);

	return target_cpu;
}

/* copy from sched/core.c */
static void set_load_weight(struct task_struct *p, bool update_load)
{
	int prio = p->static_prio - MAX_RT_PRIO;
	struct load_weight *load = &p->se.load;

	/*
	 * SCHED_IDLE tasks get minimal weight:
	 */
	if (task_has_idle_policy(p)) {
		load->weight = scale_load(WEIGHT_IDLEPRIO);
		load->inv_weight = WMULT_IDLEPRIO;
		return;
	}

	/*
	 * SCHED_OTHER tasks have to update their load when changing their
	 * weight
	 */
	if (update_load && p->sched_class == &fair_sched_class) {
		reweight_task(p, prio);
	} else {
		load->weight = scale_load(sched_prio_to_weight[prio]);
		load->inv_weight = sched_prio_to_wmult[prio];
	}
}

int idle_cpu(int cpu)
{
	struct rq *rq = cpu_rq(cpu);

	if (rq->curr != rq->idle)
		return 0;

	if (rq->nr_running)
		return 0;

#if IS_ENABLED(CONFIG_SMP)
	if (rq->ttwu_pending)
		return 0;
#endif

	return 1;
}

static void rwsem_stop_turbo_inherit(struct rw_semaphore *sem)
{
	unsigned long flags;
	struct task_struct *inherit_task;

	raw_spin_lock_irqsave(&sem->wait_lock, flags);
	inherit_task = get_inherit_task(sem);
	if (inherit_task == current) {
		stop_turbo_inherit(current, RWSEM_INHERIT);
		sem->android_vendor_data1 = 0;
		trace_turbo_inherit_end(current);
	}
	raw_spin_unlock_irqrestore(&sem->wait_lock, flags);
}

static void rwsem_list_add(struct task_struct *task,
			   struct list_head *entry,
			   struct list_head *head)
{
	if (!sub_feat_enable(SUB_FEAT_LOCK)) {
		list_add_tail(entry, head);
		return;
	}

	if (is_turbo_task(task)) {
		struct list_head *pos = NULL;
		struct list_head *n = NULL;
		struct rwsem_waiter *waiter = NULL;

		/* insert turbo task pior to first non-turbo task */
		list_for_each_safe(pos, n, head) {
			waiter = list_entry(pos,
					struct rwsem_waiter, list);
			if (!is_turbo_task(waiter->task)) {
				list_add(entry, waiter->list.prev);
				return;
			}
		}
	}
	list_add_tail(entry, head);
}

static void rwsem_start_turbo_inherit(struct rw_semaphore *sem)
{
	bool should_inherit;
	struct task_struct *owner, *inherited_owner;
	struct task_turbo_t *turbo_data;

	if (!sub_feat_enable(SUB_FEAT_LOCK))
		return;

	owner = rwsem_owner(sem);
	should_inherit = should_set_inherit_turbo(current);
	if (should_inherit) {
		inherited_owner = get_inherit_task(sem);
		turbo_data = get_task_turbo_t(current);
		if (owner && !is_rwsem_reader_owned(sem) &&
		    !is_turbo_task(owner) &&
		    !inherited_owner) {
			start_turbo_inherit(owner,
					    RWSEM_INHERIT,
					    turbo_data->inherit_cnt);
			sem->android_vendor_data1 = (u64)owner;
			trace_turbo_inherit_start(current, owner);
		}
	}
}

static bool start_turbo_inherit(struct task_struct *task,
				int type,
				int cnt)
{
	struct task_turbo_t *turbo_data;

	if (type <= START_INHERIT || type >= END_INHERIT)
		return false;

	add_inherit_types(task, type);
	turbo_data = get_task_turbo_t(task);
	if (turbo_data->inherit_cnt < cnt + 1)
		turbo_data->inherit_cnt = cnt + 1;

	/* scheduler tuning start */
	set_scheduler_tuning(task);
	return true;
}

static bool stop_turbo_inherit(struct task_struct *task,
				int type)
{
	unsigned int inherit_types;
	bool ret = false;
	struct task_turbo_t *turbo_data;

	if (type <= START_INHERIT || type >= END_INHERIT)
		goto done;

	turbo_data = get_task_turbo_t(task);
	inherit_types = atomic_read(&turbo_data->inherit_types);
	if (inherit_types == 0)
		goto done;

	sub_inherit_types(task, type);
	turbo_data = get_task_turbo_t(task);
	inherit_types = atomic_read(&turbo_data->inherit_types);
	if (inherit_types > 0)
		goto done;

	/* scheduler tuning stop */
	unset_scheduler_tuning(task);
	turbo_data->inherit_cnt = 0;
	ret = true;
done:
	return ret;
}

static inline void set_scheduler_tuning(struct task_struct *task)
{
	int cur_nice = task_nice(task);

	if (!fair_policy(task->policy))
		return;

	if (!sub_feat_enable(SUB_FEAT_SCHED))
		return;

	/* trigger renice for turbo task */
	set_user_nice(task, 0xbeef);

	trace_sched_turbo_nice_set(task, NICE_TO_PRIO(cur_nice), task->prio);
}

static inline void unset_scheduler_tuning(struct task_struct *task)
{
	int cur_prio = task->prio;

	if (!fair_policy(task->policy))
		return;

	set_user_nice(task, 0xbeee);

	trace_sched_turbo_nice_set(task, cur_prio, task->prio);
}

static inline void add_inherit_types(struct task_struct *task, int type)
{
	struct task_turbo_t *turbo_data;

	turbo_data = get_task_turbo_t(task);
	atomic_add(type_number(type), &turbo_data->inherit_types);
}

static inline void sub_inherit_types(struct task_struct *task, int type)
{
	struct task_turbo_t *turbo_data;

	turbo_data = get_task_turbo_t(task);
	atomic_sub(type_number(type), &turbo_data->inherit_types);
}

static bool binder_start_turbo_inherit(struct task_struct *from,
					struct task_struct *to)
{
	bool ret = false;
	struct task_turbo_t *from_turbo_data;

	if (!sub_feat_enable(SUB_FEAT_BINDER))
		goto done;

	if (!from || !to)
		goto done;

	if (!is_turbo_task(from) ||
		!test_turbo_cnt(from)) {
		from_turbo_data = get_task_turbo_t(from);
		trace_turbo_inherit_failed(from_turbo_data->turbo,
					atomic_read(&from_turbo_data->inherit_types),
					from_turbo_data->inherit_cnt, __LINE__);
		goto done;
	}

	if (!is_turbo_task(to)) {
		from_turbo_data = get_task_turbo_t(from);
		ret = start_turbo_inherit(to, BINDER_INHERIT,
					from_turbo_data->inherit_cnt);
		trace_turbo_inherit_start(from, to);
	}
done:
	return ret;
}

static void binder_stop_turbo_inherit(struct task_struct *p)
{
	if (is_inherit_turbo(p, BINDER_INHERIT))
		stop_turbo_inherit(p, BINDER_INHERIT);
	trace_turbo_inherit_end(p);
}

static bool is_inherit_turbo(struct task_struct *task, int type)
{
	unsigned int inherit_types;
	struct task_turbo_t *turbo_data;

	if (!task)
		return false;

	turbo_data = get_task_turbo_t(task);
	inherit_types = atomic_read(&turbo_data->inherit_types);

	if (inherit_types == 0)
		return false;

	return get_value_with_type(inherit_types, type) > 0;
}

static bool is_turbo_task(struct task_struct *p)
{
	struct task_turbo_t *turbo_data;

	if (!p)
		return false;

	turbo_data = get_task_turbo_t(p);
	return turbo_data->turbo || atomic_read(&turbo_data->inherit_types);
}

static bool test_turbo_cnt(struct task_struct *task)
{
	struct task_turbo_t *turbo_data;

	turbo_data = get_task_turbo_t(task);
	/* TODO:limit number should be discuss */
	return turbo_data->inherit_cnt < INHERIT_THRESHOLD;
}

static inline bool should_set_inherit_turbo(struct task_struct *task)
{
	return is_turbo_task(task) && test_turbo_cnt(task);
}

inline bool latency_turbo_enable(void)
{
	return task_turbo_feats == latency_turbo;
}

inline bool launch_turbo_enable(void)
{
	return task_turbo_feats == launch_turbo;
}

static void init_turbo_attr(struct task_struct *p)
{
	struct task_turbo_t *turbo_data = get_task_turbo_t(p);

	turbo_data->turbo = TURBO_DISABLE;
	turbo_data->render = 0;
	atomic_set(&(turbo_data->inherit_types), 0);
	turbo_data->inherit_cnt = 0;
}

int get_turbo_feats(void)
{
	return task_turbo_feats;
}

static bool sub_feat_enable(int type)
{
	return get_turbo_feats() & type;
}

/*
 * set task to turbo by pid
 */
static int set_turbo_task(int pid, int val)
{
	struct task_struct *p;
	struct task_turbo_t *turbo_data;
	int retval = 0;

	if (pid < 0 || pid > PID_MAX_DEFAULT)
		return -EINVAL;

	if (val < 0 || val > 1)
		return -EINVAL;

	rcu_read_lock();
	p = find_task_by_vpid(pid);

	if (p != NULL) {
		get_task_struct(p);
		turbo_data = get_task_turbo_t(p);
		turbo_data->turbo = val;
		/*TODO: scheduler tuning */
		if (turbo_data->turbo == TURBO_ENABLE)
			set_scheduler_tuning(p);
		else
			unset_scheduler_tuning(p);
		trace_turbo_set(p);
		put_task_struct(p);
	} else
		retval = -ESRCH;
	rcu_read_unlock();

	return retval;
}

static int unset_turbo_task(int pid)
{
	return set_turbo_task(pid, TURBO_DISABLE);
}

static int set_task_turbo_feats(const char *buf,
				const struct kernel_param *kp)
{
	int ret = 0, i;
	unsigned int val;

	ret = kstrtouint(buf, 0, &val);


	spin_lock(&TURBO_SPIN_LOCK);
	if (val == latency_turbo ||
	    val == launch_turbo  || val == 0)
		ret = param_set_uint(buf, kp);
	else
		ret = -EINVAL;

	/* if disable turbo, remove all turbo tasks */
	/* spin_lock(&TURBO_SPIN_LOCK); */
	if (val == 0) {
		for (i = 0; i < TURBO_PID_COUNT; i++) {
			if (turbo_pid[i]) {
				unset_turbo_task(turbo_pid[i]);
				turbo_pid[i] = 0;
			}
		}
	}
	spin_unlock(&TURBO_SPIN_LOCK);

	if (!ret)
		pr_info("%s: task_turbo_feats is change to %d successfully",
				TAG, task_turbo_feats);
	return ret;
}

static struct kernel_param_ops task_turbo_feats_param_ops = {
	.set = set_task_turbo_feats,
	.get = param_get_uint,
};

param_check_uint(feats, &task_turbo_feats);
module_param_cb(feats, &task_turbo_feats_param_ops, &task_turbo_feats, 0644);
MODULE_PARM_DESC(feats, "enable task turbo features if needed");

static bool add_turbo_list_locked(pid_t pid);
static void remove_turbo_list_locked(pid_t pid);

/*
 * use pid set turbo task
 */
static int add_turbo_list_by_pid(pid_t pid)
{
	int retval = -EINVAL;

	if (!task_turbo_feats)
		return retval;

	if (pid < 0 || pid > PID_MAX_DEFAULT)
		return retval;

	spin_lock(&TURBO_SPIN_LOCK);
	if (!add_turbo_list_locked(pid))
		goto unlock;

	retval = set_turbo_task(pid, TURBO_ENABLE);
unlock:
	spin_unlock(&TURBO_SPIN_LOCK);
	return retval;
}

static pid_t turbo_pid_param;
static int set_turbo_task_param(const char *buf,
				const struct kernel_param *kp)
{
	int retval = 0;
	pid_t pid;

	retval = kstrtouint(buf, 0, &pid);

	if (!retval)
		retval = add_turbo_list_by_pid(pid);

	if (!retval)
		turbo_pid_param = pid;

	return retval;
}

static struct kernel_param_ops turbo_pid_param_ops = {
	.set = set_turbo_task_param,
	.get = param_get_int,
};

param_check_uint(turbo_pid, &turbo_pid_param);
module_param_cb(turbo_pid, &turbo_pid_param_ops, &turbo_pid_param, 0644);
MODULE_PARM_DESC(turbo_pid, "set turbo task by pid");

static int unset_turbo_list_by_pid(pid_t pid)
{
	int retval = -EINVAL;

	if (pid < 0 || pid > PID_MAX_DEFAULT)
		return retval;

	spin_lock(&TURBO_SPIN_LOCK);
	remove_turbo_list_locked(pid);
	retval = unset_turbo_task(pid);
	spin_unlock(&TURBO_SPIN_LOCK);
	return retval;
}

static pid_t unset_turbo_pid_param;
static int unset_turbo_task_param(const char *buf,
				  const struct kernel_param *kp)
{
	int retval = 0;
	pid_t pid;

	retval = kstrtouint(buf, 0, &pid);

	if (!retval)
		retval = unset_turbo_list_by_pid(pid);

	if (!retval)
		unset_turbo_pid_param = pid;

	return retval;
}

static struct kernel_param_ops unset_turbo_pid_param_ops = {
	.set = unset_turbo_task_param,
	.get = param_get_int,
};

param_check_uint(unset_turbo_pid, &unset_turbo_pid_param);
module_param_cb(unset_turbo_pid, &unset_turbo_pid_param_ops,
		&unset_turbo_pid_param, 0644);
MODULE_PARM_DESC(unset_turbo_pid, "unset turbo task by pid");

static inline int get_st_group_id(struct task_struct *task)
{
#if IS_ENABLED(CONFIG_CGROUP_SCHED)
	const int subsys_id = cpu_cgrp_id;
	struct cgroup *grp;

	rcu_read_lock();
	grp = task_cgroup(task, subsys_id);
	rcu_read_unlock();
	return grp->kn->id;
#else
	return 0;
#endif
}

static inline bool cgroup_check_set_turbo(struct task_struct *p)
{
	struct task_turbo_t *turbo_data;

	turbo_data = get_task_turbo_t(p);

	if (!launch_turbo_enable())
		return false;

	if (turbo_data->turbo)
		return false;

	/* set critical tasks for UI or UX to turbo */
	return (turbo_data->render ||
	       (p == p->group_leader &&
		p->real_parent->pid != 1));
}

/*
 * record task to turbo list
 */
static bool add_turbo_list_locked(pid_t pid)
{
	int i, free_idx = -1;
	bool ret = false;

	if (unlikely(!get_turbo_feats()))
		goto done;

	for (i = 0; i < TURBO_PID_COUNT; i++) {
		if (free_idx < 0 && !turbo_pid[i])
			free_idx = i;

		if (unlikely(turbo_pid[i] == pid)) {
			free_idx = i;
			break;
		}
	}

	if (free_idx >= 0) {
		turbo_pid[free_idx] = pid;
		ret = true;
	}
done:
	return ret;
}

static void add_turbo_list(struct task_struct *p)
{
	struct task_turbo_t *turbo_data;

	spin_lock(&TURBO_SPIN_LOCK);
	if (add_turbo_list_locked(p->pid)) {
		turbo_data = get_task_turbo_t(p);
		turbo_data->turbo = TURBO_ENABLE;
		/* TODO: scheduler tuninng */
		set_scheduler_tuning(p);
		trace_turbo_set(p);
	}
	spin_unlock(&TURBO_SPIN_LOCK);
}

/*
 * remove task from turbo list
 */
static void remove_turbo_list_locked(pid_t pid)
{
	int i;

	for (i = 0; i < TURBO_PID_COUNT; i++) {
		if (turbo_pid[i] == pid) {
			turbo_pid[i] = 0;
			break;
		}
	}
}

static void remove_turbo_list(struct task_struct *p)
{
	struct task_turbo_t *turbo_data;

	spin_lock(&TURBO_SPIN_LOCK);
	turbo_data = get_task_turbo_t(p);
	remove_turbo_list_locked(p->pid);
	turbo_data->turbo = TURBO_DISABLE;
	unset_scheduler_tuning(p);
	trace_turbo_set(p);
	spin_unlock(&TURBO_SPIN_LOCK);
}

static void probe_android_vh_cgroup_set_task(void *ignore, int ret, struct task_struct *p)
{
	struct task_turbo_t *turbo_data;

	if (ret)
		return;

	if (get_st_group_id(p) == TOP_APP_GROUP_ID) {
		if (!cgroup_check_set_turbo(p))
			return;
		add_turbo_list(p);
	} else {
		turbo_data = get_task_turbo_t(p);
		if (turbo_data->turbo)
			remove_turbo_list(p);
	}
}

static void probe_android_vh_syscall_prctl_finished(void *ignore, int option, struct task_struct *p)
{
	if (option == PR_SET_NAME)
		sys_set_turbo_task(p);
}

static inline void fillin_cluster(struct cluster_info *cinfo,
		struct hmp_domain *hmpd)
{
	int cpu;
	unsigned long cpu_perf;

	cinfo->hmpd = hmpd;
	cinfo->cpu = cpumask_any(&cinfo->hmpd->possible_cpus);

	for_each_cpu(cpu, &hmpd->possible_cpus) {
		cpu_perf = arch_scale_cpu_capacity(cpu);
		if (cpu_perf > 0)
			break;
	}
	cinfo->cpu_perf = cpu_perf;

	if (cpu_perf == 0)
		pr_info("%s: Uninitialized CPU performance (CPU mask: %lx)",
				TAG, cpumask_bits(&hmpd->possible_cpus)[0]);
}

int hmp_compare(void *priv, struct list_head *a, struct list_head *b)
{
	struct cluster_info ca;
	struct cluster_info cb;

	fillin_cluster(&ca, list_entry(a, struct hmp_domain, hmp_domains));
	fillin_cluster(&cb, list_entry(b, struct hmp_domain, hmp_domains));

	return (ca.cpu_perf > cb.cpu_perf) ? -1 : 1;
}

void init_hmp_domains(void)
{
	struct hmp_domain *domain;
	struct cpumask cpu_mask;
	int id, maxid;

	cpumask_clear(&cpu_mask);
	maxid = arch_get_nr_clusters();

	/*
	 * Initialize hmp_domains
	 * Must be ordered with respect to compute capacity.
	 * Fastest domain at head of list.
	 */
	for (id = 0; id < maxid; id++) {
		arch_get_cluster_cpus(&cpu_mask, id);
		domain = (struct hmp_domain *)
			kmalloc(sizeof(struct hmp_domain), GFP_KERNEL);
		if (domain) {
			cpumask_copy(&domain->possible_cpus, &cpu_mask);
			cpumask_and(&domain->cpus, cpu_online_mask,
				&domain->possible_cpus);
			list_add(&domain->hmp_domains, &hmp_domains);
		}
	}

	/*
	 * Sorting HMP domain by CPU capacity
	 */
	list_sort(NULL, &hmp_domains, &hmp_compare);
	pr_info("Sort hmp_domains from little to big:\n");
	for_each_hmp_domain_L_first(domain) {
		pr_info("    cpumask: 0x%02lx\n",
				*cpumask_bits(&domain->possible_cpus));
	}
	hmp_cpu_mask_setup();
}

void hmp_cpu_mask_setup(void)
{
	struct hmp_domain *domain;
	struct list_head *pos;
	int cpu;

	pr_info("Initializing HMP scheduler:\n");

	/* Initialize hmp_domains using platform code */
	if (list_empty(&hmp_domains)) {
		pr_info("HMP domain list is empty!\n");
		return;
	}

	/* Print hmp_domains */
	list_for_each(pos, &hmp_domains) {
		domain = list_entry(pos, struct hmp_domain, hmp_domains);

		for_each_cpu(cpu, &domain->possible_cpus)
			per_cpu(hmp_cpu_domain, cpu) = domain;
	}
	pr_info("Initializing HMP scheduler done\n");
}

int arch_get_nr_clusters(void)
{
	int __arch_nr_clusters = -1;
	int max_id = 0;
	unsigned int cpu;

	/* assume socket id is monotonic increasing without gap. */
	for_each_possible_cpu(cpu) {
		struct cpu_topology *cpu_topo = &cpu_topology[cpu];

		if (cpu_topo->package_id > max_id)
			max_id = cpu_topo->package_id;
	}
	__arch_nr_clusters = max_id + 1;
	return __arch_nr_clusters;
}

void arch_get_cluster_cpus(struct cpumask *cpus, int package_id)
{
	unsigned int cpu;

	cpumask_clear(cpus);
	for_each_possible_cpu(cpu) {
		struct cpu_topology *cpu_topo = &cpu_topology[cpu];

		if (cpu_topo->package_id == package_id)
			cpumask_set_cpu(cpu, cpus);
	}
}

static void sys_set_turbo_task(struct task_struct *p)
{
	struct task_turbo_t *turbo_data;

	if (strcmp(p->comm, RENDER_THREAD_NAME))
		return;

	if (!launch_turbo_enable())
		return;

	if (get_st_group_id(p) != TOP_APP_GROUP_ID)
		return;

	turbo_data = get_task_turbo_t(p);
	turbo_data->render = 1;
	add_turbo_list(p);
}

static int __init init_task_turbo(void)
{
	int ret, ret_erri_line;

	ret = register_trace_android_rvh_rtmutex_prepare_setprio(
			probe_android_rvh_rtmutex_prepare_setprio, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_rvh_prepare_prio_fork(
			probe_android_rvh_prepare_prio_fork, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_rvh_finish_prio_fork(
			probe_android_rvh_finish_prio_fork, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_rvh_set_user_nice(
			probe_android_rvh_set_user_nice, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_rvh_setscheduler(
			probe_android_rvh_setscheduler, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
	goto failed;
	}

	ret = register_trace_android_vh_binder_transaction_init(
			probe_android_vh_binder_transaction_init, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_vh_binder_set_priority(
			probe_android_vh_binder_set_priority, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
	goto failed;
	}

	ret = register_trace_android_vh_binder_restore_priority(
			probe_android_vh_binder_restore_priority, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_vh_rwsem_init(
			probe_android_vh_rwsem_init, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_vh_rwsem_wake(
			probe_android_vh_rwsem_wake, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_vh_rwsem_write_finished(
			probe_android_vh_rwsem_write_finished, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_vh_alter_rwsem_list_add(
			probe_android_vh_alter_rwsem_list_add, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_vh_alter_futex_plist_add(
			probe_android_vh_alter_futex_plist_add, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_rvh_select_task_rq_fair(
			probe_android_rvh_select_task_rq_fair, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_vh_cgroup_set_task(
			probe_android_vh_cgroup_set_task, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	ret = register_trace_android_vh_syscall_prctl_finished(
			probe_android_vh_syscall_prctl_finished, NULL);
	if (ret) {
		ret_erri_line = __LINE__;
		goto failed;
	}

	init_hmp_domains();

failed:
	if (ret)
		pr_err("register hooks failed, ret %d line %d\n", ret, ret_erri_line);

	return ret;
}
static void  __exit exit_task_turbo(void)
{
	/*
	 * vendor hook cannot unregister, please check vendor_hook.h
	 */
}
module_init(init_task_turbo);
module_exit(exit_task_turbo);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("MediaTek task-turbo");
