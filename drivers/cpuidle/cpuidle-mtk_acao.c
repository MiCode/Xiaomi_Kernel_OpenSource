/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/cpuidle.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>

#include <asm/cpuidle.h>
#include <asm/suspend.h>

#define CREATE_TRACE_POINTS
#include <trace/events/mtk_idle_event.h>

int __attribute__((weak)) mtk_cpuidle_register_driver(void)
{
	return -ENODEV;
}

void __attribute__((weak)) mtk_cpuidle_unregister_driver(void)
{
}

int __init mtk_acao_cpuidle_init(void)
{
	int cpu, ret;
	struct cpuidle_device *dev;

	/*
	 * Initialize idle states data, starting at index 1.
	 * This driver is DT only, if no DT idle states are detected (ret == 0)
	 * let the driver initialization fail accordingly since there is no
	 * reason to initialize the idle driver if only wfi is supported.
	 */
#if 0
	ret = dt_init_idle_driver(drv, mtk_idle_state_match, 1);
	if (ret <= 0)
		return ret ? : -ENODEV;
#endif

	ret = mtk_cpuidle_register_driver();
	if (ret) {
		pr_info("Failed to register mtk cpuidle driver (%d)\n", ret);
		return ret;
	}

	/*
	 * Call arch CPU operations in order to initialize
	 * idle states suspend back-end specific data
	 */
	for_each_possible_cpu(cpu) {
		ret = arm_cpuidle_init(cpu);

		/*
		 * Skip the cpuidle device initialization if the reported
		 * failure is a HW misconfiguration/breakage (-ENXIO).
		 */
		if (ret == -ENXIO)
			continue;

		if (ret) {
			pr_info("CPU %d failed to init idle CPU ops\n", cpu);
			goto out_fail;
		}

		dev = kzalloc(sizeof(*dev), GFP_KERNEL);
		if (!dev)
			goto out_fail;
		dev->cpu = cpu;

		ret = cpuidle_register_device(dev);
		if (ret) {
			pr_info("Failed to register cpuidle device for CPU %d\n",
			       cpu);
			kfree(dev);
			goto out_fail;
		}
	}

	return 0;

out_fail:
	while (cpu > 0) {
		cpu--;
		dev = per_cpu(cpuidle_devices, cpu);
		cpuidle_unregister_device(dev);
		kfree(dev);
	}

	mtk_cpuidle_unregister_driver();

	return ret;
}

device_initcall(mtk_acao_cpuidle_init);
