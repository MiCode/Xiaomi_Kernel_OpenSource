/* Copyright (c) 2016-2017, 2019 The Linux Foundation. All rights reserved.
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

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

static int qcom_llcc_core_probe(struct platform_device *pdev)
{
	struct regmap *llcc_regmap;
	struct device *dev = &pdev->dev;
	u32 b_off = 0;
	int ret = 0;

	llcc_regmap = syscon_node_to_regmap(dev->of_node);

	if (IS_ERR(llcc_regmap)) {
		dev_err(&pdev->dev, "Cannot find regmap for llcc\n");
		return PTR_ERR(llcc_regmap);
	}

	ret = of_property_read_u32(dev->of_node,
			"qcom,llcc-broadcast-off", &b_off);
	if (ret) {
		dev_err(&pdev->dev, "Unable to read broadcast-off\n");
		return -EINVAL;
	}

	return 0;
}

static int qcom_llcc_core_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id qcom_llcc_core_match_table[] = {
	{ .compatible = "qcom,llcc-core" },
	{ },
};

static struct platform_driver qcom_llcc_core_driver = {
	.probe = qcom_llcc_core_probe,
	.remove = qcom_llcc_core_remove,
	.driver = {
		.name = "qcom_llcc_core",
		.owner = THIS_MODULE,
		.of_match_table = qcom_llcc_core_match_table,
	},
};

static int __init qcom_llcc_core_init(void)
{
	return platform_driver_register(&qcom_llcc_core_driver);
}
module_init(qcom_llcc_core_init);

static void __exit qcom_llcc_core_exit(void)
{
	platform_driver_unregister(&qcom_llcc_core_driver);
}
module_exit(qcom_llcc_core_exit);

MODULE_DESCRIPTION("Qualcomm Technologies Inc LLCC Core Driver");
MODULE_LICENSE("GPL v2");
