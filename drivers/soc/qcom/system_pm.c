/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#include <soc/qcom/rpmh.h>

#define PDC_TIME_VALID_SHIFT	31
#define PDC_TIME_UPPER_MASK	0xFFFFFF

static struct rpmh_client *rpmh_client;

static int setup_wakeup(uint64_t sleep_val)
{
	struct tcs_cmd cmd[3] = { { 0 } };

	cmd[0].data = sleep_val & 0xFFFFFFFF;
	cmd[1].data = (sleep_val >> 32) & PDC_TIME_UPPER_MASK;
	cmd[1].data |= 1 << PDC_TIME_VALID_SHIFT;

	return rpmh_write_control(rpmh_client, cmd, ARRAY_SIZE(cmd));
}

/**
 * system_sleep_enter() - Activties done when entering system low power modes
 *
 * @sleep_val: The qtimer value for the next wakeup time
 *
 * Returns 0 for success or error values from writing the timer value in the
 * hardware block.
 */
int system_sleep_enter(uint64_t sleep_val)
{
	int ret;

	if (IS_ERR_OR_NULL(rpmh_client))
		return -EFAULT;

	ret = rpmh_flush(rpmh_client);
	if (ret)
		return ret;

	return setup_wakeup(sleep_val);
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
