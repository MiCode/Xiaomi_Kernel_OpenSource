/*
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *  Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>

#include <asm/hardware/gic.h>
#include <asm/cacheflush.h>
#include <asm/cputype.h>
#include <asm/mach-types.h>

#include <mach/socinfo.h>
#include <mach/smp.h>
#include <mach/hardware.h>
#include <mach/msm_iomap.h>

#include "pm.h"
#include "scm-boot.h"
#include "spm.h"

int pen_release = -1;

/* Initialize the present map (cpu_set(i, cpu_present_map)). */
void __init platform_smp_prepare_cpus(unsigned int max_cpus)
{
	int i;

	for (i = 0; i < max_cpus; i++)
		cpu_set(i, cpu_present_map);
}

void __init smp_init_cpus(void)
{
	unsigned int i, ncores = get_core_count();

	for (i = 0; i < ncores; i++)
		cpu_set(i, cpu_possible_map);

	set_smp_cross_call(gic_raise_softirq);
}

static int __cpuinit scorpion_release_secondary(void)
{
	void *base_ptr = ioremap_nocache(0x00902000, SZ_4K*2);
	if (!base_ptr)
		return -EINVAL;

	writel_relaxed(0x0, base_ptr+0x15A0);
	dmb();
	writel_relaxed(0x0, base_ptr+0xD80);
	writel_relaxed(0x3, base_ptr+0xE64);
	mb();
	iounmap(base_ptr);

	return 0;
}

static int __cpuinit krait_release_secondary_sim(int cpu)
{
	void *base_ptr = ioremap_nocache(0x02088000 + (cpu * 0x10000), SZ_4K);
	if (!base_ptr)
		return -ENODEV;

	if (machine_is_msm8960_sim() || machine_is_msm8960_rumi3()) {
		writel_relaxed(0x10, base_ptr+0x04);
		writel_relaxed(0x80, base_ptr+0x04);
	}

	if (machine_is_apq8064_sim())
		writel_relaxed(0xf0000, base_ptr+0x04);

	mb();
	iounmap(base_ptr);
	return 0;
}

static int __cpuinit krait_release_secondary(int cpu)
{
	void *base_ptr = ioremap_nocache(0x02088000 + (cpu * 0x10000), SZ_4K);
	if (!base_ptr)
		return -ENODEV;

	msm_spm_turn_on_cpu_rail(cpu);

	writel_relaxed(0x109, base_ptr+0x04);
	writel_relaxed(0x101, base_ptr+0x04);
	ndelay(300);

	writel_relaxed(0x121, base_ptr+0x04);
	udelay(2);

	writel_relaxed(0x020, base_ptr+0x04);
	udelay(2);

	writel_relaxed(0x000, base_ptr+0x04);
	udelay(100);

	writel_relaxed(0x080, base_ptr+0x04);
	mb();
	iounmap(base_ptr);
	return 0;
}

static int __cpuinit release_secondary(unsigned int cpu)
{
	BUG_ON(cpu >= get_core_count());

	if (cpu_is_msm8x60())
		return scorpion_release_secondary();

	if (machine_is_msm8960_sim() || machine_is_msm8960_rumi3() ||
	    machine_is_apq8064_sim())
		return krait_release_secondary_sim(cpu);

	if (cpu_is_msm8960() || cpu_is_msm8930() || cpu_is_apq8064())
		return krait_release_secondary(cpu);

	WARN(1, "unknown CPU case in release_secondary\n");
	return -EINVAL;
}

DEFINE_PER_CPU(int, cold_boot_done);
static int cold_boot_flags[] = {
	0,
	SCM_FLAG_COLDBOOT_CPU1,
	SCM_FLAG_COLDBOOT_CPU2,
	SCM_FLAG_COLDBOOT_CPU3,
};

/* Executed by primary CPU, brings other CPUs out of reset. Called at boot
   as well as when a CPU is coming out of shutdown induced by echo 0 >
   /sys/devices/.../cpuX.
*/
int __cpuinit boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	int cnt = 0;
	int ret;
	int flag = 0;

	pr_debug("Starting secondary CPU %d\n", cpu);

	/* Set preset_lpj to avoid subsequent lpj recalculations */
	preset_lpj = loops_per_jiffy;

	if (cpu > 0 && cpu < ARRAY_SIZE(cold_boot_flags))
		flag = cold_boot_flags[cpu];
	else
		__WARN();

	if (per_cpu(cold_boot_done, cpu) == false) {
		ret = scm_set_boot_addr((void *)
					virt_to_phys(msm_secondary_startup),
					flag);
		if (ret == 0)
			release_secondary(cpu);
		else
			printk(KERN_DEBUG "Failed to set secondary core boot "
					  "address\n");
		per_cpu(cold_boot_done, cpu) = true;
	}

	pen_release = cpu;
	dmac_flush_range((void *)&pen_release,
			 (void *)(&pen_release + sizeof(pen_release)));
	__asm__("sev");
	mb();

	/* Use smp_cross_call() to send a soft interrupt to wake up
	 * the other core.
	 */
	gic_raise_softirq(cpumask_of(cpu), 1);

	while (pen_release != 0xFFFFFFFF) {
		dmac_inv_range((void *)&pen_release,
			       (void *)(&pen_release+sizeof(pen_release)));
		usleep(500);
		if (cnt++ >= 10)
			break;
	}

	return 0;
}

/* Initialization routine for secondary CPUs after they are brought out of
 * reset.
*/
void __cpuinit platform_secondary_init(unsigned int cpu)
{
	pr_debug("CPU%u: Booted secondary processor\n", cpu);

	WARN_ON(msm_platform_secondary_init(cpu));

	trace_hardirqs_off();

	/* Edge trigger PPIs except AVS_SVICINT and AVS_SVICINTSWDONE */
	writel(0xFFFFD7FF, MSM_QGIC_DIST_BASE + GIC_DIST_CONFIG + 4);

	/* RUMI does not adhere to GIC spec by enabling STIs by default.
	 * Enable/clear is supposed to be RO for STIs, but is RW on RUMI.
	 */
	if (!machine_is_msm8x60_sim())
		writel(0x0000FFFF, MSM_QGIC_DIST_BASE + GIC_DIST_ENABLE_SET);

	gic_secondary_init(0);
}
