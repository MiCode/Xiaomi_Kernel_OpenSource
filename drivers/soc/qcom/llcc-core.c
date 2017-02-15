/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

/* Config registers offsets*/
#define COMMON_CFG0		0x00030004
#define DRP_ECC_ERROR_CFG	0x00040000

/* TRP, DRP interrupt register offsets */
#define CMN_INTERRUPT_0_ENABLE		0x0003001C
#define CMN_INTERRUPT_2_ENABLE		0x0003003C
#define TRP_INTERRUPT_0_ENABLE		0x00020488
#define DRP_INTERRUPT_ENABLE		0x0004100C

#define DATA_RAM_ECC_ENABLE	0x1
#define SB_ERROR_THRESHOLD	0x1
#define SB_ERROR_THRESHOLD_SHIFT	24
#define SB_DB_TRP_INTERRUPT_ENABLE	0x3
#define TRP0_INTERRUPT_ENABLE	0x1
#define DRP0_INTERRUPT_ENABLE	BIT(6)
#define COMMON_INTERRUPT_0_AMON BIT(8)
#define SB_DB_DRP_INTERRUPT_ENABLE	0x3

static void qcom_llcc_core_setup(struct regmap *llcc_regmap)
{
	u32 sb_err_threshold;

	/* Enable TRP in instance 2 of common interrupt enable register */
	regmap_update_bits(llcc_regmap, CMN_INTERRUPT_2_ENABLE,
			   TRP0_INTERRUPT_ENABLE, TRP0_INTERRUPT_ENABLE);

	/* Enable ECC interrupts on Tag Ram */
	regmap_update_bits(llcc_regmap, TRP_INTERRUPT_0_ENABLE,
		SB_DB_TRP_INTERRUPT_ENABLE, SB_DB_TRP_INTERRUPT_ENABLE);

	/* Enable ECC for for data ram */
	regmap_update_bits(llcc_regmap, COMMON_CFG0,
				DATA_RAM_ECC_ENABLE, DATA_RAM_ECC_ENABLE);

	/* Enable SB error for Data RAM */
	sb_err_threshold = (SB_ERROR_THRESHOLD << SB_ERROR_THRESHOLD_SHIFT);
	regmap_write(llcc_regmap, DRP_ECC_ERROR_CFG, sb_err_threshold);

	/* Enable DRP in instance 2 of common interrupt enable register */
	regmap_update_bits(llcc_regmap, CMN_INTERRUPT_2_ENABLE,
			   DRP0_INTERRUPT_ENABLE, DRP0_INTERRUPT_ENABLE);

	/* Enable ECC interrupts on Data Ram */
	regmap_write(llcc_regmap, DRP_INTERRUPT_ENABLE,
		     SB_DB_DRP_INTERRUPT_ENABLE);

	/* Enable AMON interrupt in the common interrupt register */
	regmap_update_bits(llcc_regmap, CMN_INTERRUPT_0_ENABLE,
			COMMON_INTERRUPT_0_AMON, COMMON_INTERRUPT_0_AMON);
}

static int qcom_llcc_core_probe(struct platform_device *pdev)
{
	struct regmap *llcc_regmap;
	struct device *dev = &pdev->dev;

	llcc_regmap = syscon_node_to_regmap(dev->of_node);

	if (IS_ERR(llcc_regmap)) {
		dev_err(&pdev->dev, "Cannot find regmap for llcc\n");
		return PTR_ERR(llcc_regmap);
	}

	qcom_llcc_core_setup(llcc_regmap);

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

MODULE_DESCRIPTION("QCOM LLCC Core Driver");
MODULE_LICENSE("GPL v2");
