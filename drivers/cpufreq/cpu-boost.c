/*
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/time.h>

struct cpu_sync {
	struct task_struct *thread;
	wait_queue_head_t sync_wq;
	struct delayed_work boost_rem;
	int cpu;
	spinlock_t lock;
	bool pending;
	int src_cpu;
	unsigned int boost_min;
	unsigned int input_boost_min;
	unsigned int task_load;
	unsigned int input_boost_freq;
};

static DEFINE_PER_CPU(struct cpu_sync, sync_info);
static struct workqueue_struct *cpu_boost_wq;

static struct work_struct input_boost_work;

static unsigned int boost_ms;
module_param(boost_ms, uint, 0644);

static unsigned int sync_threshold;
module_param(sync_threshold, uint, 0644);

static bool input_boost_enabled;

static unsigned int input_boost_ms = 40;
module_param(input_boost_ms, uint, 0644);

static unsigned int migration_load_threshold = 15;
module_param(migration_load_threshold, uint, 0644);

static bool load_based_syncs;
module_param(load_based_syncs, bool, 0644);

static struct delayed_work input_boost_rem;
static u64 last_input_time;
#define MIN_INPUT_INTERVAL (150 * USEC_PER_MSEC)

static int set_input_boost_freq(const char *buf, const struct kernel_param *kp)
{
	int i, ntokens = 0;
	unsigned int val, cpu;
	const char *cp = buf;
	bool enabled = false;

	while ((cp = strpbrk(cp + 1, " :")))
		ntokens++;

	/* single number: apply to all CPUs */
	if (!ntokens) {
		if (sscanf(buf, "%u\n", &val) != 1)
			return -EINVAL;
		for_each_possible_cpu(i)
			per_cpu(sync_info, i).input_boost_freq = val;
		goto check_enable;
	}

	/* CPU:value pair */
	if (!(ntokens % 2))
		return -EINVAL;

	cp = buf;
	for (i = 0; i < ntokens; i += 2) {
		if (sscanf(cp, "%u:%u", &cpu, &val) != 2)
			return -EINVAL;
		if (cpu > num_possible_cpus())
			return -EINVAL;

		per_cpu(sync_info, cpu).input_boost_freq = val;
		cp = strchr(cp, ' ');
		cp++;
	}

check_enable:
	for_each_possible_cpu(i) {
		if (per_cpu(sync_info, i).input_boost_freq) {
			enabled = true;
			break;
		}
	}
	input_boost_enabled = enabled;

	return 0;
}

static int get_input_boost_freq(char *buf, const struct kernel_param *kp)
{
	int cnt = 0, cpu;
	struct cpu_sync *s;

	for_each_possible_cpu(cpu) {
		s = &per_cpu(sync_info, cpu);
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt,
				"%d:%u ", cpu, s->input_boost_freq);
	}
	cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, "\n");
	return cnt;
}

static const struct kernel_param_ops param_ops_input_boost_freq = {
	.set = set_input_boost_freq,
	.get = get_input_boost_freq,
};
module_param_cb(input_boost_freq, &param_ops_input_boost_freq, NULL, 0644);

/*
 * The CPUFREQ_ADJUST notifier is used to override the current policy min to
 * make sure policy min >= boost_min. The cpufreq framework then does the job
 * of enforcing the new policy.
 *
 * The sync kthread needs to run on the CPU in question to avoid deadlocks in
 * the wake up code. Achieve this by binding the thread to the respective
 * CPU. But a CPU going offline unbinds threads from that CPU. So, set it up
 * again each time the CPU comes back up. We can use CPUFREQ_START to figure
 * out a CPU is coming online instead of registering for hotplug notifiers.
 */
static int boost_adjust_notify(struct notifier_block *nb, unsigned long val,
				void *data)
{
	struct cpufreq_policy *policy = data;
	unsigned int cpu = policy->cpu;
	struct cpu_sync *s = &per_cpu(sync_info, cpu);
	unsigned int b_min = s->boost_min;
	unsigned int ib_min = s->input_boost_min;
	unsigned int min;

	switch (val) {
	case CPUFREQ_ADJUST:
		if (!b_min && !ib_min)
			break;

		min = max(b_min, ib_min);

		pr_debug("CPU%u policy min before boost: %u kHz\n",
			 cpu, policy->min);
		pr_debug("CPU%u boost min: %u kHz\n", cpu, min);

		cpufreq_verify_within_limits(policy, min, UINT_MAX);

		pr_debug("CPU%u policy min after boost: %u kHz\n",
			 cpu, policy->min);
		break;

	case CPUFREQ_START:
		set_cpus_allowed(s->thread, *cpumask_of(cpu));
		break;
	}

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

static void update_policy_online(void)
{
	unsigned int i;

	/* Re-evaluate policy to trigger adjust notifier for online CPUs */
	get_online_cpus();
	for_each_online_cpu(i) {
		pr_debug("Updating policy for CPU%d\n", i);
		cpufreq_update_policy(i);
	}
	put_online_cpus();
}

static void do_input_boost_rem(struct work_struct *work)
{
	unsigned int i;
	struct cpu_sync *i_sync_info;

	/* Reset the input_boost_min for all CPUs in the system */
	pr_debug("Resetting input boost min for all CPUs\n");
	for_each_possible_cpu(i) {
		i_sync_info = &per_cpu(sync_info, i);
		i_sync_info->input_boost_min = 0;
	}

	/* Update policies for all online CPUs */
	update_policy_online();
}

static int boost_mig_sync_thread(void *data)
{
	int dest_cpu = (long) data;
	int src_cpu, ret;
	struct cpu_sync *s = &per_cpu(sync_info, dest_cpu);
	struct cpufreq_policy dest_policy;
	struct cpufreq_policy src_policy;
	unsigned long flags;
	unsigned int req_freq;

	while (1) {
		wait_event_interruptible(s->sync_wq,
					s->pending || kthread_should_stop());

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

		req_freq = load_based_syncs ?
			(dest_policy.cpuinfo.max_freq * s->task_load) / 100 :
							src_policy.cur;

		if (req_freq <= dest_policy.cpuinfo.min_freq) {
			pr_debug("No sync. Sync Freq:%u\n", req_freq);
			continue;
		}

		if (sync_threshold)
			req_freq = min(sync_threshold, req_freq);

		cancel_delayed_work_sync(&s->boost_rem);

		s->boost_min = req_freq;

		/* Force policy re-evaluation to trigger adjust notifier. */
		get_online_cpus();
		if (cpu_online(dest_cpu)) {
			cpufreq_update_policy(dest_cpu);
			queue_delayed_work_on(dest_cpu, cpu_boost_wq,
				&s->boost_rem, msecs_to_jiffies(boost_ms));
		} else {
			s->boost_min = 0;
		}
		put_online_cpus();
	}

	return 0;
}

static int boost_migration_notify(struct notifier_block *nb,
				unsigned long unused, void *arg)
{
	struct migration_notify_data *mnd = arg;
	unsigned long flags;
	struct cpu_sync *s = &per_cpu(sync_info, mnd->dest_cpu);

	if (load_based_syncs && (mnd->load <= migration_load_threshold))
		return NOTIFY_OK;

	if (load_based_syncs && ((mnd->load < 0) || (mnd->load > 100))) {
		pr_err("cpu-boost:Invalid load: %d\n", mnd->load);
		return NOTIFY_OK;
	}

	if (!load_based_syncs && (mnd->src_cpu == mnd->dest_cpu))
		return NOTIFY_OK;

	if (!boost_ms)
		return NOTIFY_OK;

	/* Avoid deadlock in try_to_wake_up() */
	if (s->thread == current)
		return NOTIFY_OK;

	pr_debug("Migration: CPU%d --> CPU%d\n", mnd->src_cpu, mnd->dest_cpu);
	spin_lock_irqsave(&s->lock, flags);
	s->pending = true;
	s->src_cpu = mnd->src_cpu;
	s->task_load = load_based_syncs ? mnd->load : 0;
	spin_unlock_irqrestore(&s->lock, flags);
	wake_up(&s->sync_wq);

	return NOTIFY_OK;
}

static struct notifier_block boost_migration_nb = {
	.notifier_call = boost_migration_notify,
};

static void do_input_boost(struct work_struct *work)
{
	unsigned int i;
	struct cpu_sync *i_sync_info;

	cancel_delayed_work_sync(&input_boost_rem);

	/* Set the input_boost_min for all CPUs in the system */
	pr_debug("Setting input boost min for all CPUs\n");
	for_each_possible_cpu(i) {
		i_sync_info = &per_cpu(sync_info, i);
		i_sync_info->input_boost_min = i_sync_info->input_boost_freq;
	}

	/* Update policies for all online CPUs */
	update_policy_online();

	queue_delayed_work(cpu_boost_wq, &input_boost_rem,
					msecs_to_jiffies(input_boost_ms));
}

static void cpuboost_input_event(struct input_handle *handle,
		unsigned int type, unsigned int code, int value)
{
	u64 now;

	if (!input_boost_enabled)
		return;

	now = ktime_to_us(ktime_get());
	if (now - last_input_time < MIN_INPUT_INTERVAL)
		return;

	if (work_pending(&input_boost_work))
		return;

	queue_work(cpu_boost_wq, &input_boost_work);
	last_input_time = ktime_to_us(ktime_get());
}

static int cpuboost_input_connect(struct input_handler *handler,
		struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "cpufreq";

	error = input_register_handle(handle);
	if (error)
		goto err2;

	error = input_open_device(handle);
	if (error)
		goto err1;

	return 0;
err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void cpuboost_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id cpuboost_ids[] = {
	/* multi-touch touchscreen */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.absbit = { [BIT_WORD(ABS_MT_POSITION_X)] =
			BIT_MASK(ABS_MT_POSITION_X) |
			BIT_MASK(ABS_MT_POSITION_Y) },
	},
	/* touchpad */
	{
		.flags = INPUT_DEVICE_ID_MATCH_KEYBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
		.absbit = { [BIT_WORD(ABS_X)] =
			BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) },
	},
	/* Keypad */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) },
	},
	{ },
};

static struct input_handler cpuboost_input_handler = {
	.event          = cpuboost_input_event,
	.connect        = cpuboost_input_connect,
	.disconnect     = cpuboost_input_disconnect,
	.name           = "cpu-boost",
	.id_table       = cpuboost_ids,
};

static int cpu_boost_init(void)
{
	int cpu, ret;
	struct cpu_sync *s;

	cpu_boost_wq = alloc_workqueue("cpuboost_wq", WQ_HIGHPRI, 0);
	if (!cpu_boost_wq)
		return -EFAULT;

	INIT_WORK(&input_boost_work, do_input_boost);
	INIT_DELAYED_WORK(&input_boost_rem, do_input_boost_rem);

	for_each_possible_cpu(cpu) {
		s = &per_cpu(sync_info, cpu);
		s->cpu = cpu;
		init_waitqueue_head(&s->sync_wq);
		spin_lock_init(&s->lock);
		INIT_DELAYED_WORK(&s->boost_rem, do_boost_rem);
		s->thread = kthread_run(boost_mig_sync_thread,
				(void *) (long)cpu, "boost_sync/%d", cpu);
		set_cpus_allowed(s->thread, *cpumask_of(cpu));
	}
	cpufreq_register_notifier(&boost_adjust_nb, CPUFREQ_POLICY_NOTIFIER);
	atomic_notifier_chain_register(&migration_notifier_head,
					&boost_migration_nb);
	ret = input_register_handler(&cpuboost_input_handler);

	return 0;
}
late_initcall(cpu_boost_init);
