/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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
#include <mach/socinfo.h>

/*
 * If a GIC is present, then all IRQ's < 32 are PPI's and can only be
 * requested and free'd using the percpu IRQ API.
 * If a VIC is present, then only the traditional request, free API works.
 *
 * All MPCore's have GIC's. The Cortex A5 however may or may not be MPcore, but
 * it still has a GIC. Except, the 7x27a, which is an A5 and yet has a VIC.
 * So if the chip is A5 but does not have a GIC, default to the traditional
 * IRQ {request, free}_irq API.
 */

#if defined(CONFIG_ARCH_MSM_KRAITMP) || defined(CONFIG_ARCH_MSM_SCORPIONMP) \
	|| defined(CONFIG_ARCH_MSM8625) || \
	(defined(CONFIG_ARCH_MSM_CORTEX_A5) && !defined(CONFIG_MSM_VIC))

static DEFINE_PER_CPU(u32, pmu_irq_cookie);

static int
multicore_request_irq(int irq, irq_handler_t *handle_irq)
{
	int err = 0;
	int cpu;

	err = request_percpu_irq(irq, *handle_irq, "l1-armpmu",
			&pmu_irq_cookie);

	if (!err) {
		for_each_cpu(cpu, cpu_online_mask) {
			smp_call_function_single(cpu,
					enable_irq_callback, &irq, 1);
		}
	}

	return err;
}

static void
multicore_free_irq(int irq)
{
	int cpu;

	if (irq >= 0) {
		for_each_cpu(cpu, cpu_online_mask) {
			smp_call_function_single(cpu,
					disable_irq_callback, &irq, 1);
		}
		free_percpu_irq(irq, &pmu_irq_cookie);
	}
}

static struct arm_pmu_platdata multicore_data = {
	.request_pmu_irq = multicore_request_irq,
	.free_pmu_irq = multicore_free_irq,
};
#endif

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
	.id		= ARM_PMU_DEVICE_L2CC,
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

/*
 * The 8625 is a special case. Due to the requirement of a single
 * kernel image for the 7x27a and 8625 (which share IRQ headers),
 * this target breaks the uniformity of IRQ names.
 * See the file - arch/arm/mach-msm/include/mach/irqs-8625.h
 */
#ifdef CONFIG_ARCH_MSM8625
static struct resource msm8625_cpu_pmu_resource[] = {
	{
		.start = MSM8625_INT_ARMQC_PERFMON,
		.end = MSM8625_INT_ARMQC_PERFMON,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device msm8625_cpu_pmu_device = {
	.name		= "cpu-arm-pmu",
	.id		= ARM_PMU_DEVICE_CPU,
	.resource	= msm8625_cpu_pmu_resource,
	.num_resources	= ARRAY_SIZE(msm8625_cpu_pmu_resource),
};

static struct resource msm8625_l2_pmu_resource[] = {
	{
		.start = MSM8625_INT_SC_SICL2PERFMONIRPTREQ,
		.end = MSM8625_INT_SC_SICL2PERFMONIRPTREQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device msm8625_l2_pmu_device = {
	.name		= "l2-arm-pmu",
	.id		= ARM_PMU_DEVICE_L2CC,
	.resource	= msm8625_l2_pmu_resource,
	.num_resources	= ARRAY_SIZE(msm8625_l2_pmu_resource),
};
#endif

static struct platform_device *pmu_devices[] = {
	&cpu_pmu_device,
#ifdef CONFIG_CPU_HAS_L2_PMU
	&l2_pmu_device,
#endif
};

static int __init msm_pmu_init(void)
{
	/*
	 * For the targets we know are multicore's set the request/free IRQ
	 * handlers to call the percpu API.
	 * Defaults to unicore API {request,free}_irq().
	 * See arch/arm/kernel/perf_event.c
	 * See Comment above on the A5 and MSM_VIC.
	 */
#if defined(CONFIG_ARCH_MSM_KRAITMP) || defined(CONFIG_ARCH_MSM_SCORPIONMP) \
	|| (defined(CONFIG_ARCH_MSM_CORTEX_A5) && !defined(CONFIG_MSM_VIC))
	cpu_pmu_device.dev.platform_data = &multicore_data;
#endif

	/*
	 * The 7x27a and 8625 require a single kernel image.
	 * So we need to check if we're on an 8625 at runtime
	 * and point to the appropriate 'struct resource'.
	 */
#ifdef CONFIG_ARCH_MSM8625
	if (cpu_is_msm8625() || cpu_is_msm8625q()) {
		pmu_devices[0] = &msm8625_cpu_pmu_device;
		pmu_devices[1] = &msm8625_l2_pmu_device;
		msm8625_cpu_pmu_device.dev.platform_data = &multicore_data;
	}
#endif

	return platform_add_devices(pmu_devices, ARRAY_SIZE(pmu_devices));
}

arch_initcall(msm_pmu_init);
