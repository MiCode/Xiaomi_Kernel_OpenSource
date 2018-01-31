/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <asm/arch_timer.h>

#include <soc/qcom/rpmh.h>
#include <soc/qcom/system_pm.h>

#include <clocksource/arm_arch_timer.h>

#define PDC_TIME_VALID_SHIFT	31
#define PDC_TIME_UPPER_MASK	0xFFFFFF

static struct rpmh_client *rpmh_client;

static int setup_wakeup(uint32_t lo, uint32_t hi)
{
	struct tcs_cmd cmd[2] = { { 0 } };

	cmd[0].data =  hi & PDC_TIME_UPPER_MASK;
	cmd[0].data |= 1 << PDC_TIME_VALID_SHIFT;
	cmd[1].data = lo;

	return rpmh_write_control(rpmh_client, cmd, ARRAY_SIZE(cmd));
}

int system_sleep_update_wakeup(void)
{
	uint32_t lo = ~0U, hi = ~0U;

	/* Read the hardware to get the most accurate value */
	arch_timer_mem_get_cval(&lo, &hi);

	return setup_wakeup(lo, hi);
}
EXPORT_SYMBOL(system_sleep_update_wakeup);

/**
 * system_sleep_allowed() - Returns if its okay to enter system low power modes
 */
bool system_sleep_allowed(void)
{
	return (rpmh_ctrlr_idle(rpmh_client) == 0);
}
EXPORT_SYMBOL(system_sleep_allowed);

/**
 * system_sleep_enter() - Activties done when entering system low power modes
 *
 * Returns 0 for success or error values from writing the sleep/wake values to
 * the hardware block.
 */
int system_sleep_enter(void)
{
	if (IS_ERR_OR_NULL(rpmh_client))
		return -EFAULT;

	return rpmh_flush(rpmh_client);
}
EXPORT_SYMBOL(system_sleep_enter);

/**
 * system_sleep_exit() - Activities done when exiting system low power modes
 */
void system_sleep_exit(void)
{
}
EXPORT_SYMBOL(system_sleep_exit);

static int sys_pm_probe(struct platform_device *pdev)
{
	rpmh_client = rpmh_get_byindex(pdev, 0);
	if (IS_ERR_OR_NULL(rpmh_client))
		return PTR_ERR(rpmh_client);

	return 0;
}

static const struct of_device_id sys_pm_drv_match[] = {
	{ .compatible = "qcom,system-pm", },
	{ }
};

static struct platform_driver sys_pm_driver = {
	.probe = sys_pm_probe,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = sys_pm_drv_match,
	},
};
builtin_platform_driver(sys_pm_driver);
