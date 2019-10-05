// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/cpuidle.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/cpu_pm.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/smp.h>

#include <asm/cpuidle.h>
#include <asm/suspend.h>
#include <dt_idle_states.h>

#define MTK_CPUIDLE_PREPARE		(0)
#define MTK_CPUIDLE_RESUME		(1)

typedef int (*mtk_pwr_fn)(int type,
			  struct cpuidle_driver *drv,
			  int index);

static mtk_pwr_fn mtk_pwr_conservation;

static __always_inline int __mtk_lp_enter(int index)
{
	return CPU_PM_CPU_IDLE_ENTER(arm_cpuidle_suspend, index);
}

static int mtk_idle_state_enter(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int idx)
{
	int ret;

	if (mtk_pwr_conservation) {
		ret = mtk_pwr_conservation(MTK_CPUIDLE_PREPARE, drv, idx);
		idx = ret ? 0 : idx;
		ret = __mtk_lp_enter(idx);
		mtk_pwr_conservation(MTK_CPUIDLE_RESUME, drv, idx);
	} else
		ret = CPU_PM_CPU_IDLE_ENTER(arm_cpuidle_suspend, 0);

	return ret;
}

static int mtk_regular_idle_state_enter(struct cpuidle_device *dev,
					struct cpuidle_driver *drv,
					int idx)
{
	return CPU_PM_CPU_IDLE_ENTER(arm_cpuidle_suspend, idx);
}

static struct cpuidle_driver mtk_cpuidle_driver __initdata = {
	.name = "mtk_cpuidle",
	.owner = THIS_MODULE,
	.states[0] = {
		.enter                  = mtk_regular_idle_state_enter,
		.exit_latency           = 1,
		.target_residency       = 1,
		.power_usage			= UINT_MAX,
		.name                   = "wfi",
		.desc                   = "wfi",
	},
	.state_count = 1
};

static const struct of_device_id mtk_idle_state_match[] __initconst = {
	{ .compatible = "mediatek,idle-state",
	  .data = mtk_idle_state_enter },
	{ .compatible = "arm,idle-state",
	  .data = mtk_regular_idle_state_enter },
	{ },
};

static int mtk_cpuidle_pm_drv_probe(struct platform_device *pdev)
{
	if (IS_ERR_OR_NULL(pdev->dev.platform_data))
		goto mtk_probe_fail;

	mtk_pwr_conservation = *((mtk_pwr_fn *)pdev->dev.platform_data);
	return 0;

mtk_probe_fail:
	return -ENODATA;
}

int mtk_cpuidle_pm_drv_remove(struct platform_device *pdev)
{
	mtk_pwr_conservation = NULL;
	return 0;
}

static struct platform_driver mtk_cpuidle_pm_driver = {
	.driver = {
	   .name = "mtk_cpuidle_pm",
	   .owner = THIS_MODULE,
	},
	.probe = mtk_cpuidle_pm_drv_probe,
	.remove = mtk_cpuidle_pm_drv_remove,
};

static int __init mtk_cpuidle_driver_init(void)
{
	int cpu, ret;
	struct cpuidle_driver *drv;
	struct cpuidle_device *dev;

	platform_driver_register(&mtk_cpuidle_pm_driver);

	for_each_possible_cpu(cpu) {
		drv = kmemdup(&mtk_cpuidle_driver, sizeof(*drv), GFP_KERNEL);

		if (!drv) {
			ret = -ENOMEM;
			goto out_fail;
		}

		drv->cpumask = (struct cpumask *)cpumask_of(cpu);

#ifdef CONFIG_DT_IDLE_STATES
		ret = dt_init_idle_driver(drv, mtk_idle_state_match, 1);

		if (ret <= 0) {
			ret = ret ? : -ENODEV;
			goto out_kfree_drv;
		}
#endif

		ret = arm_cpuidle_init(cpu);

		if (ret) {
			pr_err("CPU %d failed to init idle CPU ops\n", cpu);
			goto out_kfree_drv;
		}

		ret = cpuidle_register(drv, NULL);

		if (ret)
			goto out_kfree_drv;
	}

	return 0;

out_kfree_drv:
	kfree(drv);
out_fail:
	while (--cpu >= 0) {
		dev = per_cpu(cpuidle_devices, cpu);
		drv = cpuidle_get_cpu_driver(dev);
		cpuidle_unregister(drv);
		kfree(drv);
	}

	return ret;
}
device_initcall_sync(mtk_cpuidle_driver_init);

