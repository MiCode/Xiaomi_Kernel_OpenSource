// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/soc/mediatek/mtk_dvfsrc.h>

static inline struct device *to_dvfsrc_dev(struct regulator_dev *rdev)
{
	return rdev_get_dev(rdev)->parent->parent;
}

static int dvfsrc_set_voltage_sel(struct regulator_dev *rdev,
				   unsigned int selector)
{
	struct device *dvfsrc_dev = to_dvfsrc_dev(rdev);

	mtk_dvfsrc_send_request(dvfsrc_dev, MTK_DVFSRC_CMD_VCORE_REQUEST,
				selector);

	return 0;
}

static int dvfsrc_get_voltage_sel(struct regulator_dev *rdev)
{
	struct device *dvfsrc_dev = to_dvfsrc_dev(rdev);
	int val, ret;

	ret = mtk_dvfsrc_query_info(dvfsrc_dev, MTK_DVFSRC_CMD_VCORE_QUERY,
				    &val);

	if (ret != 0)
		return ret;

	return val;
}

static struct regulator_ops dvfsrc_vcore_ops = {
	.list_voltage = regulator_list_voltage_table,
	.get_voltage_sel = dvfsrc_get_voltage_sel,
	.set_voltage_sel = dvfsrc_set_voltage_sel,
};

static const unsigned int mt8183_voltages[] = {
	725000,
	800000,
};

static const struct regulator_desc regulator_mt8183_data = {
	.name = "dvfsrc-vcore",
	.of_match = of_match_ptr("dvfsrc-vcore"),
	.ops = &dvfsrc_vcore_ops,
	.type = REGULATOR_VOLTAGE,
	.id = 0,
	.owner = THIS_MODULE,
	.n_voltages = ARRAY_SIZE(mt8183_voltages),
	.volt_table = mt8183_voltages,
};

static int dvfsrc_vcore_regulator_probe(struct platform_device *pdev)
{
	struct regulator_config config = { };
	struct regulator_dev *rdev;
	const struct regulator_desc *init_data;

	init_data = of_device_get_match_data(&pdev->dev);
	if (!init_data)
		return -EINVAL;

	config.dev = &pdev->dev;
	rdev = devm_regulator_register(&pdev->dev, init_data, &config);
	if (IS_ERR(rdev)) {
		dev_err(&pdev->dev, "failed to register %s\n",
			init_data->name);
		return PTR_ERR(rdev);
	}

	return 0;
}

static const struct of_device_id mtk_dvfsrc_regulator_match[] = {
	{
		.compatible = "mediatek,mt8183-dvfsrc-regulator",
		.data = &regulator_mt8183_data,
	}, {
		/* sentinel */
	},
};

static struct platform_driver mtk_dvfsrc_regulator_driver = {
	.driver = {
		.name  = "mtk-dvfsrc-regulator",
		.of_match_table = mtk_dvfsrc_regulator_match,
	},
	.probe = dvfsrc_vcore_regulator_probe,
};

static int __init mtk_dvfsrc_regulator_init(void)
{
	return platform_driver_register(&mtk_dvfsrc_regulator_driver);
}
subsys_initcall(mtk_dvfsrc_regulator_init);

static void __exit mtk_dvfsrc_regulator_exit(void)
{
	platform_driver_unregister(&mtk_dvfsrc_regulator_driver);
}
module_exit(mtk_dvfsrc_regulator_exit);

MODULE_AUTHOR("Arvin wang <arvin.wang@mediatek.com>");
MODULE_LICENSE("GPL v2");
