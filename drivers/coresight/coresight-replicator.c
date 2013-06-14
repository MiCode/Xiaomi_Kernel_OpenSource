/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/of_coresight.h>
#include <linux/coresight.h>

#include "coresight-priv.h"

#define replicator_writel(drvdata, val, off)	\
				__raw_writel((val), drvdata->base + off)
#define replicator_readl(drvdata, off)		\
				__raw_readl(drvdata->base + off)

#define REPLICATOR_LOCK(drvdata)					\
do {									\
	mb();								\
	replicator_writel(drvdata, 0x0, CORESIGHT_LAR);			\
} while (0)
#define REPLICATOR_UNLOCK(drvdata)					\
do {									\
	replicator_writel(drvdata, CORESIGHT_UNLOCK, CORESIGHT_LAR);	\
	mb();								\
} while (0)

#define REPLICATOR_IDFILTER0		(0x000)
#define REPLICATOR_IDFILTER1		(0x004)
#define REPLICATOR_ITATBCTR0		(0xEFC)
#define REPLICATOR_ITATBCTR1		(0xEF8)

struct replicator_drvdata {
	void __iomem		*base;
	struct device		*dev;
	struct coresight_device	*csdev;
	struct clk		*clk;
};

static void __replicator_enable(struct replicator_drvdata *drvdata, int outport)
{
	REPLICATOR_UNLOCK(drvdata);

	if (outport == 0) {
		replicator_writel(drvdata, 0x0, REPLICATOR_IDFILTER0);
		replicator_writel(drvdata, 0xFF, REPLICATOR_IDFILTER1);
	} else {
		replicator_writel(drvdata, 0x0, REPLICATOR_IDFILTER1);
		replicator_writel(drvdata, 0xFF, REPLICATOR_IDFILTER0);
	}

	REPLICATOR_LOCK(drvdata);
}

static int replicator_enable(struct coresight_device *csdev, int inport,
			     int outport)
{
	struct replicator_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	int ret;

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	__replicator_enable(drvdata, outport);

	dev_info(drvdata->dev, "REPLICATOR enabled\n");
	return 0;
}

static void __replicator_disable(struct replicator_drvdata *drvdata,
				 int outport)
{
	REPLICATOR_UNLOCK(drvdata);

	if (outport == 0)
		replicator_writel(drvdata, 0xFF, REPLICATOR_IDFILTER0);
	else
		replicator_writel(drvdata, 0xFF, REPLICATOR_IDFILTER1);

	REPLICATOR_LOCK(drvdata);
}

static void replicator_disable(struct coresight_device *csdev, int inport,
			       int outport)
{
	struct replicator_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	__replicator_disable(drvdata, outport);

	clk_disable_unprepare(drvdata->clk);

	dev_info(drvdata->dev, "REPLICATOR disabled\n");
}

static const struct coresight_ops_link replicator_link_ops = {
	.enable		= replicator_enable,
	.disable	= replicator_disable,
};

static const struct coresight_ops replicator_cs_ops = {
	.link_ops	= &replicator_link_ops,
};

static int replicator_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata;
	struct replicator_drvdata *drvdata;
	struct resource *res;
	struct coresight_desc *desc;

	if (coresight_fuse_access_disabled())
		return -EPERM;

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

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "replicator-base");
	if (!res)
		return -ENODEV;

	drvdata->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!drvdata->base)
		return -ENOMEM;

	drvdata->clk = devm_clk_get(dev, "core_clk");
	if (IS_ERR(drvdata->clk))
		return PTR_ERR(drvdata->clk);

	ret = clk_set_rate(drvdata->clk, CORESIGHT_CLK_RATE_TRACE);
	if (ret)
		return ret;

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	desc->type = CORESIGHT_DEV_TYPE_LINK;
	desc->subtype.sink_subtype = CORESIGHT_DEV_SUBTYPE_LINK_SPLIT;
	desc->ops = &replicator_cs_ops;
	desc->pdata = pdev->dev.platform_data;
	desc->dev = &pdev->dev;
	desc->owner = THIS_MODULE;
	drvdata->csdev = coresight_register(desc);
	if (IS_ERR(drvdata->csdev))
		return PTR_ERR(drvdata->csdev);

	dev_info(dev, "REPLICATOR initialized\n");
	return 0;
}

static int replicator_remove(struct platform_device *pdev)
{
	struct replicator_drvdata *drvdata = platform_get_drvdata(pdev);

	coresight_unregister(drvdata->csdev);
	return 0;
}

static struct of_device_id replicator_match[] = {
	{.compatible = "qcom,coresight-replicator"},
	{}
};

static struct platform_driver replicator_driver = {
	.probe          = replicator_probe,
	.remove         = replicator_remove,
	.driver         = {
		.name   = "coresight-replicator",
		.owner	= THIS_MODULE,
		.of_match_table = replicator_match,
	},
};

static int __init replicator_init(void)
{
	return platform_driver_register(&replicator_driver);
}
module_init(replicator_init);

static void __exit replicator_exit(void)
{
	platform_driver_unregister(&replicator_driver);
}
module_exit(replicator_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CoreSight Replicator driver");
