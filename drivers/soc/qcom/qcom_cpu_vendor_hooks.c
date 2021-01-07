// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "VendorHooks: " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/sched/debug.h>

#include <trace/hooks/debug.h>
#include <trace/hooks/printk.h>

static DEFINE_PER_CPU(struct pt_regs, regs_before_stop);
static DEFINE_RAW_SPINLOCK(stop_lock);

static void printk_hotplug(void *unused, int *flag)
{
	*flag = 1;
}

static void trace_ipi_stop(void *unused, struct pt_regs *regs)
{
	unsigned int cpu = smp_processor_id();
	unsigned long flags;

	per_cpu(regs_before_stop, cpu) = *regs;
	raw_spin_lock_irqsave(&stop_lock, flags);
	pr_crit("CPU%u: stopping\n", cpu);
	show_regs(regs);
	raw_spin_unlock_irqrestore(&stop_lock, flags);
}

static int cpu_vendor_hooks_driver_probe(struct platform_device *pdev)
{
	int ret;

	ret = register_trace_android_vh_ipi_stop(trace_ipi_stop, NULL);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register android_vh_ipi_stop hook\n");
		return ret;
	}

	ret = register_trace_android_vh_printk_hotplug(printk_hotplug, NULL);
	if (ret) {
		dev_err(&pdev->dev, "Failed to android_vh_printk_hotplug hook\n");
		unregister_trace_android_vh_ipi_stop(trace_ipi_stop, NULL);
		return ret;
	}

	return ret;
}

static int cpu_vendor_hooks_driver_remove(struct platform_device *pdev)
{
	/* Reset all initialized global variables and unregister callbacks. */
	unregister_trace_android_vh_ipi_stop(trace_ipi_stop, NULL);
	unregister_trace_android_vh_printk_hotplug(printk_hotplug, NULL);
	return 0;
}

static const struct of_device_id cpu_vendor_hooks_of_match[] = {
	{ .compatible = "qcom,cpu-vendor-hooks" },
	{ }
};
MODULE_DEVICE_TABLE(of, cpu_vendor_hooks_of_match);

static struct platform_driver cpu_vendor_hooks_driver = {
	.driver = {
		.name = "qcom-cpu-vendor-hooks",
		.of_match_table = cpu_vendor_hooks_of_match,
	},
	.probe = cpu_vendor_hooks_driver_probe,
	.remove = cpu_vendor_hooks_driver_remove,
};

static int __init qcom_vendor_hook_driver_init(void)
{
	return platform_driver_register(&cpu_vendor_hooks_driver);
}
#if IS_MODULE(CONFIG_QCOM_CPU_VENDOR_HOOKS)
module_init(qcom_vendor_hook_driver_init);
#else
pure_initcall(qcom_vendor_hook_driver_init);
#endif

static void __exit qcom_vendor_hook_driver_exit(void)
{
	return platform_driver_unregister(&cpu_vendor_hooks_driver);
}
module_exit(qcom_vendor_hook_driver_exit);

MODULE_DESCRIPTION("QCOM CPU Vendor Hooks Driver");
MODULE_LICENSE("GPL v2");
