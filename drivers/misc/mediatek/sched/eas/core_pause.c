// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/seq_file.h>
#include <linux/energy_model.h>
#include <linux/topology.h>
#include <trace/hooks/topology.h>
#include <trace/events/sched.h>
#include <trace/hooks/sched.h>
#include <linux/cpu.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <sched/sched.h>
#include "sched_sys_common.h"
#include <linux/types.h>
#include "eas_plus.h"
#include "eas_trace.h"

#ifdef CONFIG_HOTPLUG_CPU

static DEFINE_RAW_SPINLOCK(sched_pause_lock);
static DEFINE_RAW_SPINLOCK(drain_pending_lock);

struct pause_thread_data {
	cpumask_t cpus_to_drain;
};

struct pause_thread_data drain_data = {
	.cpus_to_drain = { CPU_BITS_NONE }
};

struct task_struct *pause_drain_thread;

void detach_one_task_clone(struct task_struct *p, struct rq *rq, struct list_head *tasks)
{
	lockdep_assert_rq_held(rq);

	deactivate_task(rq, p, DEQUEUE_NOCLOCK);
	list_add(&p->se.group_node, tasks);
}

void attach_tasks_clone(struct list_head *tasks, struct rq *rq)
{
	struct task_struct *p;

	lockdep_assert_rq_held(rq);

	while (!list_empty(tasks)) {
		p = list_first_entry(tasks, struct task_struct, se.group_node);
		list_del_init(&p->se.group_node);

		WARN_ON(task_rq(p) != rq);
		activate_task(rq, p, 0);
	}
}

static void do_balance_callbacks(struct rq *rq, struct callback_head *head)
{
	void (*func)(struct rq *rq);
	struct callback_head *next;

	lockdep_assert_rq_held(rq);

	while (head) {
		func = (void (*)(struct rq *))head->func;
		next = head->next;
		head->next = NULL;
		head = next;

		func(rq);
	}
}

static inline struct callback_head *
__splice_balance_callbacks(struct rq *rq, bool split)
{
	struct callback_head *head = rq->balance_callback;

	if (likely(!head))
		return NULL;

	lockdep_assert_rq_held(rq);
	/*
	 * Must not take balance_push_callback off the list when
	 * splice_balance_callbacks() and balance_callbacks() are not
	 * in the same rq->lock section.
	 *
	 * In that case it would be possible for __schedule() to interleave
	 * and observe the list empty.
	 */
	if (split && head == &balance_push_callback)
		head = NULL;
	else
		rq->balance_callback = NULL;

	return head;
}

static inline struct callback_head *splice_balance_callbacks(struct rq *rq)
{
	return __splice_balance_callbacks(rq, true);
}

static void __balance_callbacks(struct rq *rq)
{
	do_balance_callbacks(rq, __splice_balance_callbacks(rq, false));
}

static void migrate_tasks(struct rq *dead_rq, struct rq_flags *rf)
{
	struct rq *rq = dead_rq;
	struct task_struct *next, *stop = rq->stop;
	LIST_HEAD(percpu_kthreads);
	unsigned int num_pinned_kthreads = 0;
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
		__balance_callbacks(rq); /* release rq->balance_callback */

		/*
		 * Argh ... no iterator for tasks, we need to remove the
		 * kthread from the run-queue to continue.
		 */

		if (is_per_cpu_kthread(next)) {
			detach_one_task_clone(next, rq, &percpu_kthreads);
			num_pinned_kthreads += 1;
			continue;
		}

		/*
		 * Rules for changing task_struct::cpus_mask are holding
		 * both pi_lock and rq->lock, such that holding either
		 * stabilizes the mask.
		 *
		 * Drop rq->lock is not quite as disastrous as it usually is
		 * because !cpu_active at this point, which means load-balance
		 * will not interfere. Also, stop-machine.
		 */
		rq_unlock(rq, rf);
		raw_spin_lock(&next->pi_lock);
		rq_relock(rq, rf);

		/*
		 * Since we're inside stop-machine, _nothing_ should have
		 * changed the task, WARN if weird stuff happened, because in
		 * that case the above rq->lock drop is a fail too.
		 */
		if (task_rq(next) != rq || !task_on_rq_queued(next)) {
			raw_spin_unlock(&next->pi_lock);
			continue;
		}

		/* Find suitable destination for @next, with force if needed. */
		dest_cpu = select_fallback_rq(dead_rq->cpu, next);

		if (cpu_of(rq) != dest_cpu && !is_migration_disabled(next)) {
			rq = __migrate_task(rq, rf, next, dest_cpu);
			if (rq != dead_rq) {
				rq_unlock(rq, rf);
				rq = dead_rq;
				*rf = orf;
				rq_relock(rq, rf);
			}
		} else {
			detach_one_task_clone(next, rq, &percpu_kthreads);
			num_pinned_kthreads += 1;
		}

		raw_spin_unlock(&next->pi_lock);
	}

	if (num_pinned_kthreads > 0)
		attach_tasks_clone(&percpu_kthreads, rq);

	rq->stop = stop;
}

int drain_rq_cpu_stop(void *data)
{
	struct rq *rq = this_rq();
	struct rq_flags rf;

	rq_lock_irqsave(rq, &rf);
	migrate_tasks(rq, &rf);
	rq_unlock_irqrestore(rq, &rf);

	return 0;
}

int cpu_drain_rq(unsigned int cpu)
{
	if (!cpu_online(cpu) || !cpu_active(cpu))
		return 0;

	if (available_idle_cpu(cpu))
		return 0;

	return stop_one_cpu(cpu, drain_rq_cpu_stop, NULL);
}

int __ref try_drain_rqs(void *data)
{
	cpumask_t *cpus_ptr = &((struct pause_thread_data *)data)->cpus_to_drain;
	int cpu;
	unsigned long flags;

	while (!kthread_should_stop()) {
		raw_spin_lock_irqsave(&drain_pending_lock, flags);
		if (cpumask_weight(cpus_ptr)) {
			cpumask_t local_cpus;

			cpumask_copy(&local_cpus, cpus_ptr);
			raw_spin_unlock_irqrestore(&drain_pending_lock, flags);

			for_each_cpu(cpu, &local_cpus)
				cpu_drain_rq(cpu);

			raw_spin_lock_irqsave(&drain_pending_lock, flags);
			cpumask_andnot(cpus_ptr, cpus_ptr, &local_cpus);
		}
		raw_spin_unlock_irqrestore(&drain_pending_lock, flags);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
		set_current_state(TASK_RUNNING);
	}

	return 0;
}

int pause_cpus(struct cpumask *cpus)
{
	int err = 0;
	int cpu;
	cpumask_t requested_cpus;
	u64 start_time = sched_clock();
	unsigned long flags;
	cpumask_t unpaused;

	raw_spin_lock_irqsave(&sched_pause_lock, flags);
	cpumask_copy(&requested_cpus, cpus);

	cpumask_andnot(cpus, cpus, cpu_pause_mask);

	/* No cpu need to pause */
	if (cpumask_empty(cpus)) {
		err = 1;
		goto unlock;
	}

	for_each_cpu(cpu, cpus) {
		/* Update cpu pause mask */
		cpumask_set_cpu(cpu, cpu_pause_mask);
	}

	raw_spin_lock_irqsave(&drain_pending_lock, flags);
	cpumask_or(&drain_data.cpus_to_drain, &drain_data.cpus_to_drain, cpus);
	raw_spin_unlock_irqrestore(&drain_pending_lock, flags);

	if (!IS_ERR(pause_drain_thread))
		wake_up_process(pause_drain_thread);

#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
	cpumask_andnot(&unpaused, cpu_possible_mask, cpu_pause_mask);
	set_cpu_active_bitmask(cpumask_bits(&unpaused)[0]);
#endif

unlock:
	raw_spin_unlock_irqrestore(&sched_pause_lock, flags);
	trace_sched_pause_cpus(&requested_cpus, cpus, start_time, 1, err, cpu_pause_mask);

	return err;
}

int resume_cpus(struct cpumask *cpus)
{
	int err = 0;
	int cpu;
	cpumask_t requested_cpus;
	unsigned long flags;
	u64 start_time = sched_clock();
	cpumask_t unpaused;

	raw_spin_lock_irqsave(&sched_pause_lock, flags);
	cpumask_copy(&requested_cpus, cpus);

	cpumask_and(cpus, cpus, cpu_pause_mask);

	/* No cpu need to resume */
	if (cpumask_empty(cpus)) {
		err = 1;
		goto unlock;
	}

	for_each_cpu(cpu, cpus) {
		/* Update cpu pause mask */
		cpumask_clear_cpu(cpu, cpu_pause_mask);
	}

#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
	cpumask_andnot(&unpaused, cpu_possible_mask, cpu_pause_mask);
	set_cpu_active_bitmask(cpumask_bits(&unpaused)[0]);
#endif

unlock:
	raw_spin_unlock_irqrestore(&sched_pause_lock, flags);
	trace_sched_pause_cpus(&requested_cpus, cpus, start_time, 0, err, cpu_pause_mask);

	return err;
}
EXPORT_SYMBOL(resume_cpus);

int sched_pause_cpu(int cpu)
{
	int err = 0;
	struct cpumask cpu_pause_req;
	struct device *cpu_dev;
	cpumask_t avail_cpus;

	if ((cpu >= nr_cpu_ids) || (cpu < 0)) {
		err = -EINVAL;
		return err;
	}

	if (!cpu_online(cpu) || !cpu_active(cpu)) {
		err = -EBUSY;
		return err;
	}

	cpu_dev = get_cpu_device(cpu);
	if (!cpu_dev) {
		err = -ENODEV;
		return err;
	}

	/* Prevent to pause the last 32 bit CPU */
	if (cpu_dev->offline_disabled == true) {
		err = -EBUSY;
		return err;
	}

	cpumask_andnot(&avail_cpus, cpu_active_mask, cpu_pause_mask);

	/* We cannot pause ALL cpus in the system */
	if (cpumask_weight(&avail_cpus) == 1) {
		err = -EBUSY;
		return err;
	}

	cpumask_clear(&cpu_pause_req);
	cpumask_set_cpu(cpu, &cpu_pause_req);

	err = pause_cpus(&cpu_pause_req);
	if (err) {
		pr_info("[Core Pause]Already Pause: cpu=%d, req=0x%lx, pause=0x%lx, online=0x%lx, act=0x%lx\n",
			cpu, cpu_pause_req.bits[0], cpu_pause_mask->bits[0],
			cpu_online_mask->bits[0], cpu_active_mask->bits[0]);
	} else {
		pr_info("[Core Pause]Pause success: cpu=%d, req=0x%lx, pause=0x%lx, online=0x%lx, act=0x%lx\n",
			cpu, cpu_pause_req.bits[0], cpu_pause_mask->bits[0],
			cpu_online_mask->bits[0], cpu_active_mask->bits[0]);
	}

	return err;
}
EXPORT_SYMBOL(sched_pause_cpu);

int sched_resume_cpu(int cpu)
{
	int err = 0;
	struct cpumask cpu_resume_req;

	if ((cpu >= nr_cpu_ids) || (cpu < 0)) {
		err = -EINVAL;
		return err;
	}

	if (!cpu_online(cpu) || !cpu_active(cpu)) {
		err = -EBUSY;
		return err;
	}

	cpumask_clear(&cpu_resume_req);
	cpumask_set_cpu(cpu, &cpu_resume_req);

	err = resume_cpus(&cpu_resume_req);
	if (err) {
		pr_info("[Core Pause]Already Resume: cpu=%d, req=0x%lx, pause=0x%lx, online=0x%lx, act=0x%lx\n",
			cpu, cpu_resume_req.bits[0], cpu_pause_mask->bits[0],
			cpu_online_mask->bits[0], cpu_active_mask->bits[0]);
	} else {
		pr_info("[Core Pause]Resume success: cpu=%d, req=0x%lx, pause=0x%lx, online=0x%lx, act=0x%lx\n",
			cpu, cpu_resume_req.bits[0], cpu_pause_mask->bits[0],
			cpu_online_mask->bits[0], cpu_active_mask->bits[0]);
	}

	return err;
}
EXPORT_SYMBOL(sched_resume_cpu);

void hook_rvh_is_cpus_allowed(void *unused, struct task_struct *p, int cpu, bool *allowed)
{
	if (cpu_paused(cpu)) {
		if (is_per_cpu_kthread(p))
			*allowed = true;
		else
			*allowed = false;
	}
}

void hook_rvh_set_cpus_allowed_ptr_locked(void __always_unused *data,
			const struct cpumask *cpu_valid_mask, const struct cpumask *new_mask,
			unsigned int *dest_cpu)
{
	cpumask_t avail_cpus;
	int best_cpu;

	if (cpu_paused(*dest_cpu)) {
		cpumask_andnot(&avail_cpus, cpu_valid_mask, cpu_pause_mask);

		best_cpu = cpumask_any_and_distribute(&avail_cpus, new_mask);

		if (best_cpu >= nr_cpu_ids) {
			/* If task only affinity at pause_cpu, ignore pause mask. */
			*dest_cpu = cpumask_any_and_distribute(cpu_valid_mask, new_mask);
		} else
			*dest_cpu = best_cpu;

	}
}

void hook_rvh_rto_next_cpu(void __always_unused *data,
			int rto_cpu, struct cpumask *rto_mask, int *cpu)
{
	cpumask_t avail_cpus;

	if (cpu_paused(*cpu)) {
		cpumask_andnot(&avail_cpus, rto_mask, cpu_pause_mask);
		*cpu = cpumask_next(rto_cpu, &avail_cpus);
	}
}

void hook_rvh_get_nohz_timer_target(void __always_unused *data,
					int *cpu, bool *done)
{
	struct root_domain *rd = cpu_rq(smp_processor_id())->rd;
	struct perf_domain *pd;
	int i, default_cpu = -1;
	const struct cpumask *hk_mask;
	cpumask_t unpaused;

	*done = true;

	if (housekeeping_cpu(*cpu, HK_FLAG_TIMER) && !cpu_paused(*cpu)) {
		/*
		 * Use available_idle_cpu() instead of idle_cpu().
		 * Need to pay attention to the difference between
		 * available_idle_cpu() and idle_cpu() when migration.
		 */
		if (!available_idle_cpu(*cpu))
			return;

		default_cpu = *cpu;
	}

	hk_mask = housekeeping_cpumask(HK_FLAG_TIMER);

	rcu_read_lock();
	pd = rcu_dereference(rd->pd);
	if (!pd)
		goto unlock;

	for (; pd; pd = pd->next) {
		if (!cpumask_test_cpu(*cpu, perf_domain_span(pd)))
			continue;

		for_each_cpu_and(i, perf_domain_span(pd), hk_mask) {
			if (*cpu == i)
				continue;

			if (!available_idle_cpu(i) && !cpu_paused(i)) {
				*cpu = i;
				goto unlock;
			}
		}
	}

	for_each_cpu_and(i, cpu_possible_mask, hk_mask) {
		if (*cpu == i)
			continue;

		if (!available_idle_cpu(i) && !cpu_paused(i)) {
			*cpu = i;
			goto unlock;
		}
	}

	if (default_cpu == -1) {
		cpumask_complement(&unpaused, cpu_pause_mask);
		for_each_cpu_and(i, &unpaused, hk_mask) {
			if (*cpu == i)
				continue;

			if (!available_idle_cpu(i)) {
				*cpu = i;
				goto unlock;
			}
		}

		/* no active, not-idle, housekpeeing CPU found. */
		default_cpu = cpumask_any(&unpaused);

		if (unlikely(default_cpu >= nr_cpu_ids))
			goto unlock;
	}

	*cpu = default_cpu;
unlock:
	rcu_read_unlock();
}

void hook_rvh_can_migrate_task(void __always_unused *data,
				struct task_struct *p, int dst_cpu,
				int *can_migrate)
{
	if (cpu_paused(dst_cpu))
		*can_migrate = false;

}

void hook_rvh_find_busiest_queue(void *data, int dst_cpu, struct sched_group *group,
			struct cpumask *env_cpus, struct rq **busiest,
			int *done)
{
	if (cpu_paused(dst_cpu))
		*done = 1;
}

void hook_rvh_find_new_ilb(void *data, struct cpumask *nohz_idle_cpus_mask, int *ilb)
{
	int cpu;
	const struct cpumask *hk_mask;

	hk_mask = housekeeping_cpumask(HK_FLAG_MISC);

	for_each_cpu_and(cpu, nohz_idle_cpus_mask, hk_mask) {

		if (cpu == smp_processor_id())
			continue;

		if (cpu_paused(cpu))
			continue;

		if (idle_cpu(cpu)) {
			*ilb = cpu;
			return;
		}
	}

	*ilb = nr_cpu_ids;
}

void sched_pause_init(void)
{
	struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };

	pause_drain_thread = kthread_run(try_drain_rqs, &drain_data, "pause_drain_rqs");
	if (IS_ERR(pause_drain_thread)) {
		pr_info("Error creating pause_drain_rqs\n");
		return;
	}

	sched_setscheduler_nocheck(pause_drain_thread, SCHED_FIFO, &param);

	register_trace_android_rvh_is_cpu_allowed(hook_rvh_is_cpus_allowed, NULL);
	register_trace_android_rvh_set_cpus_allowed_ptr_locked(
				hook_rvh_set_cpus_allowed_ptr_locked, NULL);
	register_trace_android_rvh_rto_next_cpu(
				hook_rvh_rto_next_cpu, NULL);
	register_trace_android_rvh_get_nohz_timer_target(
				hook_rvh_get_nohz_timer_target,	NULL);
	register_trace_android_rvh_can_migrate_task(hook_rvh_can_migrate_task, NULL);
	register_trace_android_rvh_find_busiest_queue(hook_rvh_find_busiest_queue, NULL);
	register_trace_android_rvh_find_new_ilb(hook_rvh_find_new_ilb, NULL);
}

static ssize_t show_sched_core_pause_info(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len += snprintf(buf+len, max_len-len,
			"cpu_pause_mask=0x%lx\n",
			__cpu_pause_mask.bits[0]);

	return len;
}

struct kobj_attribute sched_core_pause_info_attr =
__ATTR(sched_core_pause_info, 0400, show_sched_core_pause_info, NULL);

#endif /* CONFIG_HOTPLUG_CPU */
