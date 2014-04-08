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
#include <asm/mach-types.h>
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
		{MSM_PM_BOOT_CONFIG_REMAP_BOOT_ADDR, "remap_boot_addr"}
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
static int __devinit msm_pm_tz_boot_init(void)
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
static int __init msm_pm_tz_boot_init(void)
{
	return 0;
};

static inline void msm_pm_config_tz_before_pc(unsigned int cpu,
		unsigned long entry) {}
#endif

static int __devinit msm_pm_boot_reset_vector_init(uint32_t *reset_vector)
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

int __devinit msm_pm_boot_init(struct msm_pm_boot_platform_data *pdata)
{
	int ret = 0;
	unsigned long entry;
	void __iomem *warm_boot_ptr;

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
		if (!cpu_is_msm8625() && !cpu_is_msm8625q()) {
			void *remapped;

			/*
			 * Set the boot remap address and enable remapping of
			 * reset vector
			 */
			if (!pdata->p_addr || !pdata->v_addr)
				return -ENODEV;

			remapped = ioremap_nocache(pdata->p_addr, SZ_8);
			ret = msm_pm_boot_reset_vector_init(remapped);

			__raw_writel((pdata->p_addr | BOOT_REMAP_ENABLE),
					pdata->v_addr);

			msm_pm_boot_before_pc
				= msm_pm_config_rst_vector_before_pc;
			msm_pm_boot_after_pc
				= msm_pm_config_rst_vector_after_pc;
		} else {
			uint32_t mpa5_boot_remap_addr[2] = {0x34, 0x4C};
			uint32_t mpa5_cfg_ctl[2] = {0x30, 0x48};

			warm_boot_ptr = ioremap_nocache(
						MSM8625_WARM_BOOT_PHYS, SZ_64);
			ret = msm_pm_boot_reset_vector_init(warm_boot_ptr);

			entry = virt_to_phys(msm_pm_boot_entry);

			/*
			 * Below sequence is a work around for cores
			 * to come out of GDFS properly on 8625 target.
			 * On 8625 while cores coming out of GDFS observed
			 * the memory corruption at very first memory read.
			 */
			msm_pm_reset_vector[0] = 0xE59F000C; /* ldr r0, 0x14 */
			msm_pm_reset_vector[1] = 0xE59F1008; /* ldr r1, 0x14 */
			msm_pm_reset_vector[2] = 0xE1500001; /* cmp r0, r1 */
			msm_pm_reset_vector[3] = 0x1AFFFFFB; /* bne 0x0 */
			msm_pm_reset_vector[4] = 0xE12FFF10; /* bx  r0 */
			msm_pm_reset_vector[5] = entry; /* 0x14 */

			/*
			 * Here upper 16bits[16:31] used by CORE1
			 * lower 16bits[0:15] used by CORE0
			 */
			entry = (MSM8625_WARM_BOOT_PHYS |
				((MSM8625_WARM_BOOT_PHYS & 0xFFFF0000) >> 16));

			/* write 'entry' to boot remapper register */
			__raw_writel(entry, (pdata->v_addr +
						mpa5_boot_remap_addr[0]));

			/*
			 * Enable boot remapper for C0 [bit:25th]
			 * Enable boot remapper for C1 [bit:26th]
			 */
			__raw_writel(readl_relaxed(pdata->v_addr +
					mpa5_cfg_ctl[0]) | (0x3 << 25),
					pdata->v_addr + mpa5_cfg_ctl[0]);

			/* 8x25Q changes */
			if (cpu_is_msm8625q()) {
				/* write 'entry' to boot remapper register */
				__raw_writel(entry, (pdata->v_addr +
						mpa5_boot_remap_addr[1]));

				/*
				 * Enable boot remapper for C2 [bit:25th]
				 * Enable boot remapper for C3 [bit:26th]
				 */
				__raw_writel(readl_relaxed(pdata->v_addr +
					mpa5_cfg_ctl[1]) | (0x3 << 25),
					pdata->v_addr + mpa5_cfg_ctl[1]);
			}
			msm_pm_boot_before_pc = msm_pm_write_boot_vector;
		}
		break;
	default:
		__WARN();
	}

	return ret;
}

static int __devinit msm_pm_boot_probe(struct platform_device *pdev)
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
	case MSM_PM_BOOT_CONFIG_REMAP_BOOT_ADDR:
		if (!vaddr_val)
			goto fail;

		pdata.v_addr = ioremap_nocache(vaddr_val, SZ_8);

		pdata.p_addr = allocate_contiguous_ebi_nomap(SZ_8, SZ_64K);
		if (!pdata.p_addr) {
			key = "qcom,phy-addr";
			goto fail;
		}
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
postcore_initcall(msm_pm_boot_module_init);
