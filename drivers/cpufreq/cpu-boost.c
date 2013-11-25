/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "cpu-boost: " fmt

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/cpufreq.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/moduleparam.h>

struct cpu_sync {
	struct task_struct *thread;
	wait_queue_head_t sync_wq;
	struct delayed_work boost_rem;
	int cpu;
	spinlock_t lock;
	bool pending;
	int src_cpu;
	unsigned int boost_min;
};

static DEFINE_PER_CPU(struct cpu_sync, sync_info);
static struct workqueue_struct *boost_rem_wq;

static unsigned int boost_ms = 50;
module_param(boost_ms, uint, 0644);

/*
 * The CPUFREQ_ADJUST notifier is used to override the current policy min to
 * make sure policy min >= boost_min. The cpufreq framework then does the job
 * of enforcing the new policy.
 */
static int boost_adjust_notify(struct notifier_block *nb, unsigned long val, void *data)
{
	struct cpufreq_policy *policy = data;
	unsigned int cpu = policy->cpu;
	struct cpu_sync *s = &per_cpu(sync_info, cpu);
	unsigned int min = s->boost_min;

	if (val != CPUFREQ_ADJUST)
		return NOTIFY_OK;

	if (min == 0)
		return NOTIFY_OK;

	pr_debug("CPU%u policy min before boost: %u kHz\n",
		 cpu, policy->min);
	pr_debug("CPU%u boost min: %u kHz\n", cpu, min);

	cpufreq_verify_within_limits(policy, min, UINT_MAX);

	pr_debug("CPU%u policy min after boost: %u kHz\n",
		 cpu, policy->min);

	return NOTIFY_OK;
}

static struct notifier_block boost_adjust_nb = {
	.notifier_call = boost_adjust_notify,
};

static void do_boost_rem(struct work_struct *work)
{
	struct cpu_sync *s = container_of(work, struct cpu_sync,
						boost_rem.work);

	pr_debug("Removing boost for CPU%d\n", s->cpu);
	s->boost_min = 0;
	/* Force policy re-evaluation to trigger adjust notifier. */
	cpufreq_update_policy(s->cpu);
}

static int boost_mig_sync_thread(void *data)
{
	int dest_cpu = (int) data;
	int src_cpu, ret;
	struct cpu_sync *s = &per_cpu(sync_info, dest_cpu);
	struct cpufreq_policy dest_policy;
	struct cpufreq_policy src_policy;
	unsigned long flags;

	while(1) {
		wait_event(s->sync_wq, s->pending || kthread_should_stop());

		if (kthread_should_stop())
			break;

		spin_lock_irqsave(&s->lock, flags);
		s->pending = false;
		src_cpu = s->src_cpu;
		spin_unlock_irqrestore(&s->lock, flags);

		ret = cpufreq_get_policy(&src_policy, src_cpu);
		if (ret)
			continue;

		ret = cpufreq_get_policy(&dest_policy, dest_cpu);
		if (ret)
			continue;

		if (dest_policy.cur >= src_policy.cur ) {
			pr_debug("No sync. CPU%d@%dKHz >= CPU%d@%dKHz\n",
				 dest_cpu, dest_policy.cur, src_cpu, src_policy.cur);
			continue;
		}

		cancel_delayed_work_sync(&s->boost_rem);
		s->boost_min = src_policy.cur;
		/* Force policy re-evaluation to trigger adjust notifier. */
		cpufreq_update_policy(dest_cpu);
		queue_delayed_work_on(s->cpu, boost_rem_wq,
			&s->boost_rem, msecs_to_jiffies(boost_ms));
	}

	return 0;
}

static int boost_migration_notify(struct notifier_block *nb,
				unsigned long dest_cpu, void *arg)
{
	unsigned long flags;
	struct cpu_sync *s = &per_cpu(sync_info, dest_cpu);

	if (!boost_ms)
		return NOTIFY_OK;

	pr_debug("Migration: CPU%d --> CPU%d\n", (int) arg, (int) dest_cpu);
	spin_lock_irqsave(&s->lock, flags);
	s->pending = true;
	s->src_cpu = (int) arg;
	spin_unlock_irqrestore(&s->lock, flags);
	wake_up(&s->sync_wq);

	return NOTIFY_OK;
}

static struct notifier_block boost_migration_nb = {
	.notifier_call = boost_migration_notify,
};

static int cpu_boost_init(void)
{
	int cpu;
	struct cpu_sync *s;

	cpufreq_register_notifier(&boost_adjust_nb, CPUFREQ_POLICY_NOTIFIER);

	boost_rem_wq = alloc_workqueue("cpuboost_rem_wq", WQ_HIGHPRI, 0);
	if (!boost_rem_wq)
		return -EFAULT;

	for_each_possible_cpu(cpu) {
		s = &per_cpu(sync_info, cpu);
		s->cpu = cpu;
		init_waitqueue_head(&s->sync_wq);
		spin_lock_init(&s->lock);
		INIT_DELAYED_WORK(&s->boost_rem, do_boost_rem);
		s->thread = kthread_run(boost_mig_sync_thread, (void *)cpu,
					"boost_sync/%d", cpu);
	}
	atomic_notifier_chain_register(&migration_notifier_head,
					&boost_migration_nb);

	return 0;
}
late_initcall(cpu_boost_init);
