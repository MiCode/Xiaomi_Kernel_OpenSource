// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/cpuidle.h>
#include <linux/cpumask.h>
#include <linux/cpu_pm.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>

#include <asm/cpuidle.h>

#include "dt_idle_states.h"

#define USING_TICK_BROADCAST	1

#define IDLE_TAG	"[Power/swap]"
#define idle_dbg(fmt, args...)	pr_debug(IDLE_TAG fmt, ##args)

int __attribute__((weak)) dpidle_enter(int cpu)
{
	return 1;
}

int __attribute__((weak)) soidle_enter(int cpu)
{
	return 1;
}

int __attribute__((weak)) mcidle_enter(int cpu)
{
	return 1;
}

int __attribute__((weak)) slidle_enter(int cpu)
{
	return 1;
}

int __attribute__((weak)) rgidle_enter(int cpu)
{
	return 1;
}

static int mt_dpidle_enter(struct cpuidle_device *dev,
			      struct cpuidle_driver *drv, int index)
{
	return dpidle_enter(smp_processor_id());
}

static int mt_soidle_enter(struct cpuidle_device *dev,
			      struct cpuidle_driver *drv, int index)
{
	return soidle_enter(smp_processor_id());
}

static int mt_mcidle_enter(struct cpuidle_device *dev,
			      struct cpuidle_driver *drv, int index)
{
	return mcidle_enter(smp_processor_id());
}

static int mt_slidle_enter(struct cpuidle_device *dev,
			      struct cpuidle_driver *drv, int index)
{
	return slidle_enter(smp_processor_id());
}

static int mt_rgidle_enter(struct cpuidle_device *dev,
			      struct cpuidle_driver *drv, int index)
{
	return rgidle_enter(smp_processor_id());
}

static struct cpuidle_driver mt81xx_v2_cpuidle_driver = {
	.name             = "mt81xx_v2_cpuidle",
	.owner            = THIS_MODULE,
	.states[0] = {
		.enter            = mt_dpidle_enter,
		.exit_latency     = 2000,            /* 2 ms */
		.target_residency = 1,
#if USING_TICK_BROADCAST
		.flags            = CPUIDLE_FLAG_TIMER_STOP,
#else
		.flags            = CPUIDLE_FLAG_TIME_VALID,
#endif
		.name             = "dpidle",
		.desc             = "deepidle",
	},
	.states[1] = {
		.enter            = mt_soidle_enter,
		.exit_latency     = 2000,            /* 2 ms */
		.target_residency = 1,
#if USING_TICK_BROADCAST
		.flags            = CPUIDLE_FLAG_TIMER_STOP,
#else
		.flags            = CPUIDLE_FLAG_TIME_VALID,
#endif
		.name             = "SODI",
		.desc             = "SODI",
	},
	.states[2] = {
		.enter            = mt_mcidle_enter,
		.exit_latency     = 2000,            /* 2 ms */
		.target_residency = 1,
#if USING_TICK_BROADCAST
		.flags            = CPUIDLE_FLAG_TIMER_STOP,
#else
		.flags            = CPUIDLE_FLAG_TIME_VALID,
#endif
		.name             = "MCDI",
		.desc             = "MCDI",
	},
	.states[3] = {
		.enter            = mt_slidle_enter,
		.exit_latency     = 2000,            /* 2 ms */
		.target_residency = 1,
#if USING_TICK_BROADCAST
#else
		.flags            = CPUIDLE_FLAG_TIME_VALID,
#endif
		.name             = "slidle",
		.desc             = "slidle",
	},
	.states[4] = {
		.enter            = mt_rgidle_enter,
		.exit_latency     = 2000,            /* 2 ms */
		.target_residency = 1,
#if USING_TICK_BROADCAST
#else
		.flags            = CPUIDLE_FLAG_TIME_VALID,
#endif
		.name             = "rgidle",
		.desc             = "WFI",
	},
	.state_count = 5,
	.safe_state_index = 0,
};

#ifdef CONFIG_ARM64

static const struct of_device_id mt81xx_v2_idle_state_match[] __initconst = {
	{ .compatible = "arm,idle-state" },
	{ },
};

/*
 * arm64_idle_init
 *
 * Registers the arm64 specific cpuidle driver with the cpuidle
 * framework. It relies on core code to parse the idle states
 * and initialize them using driver data structures accordingly.
 */
int __init mt81xx_v2_cpuidle_init(void)
{
	int cpu, ret;
	struct cpuidle_driver *drv = &mt81xx_v2_cpuidle_driver;
	struct cpuidle_device *dev;

	ret = cpuidle_register_driver(drv);
	if (ret) {
		pr_err("Failed to register cpuidle driver\n");
		return ret;
	}
	/*
	 * Call arch CPU operations in order to initialize
	 * idle states suspend back-end specific data
	 */
	for_each_possible_cpu(cpu) {
		ret = arm_cpuidle_init(cpu);

		/*
		 * Skip the cpuidle device initialization if the reported
		 * failure is a HW misconfiguration/breakage (-ENXIO).
		 */
		if (ret == -ENXIO)
			continue;

		if (ret) {
			pr_err("CPU %d failed to init idle CPU ops\n", cpu);
			goto out_fail;
		}

		dev = kzalloc(sizeof(*dev), GFP_KERNEL);
		if (!dev)
			goto out_fail;
		dev->cpu = cpu;

		ret = cpuidle_register_device(dev);
		if (ret) {
			pr_err("Failed to register cpuidle device for CPU %d\n",
			       cpu);
			kfree(dev);
			goto out_fail;
		}
	}

	return 0;
out_fail:
	while (--cpu >= 0) {
		dev = per_cpu(cpuidle_devices, cpu);
		cpuidle_unregister_device(dev);
		kfree(dev);
	}

	cpuidle_unregister_driver(drv);

	return ret;
}

#else

int __init mt81xx_v2_cpuidle_init(void)
{
	return cpuidle_register(&mt81xx_v2_cpuidle_driver, NULL);
}

#endif /* CONFIG_ARM64 */

device_initcall(mt81xx_v2_cpuidle_init);
