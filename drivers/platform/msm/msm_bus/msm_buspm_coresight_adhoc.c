/* Copyright (c) 2014 The Linux Foundation. All rights reserved.
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
#include <linux/list.h>

struct msmbus_coresight_adhoc_clock_drvdata {
	int				 id;
	struct clk			*clk;
	struct list_head		 list;
};

struct msmbus_coresight_adhoc_drvdata {
	struct device			*dev;
	struct coresight_device		*csdev;
	struct coresight_desc		*desc;
	struct list_head		 clocks;
};

static int msmbus_coresight_enable_adhoc(struct coresight_device *csdev)
{
	struct msmbus_coresight_adhoc_clock_drvdata *clk;
	struct msmbus_coresight_adhoc_drvdata *drvdata =
		dev_get_drvdata(csdev->dev.parent);
	long rate;

	list_for_each_entry(clk, &drvdata->clocks, list) {
		if (clk->id == csdev->id) {
			rate = clk_round_rate(clk->clk, 1L);
			clk_set_rate(clk->clk, rate);
			return clk_prepare_enable(clk->clk);
		}
	}

	return -ENOENT;
}

static void msmbus_coresight_disable_adhoc(struct coresight_device *csdev)
{
	struct msmbus_coresight_adhoc_clock_drvdata *clk;
	struct msmbus_coresight_adhoc_drvdata *drvdata =
		dev_get_drvdata(csdev->dev.parent);

	list_for_each_entry(clk, &drvdata->clocks, list) {
		if (clk->id == csdev->id)
			clk_disable_unprepare(clk->clk);
	}
}

static const struct coresight_ops_source msmbus_coresight_adhoc_source_ops = {
	.enable		= msmbus_coresight_enable_adhoc,
	.disable	= msmbus_coresight_disable_adhoc,
};

static const struct coresight_ops msmbus_coresight_cs_ops = {
	.source_ops	= &msmbus_coresight_adhoc_source_ops,
};

void msmbus_coresight_remove_adhoc(struct platform_device *pdev)
{
	struct msmbus_coresight_adhoc_clock_drvdata *clk, *next_clk;
	struct msmbus_coresight_adhoc_drvdata *drvdata =
		platform_get_drvdata(pdev);

	msmbus_coresight_disable_adhoc(drvdata->csdev);
	coresight_unregister(drvdata->csdev);
	list_for_each_entry_safe(clk, next_clk, &drvdata->clocks, list) {
		list_del(&clk->list);
		devm_kfree(&pdev->dev, clk);
	}
	devm_kfree(&pdev->dev, drvdata->desc);
	devm_kfree(&pdev->dev, drvdata);
	platform_set_drvdata(pdev, NULL);
}
EXPORT_SYMBOL(msmbus_coresight_remove_adhoc);

static int buspm_of_get_clk_adhoc(struct device_node *of_node,
	struct msmbus_coresight_adhoc_drvdata *drvdata, int id)
{
	struct msmbus_coresight_adhoc_clock_drvdata *clk;
	clk = devm_kzalloc(drvdata->dev, sizeof(*clk), GFP_KERNEL);

	if (!clk)
		return -ENOMEM;

	clk->id = id;

	clk->clk = of_clk_get_by_name(of_node, "bus_clk");
	if (IS_ERR(clk->clk)) {
		pr_err("Error: unable to get clock for coresight node %d\n",
			id);
		goto err;
	}

	list_add(&clk->list, &drvdata->clocks);
	return 0;

err:
	devm_kfree(drvdata->dev, clk);
	return -EINVAL;
}

int msmbus_coresight_init_adhoc(struct platform_device *pdev,
		struct device_node *of_node)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata;
	struct msmbus_coresight_adhoc_drvdata *drvdata;
	struct coresight_desc *desc;

	pdata = of_get_coresight_platform_data(dev, of_node);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);

	drvdata = platform_get_drvdata(pdev);
	dev_dbg(dev, "info: removed buspm module from kernel space\n");
	if (IS_ERR_OR_NULL(drvdata)) {
		drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
		if (!drvdata) {
			pr_err("coresight: Alloc for drvdata failed\n");
			return -ENOMEM;
		}
		INIT_LIST_HEAD(&drvdata->clocks);
		drvdata->dev = &pdev->dev;
		platform_set_drvdata(pdev, drvdata);
	}
	ret = buspm_of_get_clk_adhoc(of_node, drvdata, pdata->id);
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
	desc->pdata = pdata;
	desc->dev = &pdev->dev;
	desc->owner = THIS_MODULE;
	drvdata->desc = desc;
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
EXPORT_SYMBOL(msmbus_coresight_init_adhoc);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM BusPM Adhoc CoreSight Driver");
