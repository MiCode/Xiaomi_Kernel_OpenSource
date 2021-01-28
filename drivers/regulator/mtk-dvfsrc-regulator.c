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

#define DVFSRC_ID_VCORE		0
#define DVFSRC_ID_VSCP		1


/*
 * DVFSRC regulators' information
 *
 * @desc: standard fields of regulator description.
 * @voltage_selector:  Selector used for get_voltage_sel() and
 *			   set_voltage_sel() callbacks
 */
struct dvfsrc_regulator {
	struct regulator_desc	desc;
};

/*
 * MTK DVFSRC regulators' init data
 *
 * @size: num of regulators
 * @regulator_info: regulator info.
 */
struct dvfsrc_regulator_init_data {
	u32 size;
	struct dvfsrc_regulator *regulator_info;
};

static inline struct device *to_dvfsrc_dev(struct regulator_dev *rdev)
{
	return rdev_get_dev(rdev)->parent->parent;
}

static int dvfsrc_set_voltage_sel(struct regulator_dev *rdev,
				   unsigned int selector)
{
	struct device *dvfsrc_dev = to_dvfsrc_dev(rdev);
	int id = rdev_get_id(rdev);

	switch (id) {
	case DVFSRC_ID_VCORE:
		mtk_dvfsrc_send_request(dvfsrc_dev,
					MTK_DVFSRC_CMD_VCORE_REQUEST,
					selector);
	break;
	case DVFSRC_ID_VSCP:
		mtk_dvfsrc_send_request(dvfsrc_dev,
					MTK_DVFSRC_CMD_VSCP_REQUEST,
					selector);
	break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int dvfsrc_get_voltage_sel(struct regulator_dev *rdev)
{
	struct device *dvfsrc_dev = to_dvfsrc_dev(rdev);
	int id = rdev_get_id(rdev);
	int val, ret;

	switch (id) {
	case DVFSRC_ID_VCORE:
		ret = mtk_dvfsrc_query_info(dvfsrc_dev,
				MTK_DVFSRC_CMD_VCORE_QUERY, &val);
	break;
	case DVFSRC_ID_VSCP:
		ret = mtk_dvfsrc_query_info(dvfsrc_dev,
				MTK_DVFSRC_CMD_VCP_QUERY, &val);
	break;
	default:
		return -EINVAL;
	}

	if (ret != 0)
		return ret;

	return val;
}

static struct regulator_ops dvfsrc_vcore_ops = {
	.list_voltage = regulator_list_voltage_table,
	.get_voltage_sel = dvfsrc_get_voltage_sel,
	.set_voltage_sel = dvfsrc_set_voltage_sel,
};

#define MT_DVFSRC_REGULAR(match, _name,	_volt_table)	\
[DVFSRC_ID_##_name] = {					\
	.desc = {					\
		.name = match,				\
		.of_match = of_match_ptr(match),	\
		.ops = &dvfsrc_vcore_ops,		\
		.type = REGULATOR_VOLTAGE,		\
		.id = DVFSRC_ID_##_name,		\
		.owner = THIS_MODULE,			\
		.n_voltages = ARRAY_SIZE(_volt_table),	\
		.volt_table = _volt_table,		\
	},	\
}

static const unsigned int mt6779_voltages[] = {
	650000,
	725000,
	825000,
};

static struct dvfsrc_regulator mt6779_regulators[] = {
	MT_DVFSRC_REGULAR("dvfsrc-vcore", VCORE,
		mt6779_voltages),
	MT_DVFSRC_REGULAR("dvfsrc-vscp", VSCP,
		mt6779_voltages),
};

static const struct dvfsrc_regulator_init_data regulator_mt6779_data = {
	.size = ARRAY_SIZE(mt6779_regulators),
	.regulator_info = &mt6779_regulators[0],
};

static const unsigned int mt6761_voltages[] = {
	650000,
	0,
	700000,
	800000,
};

static struct dvfsrc_regulator mt6761_regulators[] = {
	MT_DVFSRC_REGULAR("dvfsrc-vcore", VCORE,
		mt6761_voltages),
	MT_DVFSRC_REGULAR("dvfsrc-vscp", VSCP,
		mt6761_voltages),
};

static const struct dvfsrc_regulator_init_data regulator_mt6761_data = {
	.size = ARRAY_SIZE(mt6761_regulators),
	.regulator_info = &mt6761_regulators[0],
};

static int dvfsrc_vcore_regulator_probe(struct platform_device *pdev)
{
	struct regulator_config config = { };
	struct regulator_dev *rdev;
	const struct dvfsrc_regulator_init_data *regulator_init_data;
	struct dvfsrc_regulator *mt_regulators;
	int i;

	regulator_init_data = of_device_get_match_data(&pdev->dev);
	if (!regulator_init_data)
		return -EINVAL;

	mt_regulators = regulator_init_data->regulator_info;

	for (i = 0; i < regulator_init_data->size; i++) {
		config.dev = &pdev->dev;
		config.driver_data = (mt_regulators + i);
		rdev = devm_regulator_register(&pdev->dev,
				&(mt_regulators + i)->desc, &config);
		if (IS_ERR(rdev)) {
			dev_notice(&pdev->dev, "failed to register %s\n",
				(mt_regulators + i)->desc.name);
			return PTR_ERR(rdev);
		}
	}

	dev_info(&pdev->dev, "initialized mtk,dvfsrc regulator\n");
	return 0;
}

static const struct of_device_id mtk_dvfsrc_regulator_match[] = {
	{ .compatible = "mediatek,dvfsrc-mt6779-regulator",
		.data = &regulator_mt6779_data },
	{ .compatible = "mediatek,dvfsrc-mt6761-regulator",
		.data = &regulator_mt6761_data },
	{},
};

static struct platform_driver mtk_dvfsrc_regulator_driver = {
	.driver = {
		.name  = "mtk_dvfsrc-regulator",
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

