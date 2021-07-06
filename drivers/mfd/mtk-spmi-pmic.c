// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Author: Argus Lin <argus.lin@mediatek.com>
 */

#include <linux/kernel.h>
#include <linux/mfd/mt6315/registers.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/pmif.h>
#include <linux/regmap.h>
#include <linux/spmi.h>

#define PMIC_VER		0x1510
#define COMMON_SUBTYPE		0x00

static const struct of_device_id pmic_spmi_id_table[] = {
	{ .compatible = "mtk,spmi-pmic", .data = (void *)COMMON_SUBTYPE },
	{ .compatible = "mediatek,mt6315", .data = (void *)COMMON_SUBTYPE },
	{ }
};

static const struct regmap_config spmi_regmap_config = {
	.reg_bits	= 16,
	.val_bits	= 8,
	.max_register	= 0x16d0,
	.fast_io	= true,
};

static const struct regmap_config spmi_regmap_config_V2 = {
	.reg_bits	= 16,
	.val_bits	= 8,
	.max_register	= 0x2000,
	.fast_io	= true,
};

static int pmic_spmi_probe(struct spmi_device *sdev)
{
	struct regmap *regmap;

	/* Only the first slave id for a PMIC contains this information */
	switch (sdev->usid) {
	case 3:
	case 6:
	case 7:
		pr_notice("%s MT6315 usid:%d\n", __func__, sdev->usid);
		regmap = devm_regmap_init_spmi_ext(sdev,
				&spmi_regmap_config);
		if (IS_ERR(regmap))
			return PTR_ERR(regmap);
		break;
	case 9:
		pr_notice("%s MT6362 usid:%d\n", __func__, sdev->usid);
		regmap = devm_regmap_init_spmi_ext(sdev,
				&spmi_regmap_config_V2);
		if (IS_ERR(regmap))
			return PTR_ERR(regmap);
		break;
	case 8:
	default:
		pr_notice("%s unknown usid:%d\n", __func__, sdev->usid);
		break;
	}
	return devm_of_platform_populate(&sdev->dev);
}

MODULE_DEVICE_TABLE(of, pmic_spmi_id_table);

static struct spmi_driver pmic_spmi_driver = {
	.probe = pmic_spmi_probe,
	.driver = {
		.name = "pmic-spmi",
		.of_match_table = pmic_spmi_id_table,
	},
};
module_spmi_driver(pmic_spmi_driver);

MODULE_DESCRIPTION("Mediatek SPMI PMIC driver");
MODULE_ALIAS("spmi:spmi-pmic");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Argus Lin <argus.lin@mediatek.com>");
