/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/reboot.h>
#include <linux/pm.h>
#include <asm/system_misc.h>
#include <mach/proc_comm.h>

#include "devices-msm7x2xa.h"
#include "smd_rpcrouter.h"

static uint32_t restart_reason = 0x776655AA;

static void msm_pm_power_off(void)
{
	msm_rpcrouter_close();
	msm_proc_comm(PCOM_POWER_DOWN, 0, 0);
	for (;;)
		;
}

static void msm_pm_restart(char str, const char *cmd)
{
	msm_rpcrouter_close();
	pr_debug("The reset reason is %x\n", restart_reason);

	/* Disable interrupts */
	local_irq_disable();
	local_fiq_disable();

	/*
	 * Take out a flat memory mapping  and will
	 * insert a 1:1 mapping in place of
	 * the user-mode pages to ensure predictable results
	 * This function takes care of flushing the caches
	 * and flushing the TLB.
	 */
	setup_mm_for_reboot();

	msm_proc_comm(PCOM_RESET_CHIP, &restart_reason, 0);

	for (;;)
		;
}

static int msm_reboot_call
	(struct notifier_block *this, unsigned long code, void *_cmd)
{
	if ((code == SYS_RESTART) && _cmd) {
		char *cmd = _cmd;
		if (!strncmp(cmd, "bootloader", 10)) {
			restart_reason = 0x77665500;
		} else if (!strncmp(cmd, "recovery", 8)) {
			restart_reason = 0x77665502;
		} else if (!strncmp(cmd, "eraseflash", 10)) {
			restart_reason = 0x776655EF;
		} else if (!strncmp(cmd, "oem-", 4)) {
			unsigned long code;
			int res;
			res = kstrtoul(cmd + 4, 16, &code);
			code &= 0xff;
			restart_reason = 0x6f656d00 | code;
		} else {
			restart_reason = 0x77665501;
		}
	}
	return NOTIFY_DONE;
}

static struct notifier_block msm_reboot_notifier = {
	.notifier_call = msm_reboot_call,
};

static int __init msm_pm_restart_init(void)
{
	int ret;

	pm_power_off = msm_pm_power_off;
	arm_pm_restart = msm_pm_restart;

	ret = register_reboot_notifier(&msm_reboot_notifier);
	if (ret)
		pr_err("Failed to register reboot notifier\n");

	return ret;
}
late_initcall(msm_pm_restart_init);
