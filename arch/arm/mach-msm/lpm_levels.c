/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cpu.h>
#include <linux/of.h>
#include <mach/mpm.h>
#include "pm.h"
#include "rpm-notifier.h"
#include "spm.h"
#include "idle.h"

enum {
	MSM_LPM_LVL_DBG_SUSPEND_LIMITS = BIT(0),
	MSM_LPM_LVL_DBG_IDLE_LIMITS = BIT(1),
};

enum {
	MSM_SCM_L2_ON = 0,
	MSM_SCM_L2_OFF = 1,
	MSM_SCM_L2_GDHS = 3,
};

struct msm_rpmrs_level {
	enum msm_pm_sleep_mode sleep_mode;
	uint32_t l2_cache;
	bool available;
	uint32_t latency_us;
	uint32_t steady_state_power;
	uint32_t energy_overhead;
	uint32_t time_overhead_us;
};

struct lpm_lookup_table {
	uint32_t modes;
	const char *mode_name;
};

static void msm_lpm_level_update(void);

static int msm_lpm_cpu_callback(struct notifier_block *cpu_nb,
				unsigned long action, void *hcpu);

static struct notifier_block __refdata msm_lpm_cpu_nblk = {
	.notifier_call = msm_lpm_cpu_callback,
};

static uint32_t allowed_l2_mode;
static uint32_t sysfs_dbg_l2_mode = MSM_SPM_L2_MODE_POWER_COLLAPSE;
static uint32_t default_l2_mode;

static bool no_l2_saw;

static ssize_t msm_lpm_levels_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t msm_lpm_levels_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count);

#define ADJUST_LATENCY(x)	\
	((x == MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE) ?\
		(num_online_cpus()) / 2 : 0)

static int msm_lpm_lvl_dbg_msk;

module_param_named(
	debug_mask, msm_lpm_lvl_dbg_msk, int, S_IRUGO | S_IWUSR | S_IWGRP
);

static struct msm_rpmrs_level *msm_lpm_levels;
static int msm_lpm_level_count;

static struct kobj_attribute lpm_l2_kattr = __ATTR(l2,  S_IRUGO|S_IWUSR,\
		msm_lpm_levels_attr_show, msm_lpm_levels_attr_store);

static struct attribute *lpm_levels_attr[] = {
	&lpm_l2_kattr.attr,
	NULL,
};

static struct attribute_group lpm_levels_attr_grp = {
	.attrs = lpm_levels_attr,
};

/* SYSFS */
static ssize_t msm_lpm_levels_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct kernel_param kp;
	int rc;

	kp.arg = &sysfs_dbg_l2_mode;

	rc = param_get_uint(buf, &kp);

	if (rc > 0) {
		strlcat(buf, "\n", PAGE_SIZE);
		rc++;
	}

	return rc;
}

static ssize_t msm_lpm_levels_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct kernel_param kp;
	unsigned int temp;
	int rc;

	kp.arg = &temp;
	rc = param_set_uint(buf, &kp);
	if (rc)
		return rc;

	sysfs_dbg_l2_mode = temp;
	msm_lpm_level_update();

	return count;
}

static int msm_pm_get_sleep_mode_value(struct device_node *node,
			const char *key, uint32_t *sleep_mode_val)
{
	int i;
	struct lpm_lookup_table {
		uint32_t modes;
		const char *mode_name;
	};
	struct lpm_lookup_table pm_sm_lookup[] = {
		{MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT,
			"wfi"},
		{MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT,
			"ramp_down_and_wfi"},
		{MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE,
			"standalone_pc"},
		{MSM_PM_SLEEP_MODE_POWER_COLLAPSE,
			"pc"},
		{MSM_PM_SLEEP_MODE_RETENTION,
			"retention"},
		{MSM_PM_SLEEP_MODE_POWER_COLLAPSE_SUSPEND,
			"pc_suspend"},
		{MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN,
			"pc_no_xo_shutdown"}
	};
	int ret;
	const char *mode_name;

	ret = of_property_read_string(node, key, &mode_name);
	if (!ret) {
		ret = -EINVAL;
		for (i = 0; i < ARRAY_SIZE(pm_sm_lookup); i++) {
			if (!strcmp(mode_name, pm_sm_lookup[i].mode_name)) {
				*sleep_mode_val = pm_sm_lookup[i].modes;
				ret = 0;
				break;
			}
		}
	}
	return ret;
}

static int msm_lpm_set_l2_mode(int sleep_mode)
{
	int lpm = sleep_mode;
	int rc = 0;

	if (no_l2_saw)
		goto bail_set_l2_mode;

	msm_pm_set_l2_flush_flag(MSM_SCM_L2_ON);

	switch (sleep_mode) {
	case MSM_SPM_L2_MODE_POWER_COLLAPSE:
		msm_pm_set_l2_flush_flag(MSM_SCM_L2_OFF);
		break;
	case MSM_SPM_L2_MODE_GDHS:
		msm_pm_set_l2_flush_flag(MSM_SCM_L2_GDHS);
		break;
	case MSM_SPM_L2_MODE_PC_NO_RPM:
		msm_pm_set_l2_flush_flag(MSM_SCM_L2_OFF);
		break;
	case MSM_SPM_L2_MODE_RETENTION:
	case MSM_SPM_L2_MODE_DISABLED:
		break;
	default:
		lpm = MSM_SPM_L2_MODE_DISABLED;
		break;
	}

	rc = msm_spm_l2_set_low_power_mode(lpm, true);

	if (rc) {
		if (rc == -ENXIO)
			WARN_ON_ONCE(1);
		else
			pr_err("%s: Failed to set L2 low power mode %d, ERR %d",
			__func__, lpm, rc);
	}

bail_set_l2_mode:
	return rc;
}

static void msm_lpm_level_update(void)
{
	int lpm_level;
	struct msm_rpmrs_level *level = NULL;
	uint32_t max_l2_mode;
	static DEFINE_MUTEX(lpm_lock);

	mutex_lock(&lpm_lock);

	max_l2_mode = min(allowed_l2_mode, sysfs_dbg_l2_mode);

	for (lpm_level = 0; lpm_level < msm_lpm_level_count; lpm_level++) {
		level = &msm_lpm_levels[lpm_level];
		level->available = !(level->l2_cache > max_l2_mode);
	}
	mutex_unlock(&lpm_lock);
}

int msm_lpm_enter_sleep(uint32_t sclk_count, void *limits,
		bool from_idle, bool notify_rpm)
{
	int ret = 0;
	int debug_mask;
	uint32_t l2 = *(uint32_t *)limits;

	if (from_idle)
		debug_mask = msm_lpm_lvl_dbg_msk &
			MSM_LPM_LVL_DBG_IDLE_LIMITS;
	else
		debug_mask = msm_lpm_lvl_dbg_msk &
			MSM_LPM_LVL_DBG_SUSPEND_LIMITS;

	if (debug_mask)
		pr_info("%s(): l2:%d", __func__, l2);

	ret = msm_lpm_set_l2_mode(l2);

	if (ret) {
		if (ret == -ENXIO)
			ret = 0;
		else {
			pr_warn("%s(): Failed to set L2 SPM Mode %d",
					__func__, l2);
			goto bail;
		}
	}

	if (notify_rpm) {
		ret = msm_rpm_enter_sleep(debug_mask);
		if (ret) {
			pr_warn("%s(): RPM failed to enter sleep err:%d\n",
					__func__, ret);
			goto bail;
		}

		msm_mpm_enter_sleep(sclk_count, from_idle);
	}
bail:
	return ret;
}

static void msm_lpm_exit_sleep(void *limits, bool from_idle,
		bool notify_rpm, bool collapsed)
{

	msm_lpm_set_l2_mode(default_l2_mode);

	if (notify_rpm) {
		msm_mpm_exit_sleep(from_idle);
		msm_rpm_exit_sleep();
	}
}

void msm_lpm_show_resources(void)
{
	/* TODO */
	return;
}

s32 msm_cpuidle_get_deep_idle_latency(void)
{
	int i;
	struct msm_rpmrs_level *level = msm_lpm_levels, *best = level;

	if (!level)
		return 0;

	for (i = 0; i < msm_lpm_level_count; i++, level++) {
		if (!level->available)
			continue;
		if (level->sleep_mode != MSM_PM_SLEEP_MODE_POWER_COLLAPSE)
			continue;
		/* Pick the first power collapse mode by default */
		if (best->sleep_mode != MSM_PM_SLEEP_MODE_POWER_COLLAPSE)
			best = level;
		/* Find the lowest latency for power collapse */
		if (level->latency_us < best->latency_us)
			best = level;
	}
	return best->latency_us - 1;
}

static int msm_lpm_cpu_callback(struct notifier_block *cpu_nb,
	unsigned long action, void *hcpu)
{
	switch (action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
		allowed_l2_mode = default_l2_mode;
		msm_lpm_level_update();
		break;
	case CPU_DEAD_FROZEN:
	case CPU_DEAD:
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
		if (num_online_cpus() == 1)
			allowed_l2_mode = MSM_SPM_L2_MODE_POWER_COLLAPSE;
		msm_lpm_level_update();
		break;
	}
	return NOTIFY_OK;
}

static void *msm_lpm_lowest_limits(bool from_idle,
		enum msm_pm_sleep_mode sleep_mode,
		struct msm_pm_time_params *time_param, uint32_t *power)
{
	unsigned int cpu = smp_processor_id();
	struct msm_rpmrs_level *best_level = NULL;
	uint32_t best_level_pwr = 0;
	uint32_t pwr;
	int i;
	bool modify_event_timer;
	uint32_t next_wakeup_us = time_param->sleep_us;
	uint32_t lvl_latency_us = 0;
	uint32_t lvl_overhead_us = 0;
	uint32_t lvl_overhead_energy = 0;

	if (!msm_lpm_levels)
		return NULL;

	for (i = 0; i < msm_lpm_level_count; i++) {
		struct msm_rpmrs_level *level = &msm_lpm_levels[i];

		modify_event_timer = false;

		if (!level->available)
			continue;

		if (sleep_mode != level->sleep_mode)
			continue;

		lvl_latency_us =
			level->latency_us + (level->latency_us *
						ADJUST_LATENCY(sleep_mode));

		lvl_overhead_us =
			level->time_overhead_us + (level->time_overhead_us *
						ADJUST_LATENCY(sleep_mode));

		lvl_overhead_energy =
			level->energy_overhead + level->energy_overhead *
						ADJUST_LATENCY(sleep_mode);

		if (time_param->latency_us < lvl_latency_us)
			continue;

		if (time_param->next_event_us &&
			time_param->next_event_us < lvl_latency_us)
			continue;

		if (time_param->next_event_us) {
			if ((time_param->next_event_us < time_param->sleep_us)
			|| ((time_param->next_event_us - lvl_latency_us) <
				time_param->sleep_us)) {
				modify_event_timer = true;
				next_wakeup_us = time_param->next_event_us -
						lvl_latency_us;
			}
		}

		if (next_wakeup_us <= lvl_overhead_us)
			continue;

		if ((MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE == sleep_mode)
			|| (MSM_PM_SLEEP_MODE_POWER_COLLAPSE == sleep_mode))
			if (!cpu && msm_rpm_waiting_for_ack())
					break;

		if (next_wakeup_us <= 1) {
			pwr = lvl_overhead_energy;
		} else if (next_wakeup_us <= lvl_overhead_us) {
			pwr = lvl_overhead_energy / next_wakeup_us;
		} else if ((next_wakeup_us >> 10)
				> lvl_overhead_us) {
			pwr = level->steady_state_power;
		} else {
			pwr = level->steady_state_power;
			pwr -= (lvl_overhead_us *
				level->steady_state_power) /
						next_wakeup_us;
			pwr += lvl_overhead_energy / next_wakeup_us;
		}

		if (!best_level || (best_level_pwr >= pwr)) {
			best_level = level;
			best_level_pwr = pwr;
			if (power)
				*power = pwr;
			if (modify_event_timer &&
				(sleep_mode !=
					MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT))
				time_param->modified_time_us =
					time_param->next_event_us -
						lvl_latency_us;
			else
				time_param->modified_time_us = 0;
		}
	}

	return best_level ? &best_level->l2_cache : NULL;
}

static struct msm_pm_sleep_ops msm_lpm_ops = {
	.lowest_limits = msm_lpm_lowest_limits,
	.enter_sleep = msm_lpm_enter_sleep,
	.exit_sleep = msm_lpm_exit_sleep,
};

static int msm_lpm_get_l2_cache_value(struct device_node *node,
			char *key, uint32_t *l2_val)
{
	int i;
	struct lpm_lookup_table l2_mode_lookup[] = {
		{MSM_SPM_L2_MODE_POWER_COLLAPSE, "l2_cache_pc"},
		{MSM_SPM_L2_MODE_PC_NO_RPM, "l2_cache_pc_no_rpm"},
		{MSM_SPM_L2_MODE_GDHS, "l2_cache_gdhs"},
		{MSM_SPM_L2_MODE_RETENTION, "l2_cache_retention"},
		{MSM_SPM_L2_MODE_DISABLED, "l2_cache_active"}
	};
	const char *l2_str;
	int ret;

	ret = of_property_read_string(node, key, &l2_str);
	if (!ret) {
		ret = -EINVAL;
		for (i = 0; i < ARRAY_SIZE(l2_mode_lookup); i++) {
			if (!strcmp(l2_str, l2_mode_lookup[i].mode_name)) {
				*l2_val = l2_mode_lookup[i].modes;
				ret = 0;
				break;
			}
		}
	}
	return ret;
}

static int __devinit msm_lpm_levels_sysfs_add(void)
{
	struct kobject *module_kobj = NULL;
	struct kobject *low_power_kobj = NULL;
	int rc = 0;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("%s: cannot find kobject for module %s\n",
			__func__, KBUILD_MODNAME);
		rc = -ENOENT;
		goto resource_sysfs_add_exit;
	}

	low_power_kobj = kobject_create_and_add(
				"enable_low_power", module_kobj);
	if (!low_power_kobj) {
		pr_err("%s: cannot create kobject\n", __func__);
		rc = -ENOMEM;
		goto resource_sysfs_add_exit;
	}

	rc = sysfs_create_group(low_power_kobj, &lpm_levels_attr_grp);
resource_sysfs_add_exit:
	if (rc) {
		if (low_power_kobj) {
			sysfs_remove_group(low_power_kobj,
						&lpm_levels_attr_grp);
			kobject_del(low_power_kobj);
		}
	}

	return rc;
}

static int __devinit msm_lpm_levels_probe(struct platform_device *pdev)
{
	struct msm_rpmrs_level *levels = NULL;
	struct msm_rpmrs_level *level = NULL;
	struct device_node *node = NULL;
	char *key = NULL;
	uint32_t val = 0;
	int ret = 0;
	uint32_t num_levels = 0;
	int idx = 0;

	for_each_child_of_node(pdev->dev.of_node, node)
		num_levels++;

	levels = kzalloc(num_levels * sizeof(struct msm_rpmrs_level),
			GFP_KERNEL);
	if (!levels)
		return -ENOMEM;

	for_each_child_of_node(pdev->dev.of_node, node) {
		level = &levels[idx++];
		level->available = false;

		key = "qcom,mode";
		ret = msm_pm_get_sleep_mode_value(node, key, &val);
		if (ret)
			goto fail;
		level->sleep_mode = val;

		key = "qcom,l2";
		ret = msm_lpm_get_l2_cache_value(node, key, &val);
		if (ret)
			goto fail;
		level->l2_cache = val;

		key = "qcom,latency-us";
		ret = of_property_read_u32(node, key, &val);
		if (ret)
			goto fail;
		level->latency_us = val;

		key = "qcom,ss-power";
		ret = of_property_read_u32(node, key, &val);
		if (ret)
			goto fail;
		level->steady_state_power = val;

		key = "qcom,energy-overhead";
		ret = of_property_read_u32(node, key, &val);
		if (ret)
			goto fail;
		level->energy_overhead = val;

		key = "qcom,time-overhead";
		ret = of_property_read_u32(node, key, &val);
		if (ret)
			goto fail;
		level->time_overhead_us = val;

		level->available = true;
	}

	node = pdev->dev.of_node;
	key = "qcom,no-l2-saw";
	no_l2_saw = of_property_read_bool(node, key);

	msm_lpm_levels = levels;
	msm_lpm_level_count = idx;

	if (num_online_cpus() == 1)
		allowed_l2_mode = MSM_SPM_L2_MODE_POWER_COLLAPSE;

	/* Do the following two steps only if L2 SAW is present */
	if (!no_l2_saw) {
		key = "qcom,default-l2-state";
		if (msm_lpm_get_l2_cache_value(node, key, &default_l2_mode))
			goto fail;

		if (msm_lpm_levels_sysfs_add())
			goto fail;
		register_hotcpu_notifier(&msm_lpm_cpu_nblk);
		msm_pm_set_l2_flush_flag(0);
	} else {
		msm_pm_set_l2_flush_flag(1);
		default_l2_mode = MSM_SPM_L2_MODE_POWER_COLLAPSE;
	}

	msm_lpm_level_update();
	msm_pm_set_sleep_ops(&msm_lpm_ops);
	return 0;
fail:
	pr_err("%s: Error in name %s key %s\n", __func__, node->full_name, key);
	kfree(levels);
	return -EFAULT;
}

static struct of_device_id msm_lpm_levels_match_table[] = {
	{.compatible = "qcom,lpm-levels"},
	{},
};

static struct platform_driver msm_lpm_levels_driver = {
	.probe = msm_lpm_levels_probe,
	.driver = {
		.name = "lpm-levels",
		.owner = THIS_MODULE,
		.of_match_table = msm_lpm_levels_match_table,
	},
};

static int __init msm_lpm_levels_module_init(void)
{
	return platform_driver_register(&msm_lpm_levels_driver);
}
late_initcall(msm_lpm_levels_module_init);
