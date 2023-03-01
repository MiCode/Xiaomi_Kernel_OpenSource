// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm_opp.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

static struct platform_device *vm_cpufreq_pdev;

static int vm_cpufreq_target_index(struct cpufreq_policy *policy, unsigned int index)
{
	return 0;
}

static unsigned int vm_cpufreq_get(unsigned int cpu)
{
	struct cpufreq_policy *policy;

	policy = cpufreq_cpu_get_raw(cpu);
	if (!policy)
		return 0;

	pr_debug("CPU%d frequency is %d\n", cpu,
			policy->freq_table[0].frequency);

	return policy->freq_table[0].frequency;
}

static int vm_cpufreq_init(struct cpufreq_policy *policy)
{
	struct cpufreq_frequency_table *freq_table;
	struct device *cpu_dev;
	int ret;

	cpu_dev = get_cpu_device(policy->cpu);
	if (!cpu_dev) {
		pr_err("failed to get cpu%d device\n", policy->cpu);
		return -ENODEV;
	}

	/* Get OPP-sharing information from "operating-points-v2" bindings */
	ret = dev_pm_opp_of_get_sharing_cpus(cpu_dev, policy->cpus);
	if (ret) {
		dev_err(cpu_dev, "failed to get opp-sharing information\n");
		return ret;
	}

	/*
	 * Initialize OPP tables for all policy->cpus. They will be shared by
	 * all CPUs which have marked their CPUs shared with OPP bindings.
	 */
	ret = dev_pm_opp_of_cpumask_add_table(policy->cpus);
	if (ret) {
		dev_err(cpu_dev, "no OPP table for cpu%d\n", policy->cpu);
		return ret;
	}

	ret = dev_pm_opp_init_cpufreq_table(cpu_dev, &freq_table);
	if (ret) {
		dev_err(cpu_dev, "failed to init cpufreq table: %d\n", ret);
		return ret;
	}

	policy->freq_table = freq_table;

	return 0;
}

static int vm_cpufreq_exit(struct cpufreq_policy *policy)
{
	return 0;
}

static struct cpufreq_driver vm_cpufreq_driver = {
	.verify = cpufreq_generic_frequency_table_verify,
	.target_index = vm_cpufreq_target_index,
	.get = vm_cpufreq_get,
	.init = vm_cpufreq_init,
	.exit = vm_cpufreq_exit,
	.register_em = cpufreq_register_em_with_opp,
	.name = "cpufreq-vm",
	.attr = cpufreq_generic_attr,
};

static int vm_cpufreq_probe(struct platform_device *pdev)
{
	int ret;

	ret = cpufreq_register_driver(&vm_cpufreq_driver);
	if (ret) {
		dev_err(&pdev->dev, "failed register driver: %d\n", ret);
		return ret;
	}

	dev_info(&pdev->dev, "probed successfully\n");

	return ret;
}

static int vm_cpufreq_remove(struct platform_device *pdev)
{
	cpufreq_unregister_driver(&vm_cpufreq_driver);
	return 0;
}

static struct platform_driver vm_cpufreq_platdrv = {
	.probe		= vm_cpufreq_probe,
	.remove		= vm_cpufreq_remove,
	.driver = {
		.name	= "cpufreq-vm",
	},
};

static int __init vm_cpufreq_module_init(void)
{
	int ret;

	ret = platform_driver_register(&vm_cpufreq_platdrv);
	if (ret)
		return ret;

	if (of_machine_is_compatible("qcom,quinvm")) {
		vm_cpufreq_pdev = platform_device_register_simple("cpufreq-vm",
				-1, NULL, 0);
		if (IS_ERR(vm_cpufreq_pdev)) {
			platform_driver_unregister(&vm_cpufreq_platdrv);
			return PTR_ERR(vm_cpufreq_pdev);
		}
	}

	return 0;
}
module_init(vm_cpufreq_module_init);

static void __exit vm_cpufreq_module_exit(void)
{
	platform_device_unregister(vm_cpufreq_pdev);
	platform_driver_unregister(&vm_cpufreq_platdrv);
}
module_exit(vm_cpufreq_module_exit);

MODULE_ALIAS("platform:cpufreq-vm");
MODULE_DESCRIPTION("Virtual machine cpufreq driver");
MODULE_LICENSE("GPL v2");
