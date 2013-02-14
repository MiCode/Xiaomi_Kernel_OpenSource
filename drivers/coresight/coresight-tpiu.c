/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

#define tpiu_writel(drvdata, val, off)	__raw_writel((val), drvdata->base + off)
#define tpiu_readl(drvdata, off)	__raw_readl(drvdata->base + off)

#define TPIU_LOCK(drvdata)						\
do {									\
	mb();								\
	tpiu_writel(drvdata, 0x0, CORESIGHT_LAR);			\
} while (0)
#define TPIU_UNLOCK(drvdata)						\
do {									\
	tpiu_writel(drvdata, CORESIGHT_UNLOCK, CORESIGHT_LAR);		\
	mb();								\
} while (0)

#define TPIU_SUPP_PORTSZ	(0x000)
#define TPIU_CURR_PORTSZ	(0x004)
#define TPIU_SUPP_TRIGMODES	(0x100)
#define TPIU_TRIG_CNTRVAL	(0x104)
#define TPIU_TRIG_MULT		(0x108)
#define TPIU_SUPP_TESTPATM	(0x200)
#define TPIU_CURR_TESTPATM	(0x204)
#define TPIU_TEST_PATREPCNTR	(0x208)
#define TPIU_FFSR		(0x300)
#define TPIU_FFCR		(0x304)
#define TPIU_FSYNC_CNTR		(0x308)
#define TPIU_EXTCTL_INPORT	(0x400)
#define TPIU_EXTCTL_OUTPORT	(0x404)
#define TPIU_ITTRFLINACK	(0xEE4)
#define TPIU_ITTRFLIN		(0xEE8)
#define TPIU_ITATBDATA0		(0xEEC)
#define TPIU_ITATBCTR2		(0xEF0)
#define TPIU_ITATBCTR1		(0xEF4)
#define TPIU_ITATBCTR0		(0xEF8)

struct tpiu_drvdata {
	void __iomem		*base;
	struct device		*dev;
	struct coresight_device	*csdev;
	struct clk		*clk;
};

static void __tpiu_enable(struct tpiu_drvdata *drvdata)
{
	TPIU_UNLOCK(drvdata);

	/* TODO: fill this up */

	TPIU_LOCK(drvdata);
}

static int tpiu_enable(struct coresight_device *csdev)
{
	struct tpiu_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	int ret;

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	__tpiu_enable(drvdata);

	dev_info(drvdata->dev, "TPIU enabled\n");
	return 0;
}

static void __tpiu_disable(struct tpiu_drvdata *drvdata)
{
	TPIU_UNLOCK(drvdata);

	tpiu_writel(drvdata, 0x3000, TPIU_FFCR);
	tpiu_writel(drvdata, 0x3040, TPIU_FFCR);

	TPIU_LOCK(drvdata);
}

static void tpiu_disable(struct coresight_device *csdev)
{
	struct tpiu_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	__tpiu_disable(drvdata);

	clk_disable_unprepare(drvdata->clk);

	dev_info(drvdata->dev, "TPIU disabled\n");
}

static void tpiu_abort(struct coresight_device *csdev)
{
	struct tpiu_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	__tpiu_disable(drvdata);

	dev_info(drvdata->dev, "TPIU aborted\n");
}

static const struct coresight_ops_sink tpiu_sink_ops = {
	.enable		= tpiu_enable,
	.disable	= tpiu_disable,
	.abort		= tpiu_abort,
};

static const struct coresight_ops tpiu_cs_ops = {
	.sink_ops	= &tpiu_sink_ops,
};

static int __devinit tpiu_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata;
	struct tpiu_drvdata *drvdata;
	struct resource *res;
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

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
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

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	/* Disable tpiu to support older targets that need this */
	__tpiu_disable(drvdata);

	clk_disable_unprepare(drvdata->clk);

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	desc->type = CORESIGHT_DEV_TYPE_SINK;
	desc->subtype.sink_subtype = CORESIGHT_DEV_SUBTYPE_SINK_PORT;
	desc->ops = &tpiu_cs_ops;
	desc->pdata = pdev->dev.platform_data;
	desc->dev = &pdev->dev;
	desc->owner = THIS_MODULE;
	drvdata->csdev = coresight_register(desc);
	if (IS_ERR(drvdata->csdev))
		return PTR_ERR(drvdata->csdev);

	dev_info(dev, "TPIU initialized\n");
	return 0;
}

static int __devexit tpiu_remove(struct platform_device *pdev)
{
	struct tpiu_drvdata *drvdata = platform_get_drvdata(pdev);

	coresight_unregister(drvdata->csdev);
	return 0;
}

static struct of_device_id tpiu_match[] = {
	{.compatible = "arm,coresight-tpiu"},
	{}
};

static struct platform_driver tpiu_driver = {
	.probe          = tpiu_probe,
	.remove         = __devexit_p(tpiu_remove),
	.driver         = {
		.name   = "coresight-tpiu",
		.owner	= THIS_MODULE,
		.of_match_table = tpiu_match,
	},
};

static int __init tpiu_init(void)
{
	return platform_driver_register(&tpiu_driver);
}
module_init(tpiu_init);

static void __exit tpiu_exit(void)
{
	platform_driver_unregister(&tpiu_driver);
}
module_exit(tpiu_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CoreSight Trace Port Interface Unit driver");
