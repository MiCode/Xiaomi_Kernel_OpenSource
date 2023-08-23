/* Copyright (c) 2011-2014, 2016, 2018-2019, The Linux Foundation.
 * All rights reserved.
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
#include <linux/init.h>
#include <soc/qcom/scm-boot.h>
#include <asm/cacheflush.h>
#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include "pm-boot.h"
#include "idle.h"

#define CPU_INDEX(cluster, cpu) (cluster * MAX_CPUS_PER_CLUSTER + cpu)

static void (*msm_pm_boot_before_pc)(unsigned int cpu, unsigned long entry);
static void (*msm_pm_boot_after_pc)(unsigned int cpu);

static int msm_pm_tz_boot_init(void)
{
	int ret;
	phys_addr_t warmboot_addr = virt_to_phys(msm_pm_boot_entry);

	if (scm_is_mc_boot_available())
		ret = scm_set_warm_boot_addr_mc_for_all(warmboot_addr);
	else {
		unsigned int flag = 0;

		if (num_possible_cpus() == 1)
			flag = SCM_FLAG_WARMBOOT_CPU0;
		else if (num_possible_cpus() == 2)
			flag = SCM_FLAG_WARMBOOT_CPU0 | SCM_FLAG_WARMBOOT_CPU1;
		else if (num_possible_cpus() == 4)
			flag = SCM_FLAG_WARMBOOT_CPU0 | SCM_FLAG_WARMBOOT_CPU1 |
				SCM_FLAG_WARMBOOT_CPU2 | SCM_FLAG_WARMBOOT_CPU3;
		else
			pr_warn("%s: set warmboot address failed\n",
								__func__);

		ret = scm_set_boot_addr(virt_to_phys(msm_pm_boot_entry), flag);
	}
	return ret;
}
static void msm_pm_write_boot_vector(unsigned int cpu, unsigned long address)
{
	uint32_t clust_id = MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 1);
	uint32_t cpu_id = MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 0);
	unsigned long *start_address;
	unsigned long *end_address;

	if (clust_id >= MAX_NUM_CLUSTER || cpu_id >= MAX_CPUS_PER_CLUSTER)
		WARN_ON(cpu);

	msm_pm_boot_vector[CPU_INDEX(clust_id, cpu_id)] = address;
	start_address = &msm_pm_boot_vector[CPU_INDEX(clust_id, cpu_id)];
	end_address = &msm_pm_boot_vector[CPU_INDEX(clust_id, cpu_id + 1)];
	dmac_clean_range((void *)start_address, (void *)end_address);
}

static void msm_pm_config_tz_before_pc(unsigned int cpu,
		unsigned long entry)
{
	msm_pm_write_boot_vector(cpu, entry);
}

void msm_pm_boot_config_before_pc(unsigned int cpu, unsigned long entry)
{
	if (msm_pm_boot_before_pc)
		msm_pm_boot_before_pc(cpu, entry);
}

void msm_pm_boot_config_after_pc(unsigned int cpu)
{
	if (msm_pm_boot_after_pc)
		msm_pm_boot_after_pc(cpu);
}

static int __init msm_pm_boot_init(void)
{
	int ret = 0;

	ret = msm_pm_tz_boot_init();
	msm_pm_boot_before_pc = msm_pm_config_tz_before_pc;
	msm_pm_boot_after_pc = NULL;

	return ret;
}
postcore_initcall(msm_pm_boot_init);
