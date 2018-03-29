/*
 * Copyright (C) 2015 MediaTek Inc.
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
#include "mt_ppm_internal.h"
#include "mach/mt_pmic.h"


static void ppm_pwrthro_update_limit_cb(enum ppm_power_state new_state);
static void ppm_pwrthro_status_change_cb(bool enable);
static void ppm_pwrthro_mode_change_cb(enum ppm_mode mode);

/* other members will init by ppm_main */
static struct ppm_policy_data pwrthro_policy = {
	.name			= __stringify(PPM_POLICY_PWR_THRO),
	.lock			= __MUTEX_INITIALIZER(pwrthro_policy.lock),
	.policy			= PPM_POLICY_PWR_THRO,
	.priority		= PPM_POLICY_PRIO_POWER_BUDGET_BASE,
	.get_power_state_cb	= NULL,	/* decide in ppm main via min power budget */
	.update_limit_cb	= ppm_pwrthro_update_limit_cb,
	.status_change_cb	= ppm_pwrthro_status_change_cb,
	.mode_change_cb		= ppm_pwrthro_mode_change_cb,
};


static void ppm_pwrthro_update_limit_cb(enum ppm_power_state new_state)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_ver("@%s: pwrthro policy update limit for new state = %s\n",
		__func__, ppm_get_power_state_name(new_state));

	ppm_hica_set_default_limit_by_state(new_state, &pwrthro_policy);

	/* update limit according to power budget */
	ppm_main_update_req_by_pwr(new_state, &pwrthro_policy.req);

	FUNC_EXIT(FUNC_LV_POLICY);
}

static void ppm_pwrthro_status_change_cb(bool enable)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_ver("@%s: pwrthro policy status changed to %d\n", __func__, enable);

	FUNC_EXIT(FUNC_LV_POLICY);
}

static void ppm_pwrthro_mode_change_cb(enum ppm_mode mode)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_ver("@%s: ppm mode changed to %d\n", __func__, mode);

	FUNC_EXIT(FUNC_LV_POLICY);
}

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
	pwrthro_policy.is_activated = (limited_power) ? true : false;
	ppm_unlock(&pwrthro_policy.lock);
	ppm_task_wakeup();

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
	pwrthro_policy.is_activated = (limited_power) ? true : false;
	ppm_unlock(&pwrthro_policy.lock);
	ppm_task_wakeup();

end:
	FUNC_EXIT(FUNC_LV_API);
}
#endif

#ifndef DISABLE_LOW_BATTERY_PROTECT
void ppm_pwrthro_low_bat_protect(LOW_BATTERY_LEVEL level)
{
	unsigned int limited_power = ~0;

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
		limited_power = PWRTHRO_LOW_BAT_LV1_MW;
		break;
	case LOW_BATTERY_LEVEL_2:
		limited_power = PWRTHRO_LOW_BAT_LV2_MW;
		break;
	default:
		/* Unlimit */
		limited_power = 0;
		break;
	}

	pwrthro_policy.req.power_budget = limited_power;
	pwrthro_policy.is_activated = (limited_power) ? true : false;
	ppm_unlock(&pwrthro_policy.lock);
	ppm_task_wakeup();

end:
	FUNC_EXIT(FUNC_LV_API);
}
#endif

static int __init ppm_pwrthro_policy_init(void)
{
	int ret = 0;

	FUNC_ENTER(FUNC_LV_POLICY);

	if (ppm_main_register_policy(&pwrthro_policy)) {
		ppm_err("@%s: pwrthro policy register failed\n", __func__);
		ret = -EINVAL;
		goto out;
	}

#ifndef DISABLE_BATTERY_PERCENT_PROTECT
	register_battery_percent_notify(&ppm_pwrthro_bat_per_protect, BATTERY_PERCENT_PRIO_CPU_L);
#endif

#ifndef DISABLE_BATTERY_OC_PROTECT
	register_battery_oc_notify(&ppm_pwrthro_bat_oc_protect, BATTERY_OC_PRIO_CPU_L);
#endif

#ifndef DISABLE_LOW_BATTERY_PROTECT
	register_low_battery_notify(&ppm_pwrthro_low_bat_protect, LOW_BATTERY_PRIO_CPU_L);
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

