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
#include "pinctrl-waipio.h"

static const struct msm_pinctrl_soc_data waipio_pinctrl = {
	.pins = waipio_pins,
	.npins = ARRAY_SIZE(waipio_pins),
	.functions = waipio_functions,
	.nfunctions = ARRAY_SIZE(waipio_functions),
	.groups = waipio_groups,
	.ngroups = ARRAY_SIZE(waipio_groups),
	.ngpios = 211,
	.wakeirq_map = waipio_pdc_map,
	.nwakeirq_map = ARRAY_SIZE(waipio_pdc_map),
	.qup_regs = waipio_qup_regs,
	.nqup_regs = ARRAY_SIZE(waipio_qup_regs),
};

static const struct msm_pinctrl_soc_data waipio_vm_pinctrl = {
	.pins = waipio_pins,
	.npins = ARRAY_SIZE(waipio_pins),
	.functions = waipio_functions,
	.nfunctions = ARRAY_SIZE(waipio_functions),
	.groups = waipio_groups,
	.ngroups = ARRAY_SIZE(waipio_groups),
	.ngpios = 211,
};

static void qcom_trace_gpio_read(void *unused,
				 struct gpio_device *gdev,
				 bool *block_gpio_read)
{
	*block_gpio_read = true;
}

static int waipio_pinctrl_probe(struct platform_device *pdev)
{
	const struct msm_pinctrl_soc_data *pinctrl_data;
	struct device *dev = &pdev->dev;

	pinctrl_data = of_device_get_match_data(&pdev->dev);
	if (!pinctrl_data)
		return -EINVAL;

	if (of_device_is_compatible(dev->of_node, "qcom,waipio-vm-pinctrl"))
		register_trace_android_vh_gpio_block_read(qcom_trace_gpio_read,
							  NULL);

	return msm_pinctrl_probe(pdev, pinctrl_data);
}

static const struct of_device_id waipio_pinctrl_of_match[] = {
	{ .compatible = "qcom,waipio-pinctrl", .data = &waipio_pinctrl},
	{ .compatible = "qcom,waipio-vm-pinctrl", .data = &waipio_vm_pinctrl},
	{ },
};

static struct platform_driver waipio_pinctrl_driver = {
	.driver = {
		.name = "waipio-pinctrl",
		.of_match_table = waipio_pinctrl_of_match,
	},
	.probe = waipio_pinctrl_probe,
	.remove = msm_pinctrl_remove,
};

static int __init waipio_pinctrl_init(void)
{
	return platform_driver_register(&waipio_pinctrl_driver);
}
arch_initcall(waipio_pinctrl_init);

static void __exit waipio_pinctrl_exit(void)
{
	platform_driver_unregister(&waipio_pinctrl_driver);
}
module_exit(waipio_pinctrl_exit);

MODULE_DESCRIPTION("QTI waipio pinctrl driver");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, waipio_pinctrl_of_match);
MODULE_SOFTDEP("pre: qcom_tlmm_vm_irqchip");
