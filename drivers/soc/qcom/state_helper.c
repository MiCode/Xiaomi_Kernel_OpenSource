/*
 * State Helper Driver
 *
 * Copyright (c) 2016, Pranav Vashi <neobuddy89@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/cpu.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/sched.h>
#include <linux/state_helper.h>
#include <linux/msm_thermal.h>
#include <linux/workqueue.h>

#define STATE_HELPER			"state_helper"
#define HELPER_ENABLED			1
#define DELAY_MSEC			100
#define DEFAULT_MAX_CPUS_ONLINE		NR_CPUS
#define DEFAULT_SUSP_CPUS		1
#define DEFAULT_MAX_CPUS_ECONOMIC	2
#define DEFAULT_MAX_CPUS_CRITICAL	1
#define DEFAULT_BATT_ECONOMIC		25
#define DEFAULT_BATT_CRITICAL		15
#define DEBUG_MASK			0

static struct state_helper {
	unsigned int enabled;
	unsigned int max_cpus_online;
	unsigned int max_cpus_susp;
	unsigned int max_cpus_eco;
	unsigned int max_cpus_cri;
	unsigned int batt_level_eco;
	unsigned int batt_level_cri;
	unsigned int debug;
} helper = {
	.enabled = HELPER_ENABLED,
	.max_cpus_online = DEFAULT_MAX_CPUS_ONLINE,
	.max_cpus_susp = DEFAULT_SUSP_CPUS,
	.max_cpus_eco = DEFAULT_MAX_CPUS_ECONOMIC,
	.max_cpus_cri = DEFAULT_MAX_CPUS_CRITICAL,
	.batt_level_eco = DEFAULT_BATT_ECONOMIC,
	.batt_level_cri = DEFAULT_BATT_CRITICAL,
	.debug = DEBUG_MASK
};

static struct state_info {
	unsigned int target_cpus;
	unsigned int batt_limited_cpus;
	unsigned int therm_allowed_cpus;
	unsigned int batt_level;
	long current_temp;
} info = {
	.target_cpus = NR_CPUS,
	.batt_limited_cpus = NR_CPUS,
	.therm_allowed_cpus = NR_CPUS,
	.batt_level = 100
};

static struct notifier_block notif;
static struct workqueue_struct *helper_wq;
static struct delayed_work helper_work;

#define dprintk(msg...)		\
do { 				\
	if (helper.debug)	\
		pr_info(msg);	\
} while (0)

static void target_cpus_calc(void)
{
	if (state_suspended)
		info.target_cpus = helper.max_cpus_susp;
	else
		info.target_cpus = helper.max_cpus_online;

	info.target_cpus = min(info.target_cpus,
				info.batt_limited_cpus);	
	info.target_cpus = min(info.target_cpus,
				info.therm_allowed_cpus);	
}

static void __ref state_helper_work(struct work_struct *work)
{
	int cpu;

	target_cpus_calc();

	if (info.target_cpus < num_online_cpus()) {
		for(cpu = NR_CPUS-1; cpu > 0; cpu--) {
			if (!cpu_online(cpu))
				continue;
			dprintk("%s: Switching CPU%u offline\n",
				STATE_HELPER, cpu);
			cpu_down(cpu);
			if (info.target_cpus >= num_online_cpus())
				break;
		}
	} else if (info.target_cpus > num_online_cpus()) {
		for(cpu = 1; cpu < NR_CPUS; cpu++) {
			if (cpu_online(cpu) ||
				msm_thermal_info.cpus_offlined & BIT(cpu))
				continue;
			cpu_up(cpu);
			dprintk("%s: Switching CPU%u online\n",
				STATE_HELPER, cpu);
			if (info.target_cpus <= num_online_cpus())
				break;
		}
	} else {
		dprintk("%s: Target already achieved: %u\n",
			STATE_HELPER, info.target_cpus);
		return;
	}

	if (helper.debug) {
		pr_info("%s: Battery Level: %u\n",
			STATE_HELPER, info.batt_level);
		pr_info("%s: Current Temp: %ld\n",
			STATE_HELPER, info.current_temp);
		pr_info("%s: Core Limit Temp: %u\n",
			STATE_HELPER, msm_thermal_info.core_limit_temp_degC);
		pr_info("%s: Target requested: %u\n",
			STATE_HELPER, info.target_cpus);
		for_each_possible_cpu(cpu)
			pr_info("%s: CPU%u status:%u allowed:%u\n",
				STATE_HELPER, cpu, cpu_online(cpu),
				!(msm_thermal_info.cpus_offlined & BIT(cpu)));
	}
}

static void batt_level_check(void)
{
	if (info.batt_level > helper.batt_level_eco)
		info.batt_limited_cpus = NR_CPUS;
	else if (info.batt_level > helper.batt_level_cri)
		info.batt_limited_cpus = helper.max_cpus_eco;
	else
		info.batt_limited_cpus = helper.max_cpus_cri;
}

static void thermal_check(void)
{
	int cpu, sum = 0;

	for_each_possible_cpu(cpu)
		sum += !(msm_thermal_info.cpus_offlined & BIT(cpu));

	info.therm_allowed_cpus = sum;
}

static void reschedule_nodelay(void)
{
	batt_level_check();
	thermal_check();

	cancel_delayed_work_sync(&helper_work);
	queue_delayed_work(helper_wq, &helper_work, 0);
}

void reschedule_helper(void)
{
	batt_level_check();
	thermal_check();

	if (!helper.enabled)
		return;

	cancel_delayed_work_sync(&helper_work);
	queue_delayed_work(helper_wq, &helper_work,
		msecs_to_jiffies(DELAY_MSEC));
}

void batt_level_notify(int k)
{
	if (k == info.batt_level || k < 1 || k > 100)
		return;

	/* Always stay updated. */
	info.batt_level = k;

	if (!helper.enabled)
		return;

	dprintk("%s: Received Battery Level Notification: %u\n",
			STATE_HELPER, info.batt_level);

	/* Reschedule only if required. */
	if (info.batt_level == helper.batt_level_cri || 
		info.batt_level == helper.batt_level_eco)
		reschedule_helper();
}

void thermal_notify(int cpu, int status)
{
	if (!helper.enabled)
		return;

	dprintk("%s: Received Thermal Notification for CPU%u: %u\n",
			STATE_HELPER, cpu, status);

	/* Do not reschedule; let thermal driver take care. */
	thermal_check();
}

void thermal_level_relay(long temp)
{
	info.current_temp = temp;
}

static int state_notifier_callback(struct notifier_block *this,
				unsigned long event, void *data)
{
	if (helper.enabled)
		reschedule_nodelay();

	return NOTIFY_OK;
}

static void state_helper_start(void)
{
	helper_wq =
	    alloc_workqueue("state_helper_wq", WQ_HIGHPRI | WQ_FREEZABLE, 0);
	if (!helper_wq) {
		pr_err("%s: Failed to allocate helper workqueue\n",
		       STATE_HELPER);
		goto err_out;
	}

	notif.notifier_call = state_notifier_callback;
	if (state_register_client(&notif)) {
		pr_err("%s: Failed to register State notifier callback\n",
			STATE_HELPER);
		goto err_dev;
	}

	INIT_DELAYED_WORK(&helper_work, state_helper_work);
	reschedule_helper();

	return;
err_dev:
	destroy_workqueue(helper_wq);
err_out:
	helper.enabled = 0;
	return;
}

static void __ref state_helper_stop(void)
{
	int cpu;

	state_unregister_client(&notif);
	notif.notifier_call = NULL;

	flush_workqueue(helper_wq);
	cancel_delayed_work_sync(&helper_work);

	/* Wake up all the sibling cores */
	for_each_possible_cpu(cpu)
		if (!cpu_online(cpu) &&
			!(msm_thermal_info.cpus_offlined & BIT(cpu)))
			cpu_up(cpu);
}

/************************** sysfs interface ************************/

static ssize_t show_enabled(struct kobject *kobj,
				struct kobj_attribute *attr, 
				char *buf)
{
	return sprintf(buf, "%u\n", helper.enabled);
}

static ssize_t store_enabled(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1 || val < 0 || val > 1)
		return -EINVAL;

	if (val == helper.enabled)
		return count;

	helper.enabled = val;

	if (helper.enabled)
		state_helper_start();
	else
		state_helper_stop();

	return count;
}

static ssize_t show_max_cpus_online(struct kobject *kobj,
				struct kobj_attribute *attr, 
				char *buf)
{
	return sprintf(buf, "%u\n",helper.max_cpus_online);
}

static ssize_t store_max_cpus_online(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1 || val < 1)
		return -EINVAL;

	if (val > NR_CPUS)
		val = NR_CPUS;

	helper.max_cpus_online = val;

	reschedule_helper();

	return count;
}

static ssize_t show_max_cpus_susp(struct kobject *kobj,
				struct kobj_attribute *attr, 
				char *buf)
{
	return sprintf(buf, "%u\n",helper.max_cpus_susp);
}

static ssize_t store_max_cpus_susp(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1 || val < 1)
		return -EINVAL;

	if (val > helper.max_cpus_online)
		val = helper.max_cpus_online;

	helper.max_cpus_susp = val;

	return count;
}

static ssize_t show_max_cpus_eco(struct kobject *kobj,
				struct kobj_attribute *attr, 
				char *buf)
{
	return sprintf(buf, "%u\n",helper.max_cpus_eco);
}

static ssize_t store_max_cpus_eco(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1 || val < 1)
		return -EINVAL;

	if (val > helper.max_cpus_online)
		val = helper.max_cpus_online;

	helper.max_cpus_eco = val;

	reschedule_helper();

	return count;
}

static ssize_t show_max_cpus_cri(struct kobject *kobj,
				struct kobj_attribute *attr, 
				char *buf)
{
	return sprintf(buf, "%u\n",helper.max_cpus_cri);
}

static ssize_t store_max_cpus_cri(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1 || val < 1)
		return -EINVAL;

	if (val > helper.max_cpus_eco)
		val = helper.max_cpus_eco;

	helper.max_cpus_cri = val;

	reschedule_helper();

	return count;
}

static ssize_t show_batt_level_eco(struct kobject *kobj,
				struct kobj_attribute *attr, 
				char *buf)
{
	return sprintf(buf, "%u\n",helper.batt_level_eco);
}

static ssize_t store_batt_level_eco(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1 || val < 1)
		return -EINVAL;

	if (val > 100)
		val = 100;

	helper.batt_level_eco = val;

	reschedule_helper();

	return count;
}

static ssize_t show_batt_level_cri(struct kobject *kobj,
				struct kobj_attribute *attr, 
				char *buf)
{
	return sprintf(buf, "%u\n",helper.batt_level_cri);
}

static ssize_t store_batt_level_cri(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1 || val < 1)
		return -EINVAL;

	if (val > helper.batt_level_eco)
		val = helper.batt_level_eco;

	helper.batt_level_cri = val;

	reschedule_helper();

	return count;
}

static ssize_t show_debug_mask(struct kobject *kobj,
				struct kobj_attribute *attr, 
				char *buf)
{
	return sprintf(buf, "%u\n", helper.debug);
}

static ssize_t store_debug_mask(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	ret = sscanf(buf, "%u", &val);
	if (ret != 1 || val < 0 || val > 1)
		return -EINVAL;

	if (val == helper.debug)
		return count;

	helper.debug = val;

	return count;
}

static ssize_t show_target_cpus(struct kobject *kobj,
				struct kobj_attribute *attr, 
				char *buf)
{
	if (!helper.enabled) {
		batt_level_check();
		thermal_check();
		target_cpus_calc();
	}

	return sprintf(buf, "%u\n", info.target_cpus);
}

static ssize_t show_batt_limited_cpus(struct kobject *kobj,
				struct kobj_attribute *attr, 
				char *buf)
{
	if (!helper.enabled)
		batt_level_check();

	return sprintf(buf, "%u\n", info.batt_limited_cpus);
}

static ssize_t show_therm_allowed_cpus(struct kobject *kobj,
				struct kobj_attribute *attr, 
				char *buf)
{
	if (!helper.enabled)
		thermal_check();

	return sprintf(buf, "%u\n", info.therm_allowed_cpus);
}

static ssize_t show_batt_level(struct kobject *kobj,
				struct kobj_attribute *attr, 
				char *buf)
{
	return sprintf(buf, "%u\n", info.batt_level);
}

static ssize_t show_current_temp(struct kobject *kobj,
				struct kobj_attribute *attr, 
				char *buf)
{
	return sprintf(buf, "%ld\n", info.current_temp);
}

#define KERNEL_ATTR_RW(_name) 				\
static struct kobj_attribute _name##_attr = 		\
	__ATTR(_name, 0664, show_##_name, store_##_name)

#define KERNEL_ATTR_RO(_name) 				\
static struct kobj_attribute _name##_attr = 		\
	__ATTR(_name, 0444, show_##_name, NULL)

KERNEL_ATTR_RW(enabled);
KERNEL_ATTR_RW(max_cpus_online);
KERNEL_ATTR_RW(max_cpus_susp);
KERNEL_ATTR_RW(max_cpus_eco);
KERNEL_ATTR_RW(max_cpus_cri);
KERNEL_ATTR_RW(batt_level_eco);
KERNEL_ATTR_RW(batt_level_cri);
KERNEL_ATTR_RW(debug_mask);
KERNEL_ATTR_RO(target_cpus);
KERNEL_ATTR_RO(batt_limited_cpus);
KERNEL_ATTR_RO(therm_allowed_cpus);
KERNEL_ATTR_RO(batt_level);
KERNEL_ATTR_RO(current_temp);

static struct attribute *state_helper_attrs[] = {
	&enabled_attr.attr,
	&max_cpus_online_attr.attr,
	&max_cpus_susp_attr.attr,
	&max_cpus_eco_attr.attr,
	&max_cpus_cri_attr.attr,
	&batt_level_eco_attr.attr,
	&batt_level_cri_attr.attr,
	&debug_mask_attr.attr,
	&target_cpus_attr.attr,
	&batt_limited_cpus_attr.attr,
	&therm_allowed_cpus_attr.attr,
	&batt_level_attr.attr,
	&current_temp_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = state_helper_attrs,
	.name = STATE_HELPER,
};

/************************** sysfs end ************************/

static int state_helper_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = sysfs_create_group(kernel_kobj, &attr_group);

	if (helper.enabled)
		state_helper_start();

	return ret;
}

static struct platform_device state_helper_device = {
	.name = STATE_HELPER,
	.id = -1,
};

static int state_helper_remove(struct platform_device *pdev)
{
	if (helper.enabled)
		state_helper_stop();

	return 0;
}

static struct platform_driver state_helper_driver = {
	.probe = state_helper_probe,
	.remove = state_helper_remove,
	.driver = {
		.name = STATE_HELPER,
		.owner = THIS_MODULE,
	},
};

static int __init state_helper_init(void)
{
	int ret;

	ret = platform_driver_register(&state_helper_driver);
	if (ret) {
		pr_err("%s: Driver register failed: %d\n", STATE_HELPER, ret);
		return ret;
	}

	ret = platform_device_register(&state_helper_device);
	if (ret) {
		pr_err("%s: Device register failed: %d\n", STATE_HELPER, ret);
		return ret;
	}

	pr_info("%s: Device init\n", STATE_HELPER);

	return ret;
}

static void __exit state_helper_exit(void)
{
	platform_device_unregister(&state_helper_device);
	platform_driver_unregister(&state_helper_driver);
}

late_initcall(state_helper_init);
module_exit(state_helper_exit);

MODULE_AUTHOR("Pranav Vashi <neobuddy89@gmail.com>");
MODULE_DESCRIPTION("State Helper Driver");
MODULE_LICENSE("GPLv2");
