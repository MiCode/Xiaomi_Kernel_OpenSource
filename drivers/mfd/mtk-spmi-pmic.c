// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Author: Argus Lin <argus.lin@mediatek.com>
 */

#include <linux/mfd/mt6315/registers.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spmi.h>
#include <linux/pmif.h>
#include <linux/regmap.h>
#include <linux/of_platform.h>

#define PMIC_VER		0x1510
#define COMMON_SUBTYPE		0x00

static const struct of_device_id pmic_spmi_id_table[] = {
	{ .compatible = "mtk,spmi-pmic", .data = (void *)COMMON_SUBTYPE },
	{ .compatible = "mediatek,mt6315", .data = (void *)COMMON_SUBTYPE },
	{ }
};

static void pmic_spmi_rw_test(struct regmap *map, struct device *dev)
{
	unsigned int rdata = 0, addr = 0;
	u8 wdata = 0;
	int ret;

	ret = regmap_read(map, MT6315_PMIC_TOP_MDB_RSV1_ADDR, &rdata);
	if (ret < 0)
		return;

	wdata = 0xab;
	ret = regmap_write(map, MT6315_PMIC_TOP_MDB_RSV1_ADDR, wdata);
	ret = regmap_read(map, MT6315_PMIC_TOP_MDB_RSV1_ADDR, &rdata);
	if (ret < 0)
		return;

	wdata = 0xcd;
	ret = regmap_write(map, addr, wdata);
	ret = regmap_read(map, MT6315_PMIC_TOP_MDB_RSV1_ADDR, &rdata);
	if (ret < 0)
		return;
}

static const struct regmap_config spmi_regmap_config = {
	.reg_bits	= 16,
	.val_bits	= 8,
	.max_register	= 0xffff,
	.fast_io	= true,
};

static int pmic_spmi_probe(struct spmi_device *sdev)
{
	struct regmap *regmap;

	regmap = devm_regmap_init_spmi_ext(sdev, &spmi_regmap_config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	/* Only the first slave id for a PMIC contains this information */
	pmic_spmi_rw_test(regmap, &sdev->dev);

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
