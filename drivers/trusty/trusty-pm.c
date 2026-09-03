// SPDX-License-Identifier: GPL-2.0-only
/*
 * trusty-pm.c - Unisoc platform driver
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/syscore_ops.h>
#include <linux/sched/clock.h>
#include <linux/timekeeping.h>
#include <linux/trusty/smcall.h>
#include <asm/arch_timer.h>

#include "trusty.h"
#include <trace/hooks/psci.h>

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-trusty-pm: " fmt

/*
 * This call synchronizes linux boot time to trusty OS by calling two SMC
 * fastcalls. One call passes arm arch timer counter to trusty OS to estimate
 * the period(T1) in which a SMC fastcall is sent from linux to trusty OS.
 * The other one passed the linux boot time(T2).
 * Assume that anytime the period of the SMC fastcall is same. The linux boot
 * time when trusty OS gets the second fastcall should be T1+T2.
 */
static int trusty_sync_boot_time(void)
{
	int ret = 0;
	u64 boot_time;
	u64 cnt;

	/*
	 * Use arm generic virtual timer counter as reference counter
	 * comparing to physical timer counter used in trusty OS.
	 * The offset between virtual timer and physical timer will be ingored.
	 */
	cnt = __arch_counter_get_cntvct();
	ret = trusty_fast_call32(NULL, SMC_FC_SYNC_TIMER_CNT,
				 (u32)cnt,  (u32)(cnt >> 32), 0);
	if (ret) {
		pr_err("Trusty fastcall SMC_FC_SYNC_TIMER_CNT failed(%d)\n",
		       ret);
		return ret;
	}

	/*
	 * Get linux boot time that includes system suspend time
	 */
	boot_time = ktime_get_boot_fast_ns();
	ret = trusty_fast_call32(NULL, SMC_FC_SYNC_BOOT_TIME,
				 (u32)boot_time, (u32)(boot_time >> 32), 0);
	if (ret) {
		pr_err("Trusty fastcall SMC_FC_SYNC_BOOT_TIME failed(%d)\n",
		       ret);
	}

	return ret;
}

static void trusty_pm_resume(void)
{
	trusty_sync_boot_time();
}

static struct syscore_ops trusty_pm_ops = {
	.resume = trusty_pm_resume,
};

static void trusty_resident_on_cpu(void *data, int cpu, bool *resident)
{
	*resident = !trusty_fast_call32(NULL, SMC_FC_CPU_CAN_DOWN, cpu, 0, 0);
}

static void trusty_check_cpu_suspend(void *data, u32 state, bool *deny)
{
	trusty_resident_on_cpu(NULL, smp_processor_id(), deny);
}

static int __init trusty_pm_init(void)
{
	/* Fist time sync on boot up */
	trusty_sync_boot_time();

	register_syscore_ops(&trusty_pm_ops);

	/* vendor hooks cannot be unregistered */
	register_trace_android_rvh_psci_tos_resident_on(trusty_resident_on_cpu, NULL);
	register_trace_android_rvh_psci_cpu_suspend(trusty_check_cpu_suspend, NULL);

	return 0;
}

static void __exit trusty_pm_exit(void)
{
	unregister_syscore_ops(&trusty_pm_ops);
}

module_init(trusty_pm_init);
module_exit(trusty_pm_exit);

MODULE_DESCRIPTION("Sprd trusty pm driver");
MODULE_LICENSE("GPL v2");
