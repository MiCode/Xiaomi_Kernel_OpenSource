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

#include "dt_idle_states.h"

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

static struct cpuidle_driver mt81xx_cpuidle_driver = {
	.name             = "mt81xx_cpuidle",
	.owner            = THIS_MODULE,
	.states[0] = {
		.enter            = mt_dpidle_enter,
		.exit_latency     = 2000,            /* 2 ms */
		.target_residency = 1,
		.flags            = CPUIDLE_FLAG_TIME_VALID,
		.name             = "dpidle",
		.desc             = "deepidle",
	},
	.states[1] = {
		.enter            = mt_soidle_enter,
		.exit_latency     = 2000,            /* 2 ms */
		.target_residency = 1,
		.flags            = CPUIDLE_FLAG_TIME_VALID,
		.name             = "SODI",
		.desc             = "SODI",
	},
	.states[2] = {
		.enter            = mt_mcidle_enter,
		.exit_latency     = 2000,            /* 2 ms */
		.target_residency = 1,
		.flags            = CPUIDLE_FLAG_TIME_VALID,
		.name             = "MCDI",
		.desc             = "MCDI",
	},
	.states[3] = {
		.enter            = mt_slidle_enter,
		.exit_latency     = 2000,            /* 2 ms */
		.target_residency = 1,
		.flags            = CPUIDLE_FLAG_TIME_VALID,
		.name             = "slidle",
		.desc             = "slidle",
	},
	.states[4] = {
		.enter            = mt_rgidle_enter,
		.exit_latency     = 2000,            /* 2 ms */
		.target_residency = 1,
		.flags            = CPUIDLE_FLAG_TIME_VALID,
		.name             = "rgidle",
		.desc             = "WFI",
	},
	.state_count = 5,
	.safe_state_index = 0,
};

#ifdef CONFIG_ARM64

static const struct of_device_id mt81xx_idle_state_match[] __initconst = {
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
int __init mt81xx_cpuidle_init(void)
{
	int cpu, ret;
	struct cpuidle_driver *drv = &mt81xx_cpuidle_driver;

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
int __init mt81xx_cpuidle_init(void)
{
	return cpuidle_register(&mt81xx_cpuidle_driver, NULL);
}
#endif

device_initcall(mt81xx_cpuidle_init);
