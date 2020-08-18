/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/pm_qos.h>

static struct pm_qos_request boost_ddr_opp_req;

struct mtk_dvfsrc_boost {
	struct device *dev;
	int perf_state;
};

static ssize_t dramboost_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	struct mtk_dvfsrc_boost *boost = dev_get_drvdata(dev);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val == 1)
		pm_qos_update_request(&boost_ddr_opp_req,
						boost->perf_state);
	else
		pm_qos_update_request(&boost_ddr_opp_req,
					PM_QOS_DDR_OPP_DEFAULT_VALUE);

	return count;
}

static DEVICE_ATTR_WO(dramboost);


static struct attribute *dvfsrc_boost_attributes[] = {
	&dev_attr_dramboost.attr,
	NULL,
};

static struct attribute_group dvfsrc_boost_attr_group = {
	.name = "dramboost",
	.attrs = dvfsrc_boost_attributes,
};

static int mtk_dvfsrc_boost_probe(struct platform_device *pdev)
{
	struct mtk_dvfsrc_boost *dramboost;
	struct device *dev = &pdev->dev;
	struct device_node *node = pdev->dev.of_node;
	int result;

	dramboost = devm_kzalloc(dev, sizeof(*dramboost), GFP_KERNEL);
	if (!dramboost)
		return -ENOMEM;

	if (of_property_read_u32(node, "boost_opp",
		(u32 *) &dramboost->perf_state))
		dramboost->perf_state = 0;

	if (dramboost->perf_state < 0)
		return -EINVAL;

	dramboost->dev = dev;
	platform_set_drvdata(pdev, dramboost);
	pm_qos_add_request(&boost_ddr_opp_req, PM_QOS_DDR_OPP,
			PM_QOS_DDR_OPP_DEFAULT_VALUE);

	result = sysfs_create_group(&dev->kobj, &dvfsrc_boost_attr_group);

	return result;
}

static int mtk_dvfsrc_boost_remove(struct platform_device *pdev)
{
	return 0;
}


static const struct of_device_id mtk_dvfsrc_boost_of_match[] = {
	{
		.compatible = "mediatek,dvfsrc-boost",
	}, {
		/* sentinel */
	},
};

static struct platform_driver mtk_dvfsrc_boost_driver = {
	.probe	= mtk_dvfsrc_boost_probe,
	.remove	= mtk_dvfsrc_boost_remove,
	.driver = {
		.name = "mtk-dvfsrc-boost",
		.of_match_table = of_match_ptr(mtk_dvfsrc_boost_of_match),
	},
};

static int __init mtk_dvfsrc_boost_init(void)
{
	return platform_driver_register(&mtk_dvfsrc_boost_driver);
}
late_initcall(mtk_dvfsrc_boost_init)

static void __exit mtk_dvfsrc_boost_exit(void)
{
	platform_driver_unregister(&mtk_dvfsrc_boost_driver);
}
module_exit(mtk_dvfsrc_boost_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MTK dvfsrc boost driver");

