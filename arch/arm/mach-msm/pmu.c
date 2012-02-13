/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/platform_device.h>
#include <asm/pmu.h>
#include <mach/irqs.h>

static struct resource cpu_pmu_resource[] = {
	{
		.start = INT_ARMQC_PERFMON,
		.end = INT_ARMQC_PERFMON,
		.flags	= IORESOURCE_IRQ,
	},
};

#ifdef CONFIG_CPU_HAS_L2_PMU
static struct resource l2_pmu_resource[] = {
	{
		.start = SC_SICL2PERFMONIRPTREQ,
		.end = SC_SICL2PERFMONIRPTREQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device l2_pmu_device = {
	.name		= "l2-arm-pmu",
	.id		= ARM_PMU_DEVICE_L2,
	.resource	= l2_pmu_resource,
	.num_resources	= ARRAY_SIZE(l2_pmu_resource),
};

#endif

static struct platform_device cpu_pmu_device = {
	.name		= "cpu-arm-pmu",
	.id		= ARM_PMU_DEVICE_CPU,
	.resource	= cpu_pmu_resource,
	.num_resources	= ARRAY_SIZE(cpu_pmu_resource),
};

static struct platform_device *pmu_devices[] = {
	&cpu_pmu_device,
#ifdef CONFIG_CPU_HAS_L2_PMU
	&l2_pmu_device,
#endif
};

static int __init msm_pmu_init(void)
{
	return platform_add_devices(pmu_devices, ARRAY_SIZE(pmu_devices));
}

arch_initcall(msm_pmu_init);
