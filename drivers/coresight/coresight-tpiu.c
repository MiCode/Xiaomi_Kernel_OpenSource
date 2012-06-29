/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/coresight.h>

#include "coresight-priv.h"

#define tpiu_writel(drvdata, val, off)	__raw_writel((val), drvdata->base + off)
#define tpiu_readl(drvdata, off)	__raw_readl(drvdata->base + off)

#define TPIU_SUPP_PORTSZ				(0x000)
#define TPIU_CURR_PORTSZ				(0x004)
#define TPIU_SUPP_TRIGMODES				(0x100)
#define TPIU_TRIG_CNTRVAL				(0x104)
#define TPIU_TRIG_MULT					(0x108)
#define TPIU_SUPP_TESTPATM				(0x200)
#define TPIU_CURR_TESTPATM				(0x204)
#define TPIU_TEST_PATREPCNTR				(0x208)
#define TPIU_FFSR					(0x300)
#define TPIU_FFCR					(0x304)
#define TPIU_FSYNC_CNTR					(0x308)
#define TPIU_EXTCTL_INPORT				(0x400)
#define TPIU_EXTCTL_OUTPORT				(0x404)
#define TPIU_ITTRFLINACK				(0xEE4)
#define TPIU_ITTRFLIN					(0xEE8)
#define TPIU_ITATBDATA0					(0xEEC)
#define TPIU_ITATBCTR2					(0xEF0)
#define TPIU_ITATBCTR1					(0xEF4)
#define TPIU_ITATBCTR0					(0xEF8)


#define TPIU_LOCK()							\
do {									\
	mb();								\
	tpiu_writel(drvdata, 0x0, CORESIGHT_LAR);			\
} while (0)
#define TPIU_UNLOCK()							\
do {									\
	tpiu_writel(drvdata, CORESIGHT_UNLOCK, CORESIGHT_LAR);		\
	mb();								\
} while (0)

struct tpiu_drvdata {
	void __iomem	*base;
	bool		enabled;
	struct device	*dev;
	struct clk	*clk;
};

static struct tpiu_drvdata *drvdata;

static void __tpiu_disable(void)
{
	TPIU_UNLOCK();

	tpiu_writel(drvdata, 0x3000, TPIU_FFCR);
	tpiu_writel(drvdata, 0x3040, TPIU_FFCR);

	TPIU_LOCK();
}

void tpiu_disable(void)
{
	__tpiu_disable();
	drvdata->enabled = false;
	dev_info(drvdata->dev, "TPIU disabled\n");
}

static int __devinit tpiu_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;

	drvdata = kzalloc(sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata) {
		ret = -ENOMEM;
		goto err_kzalloc_drvdata;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -EINVAL;
		goto err_res;
	}

	drvdata->base = ioremap_nocache(res->start, resource_size(res));
	if (!drvdata->base) {
		ret = -EINVAL;
		goto err_ioremap;
	}

	drvdata->dev = &pdev->dev;

	drvdata->clk = clk_get(drvdata->dev, "core_clk");
	if (IS_ERR(drvdata->clk)) {
		ret = PTR_ERR(drvdata->clk);
		goto err_clk_get;
	}

	ret = clk_set_rate(drvdata->clk, CORESIGHT_CLK_RATE_TRACE);
	if (ret)
		goto err_clk_rate;

	dev_info(drvdata->dev, "TPIU initialized\n");
	return 0;

err_clk_rate:
	clk_put(drvdata->clk);
err_clk_get:
	iounmap(drvdata->base);
err_ioremap:
err_res:
	kfree(drvdata);
err_kzalloc_drvdata:
	dev_err(drvdata->dev, "TPIU init failed\n");
	return ret;
}

static int __devexit tpiu_remove(struct platform_device *pdev)
{
	if (drvdata->enabled)
		tpiu_disable();
	clk_put(drvdata->clk);
	iounmap(drvdata->base);
	kfree(drvdata);

	return 0;
}

static struct of_device_id tpiu_match[] = {
	{.compatible = "qcom,msm-tpiu"},
	{}
};

static struct platform_driver tpiu_driver = {
	.probe          = tpiu_probe,
	.remove         = __devexit_p(tpiu_remove),
	.driver         = {
		.name   = "msm_tpiu",
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
