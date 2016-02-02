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

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/of_coresight.h>
#include <linux/coresight.h>
#include <linux/io.h>
#include <linux/of.h>

struct msmbus_coresight_drvdata {
	struct device			*dev;
	struct coresight_device		*csdev;
	struct clk			*clk;
	const char			*clk_name;
	const char			*clknode;
};

static int msmbus_coresight_enable(struct coresight_device *csdev)
{
	struct msmbus_coresight_drvdata *drvdata =
		dev_get_drvdata(csdev->dev.parent);

	return clk_prepare_enable(drvdata->clk);
}

static void msmbus_coresight_disable(struct coresight_device *csdev)
{
	struct msmbus_coresight_drvdata *drvdata =
		dev_get_drvdata(csdev->dev.parent);

	clk_disable_unprepare(drvdata->clk);
}

static const struct coresight_ops_source msmbus_coresight_source_ops = {
	.enable		= msmbus_coresight_enable,
	.disable	= msmbus_coresight_disable,
};

static const struct coresight_ops msmbus_coresight_cs_ops = {
	.source_ops	= &msmbus_coresight_source_ops,
};

void msmbus_coresight_remove(struct platform_device *pdev)
{
	struct msmbus_coresight_drvdata *drvdata = platform_get_drvdata(pdev);

	msmbus_coresight_disable(drvdata->csdev);
	coresight_unregister(drvdata->csdev);
	devm_kfree(&pdev->dev, drvdata);
	platform_set_drvdata(pdev, NULL);
}
EXPORT_SYMBOL(msmbus_coresight_remove);

static int buspm_of_get_clk(struct device_node *of_node,
	struct msmbus_coresight_drvdata *drvdata)
{
	if (of_property_read_string(of_node, "qcom,fabclk-dual",
						&drvdata->clk_name)) {
		pr_err("Error: Unable to find clock from of_node\n");
		return -EINVAL;
	}

	if (of_property_read_string(of_node, "label", &drvdata->clknode)) {
		pr_err("Error: Unable to find clock-node from of_node\n");
		return -EINVAL;
	}

	drvdata->clk = clk_get_sys(drvdata->clknode, drvdata->clk_name);
	if (IS_ERR(drvdata->clk)) {
		pr_err("Error: clk_get_sys failed for: %s\n",
			drvdata->clknode);
		return -EINVAL;
	}

	return 0;
}

int msmbus_coresight_init(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata;
	struct msmbus_coresight_drvdata *drvdata;
	struct coresight_desc *desc;

	if (pdev->dev.of_node) {
		pdata = of_get_coresight_platform_data(dev, pdev->dev.of_node);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);
		pdev->dev.platform_data = pdata;
	}

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata) {
		pr_err("coresight: Alloc for drvdata failed\n");
		return -ENOMEM;
	}

	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);
	ret = buspm_of_get_clk(pdev->dev.of_node, drvdata);
	if (ret) {
		pr_err("Error getting clocks\n");
		ret = -ENXIO;
		goto err1;
	}

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc) {
		pr_err("coresight: Error allocating memory\n");
		ret = -ENOMEM;
		goto err1;
	}

	desc->type = CORESIGHT_DEV_TYPE_SOURCE;
	desc->subtype.source_subtype = CORESIGHT_DEV_SUBTYPE_SOURCE_BUS;
	desc->ops = &msmbus_coresight_cs_ops;
	desc->pdata = pdev->dev.platform_data;
	desc->dev = &pdev->dev;
	desc->owner = THIS_MODULE;
	drvdata->csdev = coresight_register(desc);
	if (IS_ERR(drvdata->csdev)) {
		pr_err("coresight: Coresight register failed\n");
		ret = PTR_ERR(drvdata->csdev);
		goto err0;
	}

	dev_info(dev, "msmbus_coresight initialized\n");

	return 0;
err0:
	devm_kfree(dev, desc);
err1:
	devm_kfree(dev, drvdata);
	platform_set_drvdata(pdev, NULL);
	return ret;
}
EXPORT_SYMBOL(msmbus_coresight_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM BusPM CoreSight Driver");
