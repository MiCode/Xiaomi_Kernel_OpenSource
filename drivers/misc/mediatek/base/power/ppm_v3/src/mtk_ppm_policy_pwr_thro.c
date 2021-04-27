/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#include "mach/upmu_sw.h"
#include "mtk_ppm_internal.h"
#include "mach/mtk_pmic.h"

#if defined(LOW_BATTERY_PT_SETTING_V2) || defined(LBAT_LIMIT_BCPU_OPP)
#define ENABLE_OPP_LIMIT
#endif

static void ppm_pwrthro_update_limit_cb(void);
static void ppm_pwrthro_status_change_cb(bool enable);
#if defined(ENABLE_OPP_LIMIT)
static bool ppm_pwrthro_is_policy_active(void);
static void mt_ppm_pwrthro_set_freq_limit(unsigned int cluster, int min_freq_idx, int max_freq_idx);
#endif

/* other members will init by ppm_main */
static struct ppm_policy_data pwrthro_policy = {
	.name			= __stringify(PPM_POLICY_PWR_THRO),
	.lock			= __MUTEX_INITIALIZER(pwrthro_policy.lock),
	.policy			= PPM_POLICY_PWR_THRO,
	.priority		= PPM_POLICY_PRIO_POWER_BUDGET_BASE,
	.update_limit_cb	= ppm_pwrthro_update_limit_cb,
	.status_change_cb	= ppm_pwrthro_status_change_cb,
};

#if defined(ENABLE_OPP_LIMIT)
static struct ppm_pwrthro_data {
	struct ppm_user_limit limit[NR_PPM_CLUSTERS];
} pwrthro_limit_data;
#endif

static void ppm_pwrthro_update_limit_cb(void)
{
#if defined(ENABLE_OPP_LIMIT)
	int i;
#endif

	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_clear_policy_limit(&pwrthro_policy);

	/* update limit according to power budget */
#if defined(ENABLE_OPP_LIMIT)
	if (pwrthro_policy.req.power_budget != 0)
#endif
		ppm_update_req_by_pwr(&pwrthro_policy.req);

#if defined(ENABLE_OPP_LIMIT)
	for_each_ppm_clusters(i) {
		if (pwrthro_limit_data.limit[i].min_freq_idx != -1 &&
			pwrthro_policy.req.limit[i].min_cpufreq_idx >
			pwrthro_limit_data.limit[i].min_freq_idx)
			pwrthro_policy.req.limit[i].min_cpufreq_idx =
				pwrthro_limit_data.limit[i].min_freq_idx;
		if (pwrthro_limit_data.limit[i].max_freq_idx != -1 &&
			pwrthro_policy.req.limit[i].max_cpufreq_idx <
			pwrthro_limit_data.limit[i].max_freq_idx)
			pwrthro_policy.req.limit[i].max_cpufreq_idx =
				pwrthro_limit_data.limit[i].max_freq_idx;
		if (pwrthro_limit_data.limit[i].min_core_num != -1 &&
			pwrthro_policy.req.limit[i].min_cpu_core <
			pwrthro_limit_data.limit[i].min_core_num)
			pwrthro_policy.req.limit[i].min_cpu_core =
				pwrthro_limit_data.limit[i].min_core_num;
		if (pwrthro_limit_data.limit[i].max_core_num != -1 &&
			pwrthro_policy.req.limit[i].max_cpu_core >
			pwrthro_limit_data.limit[i].max_core_num)
			pwrthro_policy.req.limit[i].max_cpu_core =
				pwrthro_limit_data.limit[i].max_core_num;
		if (pwrthro_policy.req.limit[i].min_cpu_core
			> pwrthro_policy.req.limit[i].max_cpu_core)
			pwrthro_policy.req.limit[i].min_cpu_core =
				pwrthro_policy.req.limit[i].max_cpu_core;
	}
#endif

	FUNC_EXIT(FUNC_LV_POLICY);
}

static void ppm_pwrthro_status_change_cb(bool enable)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_ver("pwrthro policy status changed to %d\n", enable);

	FUNC_EXIT(FUNC_LV_POLICY);
}

#if defined(ENABLE_OPP_LIMIT)
static bool ppm_pwrthro_is_policy_active(void)
{
	int i;

	for_each_ppm_clusters(i) {
		if (pwrthro_limit_data.limit[i].min_freq_idx != -1 ||
		    pwrthro_limit_data.limit[i].max_freq_idx != -1 ||
		    pwrthro_limit_data.limit[i].min_core_num != -1 ||
		    pwrthro_limit_data.limit[i].max_core_num != -1) {
			return true;
		}
	}
	if (pwrthro_policy.req.power_budget != 0)
		return true;

	return false;
}
static void mt_ppm_pwrthro_set_freq_limit(unsigned int cluster, int min_freq_idx, int max_freq_idx)
{
	if (cluster >= NR_PPM_CLUSTERS) {
		ppm_err("Invalid input: cluster = %d\n", cluster);
		return;
	}

	if ((max_freq_idx != -1 &&
		(max_freq_idx < get_cluster_max_cpufreq_idx(cluster) ||
		max_freq_idx > get_cluster_min_cpufreq_idx(cluster)))
		|| (min_freq_idx != -1
		&& (min_freq_idx > get_cluster_min_cpufreq_idx(cluster) ||
		min_freq_idx < get_cluster_max_cpufreq_idx(cluster)))) {
		ppm_err("Invalid input: cl=%d, min/max freqidx=%d/%d\n",
			cluster, min_freq_idx, max_freq_idx);
		return;
	}

	pr_debug("%s: input cpu freq boundary in pwrthro policy: cluster %d min/max freqidx = %d/%d\n",
		__func__, cluster, min_freq_idx, max_freq_idx);

	if (!pwrthro_policy.is_enabled) {
		ppm_warn("@%s: pwrthro policy is not enabled!\n", __func__);
		return;
	}

	if (min_freq_idx < max_freq_idx && max_freq_idx != -1 && min_freq_idx != -1)
		min_freq_idx = max_freq_idx;

	/* update cpu freq setting */
	pwrthro_limit_data.limit[cluster].min_freq_idx = min_freq_idx;
	pwrthro_limit_data.limit[cluster].max_freq_idx = max_freq_idx;

	/* error check */
	if (pwrthro_limit_data.limit[cluster].min_freq_idx != -1
		&& pwrthro_limit_data.limit[cluster].min_freq_idx
		< pwrthro_limit_data.limit[cluster].max_freq_idx)
		pwrthro_limit_data.limit[cluster].min_freq_idx =
			pwrthro_limit_data.limit[cluster].max_freq_idx;

	pwrthro_policy.is_activated = ppm_pwrthro_is_policy_active();

	if (pwrthro_policy.is_activated == false)
		ppm_clear_policy_limit(&pwrthro_policy);

}
#endif

#ifndef DISABLE_BATTERY_PERCENT_PROTECT
static void ppm_pwrthro_bat_per_protect(BATTERY_PERCENT_LEVEL level)
{
	unsigned int limited_power = ~0;

	FUNC_ENTER(FUNC_LV_API);

	ppm_ver("@%s: bat percent lv = %d\n", __func__, level);

	ppm_lock(&pwrthro_policy.lock);

	if (!pwrthro_policy.is_enabled) {
		ppm_warn("@%s: pwrthro policy is not enabled!\n", __func__);
		ppm_unlock(&pwrthro_policy.lock);
		goto end;
	}

	switch (level) {
	case BATTERY_PERCENT_LEVEL_1:
		limited_power = PWRTHRO_BAT_PER_MW;
		break;
	default:
		/* Unlimit */
		limited_power = 0;
		break;
	}

	pwrthro_policy.req.power_budget = limited_power;
#if defined(ENABLE_OPP_LIMIT)
	pwrthro_policy.is_activated = ppm_pwrthro_is_policy_active();
#else
	pwrthro_policy.is_activated = (limited_power) ? true : false;
#endif
	ppm_unlock(&pwrthro_policy.lock);
	mt_ppm_main();

end:
	FUNC_EXIT(FUNC_LV_API);
}
#endif

#ifndef DISABLE_BATTERY_OC_PROTECT
static void ppm_pwrthro_bat_oc_protect(BATTERY_OC_LEVEL level)
{
	unsigned int limited_power = ~0;

	FUNC_ENTER(FUNC_LV_API);

	ppm_ver("@%s: bat OC lv = %d\n", __func__, level);

	ppm_lock(&pwrthro_policy.lock);

	if (!pwrthro_policy.is_enabled) {
		ppm_warn("@%s: pwrthro policy is not enabled!\n", __func__);
		ppm_unlock(&pwrthro_policy.lock);
		goto end;
	}

	switch (level) {
	case BATTERY_OC_LEVEL_1:
		limited_power = PWRTHRO_BAT_OC_MW;
		break;
	default:
		/* Unlimit */
		limited_power = 0;
		break;
	}

	pwrthro_policy.req.power_budget = limited_power;
#if defined(ENABLE_OPP_LIMIT)
	pwrthro_policy.is_activated = ppm_pwrthro_is_policy_active();
#else
	pwrthro_policy.is_activated = (limited_power) ? true : false;
#endif
	ppm_unlock(&pwrthro_policy.lock);
	mt_ppm_main();

end:
	FUNC_EXIT(FUNC_LV_API);
}
#endif

#ifndef DISABLE_LOW_BATTERY_PROTECT
void ppm_pwrthro_low_bat_protect(LOW_BATTERY_LEVEL level)
{
#ifndef LOW_BATTERY_PT_SETTING_V2
	unsigned int limited_power = ~0;
#endif

	FUNC_ENTER(FUNC_LV_API);

	ppm_ver("@%s: low bat lv = %d\n", __func__, level);

	ppm_lock(&pwrthro_policy.lock);

	if (!pwrthro_policy.is_enabled) {
		ppm_warn("@%s: pwrthro policy is not enabled!\n", __func__);
		ppm_unlock(&pwrthro_policy.lock);
		goto end;
	}

	switch (level) {
	case LOW_BATTERY_LEVEL_1:
#ifdef LOW_BATTERY_PT_SETTING_V2
		mt_ppm_pwrthro_set_freq_limit(1, -1, PWRTHRO_LOW_BAT_V2_LV1_OPP);
#else
		limited_power = PWRTHRO_LOW_BAT_LV1_MW;
#endif
		break;
	case LOW_BATTERY_LEVEL_2:
#ifdef LOW_BATTERY_PT_SETTING_V2
		mt_ppm_pwrthro_set_freq_limit(1, -1, PWRTHRO_LOW_BAT_V2_LV2_OPP);
#else
		limited_power = PWRTHRO_LOW_BAT_LV2_MW;
#endif
		break;
#ifdef LOW_BATTERY_PT_SETTING_V2
	case LOW_BATTERY_LEVEL_3:
		mt_ppm_pwrthro_set_freq_limit(1, -1, PWRTHRO_LOW_BAT_V2_LV3_OPP);
		break;
#endif
	default:
		/* Unlimit */
#ifdef LOW_BATTERY_PT_SETTING_V2
		mt_ppm_pwrthro_set_freq_limit(1, -1, -1);
#else
		limited_power = 0;
#endif
		break;
	}

#ifndef LOW_BATTERY_PT_SETTING_V2
	pwrthro_policy.req.power_budget = limited_power;
#endif
#if defined(ENABLE_OPP_LIMIT)
	pwrthro_policy.is_activated = ppm_pwrthro_is_policy_active();
#else
	pwrthro_policy.is_activated = (limited_power) ? true : false;
#endif
	ppm_unlock(&pwrthro_policy.lock);
	mt_ppm_main();

end:
	FUNC_EXIT(FUNC_LV_API);
}

#ifdef LBAT_LIMIT_BCPU_OPP
void ppm_pwrthro_low_bat_protect_ext(LOW_BATTERY_LEVEL level)
{
	FUNC_ENTER(FUNC_LV_API);

	ppm_ver("@%s: low bat lv = %d\n", __func__, level);

	ppm_lock(&pwrthro_policy.lock);

	if (!pwrthro_policy.is_enabled) {
		ppm_warn("@%s: pwrthro policy is not enabled!\n", __func__);
		ppm_unlock(&pwrthro_policy.lock);
		goto end;
	}

	switch (level) {
	case LOW_BATTERY_LEVEL_1:
		mt_ppm_pwrthro_set_freq_limit(1, -1, PWRTHRO_LOW_BAT_LV1_OPP);
		break;
	case LOW_BATTERY_LEVEL_2:
		break;
	default:
		/* Unlimit */
		mt_ppm_pwrthro_set_freq_limit(1, -1, -1);
		break;
	}

	pwrthro_policy.is_activated = ppm_pwrthro_is_policy_active();
	ppm_unlock(&pwrthro_policy.lock);
	mt_ppm_main();

end:
	FUNC_EXIT(FUNC_LV_API);
}
#endif /* LBAT_LIMIT_BCPU_OPP */
#endif /* DISABLE_LOW_BATTERY_PROTECT */

static int __init ppm_pwrthro_policy_init(void)
{
	int ret = 0;
#if defined(ENABLE_OPP_LIMIT)
	int i = 0;
#endif

	FUNC_ENTER(FUNC_LV_POLICY);

	if (ppm_main_register_policy(&pwrthro_policy)) {
		ppm_err("@%s: pwrthro policy register failed\n", __func__);
		ret = -EINVAL;
		goto out;
	}

#ifndef DISABLE_BATTERY_PERCENT_PROTECT
	register_battery_percent_notify(&ppm_pwrthro_bat_per_protect,
		BATTERY_PERCENT_PRIO_CPU_L);
#endif

#ifndef DISABLE_BATTERY_OC_PROTECT
	register_battery_oc_notify(&ppm_pwrthro_bat_oc_protect,
		BATTERY_OC_PRIO_CPU_L);
#endif

#ifndef DISABLE_LOW_BATTERY_PROTECT
	register_low_battery_notify(&ppm_pwrthro_low_bat_protect,
		LOW_BATTERY_PRIO_CPU_L);
#ifdef LBAT_LIMIT_BCPU_OPP
	register_low_battery_notify_ext(&ppm_pwrthro_low_bat_protect_ext,
		LOW_BATTERY_PRIO_CPU_L);
#endif
#endif

#if defined(ENABLE_OPP_LIMIT)
	for_each_ppm_clusters(i) {
		pwrthro_limit_data.limit[i].min_freq_idx = -1;
		pwrthro_limit_data.limit[i].max_freq_idx = -1;
		pwrthro_limit_data.limit[i].min_core_num = -1;
		pwrthro_limit_data.limit[i].max_core_num = -1;
	}
#endif

	ppm_info("@%s: register %s done!\n", __func__, pwrthro_policy.name);

out:
	FUNC_EXIT(FUNC_LV_POLICY);

	return ret;
}

static void __exit ppm_pwrthro_policy_exit(void)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_main_unregister_policy(&pwrthro_policy);

	FUNC_EXIT(FUNC_LV_POLICY);
}

module_init(ppm_pwrthro_policy_init);
module_exit(ppm_pwrthro_policy_exit);

