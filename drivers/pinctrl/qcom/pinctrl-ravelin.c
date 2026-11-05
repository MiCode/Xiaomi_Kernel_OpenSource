// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>

#include "pinctrl-msm.h"
#include "pinctrl-ravelin.h"

static const struct msm_pinctrl_soc_data ravelin_tlmm = {
	.pins = ravelin_pins,
	.npins = ARRAY_SIZE(ravelin_pins),
	.functions = ravelin_functions,
	.nfunctions = ARRAY_SIZE(ravelin_functions),
	.groups = ravelin_groups,
	.ngroups = ARRAY_SIZE(ravelin_groups),
	.ngpios = 137,
	.wakeirq_map = ravelin_pdc_map,
	.nwakeirq_map = ARRAY_SIZE(ravelin_pdc_map),
	.egpio_func = 11,
};

static const struct msm_pinctrl_soc_data ravelin_vm_tlmm = {
	.pins = ravelin_pins,
	.npins = ARRAY_SIZE(ravelin_pins),
	.functions = ravelin_functions,
	.nfunctions = ARRAY_SIZE(ravelin_functions),
	.groups = ravelin_groups,
	.ngroups = ARRAY_SIZE(ravelin_groups),
	.ngpios = 137,
	.egpio_func = 11,
};

static int ravelin_tlmm_probe(struct platform_device *pdev)
{
	const struct msm_pinctrl_soc_data *pinctrl_data;

	pinctrl_data = of_device_get_match_data(&pdev->dev);
	if (!pinctrl_data)
		return -EINVAL;

	return msm_pinctrl_probe(pdev, pinctrl_data);
}

static const struct of_device_id ravelin_tlmm_of_match[] = {
	{ .compatible = "qcom,ravelin-tlmm", .data = &ravelin_tlmm},
	{ .compatible = "qcom,ravelin-vm-tlmm", .data = &ravelin_vm_tlmm},
	{ },
};

static struct platform_driver ravelin_tlmm_driver = {
	.driver = {
		.name = "ravelin-tlmm",
		.of_match_table = ravelin_tlmm_of_match,
	},
	.probe = ravelin_tlmm_probe,
	.remove = msm_pinctrl_remove,
};

static int __init ravelin_tlmm_init(void)
{
	return platform_driver_register(&ravelin_tlmm_driver);
}
arch_initcall(ravelin_tlmm_init);

static void __exit ravelin_tlmm_exit(void)
{
	platform_driver_unregister(&ravelin_tlmm_driver);
}
module_exit(ravelin_tlmm_exit);

MODULE_DESCRIPTION("QTI ravelin tlmm driver");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(of, ravelin_tlmm_of_match);
MODULE_SOFTDEP("pre: qcom_tlmm_vm_irqchip");
