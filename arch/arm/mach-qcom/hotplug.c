/*
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *  Copyright (c) 2011-2014, 2016, 2018, The Linux Foundation.
 *  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/smp.h>
#include <linux/cpu.h>
#include <soc/qcom/spm.h>
#include <soc/qcom/pm-legacy.h>
#include <asm/smp_plat.h>
#include "platsmp.h"
#include <soc/qcom/jtag.h>

static cpumask_t cpu_dying_mask;
static DEFINE_PER_CPU(unsigned int, warm_boot_flag);

static inline void cpu_enter_lowpower(void)
{
}

static inline void cpu_leave_lowpower(void)
{
}

static inline void platform_do_lowpower(unsigned int cpu)
{
	lpm_cpu_hotplug_enter(cpu);
	/*
	 * getting here, means that we have come out of low power mode
	 * without having been woken up - this shouldn't happen
	 *
	 */
	pr_err("%s: CPU%u has failed to Hotplug\n", __func__, cpu);
}

int qcom_cpu_kill_legacy(unsigned int cpu)
{
	int ret = 0;

	if (cpumask_test_and_clear_cpu(cpu, &cpu_dying_mask))
		ret = msm_pm_wait_cpu_shutdown(cpu);

	return ret ? 0 : 1;
}

/*
 * platform-specific code to shutdown a CPU
 *
 * Called with IRQs disabled
 */
void __ref qcom_cpu_die_legacy(unsigned int cpu)
{
	if (unlikely(cpu != smp_processor_id())) {
		pr_crit("%s: running on %u, should be %u\n",
			__func__, smp_processor_id(), cpu);
		WARN_ON(cpu);
	}
	/*
	 * we're ready for shutdown now, so do it
	 */
	cpu_enter_lowpower();
	platform_do_lowpower(cpu);

	pr_debug("CPU%u: %s: normal wakeup\n", cpu, __func__);
	cpu_leave_lowpower();
}

int msm_platform_secondary_init(unsigned int cpu)
{
	int ret;
	unsigned int *warm_boot = this_cpu_ptr(&warm_boot_flag);

	if (!(*warm_boot)) {
		*warm_boot = 1;
		/*
		 * All CPU0 boots are considered warm boots (restore needed)
		 * since CPU0 is the system boot CPU and never cold-booted
		 * by the kernel.
		 */
		if (cpu)
			return 0;
	}
	msm_jtag_restore_state();
	ret = msm_spm_set_low_power_mode(MSM_SPM_MODE_CLOCK_GATING, false);

	return ret;
}

static int hotplug_dying_cpu(unsigned int cpu)
{
	cpumask_set_cpu(cpu, &cpu_dying_mask);
	return 0;
}

static int __init init_hotplug_dying(void)
{
	cpuhp_setup_state(CPUHP_AP_QCOM_SLEEP_STARTING,
		 "AP_QCOM_HOTPLUG_STARTING", NULL, hotplug_dying_cpu);

	return 0;
}
early_initcall(init_hotplug_dying);
