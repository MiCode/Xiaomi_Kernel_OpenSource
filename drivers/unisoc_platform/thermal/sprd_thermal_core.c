// SPDX-License-Identifier: GPL-2.0-only
/*
 * SPRD thermal control
 *
 * Copyright (C) 2022 Unisoc corporation. http://www.unisoc.com
 */

#define pr_fmt(fmt) "sprd_thm_ctl: " fmt

#include <linux/cpu_cooling.h>
#include <linux/cpufreq.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/thermal.h>
#include <trace/hooks/thermal.h>
#include <trace/hooks/cpufreq.h>
#include <trace/events/power.h>
#include "../../../drivers/thermal/thermal_core.h"

#define INVALID_TRIP -1
#define MAX_CLUSTER_NUM	3
#define FTRACE_CLUS0_LIMIT_FREQ_NAME "thermal-cpufreq-0-limit"
#define FTRACE_CLUS1_LIMIT_FREQ_NAME "thermal-cpufreq-1-limit"
#define FTRACE_CLUS2_LIMIT_FREQ_NAME "thermal-cpufreq-2-limit"

static int trip_switch_on, trip_control;
static unsigned int last_target_freq[MAX_CLUSTER_NUM];
static bool need_update;
static unsigned int thm_enable = 1;
static unsigned int user_power_range, last_user_power_range;
static struct thermal_zone_device *soc_tz;

struct sprd_thermal_ctl {
	struct cpufreq_policy	*policy;
	struct list_head	node;
	int			cluster_id;
	unsigned int		user_min_freq;
	bool			flag;
};

static LIST_HEAD(thermal_policy_list);

static ssize_t show_ipa_min_freq(struct cpufreq_policy *policy, char *buf)
{
	struct sprd_thermal_ctl *thm_ctl;

	list_for_each_entry(thm_ctl, &thermal_policy_list, node) {
		if (thm_ctl->policy == policy)
			return sprintf(buf, "%u\n", thm_ctl->user_min_freq);
	}

	return -EINVAL;
}

static ssize_t store_ipa_min_freq(struct cpufreq_policy *policy,
							const char *buf, size_t count)
{
	unsigned int val = 0;
	struct sprd_thermal_ctl *thm_ctl;
	int ret;

	ret = kstrtouint(buf, 10, &val);
	if (ret)
		return -EINVAL;

	if (val > policy->cpuinfo.max_freq || val < policy->cpuinfo.min_freq)
		return -EINVAL;

	list_for_each_entry(thm_ctl, &thermal_policy_list, node) {
		if (thm_ctl->policy == policy) {
			thm_ctl->user_min_freq = val;
			return count;
		}
	}

	return -EINVAL;
}

#define ipa_freq_attr_rw(_name) \
static struct freq_attr _name = \
__ATTR(_name, 0644, show_##_name, store_##_name)

ipa_freq_attr_rw(ipa_min_freq);
static const struct attribute *ipa_freq_attrs[] = {
	&ipa_min_freq.attr,
	NULL,
};

static void unisoc_thermal_register(void *data, struct cpufreq_policy *policy)
{
	struct sprd_thermal_ctl *thm_ctl;

	if (!policy) {
		pr_err("Failed to get policy\n");
		return;
	}

	if (!policy->cdev) {
		pr_err("Failed to get cdev\n");
		return;
	}

	list_for_each_entry(thm_ctl, &thermal_policy_list, node)
		if (thm_ctl->policy == policy)
			return;

	list_for_each_entry(thm_ctl, &thermal_policy_list, node)
		if (thm_ctl->policy == NULL) {
			thm_ctl->policy = policy;
			thm_ctl->cluster_id = policy->cdev->id;
			thm_ctl->user_min_freq = policy->cpuinfo.min_freq;
			thm_ctl->flag = 1;
			pr_info("Success to get policy for cdev%d\n", policy->cdev->id);
			break;
		}
}

/* Get IPA related trips. It is trip number not trip temperature. */
static void get_ipa_trips(struct thermal_zone_device *tz)
{
	int i;
	int last_active = INVALID_TRIP;
	int last_passive = INVALID_TRIP;
	bool found_first_passive = false;

	for (i = 0; i < tz->trips; i++) {
		enum thermal_trip_type type;
		int ret;

		ret = tz->ops->get_trip_type(tz, i, &type);
		if (ret) {
			dev_warn(&tz->device, "Failed to get trip point %d type: %d\n", i, ret);
			continue;
		}

		if (type == THERMAL_TRIP_PASSIVE) {
			if (!found_first_passive) {
				trip_switch_on = i;
				found_first_passive = true;
			} else  {
				last_passive = i;
			}
		} else if (type == THERMAL_TRIP_ACTIVE) {
			last_active = i;
		} else {
			break;
		}
	}

	if (last_passive != INVALID_TRIP) {
		trip_control = last_passive;
	} else if (found_first_passive) {
		trip_control = trip_switch_on;
		trip_switch_on = INVALID_TRIP;
	} else {
		trip_switch_on = INVALID_TRIP;
		trip_control = last_active;
	}
}

/* thermal power throttle control */
static void unisoc_enable_thermal_power_throttle(void *data, bool *enable, bool *override)
{
	need_update = false;

	if (!thm_enable)
		*enable = false;

	/* we only do thermal control of high temperature. */
	if (user_power_range && soc_tz->temperature >= 0)
		*override = true;

	need_update = (!user_power_range && last_user_power_range);
	last_user_power_range = user_power_range;
}

/*
 * ensure the qos max_freq update when thm_enable turns 1 to 0
 * or user_power_range turns non-zero to 0.
 */
static void unisoc_thermal_power_throttle_update(void *data, struct thermal_zone_device *tz,
						 bool *update)
{
	*update |= need_update;
	*update |= tz->passive;
}

/* modify IPA power_range by user_power_range */
static void unisoc_thermal_power_cap(void *data, unsigned int *power_range)
{
	if (user_power_range && user_power_range < *power_range) {
		pr_debug("power_range %u, user_power_range %u\n",
			*power_range, user_power_range);
		*power_range = user_power_range;
	}
}

/* systrace for cpufreq cooling device */
static void cpufreq_cooling_systrace(int cluster_id, unsigned int freq)
{
	switch (cluster_id) {
	case 0:
		trace_clock_set_rate(FTRACE_CLUS0_LIMIT_FREQ_NAME,
			freq, smp_processor_id());
		break;

	case 1:
		trace_clock_set_rate(FTRACE_CLUS1_LIMIT_FREQ_NAME,
			freq, smp_processor_id());
		break;

	case 2:
		trace_clock_set_rate(FTRACE_CLUS2_LIMIT_FREQ_NAME,
			freq, smp_processor_id());
		break;

	default:
		break;
	}

	trace_clock_set_rate("thermal-user-power-range",
		user_power_range, smp_processor_id());
}

/* Debug info for cpufreq cooling device */
static void cpufreq_cdev_debug(struct cpufreq_policy *policy, unsigned int target_freq)
{
	int ret, control_temp, cluster_id = -1;
	struct sprd_thermal_ctl *thm_ctl;

	list_for_each_entry(thm_ctl, &thermal_policy_list, node) {
		if (thm_ctl->policy == policy) {
			cluster_id = thm_ctl->cluster_id;
			break;
		}
	}

	if (cluster_id < 0)
		return;

	ret = soc_tz->ops->get_trip_temp(soc_tz, trip_control, &control_temp);
	if (!ret && (soc_tz->temperature >= (control_temp - 5000) || user_power_range)) {
		if (target_freq != last_target_freq[cluster_id]) {
			pr_info("temp:%d clus%d target_max_freq:%u, cpu online:%d, user_power:%u\n",
				soc_tz->temperature, cluster_id, target_freq,
				cpumask_weight(cpu_online_mask), user_power_range);
			last_target_freq[cluster_id] = target_freq;
		}
	}
	cpufreq_cooling_systrace(cluster_id, target_freq);
}

/* modify thermal target frequency */
static struct cpufreq_policy *find_next_cpufreq_policy(struct cpufreq_policy *curr)
{
	struct sprd_thermal_ctl *thm_ctl;
	int next_id = -1;

	/* get next cluster id */
	list_for_each_entry(thm_ctl, &thermal_policy_list, node) {
		if (!thm_ctl->flag)
			continue;

		if (curr == thm_ctl->policy)
			next_id = thm_ctl->cluster_id + 1;
	}

	if (next_id < 0)
		return NULL;

	/* find the next active cpufreq_policy */
	list_for_each_entry(thm_ctl, &thermal_policy_list, node) {
		if (!thm_ctl->flag || policy_is_inactive(thm_ctl->policy))
			continue;

		if (thm_ctl->cluster_id == next_id)
			return thm_ctl->policy;
	}

	return NULL;
}

static unsigned int get_user_min_freq(struct cpufreq_policy *policy)
{
	struct sprd_thermal_ctl *thm_ctl;
	unsigned int var = 0;

	list_for_each_entry(thm_ctl, &thermal_policy_list, node) {
		if (!thm_ctl->flag)
			continue;

		if (policy == thm_ctl->policy) {
			var = thm_ctl->user_min_freq;
			break;
		}
	}

	if (!var)
		var = policy->cpuinfo.min_freq;

	return var;
}

static void unisoc_modify_thermal_target_freq(void *data,
		struct cpufreq_policy *policy, unsigned int *target_freq)
{
	unsigned int curr_max_freq, user_min_freq;
	struct cpufreq_policy *cpufreq_policy_find;

	curr_max_freq = cpufreq_quick_get_max(policy->cpu);
	cpufreq_policy_find = find_next_cpufreq_policy(policy);

	if (cpufreq_policy_find && *target_freq < curr_max_freq) {
		user_min_freq = get_user_min_freq(cpufreq_policy_find);
		if (cpufreq_policy_find->max != user_min_freq)
			*target_freq = curr_max_freq;
	}

	user_min_freq = get_user_min_freq(policy);
	*target_freq = clamp(*target_freq, user_min_freq, policy->cpuinfo.max_freq);

	cpufreq_cdev_debug(policy, *target_freq);
}

/* modify request frequency */
static void unisoc_modify_thermal_request_freq(void *data,
		struct cpufreq_policy *policy, unsigned long *request_freq)
{
	*request_freq = cpufreq_quick_get_max(policy->cpu);
}

/* thermal temperature debug */
static int thermal_temp_debug(struct thermal_zone_device *tz)
{
	int crit_temp = 0, warn_temp;
	int ret = -EPERM;
	int count;
	enum thermal_trip_type type;

	for (count = 0; count < tz->trips; count++) {
		ret = tz->ops->get_trip_type(tz, count, &type);
		if (!ret && type == THERMAL_TRIP_CRITICAL) {
			ret = tz->ops->get_trip_temp(tz, count,
				&crit_temp);
			warn_temp = crit_temp - 5000;
			if (!ret && tz->temperature > warn_temp)
				pr_alert_ratelimited("tz id=%d type=%s temperature reached %d\n",
					tz->id, tz->type, tz->temperature);
			break;
		}
	}

	return ret;
}

static void unisoc_get_thermal_zone_device(void *data, struct thermal_zone_device *tz)
{
	thermal_temp_debug(tz);
}

/* sysfs attribute create */
static ssize_t
thm_enable_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", thm_enable);
}

static ssize_t
thm_enable_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	u32 enable;

	if (kstrtou32(buf, 10, &enable) || enable > 1)
		return -EINVAL;

	thm_enable = enable;

	return count;
}

static ssize_t
user_power_range_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", user_power_range);
}

static ssize_t
user_power_range_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	u32 power_value;

	if (kstrtou32(buf, 10, &power_value))
		return -EINVAL;

	user_power_range = power_value;
	pr_info("Set user_power_range to %d\n", user_power_range);

	return count;
}

static DEVICE_ATTR_RW(thm_enable);
static DEVICE_ATTR_RW(user_power_range);

static int unisoc_cpufreq_cb(struct notifier_block *nb,
			      unsigned long val, void *data)
{
	struct cpufreq_policy *policy = data;
	int ret;

	if (val != CPUFREQ_CREATE_POLICY)
		return 0;

	ret = sysfs_create_files(&policy->kobj, ipa_freq_attrs);
	if (ret)
		pr_err("Failed to create ipa_min_freq\n");

	return ret;
}

static struct notifier_block cpufreq_policy_notify = {
	.notifier_call = unisoc_cpufreq_cb
};

static int sprd_thermal_ctl_init(void)
{
	int i, ret;
	struct sprd_thermal_ctl *thm_ctl, *tmp_thm_ctl;

	for (i = 0; i < MAX_CLUSTER_NUM; i++) {
		thm_ctl = kzalloc(sizeof(*thm_ctl), GFP_KERNEL);

		if (thm_ctl) {
			thm_ctl->policy = NULL;
			list_add(&thm_ctl->node, &thermal_policy_list);
		} else {
			goto fail;
		}
	}

#if IS_ENABLED(CONFIG_UNISOC_SOC_THERMAL)
	soc_tz = thermal_zone_get_zone_by_name("soc-thmzone");
#else
	soc_tz = thermal_zone_get_zone_by_name("cpu-thmzone");
#endif
	if (IS_ERR(soc_tz)) {
		pr_err("Failed to get soc-thmzone or cpu-thmzone\n");
		goto fail;
	}

	ret = device_create_file(&soc_tz->device, &dev_attr_thm_enable);
	if (ret) {
		pr_err("Failed to create thm_enable\n");
		goto fail;
	}

	ret = device_create_file(&soc_tz->device, &dev_attr_user_power_range);
	if (ret) {
		pr_err("Failed to create user_power_range\n");
		goto remove_file;
	}

	ret = cpufreq_register_notifier(&cpufreq_policy_notify, CPUFREQ_POLICY_NOTIFIER);
	if (ret) {
		pr_err("Failed to register cpufreq notifier.");
		goto remove_user_power_range;
	}

	get_ipa_trips(soc_tz);

	register_trace_android_vh_thermal_register(unisoc_thermal_register, NULL);
	register_trace_android_vh_enable_thermal_power_throttle(
					unisoc_enable_thermal_power_throttle, NULL);
	register_trace_android_vh_modify_thermal_throttle_update(
					unisoc_thermal_power_throttle_update, NULL);
	register_trace_android_vh_thermal_power_cap(unisoc_thermal_power_cap, NULL);
	register_trace_android_vh_modify_thermal_target_freq(unisoc_modify_thermal_target_freq, NULL);
	register_trace_android_vh_modify_thermal_request_freq(unisoc_modify_thermal_request_freq, NULL);
	register_trace_android_vh_get_thermal_zone_device(unisoc_get_thermal_zone_device, NULL);

	return 0;

remove_user_power_range:
	device_remove_file(&soc_tz->device, &dev_attr_user_power_range);

remove_file:
	device_remove_file(&soc_tz->device, &dev_attr_thm_enable);

fail:
	list_for_each_entry_safe(thm_ctl, tmp_thm_ctl, &thermal_policy_list, node) {
		list_del(&thm_ctl->node);
		kfree(thm_ctl);
	}

	return -EINVAL;
}

static void sprd_thermal_ctl_exit(void)
{
	struct sprd_thermal_ctl *thm_ctl, *tmp_thm_ctl;

	cpufreq_unregister_notifier(&cpufreq_policy_notify, CPUFREQ_POLICY_NOTIFIER);
	unregister_trace_android_vh_thermal_register(unisoc_thermal_register, NULL);

	list_for_each_entry_safe(thm_ctl, tmp_thm_ctl, &thermal_policy_list, node) {
		sysfs_remove_files(&thm_ctl->policy->kobj, ipa_freq_attrs);
		list_del(&thm_ctl->node);
		kfree(thm_ctl);
	}

	unregister_trace_android_vh_enable_thermal_power_throttle(
						unisoc_enable_thermal_power_throttle, NULL);
	unregister_trace_android_vh_modify_thermal_throttle_update(
						unisoc_thermal_power_throttle_update, NULL);
	unregister_trace_android_vh_thermal_power_cap(unisoc_thermal_power_cap, NULL);
	unregister_trace_android_vh_modify_thermal_target_freq(unisoc_modify_thermal_target_freq, NULL);
	unregister_trace_android_vh_modify_thermal_request_freq(unisoc_modify_thermal_request_freq, NULL);
	unregister_trace_android_vh_get_thermal_zone_device(unisoc_get_thermal_zone_device, NULL);

	device_remove_file(&soc_tz->device, &dev_attr_thm_enable);
	device_remove_file(&soc_tz->device, &dev_attr_user_power_range);

}

postcore_initcall_sync(sprd_thermal_ctl_init);
module_exit(sprd_thermal_ctl_exit);

MODULE_DESCRIPTION("for sprd thermal control");
MODULE_LICENSE("GPL");
