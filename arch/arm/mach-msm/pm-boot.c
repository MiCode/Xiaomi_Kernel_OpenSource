/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <mach/msm_iomap.h>
#include <mach/socinfo.h>
#include <asm/mach-types.h>
#include <asm/sizes.h>
#include "scm-boot.h"
#include "idle.h"
#include "pm-boot.h"

static uint32_t *msm_pm_reset_vector;
static uint32_t saved_vector[2];
static void (*msm_pm_boot_before_pc)(unsigned int cpu, unsigned long entry);
static void (*msm_pm_boot_after_pc)(unsigned int cpu);

#ifdef CONFIG_MSM_SCM
static int __init msm_pm_tz_boot_init(void)
{
	int flag = 0;
	if (num_possible_cpus() == 1)
		flag = SCM_FLAG_WARMBOOT_CPU0;
	else if (num_possible_cpus() == 2)
		flag = SCM_FLAG_WARMBOOT_CPU0 | SCM_FLAG_WARMBOOT_CPU1;
	else if (num_possible_cpus() == 4)
		flag = SCM_FLAG_WARMBOOT_CPU0 | SCM_FLAG_WARMBOOT_CPU1 |
				SCM_FLAG_WARMBOOT_CPU2 | SCM_FLAG_WARMBOOT_CPU3;
	else
		__WARN();

	return scm_set_boot_addr((void *)virt_to_phys(msm_pm_boot_entry), flag);
}

static void msm_pm_config_tz_before_pc(unsigned int cpu,
		unsigned long entry)
{
	msm_pm_write_boot_vector(cpu, entry);
}
#else
static int __init msm_pm_tz_boot_init(void)
{
	return 0;
};

static inline void msm_pm_config_tz_before_pc(unsigned int cpu,
		unsigned long entry) {}
#endif

static int __init msm_pm_boot_reset_vector_init(uint32_t *reset_vector)
{
	if (!reset_vector)
		return -ENODEV;
	msm_pm_reset_vector = reset_vector;
	mb();

	return 0;
}

static void msm_pm_config_rst_vector_before_pc(unsigned int cpu,
		unsigned long entry)
{
	saved_vector[0] = msm_pm_reset_vector[0];
	saved_vector[1] = msm_pm_reset_vector[1];
	msm_pm_reset_vector[0] = 0xE51FF004; /* ldr pc, 4 */
	msm_pm_reset_vector[1] = entry;
}

static void msm_pm_config_rst_vector_after_pc(unsigned int cpu)
{
	msm_pm_reset_vector[0] = saved_vector[0];
	msm_pm_reset_vector[1] = saved_vector[1];
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
#define BOOT_REMAP_ENABLE  BIT(0)

int __init msm_pm_boot_init(struct msm_pm_boot_platform_data *pdata)
{
	int ret = 0;

	switch (pdata->mode) {
	case MSM_PM_BOOT_CONFIG_TZ:
		ret = msm_pm_tz_boot_init();
		msm_pm_boot_before_pc = msm_pm_config_tz_before_pc;
		msm_pm_boot_after_pc = NULL;
		break;
	case MSM_PM_BOOT_CONFIG_RESET_VECTOR_PHYS:
		pdata->v_addr = ioremap(pdata->p_addr, PAGE_SIZE);
		/* Fall through */
	case MSM_PM_BOOT_CONFIG_RESET_VECTOR_VIRT:

		if (!pdata->v_addr)
			return -ENODEV;

		ret = msm_pm_boot_reset_vector_init(pdata->v_addr);
		msm_pm_boot_before_pc
			= msm_pm_config_rst_vector_before_pc;
		msm_pm_boot_after_pc
			= msm_pm_config_rst_vector_after_pc;
		break;
	case MSM_PM_BOOT_CONFIG_REMAP_BOOT_ADDR:
		/*
		 * Set the boot remap address and enable remapping of
		 * reset vector
		 */
		if (!pdata->p_addr || !pdata->v_addr)
			return -ENODEV;

		__raw_writel((pdata->p_addr | BOOT_REMAP_ENABLE),
				pdata->v_addr);

		ret = msm_pm_boot_reset_vector_init(__va(pdata->p_addr));

		msm_pm_boot_before_pc
			= msm_pm_config_rst_vector_before_pc;
		msm_pm_boot_after_pc
			= msm_pm_config_rst_vector_after_pc;
		break;
	default:
		__WARN();
	}

	return ret;
}
