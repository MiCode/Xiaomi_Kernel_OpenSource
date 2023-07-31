// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>
#include <trace/hooks/gpiolib.h>

#include "pinctrl-msm.h"
#include "pinctrl-ravelin.h"

static const struct msm_pinctrl_soc_data ravelin_pinctrl = {
	.pins = ravelin_pins,
	.npins = ARRAY_SIZE(ravelin_pins),
	.functions = ravelin_functions,
	.nfunctions = ARRAY_SIZE(ravelin_functions),
	.groups = ravelin_groups,
	.ngroups = ARRAY_SIZE(ravelin_groups),
	.ngpios = 137,
	.wakeirq_map = ravelin_pdc_map,
	.nwakeirq_map = ARRAY_SIZE(ravelin_pdc_map),
};

static const struct msm_pinctrl_soc_data ravelin_vm_pinctrl = {
	.pins = ravelin_pins,
	.npins = ARRAY_SIZE(ravelin_pins),
	.functions = ravelin_functions,
	.nfunctions = ARRAY_SIZE(ravelin_functions),
	.groups = ravelin_groups,
	.ngroups = ARRAY_SIZE(ravelin_groups),
	.ngpios = 137,
};

static void qcom_trace_gpio_read(void *unused, struct gpio_device *gdev,
				bool *block_gpio_read)
{
	*block_gpio_read = true;
}

static int ravelin_pinctrl_probe(struct platform_device *pdev)
{
	const struct msm_pinctrl_soc_data *pinctrl_data;
	struct device *dev = &pdev->dev;

	pinctrl_data = of_device_get_match_data(&pdev->dev);
	if (!pinctrl_data)
		return -EINVAL;

	if (of_device_is_compatible(dev->of_node, "qcom,ravelin-vm-pinctrl"))
		register_trace_android_vh_gpio_block_read(qcom_trace_gpio_read,
							NULL);

	return msm_pinctrl_probe(pdev, pinctrl_data);
}

static const struct of_device_id ravelin_pinctrl_of_match[] = {
	{ .compatible = "qcom,ravelin-pinctrl", .data = &ravelin_pinctrl},
	{ .compatible = "qcom,ravelin-vm-pinctrl", .data = &ravelin_vm_pinctrl},
	{ },
};

static struct platform_driver ravelin_pinctrl_driver = {
	.driver = {
		.name = "ravelin-pinctrl",
		.of_match_table = ravelin_pinctrl_of_match,
	},
	.probe = ravelin_pinctrl_probe,
	.remove = msm_pinctrl_remove,
};

static int __init ravelin_pinctrl_init(void)
{
	return platform_driver_register(&ravelin_pinctrl_driver);
}
arch_initcall(ravelin_pinctrl_init);

static void __exit ravelin_pinctrl_exit(void)
{
	platform_driver_unregister(&ravelin_pinctrl_driver);
}
module_exit(ravelin_pinctrl_exit);

MODULE_DESCRIPTION("QTI ravelin pinctrl driver");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, ravelin_pinctrl_of_match);
MODULE_SOFTDEP("pre: qcom_tlmm_vm_irqchip");
