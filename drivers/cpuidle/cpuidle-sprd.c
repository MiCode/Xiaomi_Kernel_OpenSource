// SPDX-License-Identifier: GPL-2.0-only
/*
 * Spreadtrum ARM CPU idle driver.
 *
 * Copyright (C) 2019 Spreadtrum Ltd.
 * Author: Jingchao Ye <jingchao.ye@spreadtrum.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <asm/proc-fns.h>
#include <asm/suspend.h>
#include "dt_idle_states.h"
#include <linux/cpuidle-sprd.h>

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "arm-idle-sprd: " fmt

enum {
	STANDBY = 0,  /* WFI */
	L_SLEEP,      /* Light Sleep, WFI & DDR Self-refresh & MCU_SYS_SLEEP */
	H_SLEEP,      /* HEAVY/Doze Sleep */
	CORE_PD,      /* Core power down & Lightsleep */
};

static struct sprd_cpuidle_operations *sprd_cpuidle_ops;

int sprd_cpuidle_ops_init(struct sprd_cpuidle_operations *cpuidle_ops)
{
	sprd_cpuidle_ops = cpuidle_ops;
	return 0;
}

static void sprd_cpuidle_core_pd_en(void)
{
}
static void sprd_cpuidle_core_pd_dis(void)
{
}

static int sprd_cpuidle_real_suspend_fn(unsigned long cpu)
{
	cpu_do_idle();
	return 0;
}
/*
 * sprd_enter_idle_state - Programs CPU to enter the specified state
 *
 * @dev: cpuidle device
 * @drv: cpuidle driver
 * @idx: state index
 *
 * Called from the CPUidle framework to program the device to the
 * specified target state selected by the governor.
 */
static int sprd_enter_idle_state(struct cpuidle_device *dev,
				struct cpuidle_driver *drv, int idx)
{
	switch (idx) {
	case STANDBY:
		cpu_do_idle();
		break;
	case L_SLEEP:
		if (sprd_cpuidle_ops)
			sprd_cpuidle_ops->light_en();
		cpu_do_idle();
		if (sprd_cpuidle_ops)
			sprd_cpuidle_ops->light_dis();
		break;
	case H_SLEEP:
		if (sprd_cpuidle_ops)
			sprd_cpuidle_ops->doze_en();
		cpu_do_idle();
		if (sprd_cpuidle_ops)
			sprd_cpuidle_ops->doze_dis();
		break;
	case CORE_PD:
		cpu_pm_enter();
		sprd_cpuidle_core_pd_en();
		cpu_suspend(idx, sprd_cpuidle_real_suspend_fn);
		sprd_cpuidle_core_pd_dis();
		cpu_pm_exit();
		break;

	default:
		cpu_do_idle();
		WARN(1, "[CPUIDLE]: NO THIS IDLE LEVEL!!!");
	}

	return idx;
}

static struct cpuidle_driver sprd_cpuidle_driver = {
	.name = "arm-idle-sprd",
	.owner = THIS_MODULE,
	/*
	 * State at index 0 is standby wfi and considered standard
	 * on all ARM platforms. If in some platforms simple wfi
	 * can't be used as "state 0", DT bindings must be implemented
	 * to work around this issue and allow installing a special
	 * handler for idle state index 0.
	 */
	.states[0] = {
		.enter                  = sprd_enter_idle_state,
		.exit_latency           = 1,
		.target_residency       = 1,
		.power_usage		= UINT_MAX,
		.name                   = "WFI",
		.desc                   = "ARM WFI",
	}
};

static const struct of_device_id sprd_cpuidle_of_match[] __initconst = {
	{ .compatible = "sprd,pike2-idle-state",
	  .data = sprd_enter_idle_state },
	{ },
};

/*
 * sprd_cpuidle_init
 *
 * Registers the arm specific cpuidle driver with the cpuidle
 * framework. It relies on core code to parse the idle states
 * and initialize them using driver data structures accordingly.
 */
static int __init sprd_cpuidle_init(void)
{
	int ret;
	struct cpuidle_driver *drv = &sprd_cpuidle_driver;

	/*
	 * Initialize idle states data, starting at index 1.
	 * This driver is DT only, if no DT idle states are detected (ret == 0)
	 * let the driver initialization fail accordingly since there is no
	 * reason to initialize the idle driver if only wfi is supported.
	 */
	ret = dt_init_idle_driver(drv, sprd_cpuidle_of_match, 1);
	if (ret <= 0) {
		if (ret)
			pr_err("failed to initialize idle states\n");
		return ret ? : -ENODEV;
	}

	ret = cpuidle_register(drv, NULL);
	if (ret) {
		pr_err("failed to register cpuidle driver\n");
		return ret;
	}

	return 0;
}

device_initcall(sprd_cpuidle_init);
MODULE_DESCRIPTION("Sprd Cpuidle Driver");
MODULE_LICENSE("GPL v2");
