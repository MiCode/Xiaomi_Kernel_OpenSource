/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#include <linux/of.h>
#include <linux/of_coresight.h>
#include <linux/of_device.h>
#include <linux/coresight.h>

#include "coresight-priv.h"

#define fuse_writel(drvdata, val, off)	__raw_writel((val), drvdata->base + off)
#define fuse_readl(drvdata, off)	__raw_readl(drvdata->base + off)

#define OEM_CONFIG0		(0x000)
#define OEM_CONFIG1		(0x004)
#define FEATURE_CONFIG2		(0x000)

#define CORESIGHT_FUSE_VERSION_V1		"arm,coresight-fuse"
#define CORESIGHT_FUSE_VERSION_V2		"arm,coresight-fuse-v2"

/* CoreSight FUSE V1 */
#define ALL_DEBUG_DISABLE	BIT(21)
#define APPS_DBGEN_DISABLE	BIT(0)
#define APPS_NIDEN_DISABLE	BIT(1)
#define APPS_SPIDEN_DISABLE	BIT(2)
#define APPS_SPNIDEN_DISABLE	BIT(3)
#define DAP_DBGEN_DISABLE	BIT(4)
#define DAP_NIDEN_DISABLE	BIT(5)
#define DAP_SPIDEN_DISABLE	BIT(6)
#define DAP_SPNIDEN_DISABLE	BIT(7)
#define DAP_DEVICEEN_DISABLE	BIT(8)

/* CoreSight FUSE V2 */
#define ALL_DEBUG_DISABLE_V2		BIT(0)
#define APPS_DBGEN_DISABLE_V2		BIT(10)
#define APPS_NIDEN_DISABLE_V2		BIT(11)
#define APPS_SPIDEN_DISABLE_V2		BIT(12)
#define APPS_SPNIDEN_DISABLE_V2	BIT(13)
#define DAP_DBGEN_DISABLE_V2		BIT(14)
#define DAP_NIDEN_DISABLE_V2		BIT(15)
#define DAP_SPIDEN_DISABLE_V2		BIT(16)
#define DAP_SPNIDEN_DISABLE_V2		BIT(17)
#define DAP_DEVICEEN_DISABLE_V2	BIT(18)

#define QPDI_SPMI_DISABLE	BIT(2)
#define NIDNT_DISABLE	BIT(27)

struct fuse_nidnt {
	void __iomem        *base;
};

struct fuse_qpdi {
	void __iomem		*base;
};

struct fuse_drvdata {
	void __iomem		*base;
	struct fuse_nidnt	*fuse_nidnt;
	struct fuse_qpdi	*qpdi;
	struct device		*dev;
	struct coresight_device	*csdev;
	bool			fuse_v2;
};

static struct fuse_drvdata *fusedrvdata;

bool coresight_fuse_nidnt_access_disabled(void)
{
	struct fuse_drvdata *drvdata = fusedrvdata;
	uint32_t nidnt_fuse;
	bool ret;

	if (!drvdata->fuse_nidnt)
		return false;

	nidnt_fuse = fuse_readl(drvdata->fuse_nidnt, 0);

	dev_dbg(drvdata->dev, "nidnt_fuse: %lx\n", (unsigned long)nidnt_fuse);

	if (nidnt_fuse & NIDNT_DISABLE)
		ret = true;
	else
		ret = false;

	if (ret)
		dev_dbg(drvdata->dev, "coresight nidnt fuse disabled\n");

	return ret;
}
EXPORT_SYMBOL(coresight_fuse_nidnt_access_disabled);

bool coresight_fuse_access_disabled(void)
{
	struct fuse_drvdata *drvdata = fusedrvdata;
	uint32_t config0, config1;
	bool ret = false;

	config0 = fuse_readl(drvdata, OEM_CONFIG0);
	config1 = fuse_readl(drvdata, OEM_CONFIG1);

	dev_dbg(drvdata->dev, "config0: %lx\n", (unsigned long)config0);
	dev_dbg(drvdata->dev, "config1: %lx\n", (unsigned long)config1);

	if (drvdata->fuse_v2) {
		if (config1 & ALL_DEBUG_DISABLE_V2)
			ret = true;
		else if (config1 & DAP_DBGEN_DISABLE_V2)
			ret = true;
		else if (config1 & DAP_NIDEN_DISABLE_V2)
			ret = true;
		else if (config1 & DAP_SPIDEN_DISABLE_V2)
			ret = true;
		else if (config1 & DAP_SPNIDEN_DISABLE_V2)
			ret = true;
		else if (config1 & DAP_DEVICEEN_DISABLE_V2)
			ret = true;
	} else {
		if (config0 & ALL_DEBUG_DISABLE)
			ret = true;
		else if (config1 & DAP_DBGEN_DISABLE)
			ret = true;
		else if (config1 & DAP_NIDEN_DISABLE)
			ret = true;
		else if (config1 & DAP_SPIDEN_DISABLE)
			ret = true;
		else if (config1 & DAP_SPNIDEN_DISABLE)
			ret = true;
		else if (config1 & DAP_DEVICEEN_DISABLE)
			ret = true;
	}

	if (ret)
		dev_dbg(drvdata->dev, "coresight fuse disabled\n");

	return ret;
}
EXPORT_SYMBOL(coresight_fuse_access_disabled);

bool coresight_fuse_apps_access_disabled(void)
{
	struct fuse_drvdata *drvdata = fusedrvdata;
	uint32_t config0, config1;
	bool ret = false;

	config0 = fuse_readl(drvdata, OEM_CONFIG0);
	config1 = fuse_readl(drvdata, OEM_CONFIG1);

	dev_dbg(drvdata->dev, "apps config0: %lx\n", (unsigned long)config0);
	dev_dbg(drvdata->dev, "apps config1: %lx\n", (unsigned long)config1);

	if (drvdata->fuse_v2) {
		if (config1 & ALL_DEBUG_DISABLE_V2)
			ret = true;
		else if (config1 & APPS_DBGEN_DISABLE_V2)
			ret = true;
		else if (config1 & APPS_NIDEN_DISABLE_V2)
			ret = true;
		else if (config1 & APPS_SPIDEN_DISABLE_V2)
			ret = true;
		else if (config1 & APPS_SPNIDEN_DISABLE_V2)
			ret = true;
		else if (config1 & DAP_DEVICEEN_DISABLE_V2)
			ret = true;
	} else {
		if (config0 & ALL_DEBUG_DISABLE)
			ret = true;
		else if (config1 & APPS_DBGEN_DISABLE)
			ret = true;
		else if (config1 & APPS_NIDEN_DISABLE)
			ret = true;
		else if (config1 & APPS_SPIDEN_DISABLE)
			ret = true;
		else if (config1 & APPS_SPNIDEN_DISABLE)
			ret = true;
		else if (config1 & DAP_DEVICEEN_DISABLE)
			ret = true;
	}

	if (ret)
		dev_dbg(drvdata->dev, "apps fuse disabled\n");

	return ret;
}
EXPORT_SYMBOL(coresight_fuse_apps_access_disabled);

bool coresight_fuse_qpdi_access_disabled(void)
{
	struct fuse_drvdata *drvdata = fusedrvdata;
	uint32_t config;

	if (!drvdata->qpdi)
		return false;

	config = fuse_readl(drvdata->qpdi, FEATURE_CONFIG2);

	dev_dbg(drvdata->dev, "qpdi config: %lx\n", (unsigned long)config);

	if (config & QPDI_SPMI_DISABLE) {
		dev_dbg(drvdata->dev, "qpdi fuse disabled\n");
		return true;
	} else {
		return false;
	}
}
EXPORT_SYMBOL(coresight_fuse_qpdi_access_disabled);

static struct of_device_id fuse_match[] = {
	{.compatible = CORESIGHT_FUSE_VERSION_V1 },
	{.compatible = CORESIGHT_FUSE_VERSION_V2 },
	{}
};

static int fuse_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata;
	struct fuse_drvdata *drvdata;
	struct resource *res;
	struct coresight_desc *desc;
	const struct of_device_id *match;

	if (pdev->dev.of_node) {
		pdata = of_get_coresight_platform_data(dev, pdev->dev.of_node);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);
		pdev->dev.platform_data = pdata;
	}

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;
	/* Store the driver data pointer for use in exported functions */
	fusedrvdata = drvdata;
	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	match = of_match_device(fuse_match, dev);
	if (!match)
		return -EINVAL;

	if (!strcmp(match->compatible, CORESIGHT_FUSE_VERSION_V2))
		drvdata->fuse_v2 = true;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "fuse-base");
	if (!res)
		return -ENODEV;

	drvdata->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!drvdata->base)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "qpdi-fuse-base");
	if (res) {
		drvdata->qpdi = devm_kzalloc(dev, sizeof(*drvdata->qpdi),
					     GFP_KERNEL);
		if (!drvdata->qpdi)
			return -ENOMEM;

		drvdata->qpdi->base = devm_ioremap(dev, res->start,
						   resource_size(res));
		if (!drvdata->qpdi->base)
			return -ENOMEM;
	} else {
		dev_info(dev, "QPDI fuse not specified\n");
	}

	res = platform_get_resource_byname(pdev,
					   IORESOURCE_MEM,
					   "nidnt-fuse-base");
	if (res) {
		drvdata->fuse_nidnt = devm_kzalloc(dev,
						   sizeof(*drvdata->fuse_nidnt),
						   GFP_KERNEL);
		if (!drvdata->fuse_nidnt)
			return -ENOMEM;

		drvdata->fuse_nidnt->base = devm_ioremap(dev, res->start,
							 resource_size(res));
		if (!drvdata->fuse_nidnt->base)
			return -ENOMEM;
	}

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	desc->type = CORESIGHT_DEV_TYPE_NONE;
	desc->pdata = pdev->dev.platform_data;
	desc->dev = &pdev->dev;
	desc->owner = THIS_MODULE;
	drvdata->csdev = coresight_register(desc);
	if (IS_ERR(drvdata->csdev))
		return PTR_ERR(drvdata->csdev);

	dev_info(dev, "Fuse initialized\n");
	return 0;
}

static int fuse_remove(struct platform_device *pdev)
{
	struct fuse_drvdata *drvdata = platform_get_drvdata(pdev);

	coresight_unregister(drvdata->csdev);
	return 0;
}

static struct platform_driver fuse_driver = {
	.probe          = fuse_probe,
	.remove         = fuse_remove,
	.driver         = {
		.name   = "coresight-fuse",
		.owner	= THIS_MODULE,
		.of_match_table = fuse_match,
	},
};

static int __init fuse_init(void)
{
	return platform_driver_register(&fuse_driver);
}
module_init(fuse_init);

static void __exit fuse_exit(void)
{
	platform_driver_unregister(&fuse_driver);
}
module_exit(fuse_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CoreSight Fuse driver");
