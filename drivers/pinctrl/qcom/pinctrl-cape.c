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
#include "pinctrl-cape.h"

static const struct msm_pinctrl_soc_data cape_pinctrl = {
	.pins = cape_pins,
	.npins = ARRAY_SIZE(cape_pins),
	.functions = cape_functions,
	.nfunctions = ARRAY_SIZE(cape_functions),
	.groups = cape_groups,
	.ngroups = ARRAY_SIZE(cape_groups),
	.ngpios = 211,
	.qup_regs = cape_qup_regs,
	.nqup_regs = ARRAY_SIZE(cape_qup_regs),
	.wakeirq_map = cape_pdc_map,
	.nwakeirq_map = ARRAY_SIZE(cape_pdc_map),
};

static int cape_pinctrl_probe(struct platform_device *pdev)
{
	return msm_pinctrl_probe(pdev, &cape_pinctrl);
}

static const struct of_device_id cape_pinctrl_of_match[] = {
	{ .compatible = "qcom,cape-pinctrl", .data = &cape_pinctrl},
	{ },
};

static struct platform_driver cape_pinctrl_driver = {
	.driver = {
		.name = "cape-pinctrl",
		.of_match_table = cape_pinctrl_of_match,
	},
	.probe = cape_pinctrl_probe,
	.remove = msm_pinctrl_remove,
};

static int __init cape_pinctrl_init(void)
{
	return platform_driver_register(&cape_pinctrl_driver);
}
arch_initcall(cape_pinctrl_init);

static void __exit cape_pinctrl_exit(void)
{
	platform_driver_unregister(&cape_pinctrl_driver);
}
module_exit(cape_pinctrl_exit);

MODULE_DESCRIPTION("QTI cape pinctrl driver");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, cape_pinctrl_of_match);
