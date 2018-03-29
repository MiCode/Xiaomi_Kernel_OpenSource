/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpuidle.h>
#include <linux/cpumask.h>
#include <linux/cpu_pm.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>

#include <asm/cpuidle.h>
#include <asm/suspend.h>

#include "dt_idle_states.h"

#if defined(CONFIG_ARCH_MT6755) || defined(CONFIG_ARCH_MT6757) || defined(CONFIG_ARCH_ELBRUS)
#define USING_TICK_BROADCAST
#endif

int __attribute__((weak)) dpidle_enter(int cpu)
{
	return 1;
}

int __attribute__((weak)) soidle3_enter(int cpu)
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

static int mt_soidle3_enter(struct cpuidle_device *dev,
			      struct cpuidle_driver *drv, int index)
{
	return soidle3_enter(smp_processor_id());
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

static struct cpuidle_driver mt67xx_v2_cpuidle_driver = {
	.name             = "mt67xx_v2_cpuidle",
	.owner            = THIS_MODULE,
	.states[0] = {
		.enter            = mt_dpidle_enter,
		.exit_latency     = 2000,            /* 2 ms */
		.target_residency = 1,
#ifdef USING_TICK_BROADCAST
		.flags            = CPUIDLE_FLAG_TIME_VALID | CPUIDLE_FLAG_TIMER_STOP,
#else
		.flags            = CPUIDLE_FLAG_TIME_VALID,
#endif
		.name             = "dpidle",
		.desc             = "deepidle",
	},
	.states[1] = {
		.enter            = mt_soidle3_enter,
		.exit_latency     = 5000,            /* 5 ms */
		.target_residency = 1,
#ifdef USING_TICK_BROADCAST
		.flags            = CPUIDLE_FLAG_TIME_VALID | CPUIDLE_FLAG_TIMER_STOP,
#else
		.flags            = CPUIDLE_FLAG_TIME_VALID,
#endif
		.name             = "SODI3",
		.desc             = "SODI3",
	},
	.states[2] = {
		.enter            = mt_soidle_enter,
		.exit_latency     = 2000,            /* 2 ms */
		.target_residency = 1,
#ifdef USING_TICK_BROADCAST
		.flags            = CPUIDLE_FLAG_TIME_VALID | CPUIDLE_FLAG_TIMER_STOP,
#else
		.flags            = CPUIDLE_FLAG_TIME_VALID,
#endif
		.name             = "SODI",
		.desc             = "SODI",
	},
	.states[3] = {
		.enter            = mt_mcidle_enter,
		.exit_latency     = 2000,            /* 2 ms */
		.target_residency = 1,
#ifdef USING_TICK_BROADCAST
		.flags            = CPUIDLE_FLAG_TIME_VALID | CPUIDLE_FLAG_TIMER_STOP,
#else
		.flags            = CPUIDLE_FLAG_TIME_VALID,
#endif
		.name             = "MCDI",
		.desc             = "MCDI",
	},
	.states[4] = {
		.enter            = mt_slidle_enter,
		.exit_latency     = 2000,            /* 2 ms */
		.target_residency = 1,
		.flags            = CPUIDLE_FLAG_TIME_VALID,
		.name             = "slidle",
		.desc             = "slidle",
	},
	.states[5] = {
		.enter            = mt_rgidle_enter,
		.exit_latency     = 2000,            /* 2 ms */
		.target_residency = 1,
		.flags            = CPUIDLE_FLAG_TIME_VALID,
		.name             = "rgidle",
		.desc             = "WFI",
	},
	.state_count = 6,
	.safe_state_index = 0,
};

#if defined(CONFIG_ARM64) && !defined(CONFIG_MTK_FPGA)

static const struct of_device_id mt67xx_v2_idle_state_match[] __initconst = {
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
int __init mt67xx_v2_cpuidle_init(void)
{
	int cpu, ret;
	struct cpuidle_driver *drv = &mt67xx_v2_cpuidle_driver;

	/*
	 * Call arch CPU operations in order to initialize
	 * idle states suspend back-end specific data
	 */
	for_each_possible_cpu(cpu) {
		ret = cpu_init_idle(cpu);
		if (ret) {
			pr_err("CPU %d failed to init idle CPU ops\n", cpu);
			return ret;
		}
	}

	ret = cpuidle_register(drv, NULL);
	if (ret) {
		pr_err("failed to register cpuidle driver\n");
		return ret;
	}

	return 0;
}
#else
int __init mt67xx_v2_cpuidle_init(void)
{
	return cpuidle_register(&mt67xx_v2_cpuidle_driver, NULL);
}
#endif

device_initcall(mt67xx_v2_cpuidle_init);
