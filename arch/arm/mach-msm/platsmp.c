/*
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *  Copyright (c) 2010-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <soc/qcom/pm.h>
#include <soc/qcom/scm-boot.h>
#include <soc/qcom/cpu_pwr_ctl.h>

#include <asm/cacheflush.h>
#include <asm/cputype.h>
#include <asm/mach-types.h>
#include <asm/smp_plat.h>

#include <soc/qcom/socinfo.h>

#include "platsmp.h"
#include <asm/fixmap.h>

#define MSM_APCS_IDR  0x0B011030
/*
 * Write pen_release in a way that is guaranteed to be visible to all
 * observers, irrespective of whether they're taking part in coherency
 * or not.  This is necessary for the hotplug code to work reliably.
 */
void __cpuinit write_pen_release(int val)
{
	pen_release = val;
	smp_wmb();
	__cpuc_flush_dcache_area((void *)&pen_release, sizeof(pen_release));
	outer_clean_range(__pa(&pen_release), __pa(&pen_release + 1));
}

static DEFINE_SPINLOCK(boot_lock);

void __cpuinit msm_secondary_init(unsigned int cpu)
{
	WARN_ON(msm_platform_secondary_init(cpu));

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

static int __cpuinit release_secondary_sim(unsigned long base, unsigned int cpu)
{
	void *base_ptr = ioremap_nocache(base + (cpu * 0x10000), SZ_4K);
	if (!base_ptr)
		return -ENODEV;

	writel_relaxed(0x800, base_ptr+0x04);
	writel_relaxed(0x3FFF, base_ptr+0x14);

	mb();
	iounmap(base_ptr);
	return 0;
}

static int __cpuinit arm_release_secondary(unsigned long base, unsigned int cpu)
{
	void *base_ptr = ioremap_nocache(base + (cpu * 0x10000), SZ_4K);
	if (!base_ptr)
		return -ENODEV;

	writel_relaxed(0x00000033, base_ptr+0x04);
	mb();

	writel_relaxed(0x10000001, base_ptr+0x14);
	mb();
	udelay(2);

	writel_relaxed(0x00000031, base_ptr+0x04);
	mb();

	writel_relaxed(0x00000039, base_ptr+0x04);
	mb();
	udelay(2);

	writel_relaxed(0x00020038, base_ptr+0x04);
	mb();
	udelay(2);


	writel_relaxed(0x00020008, base_ptr+0x04);
	mb();

	writel_relaxed(0x00020088, base_ptr+0x04);
	mb();

	iounmap(base_ptr);
	return 0;
}

static int __cpuinit release_from_pen(unsigned int cpu)
{
	unsigned long timeout;

	/* Set preset_lpj to avoid subsequent lpj recalculations */
	preset_lpj = loops_per_jiffy;

	/*
	 * set synchronisation state between this boot processor
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
	arch_send_wakeup_ipi_mask(cpumask_of(cpu));

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

	return pen_release != -1 ? -ENOSYS : 0;
}

DEFINE_PER_CPU(int, cold_boot_done);

static int __cpuinit msm8909_boot_secondary(unsigned int cpu,
						struct task_struct *idle)
{
	pr_debug("Starting secondary CPU %d\n", cpu);

	if (per_cpu(cold_boot_done, cpu) == false) {
		if (of_board_is_sim())
			release_secondary_sim(0xb088000, cpu);
		else if (!of_board_is_rumi())
			arm_release_secondary(0xb088000, cpu);

		per_cpu(cold_boot_done, cpu) = true;
	}
	return release_from_pen(cpu);
}

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system.
 */
static void __init arm_smp_init_cpus(void)
{
	unsigned int i, ncores;
	void __iomem *base;

	set_fixmap_io(FIX_SMP_MEM_BASE, MSM_APCS_IDR & PAGE_MASK);
	base = (void __iomem *)__fix_to_virt(FIX_SMP_MEM_BASE);
	base += MSM_APCS_IDR & ~PAGE_MASK;

	ncores = (__raw_readl(base)) & 0xF;

	if (ncores > nr_cpu_ids) {
		pr_warn("SMP: %u cores greater than maximum (%u), clipping\n",
			ncores, nr_cpu_ids);
		ncores = nr_cpu_ids;
	}

	for (i = 0; i < nr_cpu_ids; i++)
		set_cpu_possible(i, true);

}

static int cold_boot_flags[] __initdata = {
	0,
	SCM_FLAG_COLDBOOT_CPU1,
	SCM_FLAG_COLDBOOT_CPU2,
	SCM_FLAG_COLDBOOT_CPU3,
};

static void __init msm_platform_smp_prepare_cpus_mc(unsigned int max_cpus)
{
	int cpu, map;
	u32 aff0_mask = 0;
	u32 aff1_mask = 0;
	u32 aff2_mask = 0;

	for_each_present_cpu(cpu) {
		map = cpu_logical_map(cpu);
		aff0_mask |= BIT(MPIDR_AFFINITY_LEVEL(map, 0));
		aff1_mask |= BIT(MPIDR_AFFINITY_LEVEL(map, 1));
		aff2_mask |= BIT(MPIDR_AFFINITY_LEVEL(map, 2));
	}

	if (scm_set_boot_addr_mc(virt_to_phys(msm_secondary_startup),
		aff0_mask, aff1_mask, aff2_mask, SCM_FLAG_COLDBOOT_MC))
		pr_warn("Failed to set CPU boot address\n");

	/* Mark CPU0 cold boot flag as done */
	per_cpu(cold_boot_done, 0) = true;
}

static void __init msm_platform_smp_prepare_cpus(unsigned int max_cpus)
{
	int cpu, map;
	unsigned int flags = 0;

	if (scm_is_mc_boot_available())
		return msm_platform_smp_prepare_cpus_mc(max_cpus);

	for_each_present_cpu(cpu) {
		map = cpu_logical_map(cpu);
		if (map >= ARRAY_SIZE(cold_boot_flags)) {
			set_cpu_present(cpu, false);
			__WARN();
			continue;
		}
		flags |= cold_boot_flags[map];
	}

	if (scm_set_boot_addr(virt_to_phys(msm_secondary_startup), flags))
		pr_warn("Failed to set CPU boot address\n");

	/* Mark CPU0 cold boot flag as done */
	per_cpu(cold_boot_done, 0) = true;
}

int  msm_cpu_disable(unsigned int cpu)
{
	return 0; /* support hotplugging any cpu */
}

struct smp_operations msm8909_smp_ops __initdata = {
	.smp_init_cpus = arm_smp_init_cpus,
	.smp_prepare_cpus = msm_platform_smp_prepare_cpus,
	.smp_secondary_init = msm_secondary_init,
	.smp_boot_secondary = msm8909_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_die = msm_cpu_die,
	.cpu_kill = msm_cpu_kill,
	.cpu_disable = msm_cpu_disable,
#endif
};
