/*
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *  Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/smp.h>
#include <linux/cpu.h>
#include <linux/notifier.h>
#include <soc/qcom/spm.h>
#include <soc/qcom/pm.h>
#include <linux/irqchip/arm-gic.h>

#include <asm/smp_plat.h>
#include <asm/vfp.h>

#include <soc/qcom/jtag.h>

static cpumask_t cpu_dying_mask;

static DEFINE_PER_CPU(unsigned int, warm_boot_flag);

static inline void cpu_enter_lowpower(void)
{
}

static inline void cpu_leave_lowpower(void)
{
}

static inline void platform_do_lowpower(unsigned int cpu, int *spurious)
{
	/* Just enter wfi for now. TODO: Properly shut off the cpu. */
	for (;;) {

		lpm_cpu_hotplug_enter(cpu);
		if (pen_release == cpu_logical_map(cpu)) {
			/*
			 * OK, proper wakeup, we're done
			 */
			break;
		}

		/*
		 * getting here, means that we have come out of WFI without
		 * having been woken up - this shouldn't happen
		 *
		 * The trouble is, letting people know about this is not really
		 * possible, since we are currently running incoherently, and
		 * therefore cannot safely call printk() or anything else
		 * Read the pending interrupts to understand why we woke up
		 */
#ifdef CONFIG_MSM_PM
		gic_show_pending_irq();
#endif
		(*spurious)++;
	}
}

int msm_cpu_kill(unsigned int cpu)
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
void __ref msm_cpu_die(unsigned int cpu)
{
	int spurious = 0;

	if (unlikely(cpu != smp_processor_id())) {
		pr_crit("%s: running on %u, should be %u\n",
			__func__, smp_processor_id(), cpu);
		BUG();
	}
	/*
	 * we're ready for shutdown now, so do it
	 */
	cpu_enter_lowpower();
	platform_do_lowpower(cpu, &spurious);

	pr_debug("CPU%u: %s: normal wakeup\n", cpu, __func__);
	cpu_leave_lowpower();

	if (spurious)
		pr_warn("CPU%u: %u spurious wakeup calls\n", cpu, spurious);
}

static int hotplug_dying_callback(struct notifier_block *nfb,
				unsigned long action, void *hcpu)
{
	switch (action & (~CPU_TASKS_FROZEN)) {
	case CPU_DYING:
		cpumask_set_cpu((unsigned long)hcpu, &cpu_dying_mask);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}
static struct notifier_block hotplug_dying_notifier = {
	.notifier_call = hotplug_dying_callback,
};

int msm_platform_secondary_init(unsigned int cpu)
{
	int ret;
	unsigned int *warm_boot = &__get_cpu_var(warm_boot_flag);

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
#if defined(CONFIG_VFP) && defined (CONFIG_CPU_PM)
	vfp_pm_resume();
#endif
	ret = msm_spm_set_low_power_mode(MSM_SPM_MODE_CLOCK_GATING, false);

	return ret;
}

static int __init init_hotplug_dying(void)
{
	return register_hotcpu_notifier(&hotplug_dying_notifier);
}
early_initcall(init_hotplug_dying);
