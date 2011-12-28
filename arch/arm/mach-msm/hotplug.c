/*
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/smp.h>

#include <asm/cacheflush.h>
#include <asm/vfp.h>

#include <mach/pm.h>

#include "qdss.h"
#include "spm.h"

extern volatile int pen_release;

struct msm_hotplug_device {
	struct completion cpu_killed;
	unsigned int warm_boot;
};

static DEFINE_PER_CPU_SHARED_ALIGNED(struct msm_hotplug_device,
			msm_hotplug_devices);

static inline void cpu_enter_lowpower(void)
{
	/* Just flush the cache. Changing the coherency is not yet
	 * available on msm. */
	flush_cache_all();
}

static inline void cpu_leave_lowpower(void)
{
}

static inline void platform_do_lowpower(unsigned int cpu)
{
	/* Just enter wfi for now. TODO: Properly shut off the cpu. */
	for (;;) {

		msm_pm_cpu_enter_lowpower(cpu);
		if (pen_release == cpu) {
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
		 */
		pr_debug("CPU%u: spurious wakeup call\n", cpu);
	}
}

int platform_cpu_kill(unsigned int cpu)
{
	struct completion *killed =
		&per_cpu(msm_hotplug_devices, cpu).cpu_killed;

	return wait_for_completion_timeout(killed, HZ * 5);
}

/*
 * platform-specific code to shutdown a CPU
 *
 * Called with IRQs disabled
 */
void platform_cpu_die(unsigned int cpu)
{
	if (unlikely(cpu != smp_processor_id())) {
		pr_crit("%s: running on %u, should be %u\n",
			__func__, smp_processor_id(), cpu);
		BUG();
	}
	complete(&__get_cpu_var(msm_hotplug_devices).cpu_killed);
	/*
	 * we're ready for shutdown now, so do it
	 */
	cpu_enter_lowpower();
	platform_do_lowpower(cpu);

	pr_notice("CPU%u: %s: normal wakeup\n", cpu, __func__);
	cpu_leave_lowpower();
}

int platform_cpu_disable(unsigned int cpu)
{
	/*
	 * we don't allow CPU 0 to be shutdown (it is still too special
	 * e.g. clock tick interrupts)
	 */
	return cpu == 0 ? -EPERM : 0;
}

int msm_platform_secondary_init(unsigned int cpu)
{
	int ret;
	struct msm_hotplug_device *dev = &__get_cpu_var(msm_hotplug_devices);

	if (!dev->warm_boot) {
		dev->warm_boot = 1;
		init_completion(&dev->cpu_killed);
		return 0;
	}
	msm_jtag_restore_state();
#ifdef CONFIG_VFP
	vfp_reinit();
#endif
	ret = msm_spm_set_low_power_mode(MSM_SPM_MODE_CLOCK_GATING, false);

	return ret;
}
