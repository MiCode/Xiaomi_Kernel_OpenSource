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
#include <linux/irq.h>
#include <asm/pmu.h>
#include <mach/irqs.h>
#include <mach/socinfo.h>

#if defined(CONFIG_ARCH_MSM_KRAITMP) || defined(CONFIG_ARCH_MSM_SCORPIONMP)
static DEFINE_PER_CPU(u32, pmu_irq_cookie);

static void enable_irq_callback(void *info)
{
	int irq = *(unsigned int *)info;
	enable_percpu_irq(irq, IRQ_TYPE_EDGE_RISING);
}

static void disable_irq_callback(void *info)
{
	int irq = *(unsigned int *)info;
	disable_percpu_irq(irq);
}

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
	 */
#if defined(CONFIG_ARCH_MSM_KRAITMP) || defined(CONFIG_ARCH_MSM_SCORPIONMP)
	cpu_pmu_device.dev.platform_data = &multicore_data;
#endif

	return platform_add_devices(pmu_devices, ARRAY_SIZE(pmu_devices));
}

arch_initcall(msm_pmu_init);
