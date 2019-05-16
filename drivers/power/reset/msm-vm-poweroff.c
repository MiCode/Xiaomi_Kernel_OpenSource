/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/of_address.h>

#include <asm/system_misc.h>
#include <soc/qcom/watchdog.h>

static int in_panic;
static void (*old_pm_restart)(enum reboot_mode reboot_mode, const char *cmd);

static int panic_prep_restart(struct notifier_block *this,
			      unsigned long event, void *ptr)
{
	in_panic = 1;
	return NOTIFY_DONE;
}

static struct notifier_block panic_blk = {
	.notifier_call	= panic_prep_restart,
};

static void do_vm_restart(enum reboot_mode reboot_mode, const char *cmd)
{
	pr_notice("Going down for vm restart now\n");

	if (in_panic)
		msm_trigger_wdog_bite();

	if (old_pm_restart)
		old_pm_restart(reboot_mode, cmd);
}

static int vm_restart_probe(struct platform_device *pdev)
{
	atomic_notifier_chain_register(&panic_notifier_list, &panic_blk);

	old_pm_restart = arm_pm_restart;
	arm_pm_restart = do_vm_restart;

	return 0;
}

static const struct of_device_id of_vm_restart_match[] = {
	{ .compatible = "qcom,vm-restart", },
	{},
};
MODULE_DEVICE_TABLE(of, of_vm_restart_match);

static struct platform_driver vm_restart_driver = {
	.probe = vm_restart_probe,
	.driver = {
		.name = "msm-vm-restart",
		.of_match_table = of_match_ptr(of_vm_restart_match),
	},
};

static int __init vm_restart_init(void)
{
	return platform_driver_register(&vm_restart_driver);
}
pure_initcall(vm_restart_init);
