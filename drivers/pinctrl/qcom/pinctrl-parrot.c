// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>
#include <trace/hooks/gpiolib.h>

#include "pinctrl-msm.h"
#include "pinctrl-parrot.h"

static const struct msm_pinctrl_soc_data parrot_pinctrl = {
	.pins = parrot_pins,
	.npins = ARRAY_SIZE(parrot_pins),
	.functions = parrot_functions,
	.nfunctions = ARRAY_SIZE(parrot_functions),
	.groups = parrot_groups,
	.ngroups = ARRAY_SIZE(parrot_groups),
	.ngpios = 142,
	.wakeirq_map = parrot_pdc_map,
	.nwakeirq_map = ARRAY_SIZE(parrot_pdc_map),
};

static const struct msm_pinctrl_soc_data parrot_vm_pinctrl = {
	.pins = parrot_pins,
	.npins = ARRAY_SIZE(parrot_pins),
	.functions = parrot_functions,
	.nfunctions = ARRAY_SIZE(parrot_functions),
	.groups = parrot_groups,
	.ngroups = ARRAY_SIZE(parrot_groups),
	.ngpios = 142,
};

static void qcom_trace_gpio_read(void *unused, struct gpio_device *gdev,
				bool *block_gpio_read)
{
	*block_gpio_read = true;
}

static int parrot_pinctrl_probe(struct platform_device *pdev)
{
	const struct msm_pinctrl_soc_data *pinctrl_data;
	struct device *dev = &pdev->dev;

	pinctrl_data = of_device_get_match_data(&pdev->dev);
	if (!pinctrl_data)
		return -EINVAL;

	if (of_device_is_compatible(dev->of_node, "qcom,parrot-vm-pinctrl"))
		register_trace_android_vh_gpio_block_read(qcom_trace_gpio_read,
							NULL);

	return msm_pinctrl_probe(pdev, pinctrl_data);
}

static const struct of_device_id parrot_pinctrl_of_match[] = {
	{ .compatible = "qcom,parrot-pinctrl", .data = &parrot_pinctrl},
	{ .compatible = "qcom,parrot-vm-pinctrl", .data = &parrot_vm_pinctrl},
	{ },
};

static struct platform_driver parrot_pinctrl_driver = {
	.driver = {
		.name = "parrot-pinctrl",
		.of_match_table = parrot_pinctrl_of_match,
	},
	.probe = parrot_pinctrl_probe,
	.remove = msm_pinctrl_remove,
};

static int __init parrot_pinctrl_init(void)
{
	return platform_driver_register(&parrot_pinctrl_driver);
}
arch_initcall(parrot_pinctrl_init);

static void __exit parrot_pinctrl_exit(void)
{
	platform_driver_unregister(&parrot_pinctrl_driver);
}
module_exit(parrot_pinctrl_exit);

MODULE_DESCRIPTION("QTI parrot pinctrl driver");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, parrot_pinctrl_of_match);
MODULE_SOFTDEP("pre: qcom_tlmm_vm_irqchip");
