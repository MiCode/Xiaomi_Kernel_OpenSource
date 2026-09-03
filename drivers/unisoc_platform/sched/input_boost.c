// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024, Unisoc (shanghai) Technologies Co., Ltd
 */

#define pr_fmt(fmt) "unisoc-input-boost: " fmt

#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/pm_qos.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/time.h>
#include <trace/events/power.h>

#include "uni_sched.h"

struct input_boost_sync {
	int		cpu;
	unsigned int	input_boost_min;
};

static DEFINE_PER_CPU(struct input_boost_sync, sync_info);
static struct workqueue_struct *input_boost_wq;
static struct work_struct input_boost_work;

static struct delayed_work input_boost_resume;
static u64 last_input_time;
#define MIN_INPUT_INTERVAL (150 * USEC_PER_MSEC)

static DEFINE_PER_CPU(struct freq_qos_request *, qos_req);

static void boost_adjust_notify(struct cpufreq_policy *policy)
{
	unsigned int cpu = policy->cpu;
	struct input_boost_sync *s = &per_cpu(sync_info, cpu);
	unsigned int ib_min = s->input_boost_min;
	struct freq_qos_request *req = per_cpu(qos_req, cpu);
	int ret;

	pr_debug("CPU%u policy min before boost: %u kHz\n", cpu, policy->min);
	pr_debug("CPU%u boost min: %u kHz\n", cpu, ib_min);

	ret = freq_qos_update_request(req, ib_min);

	if (ret < 0) {
		pr_err("Failed to update CPU%u freq constraint in boost_adjust: %d\n",
			cpu, ib_min);
		return;
	}

	if (unlikely(trace_clock_set_rate_enabled())) {
		char buf[17] = {0};

		snprintf(buf, sizeof(buf), "cpu%d-input-boost", cpu);
		trace_clock_set_rate(buf, ib_min, cpu);
	}
	pr_debug("CPU%u policy min after boost: %u kHz\n", cpu, policy->min);
}

static void update_policy_online(void)
{
	unsigned int cpu;
	struct cpufreq_policy *policy;
	struct cpumask online_cpus;

	/* Re-evaluate policy to trigger adjust notifier for online CPUs */
	cpus_read_lock();
	online_cpus = *cpu_online_mask;
	for_each_cpu(cpu, &online_cpus) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy) {
			pr_err("%s: cpufreq policy not found for cpu%d\n", __func__, cpu);
			return;
		}

		cpumask_andnot(&online_cpus, &online_cpus, policy->related_cpus);
		boost_adjust_notify(policy);
		cpufreq_cpu_put(policy);
	}
	cpus_read_unlock();
}

static void do_input_boost_resume(struct work_struct *work)
{
	unsigned int i;
	struct input_boost_sync *i_sync_info;

	/* Reset the input_boost_min for all CPUs in the system */
	pr_debug("Resetting input boost min for all CPUs\n");
	for_each_possible_cpu(i) {
		i_sync_info = &per_cpu(sync_info, i);
		i_sync_info->input_boost_min = 0;
	}

	/* Update policies for all online CPUs */
	update_policy_online();
}

static void do_input_boost(struct work_struct *work)
{
	unsigned int cpu;
	struct input_boost_sync *i_sync_info;

	cancel_delayed_work_sync(&input_boost_resume);

	/* Set the input_boost_min for all CPUs in the system */
	pr_debug("Setting input boost min for all CPUs\n");
	for_each_possible_cpu(cpu) {
		i_sync_info = &per_cpu(sync_info, cpu);
		i_sync_info->input_boost_min = sysctl_input_boost_freq[cpu];
	}

	/* Update policies for all online CPUs */
	update_policy_online();

	queue_delayed_work(input_boost_wq, &input_boost_resume,
				msecs_to_jiffies(sysctl_input_boost_ms));
}

static void ib_event(struct input_handle *handle,
		unsigned int type, unsigned int code, int value)
{
	u64 now;

	if (!sysctl_input_boost_enable)
		return;

	now = ktime_to_us(ktime_get());
	if (now - last_input_time < MIN_INPUT_INTERVAL)
		return;

	if (work_pending(&input_boost_work))
		return;

	queue_work(input_boost_wq, &input_boost_work);

	last_input_time = ktime_to_us(ktime_get());
}

static int ib_connect(struct input_handler *handler,
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

static void ib_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id ib_ids[] = {
	/* screen single-touch */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			INPUT_DEVICE_ID_MATCH_KEYBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) },
		.keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
		.absbit = { BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) },
	},
	/* screen multi-touch */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			INPUT_DEVICE_ID_MATCH_KEYBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) },
		.keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
		.absbit = { BIT_MASK(ABS_MT_POSITION_X) | BIT_MASK(ABS_MT_POSITION_Y) },
	},
	{}
};

static struct input_handler ib_input_handler = {
	.event		= ib_event,
	.connect	= ib_connect,
	.disconnect	= ib_disconnect,
	.name		= "input-boost",
	.id_table	= ib_ids,
};

static void input_boost_data_release(void)
{
	int cpu;
	struct  cpufreq_policy *policy;
	struct freq_qos_request *req;

	for_each_possible_cpu(cpu) {
		int cpu_id;

		req = per_cpu(qos_req, cpu);
		if (req) {
			freq_qos_remove_request(req);
			policy = cpufreq_cpu_get(cpu);
			if (policy) {
				for_each_cpu(cpu_id, policy->related_cpus)
					per_cpu(qos_req, cpu_id) = NULL;

				cpufreq_cpu_put(policy);
			}
			kfree(req);
		}
	}
}

int input_boost_init(void)
{
	int cpu, ret;
	struct input_boost_sync *s;
	struct cpufreq_policy *policy;
	struct freq_qos_request *req;

	input_boost_wq = alloc_workqueue("input_boost_wq", WQ_HIGHPRI, 0);
	if (!input_boost_wq) {
		pr_err("sd:failed to alloc workqueue\n");
		return -EFAULT;
	}

	INIT_WORK(&input_boost_work, do_input_boost);
	INIT_DELAYED_WORK(&input_boost_resume, do_input_boost_resume);

	for_each_possible_cpu(cpu) {
		int cpu_id;

		s = &per_cpu(sync_info, cpu);
		s->cpu = cpu;

		policy = cpufreq_cpu_get(cpu);
		if (!policy) {
			pr_err("%s: cpufreq policy not found for cpu%d\n",
							__func__, cpu);
			ret = -ESRCH;
			goto err_out;
		}

		req = per_cpu(qos_req, cpu);
		if (!req) {
			req = kzalloc(sizeof(*req), GFP_KERNEL);
			ret = freq_qos_add_request(&policy->constraints, req,
					FREQ_QOS_MIN, FREQ_QOS_MIN_DEFAULT_VALUE);
			if (ret < 0) {
				pr_err("%s: Failed to add freq constraint (%d)\n",
							__func__, ret);
				cpufreq_cpu_put(policy);
				kfree(req);
				goto err_out;
			}

			for_each_cpu(cpu_id, policy->related_cpus)
				per_cpu(qos_req, cpu_id) = req;
		}

		cpufreq_cpu_put(policy);
	}

	ret = input_register_handler(&ib_input_handler);
	if (ret < 0) {
		pr_err("%s: Fail to register input (%d)\n", __func__, ret);
		goto err_out;
	}

	return 0;

err_out:
	destroy_workqueue(input_boost_wq);
	input_boost_data_release();

	return ret;
}
