// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>

#include "pinctrl-msm.h"
#include "pinctrl-cinder.h"

static const struct msm_pinctrl_soc_data cinder_pinctrl = {
	.pins = cinder_pins,
	.npins = ARRAY_SIZE(cinder_pins),
	.functions = cinder_functions,
	.nfunctions = ARRAY_SIZE(cinder_functions),
	.groups = cinder_groups,
	.ngroups = ARRAY_SIZE(cinder_groups),
	.ngpios = 151,
	.qup_regs = cinder_qup_regs,
	.nqup_regs = ARRAY_SIZE(cinder_qup_regs),
};

static int cinder_pinctrl_probe(struct platform_device *pdev)
{
	return msm_pinctrl_probe(pdev, &cinder_pinctrl);
}

static const struct of_device_id cinder_pinctrl_of_match[] = {
	{ .compatible = "qcom,cinder-pinctrl", },
	{ },
};

static struct platform_driver cinder_pinctrl_driver = {
	.driver = {
		.name = "cinder-pinctrl",
		.of_match_table = cinder_pinctrl_of_match,
	},
	.probe = cinder_pinctrl_probe,
	.remove = msm_pinctrl_remove,
};

static int __init cinder_pinctrl_init(void)
{
	return platform_driver_register(&cinder_pinctrl_driver);
}
arch_initcall(cinder_pinctrl_init);

static void __exit cinder_pinctrl_exit(void)
{
	platform_driver_unregister(&cinder_pinctrl_driver);
}
module_exit(cinder_pinctrl_exit);

MODULE_DESCRIPTION("QTI cinder pinctrl driver");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, cinder_pinctrl_of_match);
