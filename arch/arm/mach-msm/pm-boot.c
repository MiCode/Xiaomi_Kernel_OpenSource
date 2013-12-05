/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <mach/msm_iomap.h>
#include <mach/socinfo.h>
#include <asm/sizes.h>
#include "scm-boot.h"
#include "idle.h"
#include "pm-boot.h"

static uint32_t *msm_pm_reset_vector;
static uint32_t saved_vector[2];
static void (*msm_pm_boot_before_pc)(unsigned int cpu, unsigned long entry);
static void (*msm_pm_boot_after_pc)(unsigned int cpu);


static int msm_pm_get_boot_config_mode(struct device_node *dev,
		const char *key, uint32_t *boot_config_mode)
{
	struct pm_lookup_table {
		uint32_t boot_config_mode;
		const char *boot_config_name;
	};
	const char *boot_config_str;
	struct pm_lookup_table boot_config_lookup[] = {
		{MSM_PM_BOOT_CONFIG_TZ, "tz"},
		{MSM_PM_BOOT_CONFIG_RESET_VECTOR_PHYS, "reset_vector_phys"},
		{MSM_PM_BOOT_CONFIG_RESET_VECTOR_VIRT, "reset_vector_virt"},
	};
	int ret;
	int i;

	ret = of_property_read_string(dev, key, &boot_config_str);
	if (!ret) {
		ret = -EINVAL;
		for (i = 0; i < ARRAY_SIZE(boot_config_lookup); i++) {
			if (!strncmp(boot_config_str,
				boot_config_lookup[i].boot_config_name,
				strlen(boot_config_lookup[i].boot_config_name))
			) {
				*boot_config_mode =
					boot_config_lookup[i].boot_config_mode;
				ret = 0;
				break;
			}
		}
	}
	return ret;
}

static void msm_pm_write_boot_vector(unsigned int cpu, unsigned long address)
{
	msm_pm_boot_vector[cpu] = address;
	clean_caches((unsigned long)&msm_pm_boot_vector[cpu],
		     sizeof(msm_pm_boot_vector[cpu]),
		     virt_to_phys(&msm_pm_boot_vector[cpu]));
}

#ifdef CONFIG_MSM_SCM
static int msm_pm_tz_boot_init(void)
{
	unsigned int flag = 0;
	if (num_possible_cpus() == 1)
		flag = SCM_FLAG_WARMBOOT_CPU0;
	else if (num_possible_cpus() == 2)
		flag = SCM_FLAG_WARMBOOT_CPU0 | SCM_FLAG_WARMBOOT_CPU1;
	else if (num_possible_cpus() == 4)
		flag = SCM_FLAG_WARMBOOT_CPU0 | SCM_FLAG_WARMBOOT_CPU1 |
				SCM_FLAG_WARMBOOT_CPU2 | SCM_FLAG_WARMBOOT_CPU3;
	else
		__WARN();

	return scm_set_boot_addr(virt_to_phys(msm_pm_boot_entry), flag);
}

static void msm_pm_config_tz_before_pc(unsigned int cpu,
		unsigned long entry)
{
	msm_pm_write_boot_vector(cpu, entry);
}
#else
static int msm_pm_tz_boot_init(void)
{
	return 0;
};

static inline void msm_pm_config_tz_before_pc(unsigned int cpu,
		unsigned long entry) {}
#endif

static int msm_pm_boot_reset_vector_init(uint32_t *reset_vector)
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

int msm_pm_boot_init(struct msm_pm_boot_platform_data *pdata)
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
	default:
		__WARN();
	}

	return ret;
}

static int msm_pm_boot_probe(struct platform_device *pdev)
{
	struct msm_pm_boot_platform_data pdata;
	char *key = NULL;
	uint32_t val = 0;
	int ret = 0;
	uint32_t vaddr_val;

	pdata.p_addr = 0;
	vaddr_val = 0;

	key = "qcom,mode";
	ret = msm_pm_get_boot_config_mode(pdev->dev.of_node, key, &val);
	if (ret)
		goto fail;
	pdata.mode = val;

	key = "qcom,phy-addr";
	ret = of_property_read_u32(pdev->dev.of_node, key, &val);
	if (!ret)
		pdata.p_addr = val;


	key = "qcom,virt-addr";
	ret = of_property_read_u32(pdev->dev.of_node, key, &vaddr_val);

	switch (pdata.mode) {
	case MSM_PM_BOOT_CONFIG_RESET_VECTOR_PHYS:
		if (!pdata.p_addr) {
			key = "qcom,phy-addr";
			goto fail;
		}
		break;
	case MSM_PM_BOOT_CONFIG_RESET_VECTOR_VIRT:
		if (!vaddr_val)
			goto fail;

		pdata.v_addr = (void *)vaddr_val;
		break;
	case MSM_PM_BOOT_CONFIG_TZ:
		break;
	default:
		pr_err("%s: Unsupported boot mode %d",
			__func__, pdata.mode);
		goto fail;
	}

	return msm_pm_boot_init(&pdata);

fail:
	pr_err("Error reading %s\n", key);
	return -EFAULT;
}

static struct of_device_id msm_pm_match_table[] = {
	{.compatible = "qcom,pm-boot"},
	{},
};

static struct platform_driver msm_pm_boot_driver = {
	.probe = msm_pm_boot_probe,
	.driver = {
		.name = "pm-boot",
		.owner = THIS_MODULE,
		.of_match_table = msm_pm_match_table,
	},
};

static int __init msm_pm_boot_module_init(void)
{
	return platform_driver_register(&msm_pm_boot_driver);
}
module_init(msm_pm_boot_module_init);
