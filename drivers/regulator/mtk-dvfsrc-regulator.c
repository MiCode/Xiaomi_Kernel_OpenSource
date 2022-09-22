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

#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER)
#define CREATE_TRACE_POINTS
#include "internal.h"
#include "mtk-dvfsrc-regulator-trace.h"
#endif

#define DVFSRC_ID_VCORE		0
#define DVFSRC_ID_VSCP		1

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
	return rdev_get_dev(rdev)->parent;
}

#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER)
static int regulator_trace_consumers(struct regulator_dev *rdev, int qos_class)
{
	struct regulator *regulator;
	struct regulator_voltage *voltage;
	const char *devname;

	list_for_each_entry(regulator, &rdev->consumer_list, list) {
		voltage = &regulator->voltage[PM_SUSPEND_ON];
		devname = regulator->dev ? dev_name(regulator->dev) : "deviceless";
		trace_mtk_pm_qos_update_request_regulator(qos_class,
			voltage->min_uV / 1000, devname);
	}
	return 0;
}
#endif
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
#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER)
		regulator_trace_consumers(rdev, 20);
#endif
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
					    MTK_DVFSRC_CMD_VCORE_LEVEL_QUERY,
					    &val);
	break;
	case DVFSRC_ID_VSCP:
		ret = mtk_dvfsrc_query_info(dvfsrc_dev,
					    MTK_DVFSRC_CMD_VSCP_LEVEL_QUERY,
					    &val);
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

static const unsigned int mt8183_voltages[] = {
	725000,
	800000,
};

static struct dvfsrc_regulator mt8183_regulators[] = {
	MT_DVFSRC_REGULAR("dvfsrc-vcore", VCORE,
		mt8183_voltages),
};

static const struct dvfsrc_regulator_init_data regulator_mt8183_data = {
	.size = ARRAY_SIZE(mt8183_regulators),
	.regulator_info = &mt8183_regulators[0],
};

static const unsigned int mt6873_voltages[] = {
	575000,
	600000,
	650000,
	725000,
};

static struct dvfsrc_regulator mt6873_regulators[] = {
	MT_DVFSRC_REGULAR("dvfsrc-vcore", VCORE,
		mt6873_voltages),
	MT_DVFSRC_REGULAR("dvfsrc-vscp", VSCP,
		mt6873_voltages),
};

static const struct dvfsrc_regulator_init_data regulator_mt6873_data = {
	.size = ARRAY_SIZE(mt6873_regulators),
	.regulator_info = &mt6873_regulators[0],
};

static const unsigned int mt6853_voltages[] = {
	550000,
	600000,
	650000,
	725000,
};

static struct dvfsrc_regulator mt6853_regulators[] = {
	MT_DVFSRC_REGULAR("dvfsrc-vcore", VCORE,
		mt6853_voltages),
	MT_DVFSRC_REGULAR("dvfsrc-vscp", VSCP,
		mt6853_voltages),
};

static const struct dvfsrc_regulator_init_data regulator_mt6853_data = {
	.size = ARRAY_SIZE(mt6853_regulators),
	.regulator_info = &mt6853_regulators[0],
};

static const unsigned int mt6893_voltages[] = {
	575000,
	600000,
	650000,
	725000,
	750000,
};

static struct dvfsrc_regulator mt6893_regulators[] = {
	MT_DVFSRC_REGULAR("dvfsrc-vcore", VCORE,
		mt6893_voltages),
	MT_DVFSRC_REGULAR("dvfsrc-vscp", VSCP,
		mt6893_voltages),
};

static const struct dvfsrc_regulator_init_data regulator_mt6893_data = {
	.size = ARRAY_SIZE(mt6893_regulators),
	.regulator_info = &mt6893_regulators[0],
};

static const unsigned int mt6877_voltages[] = {
	550000,
	600000,
	650000,
	725000,
	750000,
};

static struct dvfsrc_regulator mt6877_regulators[] = {
	MT_DVFSRC_REGULAR("dvfsrc-vcore", VCORE,
		mt6877_voltages),
	MT_DVFSRC_REGULAR("dvfsrc-vscp", VSCP,
		mt6877_voltages),
};

static const struct dvfsrc_regulator_init_data regulator_mt6877_data = {
	.size = ARRAY_SIZE(mt6877_regulators),
	.regulator_info = &mt6877_regulators[0],
};

static const unsigned int mt6983_voltages[] = {
	575000,
	600000,
	650000,
	725000,
	750000,
};

static struct dvfsrc_regulator mt6983_regulators[] = {
	MT_DVFSRC_REGULAR("dvfsrc-vcore", VCORE,
		mt6983_voltages),
};

static const struct dvfsrc_regulator_init_data regulator_mt6983_data = {
	.size = ARRAY_SIZE(mt6983_regulators),
	.regulator_info = &mt6983_regulators[0],
};

static const unsigned int mt6879_voltages[] = {
	550000,
	600000,
	650000,
	725000,
	750000,
};

static struct dvfsrc_regulator mt6879_regulators[] = {
	MT_DVFSRC_REGULAR("dvfsrc-vcore", VCORE,
		mt6879_voltages),
};

static const struct dvfsrc_regulator_init_data regulator_mt6879_data = {
	.size = ARRAY_SIZE(mt6879_regulators),
	.regulator_info = &mt6879_regulators[0],
};

static const unsigned int mt6855_voltages[] = {
	550000,
	600000,
	650000,
	725000,
};

static struct dvfsrc_regulator mt6855_regulators[] = {
	MT_DVFSRC_REGULAR("dvfsrc-vcore", VCORE,
		mt6855_voltages),
};

static const struct dvfsrc_regulator_init_data regulator_mt6855_data = {
	.size = ARRAY_SIZE(mt6855_regulators),
	.regulator_info = &mt6855_regulators[0],
};

static const unsigned int mt6765_voltages[] = {
	650000,
	700000,
	700000,
	800000,
};

static struct dvfsrc_regulator mt6765_regulators[] = {
	MT_DVFSRC_REGULAR("dvfsrc-vcore", VCORE,
		mt6765_voltages),
	MT_DVFSRC_REGULAR("dvfsrc-vscp", VSCP,
		mt6765_voltages),
};

static const struct dvfsrc_regulator_init_data regulator_mt6765_data = {
	.size = ARRAY_SIZE(mt6765_regulators),
	.regulator_info = &mt6765_regulators[0],
};

static const unsigned int mt6768_voltages[] = {
	650000,
	0,
	700000,
	800000,
};

static struct dvfsrc_regulator mt6768_regulators[] = {
	MT_DVFSRC_REGULAR("dvfsrc-vcore", VCORE,
		mt6768_voltages),
	MT_DVFSRC_REGULAR("dvfsrc-vscp", VSCP,
		mt6768_voltages),
};

static const struct dvfsrc_regulator_init_data regulator_mt6768_data = {
	.size = ARRAY_SIZE(mt6768_regulators),
	.regulator_info = &mt6768_regulators[0],
};

/* MT6765  will share MT6768 driver data due to same IP */

static const struct of_device_id mtk_dvfsrc_regulator_match[] = {
	{
		.compatible = "mediatek,mt8183-dvfsrc",
		.data = &regulator_mt8183_data,
	}, {
		.compatible = "mediatek,mt6873-dvfsrc",
		.data = &regulator_mt6873_data,
	}, {
		.compatible = "mediatek,mt6853-dvfsrc",
		.data = &regulator_mt6853_data,
	}, {
		.compatible = "mediatek,mt6789-dvfsrc",
		.data = &regulator_mt6853_data,
	}, {
		.compatible = "mediatek,mt6885-dvfsrc",
		.data = &regulator_mt6873_data,
	}, {
		.compatible = "mediatek,mt6893-dvfsrc",
		.data = &regulator_mt6893_data,
	}, {
		.compatible = "mediatek,mt6833-dvfsrc",
		.data = &regulator_mt6853_data,
	}, {
		.compatible = "mediatek,mt6877-dvfsrc",
		.data = &regulator_mt6877_data,
	}, {
		.compatible = "mediatek,mt6983-dvfsrc",
		.data = &regulator_mt6983_data,
	}, {
		.compatible = "mediatek,mt6879-dvfsrc",
		.data = &regulator_mt6879_data,
	}, {
		.compatible = "mediatek,mt6895-dvfsrc",
		.data = &regulator_mt6983_data,
	}, {
		.compatible = "mediatek,mt6855-dvfsrc",
		.data = &regulator_mt6855_data,
	}, {
		.compatible = "mediatek,mt6768-dvfsrc",
		.data = &regulator_mt6768_data,
	}, {
		.compatible = "mediatek,mt6765-dvfsrc",
		.data = &regulator_mt6768_data,
	}, {
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, mtk_dvfsrc_regulator_match);

static int dvfsrc_vcore_regulator_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct device *dev = &pdev->dev;
	struct regulator_config config = { };
	struct regulator_dev *rdev;
	const struct dvfsrc_regulator_init_data *regulator_init_data;
	struct dvfsrc_regulator *mt_regulators;
	int i;

	match = of_match_node(mtk_dvfsrc_regulator_match, dev->parent->of_node);

	if (!match) {
		dev_err(dev, "invalid compatible string\n");
		return -ENODEV;
	}

	regulator_init_data = match->data;

	mt_regulators = regulator_init_data->regulator_info;
	for (i = 0; i < regulator_init_data->size; i++) {
		config.dev = dev->parent;
		config.driver_data = (mt_regulators + i);
		rdev = devm_regulator_register(dev->parent,
				&(mt_regulators + i)->desc, &config);
		if (IS_ERR(rdev)) {
			dev_err(dev, "failed to register %s\n",
				(mt_regulators + i)->desc.name);
			return PTR_ERR(rdev);
		}
	}

	return 0;
}

static struct platform_driver mtk_dvfsrc_regulator_driver = {
	.driver = {
		.name  = "mtk-dvfsrc-regulator",
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
