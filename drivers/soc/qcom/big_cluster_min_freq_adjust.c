/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)     "big_min_freq_adjust: " fmt

#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/cpu_pm.h>
#include <linux/types.h>
#include <linux/smp.h>
#include <linux/moduleparam.h>

enum min_freq_adjust {
	ADJUST_MIN_FLOOR,	/* Set min floor to user supplied value */
	RESET_MIN_FLOOR,	/* Reset min floor cpuinfo value */
};

struct big_min_freq_adjust_data {
	struct cpumask cluster_cpumask;
	unsigned int min_freq_floor;
	struct delayed_work min_freq_work;
	unsigned long min_down_delay_jiffies;
	enum min_freq_adjust min_freq_state;
	enum min_freq_adjust min_freq_request;
	spinlock_t lock;
	bool big_min_freq_on;
	bool is_init;
};
static struct big_min_freq_adjust_data big_min_freq_adjust_data;

static void cpufreq_min_freq_work(struct work_struct *work)
{
	struct big_min_freq_adjust_data *p = &big_min_freq_adjust_data;

	spin_lock(&p->lock);
	if (p->min_freq_state == p->min_freq_request) {
		spin_unlock(&p->lock);
		return;
	}
	spin_unlock(&p->lock);
	cpufreq_update_policy(cpumask_first(&p->cluster_cpumask));
}

static int cpufreq_callback(struct notifier_block *nb, unsigned long val,
				void *data)
{
	struct big_min_freq_adjust_data *p = &big_min_freq_adjust_data;
	struct cpufreq_policy *policy = data;
	unsigned int min_freq_floor;

	if (p->big_min_freq_on == false)
		return NOTIFY_DONE;

	if (val != CPUFREQ_ADJUST)
		return NOTIFY_DONE;

	if (!cpumask_test_cpu(cpumask_first(&p->cluster_cpumask),
				policy->related_cpus))
		return NOTIFY_DONE;

	spin_lock(&p->lock);
	if (p->min_freq_request == ADJUST_MIN_FLOOR)
		min_freq_floor = p->min_freq_floor;
	else
		min_freq_floor = policy->cpuinfo.min_freq;
	cpufreq_verify_within_limits(policy, min_freq_floor,
			policy->cpuinfo.max_freq);
	p->min_freq_state = p->min_freq_request;
	spin_unlock(&p->lock);

	return NOTIFY_OK;
}

static struct notifier_block cpufreq_nb = {
	.notifier_call = cpufreq_callback
};

#define AFFINITY_LEVEL_L2 1
static int cpu_pm_callback(struct notifier_block *self,
			       unsigned long cmd, void *v)
{
	struct big_min_freq_adjust_data *p = &big_min_freq_adjust_data;
	unsigned long aff_level = (unsigned long) v;
	unsigned long delay;
	int cpu;

	if (p->big_min_freq_on == false)
		return NOTIFY_DONE;

	if (aff_level != AFFINITY_LEVEL_L2)
		return NOTIFY_DONE;

	cpu = smp_processor_id();

	if (!cpumask_test_cpu(cpu, &p->cluster_cpumask))
		return NOTIFY_DONE;

	spin_lock(&p->lock);
	switch (cmd) {
	case CPU_CLUSTER_PM_ENTER:
		p->min_freq_request = RESET_MIN_FLOOR;
		delay = p->min_down_delay_jiffies;
		break;
	case CPU_CLUSTER_PM_ENTER_FAILED:
	case CPU_CLUSTER_PM_EXIT:
		p->min_freq_request = ADJUST_MIN_FLOOR;
		/* To avoid unnecessary oscillations between exit and idle */
		delay = 1;
		break;
	default:
		spin_unlock(&p->lock);
		return NOTIFY_DONE;
	}

	cancel_delayed_work(&p->min_freq_work);

	if (p->min_freq_state != p->min_freq_request) {
		if (p->min_freq_request == ADJUST_MIN_FLOOR) {
			if (p->min_freq_floor > cpufreq_quick_get(cpu))
				delay = 0;
		}
		queue_delayed_work(system_unbound_wq, &p->min_freq_work, delay);
	}
	spin_unlock(&p->lock);

	return NOTIFY_OK;
}

static struct notifier_block cpu_pm_nb = {
	.notifier_call = cpu_pm_callback
};

static unsigned long __read_mostly big_min_down_delay_ms;
#define MIN_DOWN_DELAY_MSEC 80 /* Default big_min_down_delay in msec */
#define POLICY_MIN 1094400 /* Default min_freq_floor in KHz */

static void trigger_state_machine(void *d)
{
	struct big_min_freq_adjust_data *p = &big_min_freq_adjust_data;
	bool *update_policy = d;

	if (p->min_freq_request != ADJUST_MIN_FLOOR) {
		p->min_freq_request = ADJUST_MIN_FLOOR;
		*update_policy = true;
	}
}

static int enable_big_min_freq_adjust(void)
{
	struct big_min_freq_adjust_data *p = &big_min_freq_adjust_data;
	int ret;
	bool update_policy = false;

	if (p->big_min_freq_on == true)
		return 0;

	if (!cpumask_weight(&p->cluster_cpumask)) {
		pr_err("Cluster CPU IDs not set\n");
		return -EPERM;
	}

	INIT_DEFERRABLE_WORK(&p->min_freq_work, cpufreq_min_freq_work);

	if (!big_min_down_delay_ms) {
		big_min_down_delay_ms = MIN_DOWN_DELAY_MSEC;
		p->min_down_delay_jiffies = msecs_to_jiffies(
				big_min_down_delay_ms);
	}
	if (!p->min_freq_floor)
		p->min_freq_floor = POLICY_MIN;

	ret = cpu_pm_register_notifier(&cpu_pm_nb);
	if (ret) {
		pr_err("Failed to register for PM notification\n");
		return ret;
	}

	ret = cpufreq_register_notifier(&cpufreq_nb, CPUFREQ_POLICY_NOTIFIER);
	if (ret) {
		pr_err("Failed to register for CPUFREQ POLICY notification\n");
		cpu_pm_unregister_notifier(&cpu_pm_nb);
		return ret;
	}

	p->min_freq_state = RESET_MIN_FLOOR;
	p->min_freq_request = RESET_MIN_FLOOR;
	spin_lock_init(&p->lock);
	p->big_min_freq_on = true;

	/* If BIG cluster is active at this time and continue to be active
	 * forever, in that case min frequency of the cluster will never be
	 * set to floor value.  This is to trigger the state machine and set
	 * the min freq and  min_freq_state to appropriate values.
	 *
	 * Two possibilities here.
	 * 1) If cluster is idle before this, the wakeup is unnecessary but
	 * the state machine is set to proper state.
	 * 2) If cluster is active before this, the wakeup is necessary and
	 * the state machine is set to proper state.
	 */
	smp_call_function_any(&p->cluster_cpumask,
			trigger_state_machine, &update_policy, true);
	if (update_policy)
		cpufreq_update_policy(cpumask_first(&p->cluster_cpumask));

	pr_info("big min freq ajustment enabled\n");

	return 0;
}

static bool __read_mostly big_min_freq_adjust_enabled;

static int set_big_min_freq_adjust(const char *buf,
		const struct kernel_param *kp)
{
	int ret;

	ret = param_set_bool_enable_only(buf, kp);
	if (ret) {
		pr_err("Unable to set big_min_freq_adjust_enabled: %d\n", ret);
		return ret;
	}

	if (!big_min_freq_adjust_data.is_init)
		return ret;

	return enable_big_min_freq_adjust();
}

static const struct kernel_param_ops param_ops_big_min_freq_adjust = {
	.set = set_big_min_freq_adjust,
	.get = param_get_bool,
};
module_param_cb(min_freq_adjust, &param_ops_big_min_freq_adjust,
		&big_min_freq_adjust_enabled, 0644);

module_param_named(min_freq_floor, big_min_freq_adjust_data.min_freq_floor,
		uint, 0644);

static int set_min_down_delay_ms(const char *buf, const struct kernel_param *kp)
{
	int ret;

	ret = param_set_ulong(buf, kp);
	if (ret) {
		pr_err("Unable to set big_min_down_delay_ms: %d\n", ret);
		return ret;
	}

	big_min_freq_adjust_data.min_down_delay_jiffies = msecs_to_jiffies(
			big_min_down_delay_ms);

	return 0;
}

static const struct kernel_param_ops param_ops_big_min_down_delay_ms = {
	.set = set_min_down_delay_ms,
	.get = param_get_ulong,
};
module_param_cb(min_down_delay_ms, &param_ops_big_min_down_delay_ms,
		&big_min_down_delay_ms, 0644);

#define MAX_STR_LEN 16
static char big_min_freq_cluster[MAX_STR_LEN];
static struct kparam_string big_min_freq_cluster_kps = {
	.string = big_min_freq_cluster,
	.maxlen = MAX_STR_LEN,
};

static int set_big_min_freq_cluster(const char *buf,
		const struct kernel_param *kp)
{
	struct big_min_freq_adjust_data *p = &big_min_freq_adjust_data;
	int ret;

	if (p->big_min_freq_on == true) {
		ret = -EPERM;
		goto err;
	}

	ret = param_set_copystring(buf, kp);
	if (ret)
		goto err;

	ret = cpulist_parse(big_min_freq_cluster_kps.string,
			&p->cluster_cpumask);
	if (ret) {
		cpumask_clear(&p->cluster_cpumask);
		goto err;
	}

	return 0;
err:
	pr_err("Unable to set big_min_freq_cluster: %d\n", ret);
	return ret;
}

static const struct kernel_param_ops param_ops_big_min_freq_cluster = {
	.set = set_big_min_freq_cluster,
	.get = param_get_string,
};
module_param_cb(min_freq_cluster, &param_ops_big_min_freq_cluster,
		&big_min_freq_cluster_kps, 0644);

static int __init big_min_freq_adjust_init(void)
{
	big_min_freq_adjust_data.is_init = true;
	if (!big_min_freq_adjust_enabled)
		return 0;

	return enable_big_min_freq_adjust();
}
late_initcall(big_min_freq_adjust_init);
