/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>

#include <mach/cpuidle.h>

#include "pm.h"

static DEFINE_PER_CPU_SHARED_ALIGNED(struct cpuidle_device, msm_cpuidle_devs);

static struct cpuidle_driver msm_cpuidle_driver = {
	.name = "msm_idle",
	.owner = THIS_MODULE,
};

static struct msm_cpuidle_state msm_cstates[] = {
	{0, 0, "C0", "WFI",
		MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT},

	{0, 1, "C1", "RETENTION",
		MSM_PM_SLEEP_MODE_RETENTION},

	{0, 2, "C2", "STANDALONE_POWER_COLLAPSE",
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE},

	{0, 3, "C3", "POWER_COLLAPSE",
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE},

	{1, 0, "C0", "WFI",
		MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT},

	{1, 1, "C1", "RETENTION",
		MSM_PM_SLEEP_MODE_RETENTION},

	{1, 2, "C2", "STANDALONE_POWER_COLLAPSE",
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE},

	{2, 0, "C0", "WFI",
		MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT},

	{2, 1, "C1", "RETENTION",
		MSM_PM_SLEEP_MODE_RETENTION},

	{2, 2, "C2", "STANDALONE_POWER_COLLAPSE",
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE},

	{3, 0, "C0", "WFI",
		MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT},

	{3, 1, "C1", "RETENTION",
		MSM_PM_SLEEP_MODE_RETENTION},

	{3, 2, "C2", "STANDALONE_POWER_COLLAPSE",
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE},
};

static int msm_cpuidle_enter(
	struct cpuidle_device *dev, struct cpuidle_driver *drv, int index)
{
	int ret = 0;
	int i = 0;
	enum msm_pm_sleep_mode pm_mode;
	struct cpuidle_state_usage *st_usage = NULL;

#ifdef CONFIG_CPU_PM
	cpu_pm_enter();
#endif

	pm_mode = msm_pm_idle_prepare(dev, drv, index);
	dev->last_residency = msm_pm_idle_enter(pm_mode);
	for (i = 0; i < dev->state_count; i++) {
		st_usage = &dev->states_usage[i];
		if ((enum msm_pm_sleep_mode) cpuidle_get_statedata(st_usage)
		    == pm_mode) {
			ret = i;
			break;
		}
	}

#ifdef CONFIG_CPU_PM
	cpu_pm_exit();
#endif

	local_irq_enable();

	return ret;
}

static void __init msm_cpuidle_set_states(void)
{
	int i = 0;
	int state_count = 0;
	struct msm_cpuidle_state *cstate = NULL;
	struct cpuidle_state *state = NULL;

	for (i = 0; i < ARRAY_SIZE(msm_cstates); i++) {
		cstate = &msm_cstates[i];
		/* We have an asymmetric CPU C-State in MSMs.
		 * The primary CPU can do PC while all secondary cpus
		 * can only do standalone PC as part of their idle LPM.
		 * However, the secondary cpus can do PC when hotplugged
		 * We do not care about the hotplug here.
		 * Register the C-States available for Core0.
		 */
		if (cstate->cpu)
			continue;

		state = &msm_cpuidle_driver.states[state_count];
		snprintf(state->name, CPUIDLE_NAME_LEN, cstate->name);
		snprintf(state->desc, CPUIDLE_DESC_LEN, cstate->desc);
		state->flags = 0;
		state->exit_latency = 0;
		state->power_usage = 0;
		state->target_residency = 0;
		state->enter = msm_cpuidle_enter;

		state_count++;
		BUG_ON(state_count >= CPUIDLE_STATE_MAX);
	}
	msm_cpuidle_driver.state_count = state_count;
	msm_cpuidle_driver.safe_state_index = 0;
}

static void __init msm_cpuidle_set_cpu_statedata(struct cpuidle_device *dev)
{
	int i = 0;
	int state_count = 0;
	struct cpuidle_state_usage *st_usage = NULL;
	struct msm_cpuidle_state *cstate = NULL;

	for (i = 0; i < ARRAY_SIZE(msm_cstates); i++) {
		cstate = &msm_cstates[i];
		if (cstate->cpu != dev->cpu)
			continue;

		st_usage = &dev->states_usage[state_count];
		cpuidle_set_statedata(st_usage, (void *)cstate->mode_nr);
		state_count++;
		BUG_ON(state_count > msm_cpuidle_driver.state_count);
	}

	dev->state_count = state_count; /* Per cpu state count */
}

int __init msm_cpuidle_init(void)
{
	unsigned int cpu = 0;
	int ret = 0;

	msm_cpuidle_set_states();
	ret = cpuidle_register_driver(&msm_cpuidle_driver);
	if (ret)
		pr_err("%s: failed to register cpuidle driver: %d\n",
			__func__, ret);

	for_each_possible_cpu(cpu) {
		struct cpuidle_device *dev = &per_cpu(msm_cpuidle_devs, cpu);

		dev->cpu = cpu;
		msm_cpuidle_set_cpu_statedata(dev);
		ret = cpuidle_register_device(dev);
		if (ret) {
			pr_err("%s: failed to register cpuidle device for "
				"cpu %u: %d\n", __func__, cpu, ret);
			return ret;
		}
	}

	return 0;
}
