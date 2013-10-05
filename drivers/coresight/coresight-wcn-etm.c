/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/of_coresight.h>
#include <linux/coresight.h>

struct wcn_etm_drvdata {
	struct device			*dev;
	struct coresight_device		*csdev;
};

static int wcn_etm_enable(struct coresight_device *csdev)
{
	struct wcn_etm_drvdata *drvdata =
		dev_get_drvdata(csdev->dev.parent);

	dev_info(drvdata->dev, "Wireless ETM tracing enabled\n");
	return 0;
}


static void wcn_etm_disable(struct coresight_device *csdev)
{
	struct wcn_etm_drvdata *drvdata =
		dev_get_drvdata(csdev->dev.parent);

	dev_info(drvdata->dev, "Wireless ETM tracing disabled\n");
}

static const struct coresight_ops_source wcn_etm_source_ops = {
	.enable		= wcn_etm_enable,
	.disable	= wcn_etm_disable,
};

static const struct coresight_ops wcn_cs_ops = {
	.source_ops	= &wcn_etm_source_ops,
};

static int wcn_etm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata;
	struct wcn_etm_drvdata *drvdata;
	struct coresight_desc *desc;

	if (pdev->dev.of_node) {
		pdata = of_get_coresight_platform_data(dev, pdev->dev.of_node);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);
		pdev->dev.platform_data = pdata;
	}

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	desc->type = CORESIGHT_DEV_TYPE_SOURCE;
	desc->subtype.source_subtype = CORESIGHT_DEV_SUBTYPE_SOURCE_PROC;
	desc->ops = &wcn_cs_ops;
	desc->pdata = pdev->dev.platform_data;
	desc->dev = &pdev->dev;
	desc->owner = THIS_MODULE;
	drvdata->csdev = coresight_register(desc);
	if (IS_ERR(drvdata->csdev))
		return  PTR_ERR(drvdata->csdev);

	dev_info(dev, "Wireless ETM initialized\n");
	return 0;
}

static int wcn_etm_remove(struct platform_device *pdev)
{
	struct wcn_etm_drvdata *drvdata = platform_get_drvdata(pdev);

	coresight_unregister(drvdata->csdev);
	return 0;
}

static struct of_device_id wcn_etm_match[] = {
	{.compatible = "qcom,coresight-wcn-etm"},
	{}
};

static struct platform_driver wcn_etm_driver = {
	.probe          = wcn_etm_probe,
	.remove         = wcn_etm_remove,
	.driver         = {
		.name   = "coresight-wcn-etm",
		.owner	= THIS_MODULE,
		.of_match_table = wcn_etm_match,
	},
};

int __init wcn_etm_init(void)
{
	return platform_driver_register(&wcn_etm_driver);
}
module_init(wcn_etm_init);

void __exit wcn_etm_exit(void)
{
	platform_driver_unregister(&wcn_etm_driver);
}
module_exit(wcn_etm_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CoreSight Wireless ETM driver");
