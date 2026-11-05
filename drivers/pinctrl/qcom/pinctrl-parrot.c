// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>

#include "pinctrl-msm.h"
#include "pinctrl-parrot.h"

static const struct msm_pinctrl_soc_data parrot_tlmm = {
	.pins = parrot_pins,
	.npins = ARRAY_SIZE(parrot_pins),
	.functions = parrot_functions,
	.nfunctions = ARRAY_SIZE(parrot_functions),
	.groups = parrot_groups,
	.ngroups = ARRAY_SIZE(parrot_groups),
	.ngpios = 142,
	.wakeirq_map = parrot_pdc_map,
	.nwakeirq_map = ARRAY_SIZE(parrot_pdc_map),
	.egpio_func = 11,
};

static const struct msm_pinctrl_soc_data parrot_vm_tlmm = {
	.pins = parrot_pins,
	.npins = ARRAY_SIZE(parrot_pins),
	.functions = parrot_functions,
	.nfunctions = ARRAY_SIZE(parrot_functions),
	.groups = parrot_groups,
	.ngroups = ARRAY_SIZE(parrot_groups),
	.ngpios = 142,
	.egpio_func = 11,
};

static int parrot_tlmm_probe(struct platform_device *pdev)
{
	const struct msm_pinctrl_soc_data *pinctrl_data;

	pinctrl_data = of_device_get_match_data(&pdev->dev);
	if (!pinctrl_data)
		return -EINVAL;

	return msm_pinctrl_probe(pdev, pinctrl_data);
}

static const struct of_device_id parrot_tlmm_of_match[] = {
	{ .compatible = "qcom,parrot-tlmm", .data = &parrot_tlmm},
	{ .compatible = "qcom,parrot-vm-tlmm", .data = &parrot_vm_tlmm},
	{ },
};

static struct platform_driver parrot_tlmm_driver = {
	.driver = {
		.name = "parrot-pinctrl",
		.of_match_table = parrot_tlmm_of_match,
	},
	.probe = parrot_tlmm_probe,
	.remove = msm_pinctrl_remove,
};

static int __init parrot_tlmm_init(void)
{
	return platform_driver_register(&parrot_tlmm_driver);
}
arch_initcall(parrot_tlmm_init);

static void __exit parrot_tlmm_exit(void)
{
	platform_driver_unregister(&parrot_tlmm_driver);
}
module_exit(parrot_tlmm_exit);

MODULE_DESCRIPTION("QTI parrot TLMM driver");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(of, parrot_tlmm_of_match);
MODULE_SOFTDEP("pre: qcom_tlmm_vm_irqchip");
