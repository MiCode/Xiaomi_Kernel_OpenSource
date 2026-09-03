// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023, Unisoc (shanghai) Technologies Co., Ltd
 */

#define pr_fmt(fmt)	"core_pause: " fmt

#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/sched/isolation.h>
#include <trace/hooks/sched.h>
#include "uni_sched.h"
#include "trace.h"

/* spin lock to allow calling from non-preemptible context */
static DEFINE_RAW_SPINLOCK(halt_lock);

struct halt_cpu_state {
	u64		last_halt;
	u8		reason;
};

static DEFINE_PER_CPU(struct halt_cpu_state, halt_state);
static DEFINE_RAW_SPINLOCK(walt_drain_pending_lock);

/* the amount of time allowed for enqueue operations that happen
 * just after a halt operation.
 */
#define WALT_HALT_CHECK_THRESHOLD_NS 400000

/*
 * Remove a task from the runqueue and pretend that it's migrating. This
 * should prevent migrations for the detached task and disallow further
 * changes to tsk_cpus_allowed.
 */
static void
detach_one_task_core(struct task_struct *p, struct rq *rq,
		     struct list_head *tasks)
{
	struct uni_task_struct *uni_tsk;

	lockdep_assert_held(&rq->__lock);

	p->on_rq = TASK_ON_RQ_MIGRATING;
	deactivate_task(rq, p, 0);
	uni_tsk = (struct uni_task_struct *)p->android_vendor_data1;
	uni_tsk->percpu_tsk = p;
	INIT_LIST_HEAD(&uni_tsk->percpu_kthread_node);
	list_add(&uni_tsk->percpu_kthread_node, tasks);
}

static void attach_tasks_core(struct list_head *tasks, struct rq *rq)
{
	struct task_struct *p;
	struct uni_task_struct *uni_tsk;

	lockdep_assert_held(&rq->__lock);

	while (!list_empty(tasks)) {
		uni_tsk = list_first_entry(tasks, struct uni_task_struct, percpu_kthread_node);
		p = uni_tsk->percpu_tsk;
		list_del(&uni_tsk->percpu_kthread_node);

		WARN_ON(task_rq(p) != rq);
		activate_task(rq, p, 0);
		p->on_rq = TASK_ON_RQ_QUEUED;
	}
}

/*
 * Migrate all tasks from the rq, sleeping tasks will be migrated by
 * try_to_wake_up()->select_task_rq().
 *
 * Called with rq->__lock held even though we'er in stop_machine() and
 * there's no concurrency possible, we hold the required locks anyway
 * because of lock validation efforts.
 *
 * The function will skip CPU pinned kthreads.
 */
static void migrate_tasks(struct rq *dead_rq, struct rq_flags *rf)
{
	struct rq *rq = dead_rq;
	struct task_struct *next, *stop = rq->stop;
	LIST_HEAD(percpu_kthreads);
	unsigned int num_pinned_kthreads = 1;
	struct rq_flags orf = *rf;
	int dest_cpu;

	/*
	 * Fudge the rq selection such that the below task selection loop
	 * doesn't get stuck on the currently eligible stop task.
	 *
	 * We're currently inside stop_machine() and the rq is either stuck
	 * in the stop_machine_cpu_stop() loop, or we're executing this code,
	 * either way we should never end up calling schedule() until we're
	 * done here.
	 */
	rq->stop = NULL;

	/*
	 * put_prev_task() and pick_next_task() sched
	 * class method both need to have an up-to-date
	 * value of rq->clock[_task]
	 */
	update_rq_clock(rq);

#ifdef CONFIG_SCHED_DEBUG
	/* note the clock update in orf */
	orf.clock_update_flags |= RQCF_UPDATED;
#endif

	for (;;) {
		/*
		 * There's this thread running, bail when that's the only
		 * remaining thread:
		 */
		if (rq->nr_running == 1)
			break;

		next = pick_migrate_task(rq);

		/*
		 * Argh ... no iterator for tasks, we need to remove the
		 * kthread from the run-queue to continue.
		 */

		if (is_per_cpu_kthread(next)) {
			detach_one_task_core(next, rq, &percpu_kthreads);
			num_pinned_kthreads += 1;
			continue;
		}

		/*
		 * Rules for changing task_struct::cpus_mask are holding
		 * both pi_lock and rq->__lock, such that holding either
		 * stabilizes the mask.
		 *
		 * Drop rq->__lock is not quite as disastrous as it usually is
		 * because !cpu_active at this point, which means load-balance
		 * will not interfere. Also, stop-machine.
		 */
		rq_unlock(rq, rf);
		raw_spin_lock(&next->pi_lock);
		rq_relock(rq, rf);

		/*
		 * Since we're inside stop-machine, _nothing_ should have
		 * changed the task, WARN if weird stuff happened, because in
		 * that case the above rq->__lock drop is a fail too.
		 */
		if (task_rq(next) != rq || !task_on_rq_queued(next)) {
			raw_spin_unlock(&next->pi_lock);
			continue;
		}

		/* Find suitable destination for @next */
		dest_cpu = select_fallback_rq(dead_rq->cpu, next);

		if (cpu_of(rq) != dest_cpu && !is_migration_disabled(next)) {
			/* only perform a required migration */
			rq = __migrate_task(rq, rf, next, dest_cpu);

			if (rq != dead_rq) {
				rq_unlock(rq, rf);
				rq = dead_rq;
				*rf = orf;
				rq_relock(rq, rf);
			}
		} else {
			detach_one_task_core(next, rq, &percpu_kthreads);
			num_pinned_kthreads += 1;
		}

		raw_spin_unlock(&next->pi_lock);
	}

	if (num_pinned_kthreads > 1)
		attach_tasks_core(&percpu_kthreads, rq);

	rq->stop = stop;
}

void __balance_callbacks(struct rq *rq);

static int drain_rq_cpu_stop(void *data)
{
	struct rq *rq = this_rq();
	struct rq_flags rf;
	struct uni_rq *wrq = (struct uni_rq *) rq->android_vendor_data1;

	rq_lock_irqsave(rq, &rf);
	migrate_tasks(rq, &rf);

	/*
	 * service any callbacks that were accumulated, prior to unlocking. such that
	 * any subsequent calls to rq_lock... will see an rq->balance_callback set to
	 * the default (0 or balance_push_callback);
	 */
	wrq->enqueue_counter = 0;
	__balance_callbacks(rq);
	if (wrq->enqueue_counter)
		pr_err("cpu: %d task was re-enqueued", cpu_of(rq));

	rq_unlock_irqrestore(rq, &rf);

	return 0;
}

static int cpu_drain_rq(unsigned int cpu)
{
	if (!cpu_online(cpu))
		return 0;

	if (available_idle_cpu(cpu))
		return 0;

	/* this will schedule, must not be in atomic context */
	return stop_one_cpu(cpu, drain_rq_cpu_stop, NULL);
}

/*
 * returns true if last halt is within threshold
 * note: do not take halt_lock, called from atomic context
 */
bool halt_check_last(int cpu)
{
	u64 last_halt = per_cpu_ptr(&halt_state, cpu)->last_halt;

	/* last_halt is valid, check it against sched_clock */
	if (last_halt != 0 && sched_clock() - last_halt >  WALT_HALT_CHECK_THRESHOLD_NS)
		return false;

	return true;
}

struct drain_thread_data {
	cpumask_t cpus_to_drain;
};

static struct drain_thread_data drain_data = {
	.cpus_to_drain = { CPU_BITS_NONE }
};

static int __ref try_drain_rqs(void *data)
{
	cpumask_t *cpus_ptr = &((struct drain_thread_data *)data)->cpus_to_drain;
	int cpu;
	unsigned long flags;

	while (!kthread_should_stop()) {
		raw_spin_lock_irqsave(&walt_drain_pending_lock, flags);
		if (cpumask_weight(cpus_ptr)) {
			cpumask_t local_cpus;

			cpumask_copy(&local_cpus, cpus_ptr);
			raw_spin_unlock_irqrestore(&walt_drain_pending_lock, flags);

			for_each_cpu(cpu, &local_cpus)
				cpu_drain_rq(cpu);

			raw_spin_lock_irqsave(&walt_drain_pending_lock, flags);
			cpumask_andnot(cpus_ptr, cpus_ptr, &local_cpus);

		}
		raw_spin_unlock_irqrestore(&walt_drain_pending_lock, flags);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
		set_current_state(TASK_RUNNING);
	}

	return 0;
}

struct task_struct *walt_drain_thread;

/*
 * 1) add the cpus to the halt mask
 * 2) migrate tasks off the cpu
 */
static int halt_cpus(struct cpumask *cpus)
{
	int cpu;
	int ret = 0;
	u64 start_time = sched_clock();
	struct halt_cpu_state *halt_cpu_state;
	unsigned long flags;

	for_each_cpu(cpu, cpus) {

		halt_cpu_state = per_cpu_ptr(&halt_state, cpu);

		/* set the cpu as halted */
		cpumask_set_cpu(cpu, cpu_halt_mask);

		/* guarantee mask written before updating last_halt */
		wmb();

		halt_cpu_state->last_halt = start_time;
	}
	/* signal and wakeup the drain kthread */
	raw_spin_lock_irqsave(&walt_drain_pending_lock, flags);
	cpumask_or(&drain_data.cpus_to_drain, &drain_data.cpus_to_drain, cpus);
	raw_spin_unlock_irqrestore(&walt_drain_pending_lock, flags);

	wake_up_process(walt_drain_thread);

	return ret;
}

void walt_kick_cpu(int cpu)
{
	unsigned int flags = NOHZ_KICK_MASK;

	if (cpu == -1 || !cpu_online(cpu))
		return;

	/*
	 * Access to rq::nohz_csd is serialized by NOHZ_KICK_MASK; he who sets
	 * the first flag owns it; cleared by nohz_csd_func().
	 */
	flags = atomic_fetch_or(flags, nohz_flags(cpu));
	if (flags & NOHZ_KICK_MASK)
		return;

	/*
	 * This way we generate an IPI on the target CPU which
	 * is idle. And the softirq performing nohz idle load balance
	 * will be run before returning from the IPI.
	 */
	smp_call_function_single_async(cpu, &cpu_rq(cpu)->nohz_csd);
}

/*
 * 1) remove the cpus from the halt mask
 *
 */
static int start_cpus(struct cpumask *cpus)
{
	struct halt_cpu_state *halt_cpu_state;
	int cpu;

	for_each_cpu(cpu, cpus) {
		halt_cpu_state = per_cpu_ptr(&halt_state, cpu);
		halt_cpu_state->last_halt = 0;

		/* wmb to guarantee zero'd last_halt before clearing from the mask */
		wmb();

		cpumask_clear_cpu(cpu, cpu_halt_mask);

		/* kick the cpu so it can pull tasks
		 * after the mask has been cleared.
		 */
		walt_kick_cpu(cpu);
		pr_info("resume cpu:%d\n", cpu);
	}

	return 0;
}

/* update reason for cpus in yield/halt mask */
static void update_reasons(struct cpumask *cpus, bool halt, enum pause_reason reason)
{
	int cpu;
	struct halt_cpu_state *halt_cpu_state;

	for_each_cpu(cpu, cpus) {
		halt_cpu_state = per_cpu_ptr(&halt_state, cpu);
		if (halt)
			halt_cpu_state->reason |=  reason;
		else
			halt_cpu_state->reason &= ~reason;
	}
}

/* remove cpus that are already halted */
static void update_halt_cpus(struct cpumask *cpus)
{
	int cpu;
	struct halt_cpu_state *halt_cpu_state;

	for_each_cpu(cpu, cpus) {
		halt_cpu_state = per_cpu_ptr(&halt_state, cpu);
		if (halt_cpu_state->reason)
			cpumask_clear_cpu(cpu, cpus);
	}
}

static void check_halt_reason(struct cpumask *cpus, enum pause_reason reason)
{
	int cpu;
	struct halt_cpu_state *halt_cpu_state;

	for_each_cpu(cpu, cpus) {
		halt_cpu_state = per_cpu_ptr(&halt_state, cpu);
		if (halt_cpu_state->reason &&
		    ((halt_cpu_state->reason & reason) != reason))
			cpumask_clear_cpu(cpu, cpus);
	}
}

static void dump_halt_cpus(void)
{
	int cpu;
	struct halt_cpu_state *halt_cpu_state;

	for_each_cpu(cpu, cpu_halt_mask) {
		halt_cpu_state = per_cpu_ptr(&halt_state, cpu);
		pr_info("pause cpu:%d reason:%d\n", cpu, halt_cpu_state->reason);
	}

}

/* cpus will be modified */
int core_halt_cpus(struct cpumask *cpus, enum pause_reason reason)
{
	int ret = 0;
	cpumask_t requested_cpus;
	unsigned long flags;

	raw_spin_lock_irqsave(&halt_lock, flags);

	cpumask_copy(&requested_cpus, cpus);

	/* remove cpus that are already halted */
	update_halt_cpus(cpus);

	if (cpumask_empty(cpus)) {
		update_reasons(&requested_cpus, true, reason);
		goto unlock;
	}

	ret = halt_cpus(cpus);

	if (ret < 0)
		pr_debug("halt_cpus failure ret=%d cpus=%*pbl\n", ret,
			 cpumask_pr_args(&requested_cpus));
	else
		update_reasons(&requested_cpus, true, reason);
unlock:
	dump_halt_cpus();
	raw_spin_unlock_irqrestore(&halt_lock, flags);

	return ret;
}

int pause_cpus(struct cpumask *cpus, enum pause_reason reason)
{
	if (uni_sched_disabled)
		return -EAGAIN;
	return core_halt_cpus(cpus, reason);
}
EXPORT_SYMBOL(pause_cpus);

/* cpus will be modified */
int core_start_cpus(struct cpumask *cpus, enum pause_reason reason)
{
	int ret = 0;
	cpumask_t requested_cpus;
	unsigned long flags;

	raw_spin_lock_irqsave(&halt_lock, flags);
	check_halt_reason(cpus, reason);
	if (cpumask_empty(cpus))
		goto unlock;

	cpumask_copy(&requested_cpus, cpus);
	update_reasons(&requested_cpus, false, reason);

	/* remove cpus that should still be halted */
	update_halt_cpus(cpus);

	ret = start_cpus(cpus);

	if (ret < 0) {
		pr_debug("halt_cpus failure ret=%d cpus=%*pbl\n", ret,
			 cpumask_pr_args(&requested_cpus));
		/* restore/increment ref counts in case of error */
		update_reasons(&requested_cpus, true, reason);
	}

unlock:
	raw_spin_unlock_irqrestore(&halt_lock, flags);

	return ret;
}

int resume_cpus(struct cpumask *cpus, enum pause_reason reason)
{
	if (uni_sched_disabled)
		return -EAGAIN;
	return core_start_cpus(cpus, reason);
}
EXPORT_SYMBOL(resume_cpus);

int is_cpu_paused(int cpu)
{
	return (cpu < nr_cpu_ids) && cpumask_test_cpu((cpu), cpu_halt_mask);
}
EXPORT_SYMBOL(is_cpu_paused);

static void android_rvh_get_nohz_timer_target(void *unused, int *cpu, bool *done)
{
	int i, default_cpu = -1;
	struct sched_domain *sd;
	cpumask_t unhalted;

	*done = true;

	if (housekeeping_cpu(*cpu, HK_FLAG_TIMER) && !cpu_halted(*cpu)) {
		if (!available_idle_cpu(*cpu))
			return;
		default_cpu = *cpu;
	}

	rcu_read_lock();
	for_each_domain(*cpu, sd) {
		for_each_cpu_and(i, sched_domain_span(sd),
			housekeeping_cpumask(HK_FLAG_TIMER)) {
			if (*cpu == i)
				continue;

			if (!available_idle_cpu(i) && !cpu_halted(i)) {
				*cpu = i;
				goto unlock;
			}
		}
	}

	if (default_cpu == -1) {
		cpumask_complement(&unhalted, cpu_halt_mask);
		for_each_cpu_and(i, &unhalted,
				 housekeeping_cpumask(HK_FLAG_TIMER)) {
			if (*cpu == i)
				continue;

			if (!available_idle_cpu(i)) {
				*cpu = i;
				goto unlock;
			}
		}

		/* no active, non-halted, not-idle, choose any */
		default_cpu = cpumask_any(&unhalted);

		if (unlikely(default_cpu >= nr_cpu_ids))
			goto unlock;
	}

	*cpu = default_cpu;
unlock:
	rcu_read_unlock();
}

static void android_rvh_set_cpus_allowed_by_task(void *unused,
						    const struct cpumask *cpu_valid_mask,
						    const struct cpumask *new_mask,
						    struct task_struct *p,
						    unsigned int *dest_cpu)
{
	if (unlikely(uni_sched_disabled))
		return;

	/* allow kthreads to change affinity regardless of halt status of dest_cpu */
	if (p->flags & PF_KTHREAD)
		return;

	if (cpu_halted(*dest_cpu) && !p->migration_disabled) {
		cpumask_t allowed_cpus;

		/* remove halted cpus from the valid mask, and store locally */
		cpumask_andnot(&allowed_cpus, cpu_valid_mask, cpu_halt_mask);
		cpumask_and(&allowed_cpus, &allowed_cpus, new_mask);

		/* do not modify dest_cpu if there are no cpus to choose from */
		if (!cpumask_empty(&allowed_cpus))
			*dest_cpu = cpumask_any_and_distribute(&allowed_cpus, new_mask);
	}
}

/**
 * android_rvh_is_cpu_allowed: disallow cpus that are halted
 *
 * Caveat: For 32 bit tasks that are being directed to a halted cpu, allow the halted cpu
 *         in a particular case (32 bit task, in execve, moving to a 32 bit cpu)
 *         This is to handle the call to is_cpu_allowed() from __migrate_task, in the
 *         event that a 32bit task is being execve'd.
 */
static void android_rvh_is_cpu_allowed(void *unused, struct task_struct *p, int cpu, bool *allowed)
{
	if (unlikely(uni_sched_disabled))
		return;

	if (cpumask_test_cpu(cpu, cpu_halt_mask)) {
		cpumask_t cpus_allowed;

		/* default reject for any halted cpu */
		*allowed = false;

		/*
		 * for cfs threads, active cpus in the affinity are allowed
		 * but halted cpus are not allowed
		 */
		cpumask_and(&cpus_allowed, cpu_active_mask, p->cpus_ptr);
		cpumask_andnot(&cpus_allowed, &cpus_allowed, cpu_halt_mask);

		if (!(p->flags & PF_KTHREAD)) {
			if (cpumask_empty(&cpus_allowed)) {
				/*
				 * All affined cpus are inactive or halted.
				 * Allow this cpu for user threads
				 */
				*allowed = true;
			}
			return;
		}

		if (p->flags & PF_KTHREAD) {
			if (cpumask_empty(&cpus_allowed)) {
				/*
				 * All affined cpus inactive or halted or dying.
				 * Allow this cpu for kthreads
				 */
				*allowed = true;
			}
		}
	}
}

static void android_rvh_update_cpus_allowed(void *unused, struct task_struct *p,
						cpumask_var_t cpus_requested,
						const struct cpumask *new_mask, int *ret)
{
	struct uni_task_struct *wts = (struct uni_task_struct *) p->android_vendor_data1;

	if (unlikely(uni_sched_disabled))
		return;
	if (cpumask_subset(&wts->cpus_requested, cpus_requested))
		*ret = set_cpus_allowed_ptr(p, &wts->cpus_requested);
}

static void android_rvh_sched_getaffinity(void *unused, struct task_struct *p,
					  struct cpumask *in_mask)
{
	if (unlikely(uni_sched_disabled))
		return;

	if (!(p->flags & PF_KTHREAD))
		cpumask_andnot(in_mask, in_mask, cpu_halt_mask);
}

static void android_rvh_sched_setaffinity(void *unused, struct task_struct *p,
					  const struct cpumask *in_mask,
					  int *retval)
{
	struct uni_task_struct *wts = (struct uni_task_struct *) p->android_vendor_data1;

	if (unlikely(uni_sched_disabled))
		return;

	/* nothing to do if the affinity call failed */
	if (*retval)
		return;

	/*
	 * cache the affinity for user space tasks so that they
	 * can be restored during cpuset cgroup change.
	 */
	if (!(p->flags & PF_KTHREAD))
		cpumask_and(&wts->cpus_requested, in_mask, cpu_possible_mask);
}

void core_pause_init(void)
{
	struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };

	walt_drain_thread = kthread_run(try_drain_rqs, &drain_data, "halt_drain_rqs");
	if (IS_ERR(walt_drain_thread)) {
		pr_err("Error creating walt drain thread\n");
		return;
	}

	sched_setscheduler_nocheck(walt_drain_thread, SCHED_FIFO, &param);
	cpumask_clear(cpu_halt_mask);

	register_trace_android_rvh_get_nohz_timer_target(android_rvh_get_nohz_timer_target, NULL);
	register_trace_android_rvh_set_cpus_allowed_by_task(
						android_rvh_set_cpus_allowed_by_task, NULL);
	register_trace_android_rvh_is_cpu_allowed(android_rvh_is_cpu_allowed, NULL);
	register_trace_android_rvh_update_cpus_allowed(android_rvh_update_cpus_allowed, NULL);
	register_trace_android_rvh_sched_setaffinity(android_rvh_sched_setaffinity, NULL);
	register_trace_android_rvh_sched_getaffinity(android_rvh_sched_getaffinity, NULL);
}
