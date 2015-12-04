/*
 *  Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/cpu.h>
#include <linux/notifier.h>
#include <linux/msm_rtb.h>

#define CPU_SHIFT	0
#define CPU_MASK	0xF
#define CPU_OF(n)	(((n) & CPU_MASK) << CPU_SHIFT)
#define CPUSET_SHIFT	4
#define CPUSET_MASK	0xFFFF
#define CPUSET_OF(n)	(((n) & CPUSET_MASK) << CPUSET_SHIFT)

static int hotplug_rtb_callback(struct notifier_block *nfb,
				unsigned long action, void *hcpu)
{
	/*
	 * Bits [19:4] of the data are the online mask, lower 4 bits are the
	 * cpu number that is being changed. Additionally, changes to the
	 * online_mask that will be done by the current hotplug will be made
	 * even though they aren't necessarily in the online mask yet.
	 *
	 * XXX: This design is limited to supporting at most 16 cpus
	 */
	unsigned long this_cpumask = CPUSET_OF(1 << (unsigned long)hcpu);
	unsigned long cpumask = CPUSET_OF(cpumask_bits(cpu_online_mask)[0]);
	unsigned long cpudata = CPU_OF((unsigned long)hcpu) | cpumask;

	switch (action & (~CPU_TASKS_FROZEN)) {
	case CPU_STARTING:
		uncached_logk(LOGK_HOTPLUG, (void *)(cpudata | this_cpumask));
		break;
	case CPU_DYING:
		uncached_logk(LOGK_HOTPLUG, (void *)(cpudata & ~this_cpumask));
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}
static struct notifier_block hotplug_rtb_notifier = {
	.notifier_call = hotplug_rtb_callback,
};

static int __init init_hotplug_rtb(void)
{
	return register_hotcpu_notifier(&hotplug_rtb_notifier);
}
early_initcall(init_hotplug_rtb);
