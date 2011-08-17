/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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

#include "cpuidle.h"
#include "pm.h"

static DEFINE_PER_CPU_SHARED_ALIGNED(struct cpuidle_device, msm_cpuidle_devs);
static struct cpuidle_driver msm_cpuidle_driver = {
	.name = "msm_idle",
	.owner = THIS_MODULE,
};

#ifdef CONFIG_MSM_SLEEP_STATS
static DEFINE_PER_CPU(struct atomic_notifier_head, msm_cpuidle_notifiers);

int msm_cpuidle_register_notifier(unsigned int cpu, struct notifier_block *nb)
{
	struct atomic_notifier_head *head =
		&per_cpu(msm_cpuidle_notifiers, cpu);

	return atomic_notifier_chain_register(head, nb);
}
EXPORT_SYMBOL(msm_cpuidle_register_notifier);

int msm_cpuidle_unregister_notifier(unsigned int cpu, struct notifier_block *nb)
{
	struct atomic_notifier_head *head =
		&per_cpu(msm_cpuidle_notifiers, cpu);

	return atomic_notifier_chain_unregister(head, nb);
}
EXPORT_SYMBOL(msm_cpuidle_unregister_notifier);
#endif

static int msm_cpuidle_enter(
	struct cpuidle_device *dev, struct cpuidle_state *state)
{
	int ret;
#ifdef CONFIG_MSM_SLEEP_STATS
	struct atomic_notifier_head *head =
			&__get_cpu_var(msm_cpuidle_notifiers);
#endif

	local_irq_disable();

#ifdef CONFIG_MSM_SLEEP_STATS
	atomic_notifier_call_chain(head, MSM_CPUIDLE_STATE_ENTER, NULL);
#endif

	ret = msm_pm_idle_enter((enum msm_pm_sleep_mode) (state->driver_data));

#ifdef CONFIG_MSM_SLEEP_STATS
	atomic_notifier_call_chain(head, MSM_CPUIDLE_STATE_EXIT, NULL);
#endif

	local_irq_enable();

	return ret;
}

void __init msm_cpuidle_set_states(struct msm_cpuidle_state *states,
	int nr_states, struct msm_pm_platform_data *pm_data)
{
	unsigned int cpu;

	for_each_possible_cpu(cpu) {
		struct cpuidle_device *dev = &per_cpu(msm_cpuidle_devs, cpu);
		int i;

		dev->cpu = cpu;
		dev->prepare = msm_pm_idle_prepare;

		for (i = 0; i < nr_states; i++) {
			struct msm_cpuidle_state *cstate = &states[i];
			struct cpuidle_state *state;
			struct msm_pm_platform_data *pm_mode;

			if (cstate->cpu != cpu)
				continue;

			state = &dev->states[cstate->state_nr];
			pm_mode = &pm_data[MSM_PM_MODE(cpu, cstate->mode_nr)];

			snprintf(state->name, CPUIDLE_NAME_LEN, cstate->name);
			snprintf(state->desc, CPUIDLE_DESC_LEN, cstate->desc);
			state->driver_data = (void *) cstate->mode_nr;
			state->flags = CPUIDLE_FLAG_TIME_VALID;
			state->exit_latency = pm_mode->latency;
			state->power_usage = 0;
			state->target_residency = pm_mode->residency;
			state->enter = msm_cpuidle_enter;
		}

		for (i = 0; i < CPUIDLE_STATE_MAX; i++) {
			if (dev->states[i].enter == NULL)
				break;
			dev->state_count = i + 1;
		}
	}
}

int __init msm_cpuidle_init(void)
{
	unsigned int cpu;
	int ret;

	ret = cpuidle_register_driver(&msm_cpuidle_driver);
	if (ret)
		pr_err("%s: failed to register cpuidle driver: %d\n",
			__func__, ret);

	for_each_possible_cpu(cpu) {
		struct cpuidle_device *dev = &per_cpu(msm_cpuidle_devs, cpu);

		ret = cpuidle_register_device(dev);
		if (ret) {
			pr_err("%s: failed to register cpuidle device for "
				"cpu %u: %d\n", __func__, cpu, ret);
			return ret;
		}
	}

	return 0;
}

static int __init msm_cpuidle_early_init(void)
{
#ifdef CONFIG_MSM_SLEEP_STATS
	unsigned int cpu;

	for_each_possible_cpu(cpu)
		ATOMIC_INIT_NOTIFIER_HEAD(&per_cpu(msm_cpuidle_notifiers, cpu));
#endif
	return 0;
}

early_initcall(msm_cpuidle_early_init);
