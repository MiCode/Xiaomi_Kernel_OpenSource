/*
 * Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/clk/msm-clk.h>
#include <linux/cpu.h>
#include <linux/cpu_pm.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/suspend.h>

static struct clk **clk;
static int clk_cnt;

static void clock_axi_sleep_enable(void)
{
	int i, ret;

	for (i = 0; i < clk_cnt; i++) {
		ret = clk_enable(clk[i]);
		if (ret < 0)
			pr_err("%s(): clk enable failed %d\n", __func__, i);
	}
}

static void clock_axi_sleep_disable(void)
{
	int i;

	for (i = 0; i < clk_cnt; i++)
		clk_disable(clk[i]);
}

static int clock_pm_cpu_pm_notify(struct notifier_block *self,
			unsigned long action, void *v)
{
	switch (action) {
	case CPU_PM_ENTER:
		clock_axi_sleep_enable();
		break;
	case CPU_PM_EXIT:
		clock_axi_sleep_disable();
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static int clock_pm_sys_suspend_noirq(struct device *dev)
{
	clock_axi_sleep_enable();

	return 0;
}

static int clock_pm_sys_resume_noirq(struct device *dev)
{
	clock_axi_sleep_disable();

	return 0;
}

static struct notifier_block clock_pm_cpu_pm_nb = {
	.notifier_call = clock_pm_cpu_pm_notify,
};

static int clock_pm_probe(struct platform_device *pdev)
{
	int i, ret = 0;
	const char *clk_names;

	clk_cnt = of_property_count_strings(pdev->dev.of_node, "clock-names");

	clk = devm_kzalloc(&pdev->dev, clk_cnt * sizeof(struct clk *),
								GFP_KERNEL);
	if (!clk)
		return -ENOMEM;

	for (i = 0; i < clk_cnt; i++) {
		ret = of_property_read_string_index(pdev->dev.of_node,
				"clock-names", i, &clk_names);
		if (ret) {
			pr_err("%s(): Invalid clk name\n", __func__);
			return ret;
		}

		clk[i] = devm_clk_get(&pdev->dev, clk_names);
		if (IS_ERR(clk[i])) {
			pr_err("%s(): clk get failed %s\n",
							__func__, clk_names);
			return PTR_ERR(clk[i]);
		}

		ret = clk_prepare(clk[i]);
		if (ret < 0) {
			pr_err("%s(): clk prepare failed %s\n",
							__func__, clk_names);
			for (; i > 0; i--)
				clk_unprepare(clk[i - 1]);

			return ret;
		}
	}

	cpu_pm_register_notifier(&clock_pm_cpu_pm_nb);

	return ret;
}

static int clock_pm_remove(struct platform_device *pdev)
{
	int i;

	cpu_pm_unregister_notifier(&clock_pm_cpu_pm_nb);

	for (i = 0; i < clk_cnt; i++)
		clk_unprepare(clk[i]);

	return 0;
}

static const struct dev_pm_ops clock_pm_ops = {
	.suspend_noirq		= clock_pm_sys_suspend_noirq,
	.resume_noirq		= clock_pm_sys_resume_noirq,
};

static struct of_device_id clock_pm_match_table[] = {
	{.compatible = "qcom,clock-pm"},
	{}
};

static struct platform_driver clock_pm_driver = {
	.probe = clock_pm_probe,
	.remove = clock_pm_remove,
	.driver = {
		.name = "clock-pm",
		.pm = &clock_pm_ops,
		.of_match_table = clock_pm_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init clock_pm_init(void)
{
	return platform_driver_register(&clock_pm_driver);
}
late_initcall(clock_pm_init);
