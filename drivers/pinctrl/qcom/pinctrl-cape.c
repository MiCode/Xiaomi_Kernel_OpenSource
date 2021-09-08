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

static const struct msm_gpio_wakeirq_map cape_pdc_map[] = {
	{ 2, 70 }, { 3, 77 }, { 7, 52 }, { 8, 108 }, { 10, 128 }, { 11, 53 },
	{ 12, 129 }, { 13, 130 }, { 14, 131 }, { 15, 67 }, { 19, 69 }, { 21, 132 },
	{ 23, 54 }, { 26, 56 }, { 27, 71 }, { 28, 57 }, { 31, 55 }, { 32, 58 },
	{ 34, 72 }, { 35, 43 }, { 36, 78 }, { 38, 79 }, { 39, 62 }, { 40, 80 },
	{ 41, 133 }, { 43, 81 }, { 44, 87 }, { 45, 134 }, { 46, 66 }, { 47, 63 },
	{ 50, 88 }, { 51, 89 }, { 55, 90 }, { 56, 59 }, { 59, 82 }, { 60, 60 },
	{ 62, 135 }, { 63, 91 }, { 66, 136 }, { 67, 44 }, { 69, 137 }, { 71, 97 },
	{ 75, 73 }, { 79, 74 }, { 80, 96 }, { 81, 98 }, { 82, 45 }, { 83, 99 },
	{ 84, 94 }, { 85, 100 }, { 86, 101 }, { 87, 102 }, { 88, 92 }, { 89, 83 },
	{ 90, 84 }, { 91, 85 }, { 92, 46 }, { 95, 103 }, { 96, 104 }, { 98, 105 },
	{ 99, 106 }, { 115, 95 }, { 116, 76 }, { 117, 75 }, { 118, 86 }, { 119, 93 },
	{ 133, 47 }, { 137, 42 }, { 148, 61 }, { 150, 68 }, { 153, 65 }, { 154, 48 },
	{ 155, 49 }, { 156, 64 }, { 159, 50 }, { 162, 51 }, { 166, 111 }, { 169, 114 },
	{ 171, 115 }, { 172, 116 }, { 174, 117 }, { 176, 107 }, { 181, 109 }, { 182, 110 },
	{ 185, 112 }, { 187, 113 }, { 188, 118 }, { 190, 122 }, { 192, 123 }, { 195, 124 },
	{ 201, 119 }, { 203, 120 }, { 205, 121 },
};

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
