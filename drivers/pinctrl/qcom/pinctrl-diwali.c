// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>
#include <trace/hooks/gpiolib.h>

#include "pinctrl-msm.h"
#include "pinctrl-diwali.h"

static const struct msm_pinctrl_soc_data diwali_pinctrl = {
	.pins = diwali_pins,
	.npins = ARRAY_SIZE(diwali_pins),
	.functions = diwali_functions,
	.nfunctions = ARRAY_SIZE(diwali_functions),
	.groups = diwali_groups,
	.ngroups = ARRAY_SIZE(diwali_groups),
	.ngpios = 171,
/* TODO:
 *	.qup_regs = diwali_qup_regs,
 *	.nqup_regs = ARRAY_SIZE(diwali_qup_regs),
 */
	.wakeirq_map = diwali_pdc_map,
	.nwakeirq_map = ARRAY_SIZE(diwali_pdc_map),
};

static const struct msm_pinctrl_soc_data diwali_vm_pinctrl = {
	.pins = diwali_pins,
	.npins = ARRAY_SIZE(diwali_pins),
	.functions = diwali_functions,
	.nfunctions = ARRAY_SIZE(diwali_functions),
	.groups = diwali_groups,
	.ngroups = ARRAY_SIZE(diwali_groups),
	.ngpios = 171,
};

static void qcom_trace_gpio_read(void *unused, struct gpio_device *gdev,
				 bool *block_gpio_read)
{
	*block_gpio_read = true;
}

static int diwali_pinctrl_probe(struct platform_device *pdev)
{
	const struct msm_pinctrl_soc_data *pinctrl_data;
	struct device *dev = &pdev->dev;

	pinctrl_data = of_device_get_match_data(&pdev->dev);
	if (!pinctrl_data)
		return -EINVAL;

	if (of_device_is_compatible(dev->of_node, "qcom,diwali-vm-pinctrl"))
		register_trace_android_vh_gpio_block_read(qcom_trace_gpio_read,
							  NULL);

	return msm_pinctrl_probe(pdev, pinctrl_data);
}

static const struct of_device_id diwali_pinctrl_of_match[] = {
	{ .compatible = "qcom,diwali-pinctrl", .data = &diwali_pinctrl},
	{ .compatible = "qcom,diwali-vm-pinctrl", .data = &diwali_vm_pinctrl},
	{ },
};

static struct platform_driver diwali_pinctrl_driver = {
	.driver = {
		.name = "diwali-pinctrl",
		.of_match_table = diwali_pinctrl_of_match,
	},
	.probe = diwali_pinctrl_probe,
	.remove = msm_pinctrl_remove,
};

static int __init diwali_pinctrl_init(void)
{
	return platform_driver_register(&diwali_pinctrl_driver);
}
arch_initcall(diwali_pinctrl_init);

static void __exit diwali_pinctrl_exit(void)
{
	platform_driver_unregister(&diwali_pinctrl_driver);
}
module_exit(diwali_pinctrl_exit);

MODULE_DESCRIPTION("QTI diwali pinctrl driver");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, diwali_pinctrl_of_match);
MODULE_SOFTDEP("pre: qcom_tlmm_vm_irqchip");
