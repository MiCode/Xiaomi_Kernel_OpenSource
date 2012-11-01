/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/smp.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include <asm/cacheflush.h>
#include <asm/smp_plat.h>
#include <asm/hardware/gic.h>
#include <mach/msm_iomap.h>
#include "pm.h"

#define BOOT_REMAP_ENABLE 0x01

/*
 * control for which core is the next to come out of the secondary
 * boot "holding pen"
 */
volatile int pen_release = -1;

/*
 * Write pen_release in a way that is guaranteed to be visible to all
 * observers, irrespective of whether they're taking part in coherency
 * or not.  This is necessary for the hotplug code to work reliably.
 */
static void __cpuinit write_pen_release(int val)
{
	pen_release = val;
	smp_wmb();
	__cpuc_flush_dcache_area((void *)&pen_release, sizeof(pen_release));
	outer_clean_range(__pa(&pen_release), __pa(&pen_release + 1));
}

static DEFINE_SPINLOCK(boot_lock);

void __cpuinit platform_secondary_init(unsigned int cpu)
{
	WARN_ON(msm_platform_secondary_init(cpu));

	/*
	 * if any interrupts are already enabled for the primary
	 * core (e.g. timer irq), then they will not have been enabled
	 * for us: do so
	 */
	gic_secondary_init(0);

	/*
	 * let the primary processor know we're out of the
	 * pen, then head off into the C entry point
	 */
	write_pen_release(-1);

	/*
	 * Synchronise with the boot thread.
	 */
	spin_lock(&boot_lock);
	spin_unlock(&boot_lock);
}

static int  __cpuinit release_secondary_sim(unsigned long base, int cpu)
{
	void *base_ptr;

	base_ptr = ioremap_nocache(base + (cpu * 0x10000), SZ_4K);
	if (!base_ptr) {
		pr_err("Failed to release core %u\n", cpu);
		return -ENODEV;
	}

	writel_relaxed(0x800, base_ptr+0x04);
	writel_relaxed(0x3FFF, base_ptr+0x14);
	mb();

	return 0;
}

DEFINE_PER_CPU(int, cold_boot_done);

int __cpuinit boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	unsigned long timeout;

	preset_lpj = loops_per_jiffy;

	if (per_cpu(cold_boot_done, cpu) == false) {
		release_secondary_sim(0xF9088000, cpu);
		per_cpu(cold_boot_done, cpu) = true;
	}

	/*
	 * Set synchronisation state between this boot processor
	 * and the secondary one
	 */
	spin_lock(&boot_lock);

	/*
	 * The secondary processor is waiting to be released from
	 * the holding pen - release it, then wait for it to flag
	 * that it has been released by resetting pen_release.
	 *
	 * Note that "pen_release" is the hardware CPU ID, whereas
	 * "cpu" is Linux's internal ID.
	 */
	write_pen_release(cpu_logical_map(cpu));

	/*
	 * Send the secondary CPU a soft interrupt, thereby causing
	 * the boot monitor to read the system wide flags register,
	 * and branch to the address found there.
	 */

	gic_raise_softirq(cpumask_of(cpu), 1);

	timeout = jiffies + (1 * HZ);
	while (time_before(jiffies, timeout)) {
		smp_rmb();
		if (pen_release == -1)
			break;

		udelay(10);
	}

	/*
	 * now the secondary core is starting up let it run its
	 * calibrations, then wait for it to finish
	 */
	spin_unlock(&boot_lock);

	return 0;
}

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system
 */
void __init smp_init_cpus(void)
{
	unsigned int i, ncores;

	ncores = (__raw_readl(MSM_APCS_GCC_BASE + 0x30)) & 0xF;

	for (i = 0; i < ncores; i++)
		set_cpu_possible(i, true);

	set_smp_cross_call(gic_raise_softirq);
}

void __init platform_smp_prepare_cpus(unsigned int max_cpus)
{
	int i;
	void __iomem *remap_ptr;

	/*
	 * Initialise the present map, which describes the set of CPUs
	 * actually populated at the present time
	 */
	for (i = 0; i < max_cpus; i++)
		set_cpu_present(i, true);

	/*
	 * Enable boot remapping and write the address of secondary
	 * startup into boot remapper register
	 */
	remap_ptr = ioremap_nocache(0xF9010000, SZ_4K); /* APCS_CFG_SECURE */
	if (!remap_ptr) {
		pr_err("Failed to ioremap for secondary cores\n");
		return;
	}

	__raw_writel((virt_to_phys(msm_secondary_startup)|BOOT_REMAP_ENABLE),
			(remap_ptr + 0x4));
	mb();
	iounmap(remap_ptr);
}
