/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 * Copyright (c) 2013 ARM Ltd.
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

/* MSM ARMv8 CPU Operations
 * Based on arch/arm64/kernel/smp_spin_table.c
 */

#include <linux/bitops.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/smp.h>

#include <soc/qcom/cpu_pwr_ctl.h>
#include <soc/qcom/scm-boot.h>
#include <soc/qcom/socinfo.h>
#include <soc/qcom/pm.h>
#include <soc/qcom/spm.h>
#include <soc/qcom/jtag.h>

#include <asm/barrier.h>
#include <asm/cacheflush.h>
#include <asm/cpu_ops.h>
#include <asm/cputype.h>
#include <asm/smp_plat.h>

static DEFINE_RAW_SPINLOCK(boot_lock);

DEFINE_PER_CPU(int, cold_boot_done);

static int cold_boot_flags[] = {
	0,
	SCM_FLAG_COLDBOOT_CPU1,
	SCM_FLAG_COLDBOOT_CPU2,
	SCM_FLAG_COLDBOOT_CPU3,
};

static void write_pen_release(u64 val)
{
	void *start = (void *)&secondary_holding_pen_release;
	unsigned long size = sizeof(secondary_holding_pen_release);

	secondary_holding_pen_release = val;
	smp_wmb();
	__flush_dcache_area(start, size);
}

static int secondary_pen_release(unsigned int cpu)
{
	unsigned long timeout;

	/*
	 * Set synchronisation state between this boot processor
	 * and the secondary one
	 */
	raw_spin_lock(&boot_lock);
	write_pen_release(cpu_logical_map(cpu));

	/*
	 * Wake-up cpu with am IPI
	 */
	arch_send_wakeup_ipi_mask(cpumask_of(cpu));

	timeout = jiffies + (1 * HZ);
	while (time_before(jiffies, timeout)) {
		if (secondary_holding_pen_release == INVALID_HWID)
			break;
		udelay(10);
	}
	raw_spin_unlock(&boot_lock);

	return secondary_holding_pen_release != INVALID_HWID ? -ENOSYS : 0;
}

static int __init msm_cpu_init(struct device_node *dn, unsigned int cpu)
{
	/*Nothing to do here but needed to keep framework happy */
	return 0;
}

static int __init msm_cpu_prepare(unsigned int cpu)
{
	u64 mpidr_el1 = cpu_logical_map(cpu);

	if (scm_is_mc_boot_available()) {

		if (mpidr_el1 & ~MPIDR_HWID_BITMASK) {
			pr_err("CPU%d:Failed to set boot address\n", cpu);
			return -ENOSYS;
		}

		if (scm_set_boot_addr_mc(virt_to_phys(secondary_holding_pen),
				BIT(MPIDR_AFFINITY_LEVEL(mpidr_el1, 0)),
				BIT(MPIDR_AFFINITY_LEVEL(mpidr_el1, 1)),
				BIT(MPIDR_AFFINITY_LEVEL(mpidr_el1, 2)),
				SCM_FLAG_COLDBOOT_MC)) {
			pr_warn("CPU%d:Failed to set boot address\n", cpu);
			return -ENOSYS;
		}

	} else {
		if (scm_set_boot_addr(virt_to_phys(secondary_holding_pen),
			cold_boot_flags[cpu])) {
			pr_warn("Failed to set CPU %u boot address\n", cpu);
			return -ENOSYS;
		}
	}

	return 0;
}

static int msm_cpu_boot(unsigned int cpu)
{
	int ret = 0;

	if (per_cpu(cold_boot_done, cpu) == false) {
		if (of_board_is_sim()) {
			ret = msm_unclamp_secondary_arm_cpu_sim(cpu);
			if (ret)
				return ret;
		} else {
			ret = msm_unclamp_secondary_arm_cpu(cpu);
			if (ret)
				return ret;
		}
		per_cpu(cold_boot_done, cpu) = true;
	}
	return secondary_pen_release(cpu);
}

static int msm8994_cpu_boot(unsigned int cpu)
{
	int ret = 0;

	if (per_cpu(cold_boot_done, cpu) == false) {
		if (of_board_is_sim()) {
			ret = msm_unclamp_secondary_arm_cpu_sim(cpu);
			if (ret)
				return ret;
		} else {
			ret = msm8994_unclamp_secondary_arm_cpu(cpu);
			if (ret)
				return ret;
		}
		per_cpu(cold_boot_done, cpu) = true;
	}
	return secondary_pen_release(cpu);
}

void msm_cpu_postboot(void)
{
	msm_jtag_restore_state();
	/*
	 * Let the primary processor know we're out of the pen.
	 */
	write_pen_release(INVALID_HWID);

	msm_spm_set_low_power_mode(MSM_SPM_MODE_CLOCK_GATING, false);

	/*
	 * Synchronise with the boot thread.
	 */
	raw_spin_lock(&boot_lock);
	raw_spin_unlock(&boot_lock);
}

#ifdef CONFIG_HOTPLUG_CPU
static void msm_wfi_cpu_die(unsigned int cpu)
{
	if (unlikely(cpu != smp_processor_id())) {
		pr_crit("%s: running on %u, should be %u\n",
			__func__, smp_processor_id(), cpu);
		BUG();
	}
	for (;;) {
		lpm_cpu_hotplug_enter(cpu);
		if (secondary_holding_pen_release == cpu_logical_map(cpu)) {
			/*Proper wake up */
			break;
		}
		pr_debug("CPU%u: spurious wakeup call\n", cpu);
		BUG();
	}
}
#endif

static const struct cpu_operations msm_cortex_a_ops = {
	.name		= "qcom,arm-cortex-acc",
	.cpu_init	= msm_cpu_init,
	.cpu_prepare	= msm_cpu_prepare,
	.cpu_boot	= msm_cpu_boot,
	.cpu_postboot	= msm_cpu_postboot,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_die        = msm_wfi_cpu_die,
#endif
	.cpu_suspend       = msm_pm_collapse,
};
CPU_METHOD_OF_DECLARE(msm_cortex_a_ops, &msm_cortex_a_ops);

static const struct cpu_operations msm8994_cortex_a_ops = {
	.name		= "qcom,8994-arm-cortex-acc",
	.cpu_init	= msm_cpu_init,
	.cpu_prepare	= msm_cpu_prepare,
	.cpu_boot	= msm8994_cpu_boot,
	.cpu_postboot	= msm_cpu_postboot,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_die        = msm_wfi_cpu_die,
#endif
	.cpu_suspend       = msm_pm_collapse,
};
CPU_METHOD_OF_DECLARE(msm8994_cortex_a_ops, &msm8994_cortex_a_ops);
