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
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/of_coresight.h>
#include <linux/coresight.h>
#include <linux/coresight-cti.h>

#include "coresight-priv.h"

#define cti_writel(drvdata, val, off)	__raw_writel((val), drvdata->base + off)
#define cti_readl(drvdata, off)		__raw_readl(drvdata->base + off)

#define CTI_LOCK(drvdata)						\
do {									\
	mb();								\
	cti_writel(drvdata, 0x0, CORESIGHT_LAR);			\
} while (0)
#define CTI_UNLOCK(drvdata)						\
do {									\
	cti_writel(drvdata, CORESIGHT_UNLOCK, CORESIGHT_LAR);		\
	mb();								\
} while (0)

#define CTICONTROL		(0x000)
#define CTIINTACK		(0x010)
#define CTIAPPSET		(0x014)
#define CTIAPPCLEAR		(0x018)
#define CTIAPPPULSE		(0x01C)
#define CTIINEN(n)		(0x020 + (n * 4))
#define CTIOUTEN(n)		(0x0A0 + (n * 4))
#define CTITRIGINSTATUS		(0x130)
#define CTITRIGOUTSTATUS	(0x134)
#define CTICHINSTATUS		(0x138)
#define CTICHOUTSTATUS		(0x13C)
#define CTIGATE			(0x140)
#define ASICCTL			(0x144)
#define ITCHINACK		(0xEDC)
#define ITTRIGINACK		(0xEE0)
#define ITCHOUT			(0xEE4)
#define ITTRIGOUT		(0xEE8)
#define ITCHOUTACK		(0xEEC)
#define ITTRIGOUTACK		(0xEF0)
#define ITCHIN			(0xEF4)
#define ITTRIGIN		(0xEF8)

#define CTI_MAX_TRIGGERS	(8)
#define CTI_MAX_CHANNELS	(4)

#define to_cti_drvdata(c) container_of(c, struct cti_drvdata, cti)

struct cti_drvdata {
	void __iomem			*base;
	struct device			*dev;
	struct coresight_device		*csdev;
	struct clk			*clk;
	struct mutex			mutex;
	struct coresight_cti		cti;
	int				refcnt;
};

static LIST_HEAD(cti_list);
static DEFINE_MUTEX(cti_lock);

static int cti_verify_bounds(int trig, int ch)
{
	if (trig >= CTI_MAX_TRIGGERS)
		return -EINVAL;

	if (ch >= CTI_MAX_CHANNELS)
		return -EINVAL;

	return 0;
}

static int cti_enable(struct cti_drvdata *drvdata)
{
	int ret;

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	CTI_UNLOCK(drvdata);

	cti_writel(drvdata, 0x1, CTICONTROL);

	CTI_LOCK(drvdata);
	return 0;
}

static void __cti_map_trigin(struct cti_drvdata *drvdata, int trig, int ch)
{
	uint32_t ctien;

	CTI_UNLOCK(drvdata);

	ctien = cti_readl(drvdata, CTIINEN(trig));
	cti_writel(drvdata, (ctien | 0x1 << ch), CTIINEN(trig));

	CTI_LOCK(drvdata);
}

int coresight_cti_map_trigin(struct coresight_cti *cti, int trig, int ch)
{
	struct cti_drvdata *drvdata;
	int ret = 0;

	if (IS_ERR_OR_NULL(cti))
		return -EINVAL;

	ret = cti_verify_bounds(trig, ch);
	if (ret)
		return ret;

	drvdata = to_cti_drvdata(cti);

	mutex_lock(&drvdata->mutex);
	if (drvdata->refcnt == 0) {
		ret = cti_enable(drvdata);
		if (ret)
			goto err;
	}
	drvdata->refcnt++;

	__cti_map_trigin(drvdata, trig, ch);
err:
	mutex_unlock(&drvdata->mutex);
	return ret;
}
EXPORT_SYMBOL(coresight_cti_map_trigin);

static void __cti_map_trigout(struct cti_drvdata *drvdata, int trig, int ch)
{
	uint32_t ctien;

	CTI_UNLOCK(drvdata);

	ctien = cti_readl(drvdata, CTIOUTEN(trig));
	cti_writel(drvdata, (ctien | 0x1 << ch), CTIOUTEN(trig));

	CTI_LOCK(drvdata);
}

int coresight_cti_map_trigout(struct coresight_cti *cti, int trig, int ch)
{
	struct cti_drvdata *drvdata;
	int ret = 0;

	if (IS_ERR_OR_NULL(cti))
		return -EINVAL;

	ret = cti_verify_bounds(trig, ch);
	if (ret)
		return ret;

	drvdata = to_cti_drvdata(cti);

	mutex_lock(&drvdata->mutex);
	if (drvdata->refcnt == 0) {
		ret = cti_enable(drvdata);
		if (ret)
			goto err;
	}
	drvdata->refcnt++;

	__cti_map_trigout(drvdata, trig, ch);
err:
	mutex_unlock(&drvdata->mutex);
	return ret;
}
EXPORT_SYMBOL(coresight_cti_map_trigout);

static void cti_disable(struct cti_drvdata *drvdata)
{
	CTI_UNLOCK(drvdata);

	cti_writel(drvdata, 0x1, CTICONTROL);

	CTI_LOCK(drvdata);
}

static void __cti_unmap_trigin(struct cti_drvdata *drvdata, int trig, int ch)
{
	uint32_t ctien;

	CTI_UNLOCK(drvdata);

	ctien = cti_readl(drvdata, CTIINEN(trig));
	cti_writel(drvdata, (ctien & ~(0x1 << ch)), CTIINEN(trig));

	CTI_LOCK(drvdata);
}

void coresight_cti_unmap_trigin(struct coresight_cti *cti, int trig, int ch)
{
	struct cti_drvdata *drvdata;

	if (IS_ERR_OR_NULL(cti))
		return;

	if (cti_verify_bounds(trig, ch))
		return;

	drvdata = to_cti_drvdata(cti);

	mutex_lock(&drvdata->mutex);
	__cti_unmap_trigin(drvdata, trig, ch);

	if (drvdata->refcnt == 1)
		cti_disable(drvdata);
	drvdata->refcnt--;
	mutex_unlock(&drvdata->mutex);

	clk_disable_unprepare(drvdata->clk);
}
EXPORT_SYMBOL(coresight_cti_unmap_trigin);

static void __cti_unmap_trigout(struct cti_drvdata *drvdata, int trig, int ch)
{
	uint32_t ctien;

	CTI_UNLOCK(drvdata);

	ctien = cti_readl(drvdata, CTIOUTEN(trig));
	cti_writel(drvdata, (ctien & ~(0x1 << ch)), CTIOUTEN(trig));

	CTI_LOCK(drvdata);
}

void coresight_cti_unmap_trigout(struct coresight_cti *cti, int trig, int ch)
{
	struct cti_drvdata *drvdata;

	if (IS_ERR_OR_NULL(cti))
		return;

	if (cti_verify_bounds(trig, ch))
		return;

	drvdata = to_cti_drvdata(cti);

	mutex_lock(&drvdata->mutex);
	__cti_unmap_trigout(drvdata, trig, ch);

	if (drvdata->refcnt == 1)
		cti_disable(drvdata);
	drvdata->refcnt--;
	mutex_unlock(&drvdata->mutex);

	clk_disable_unprepare(drvdata->clk);
}
EXPORT_SYMBOL(coresight_cti_unmap_trigout);

struct coresight_cti *coresight_cti_get(const char *name)
{
	struct coresight_cti *cti;

	mutex_lock(&cti_lock);
	list_for_each_entry(cti, &cti_list, link) {
		if (!strncmp(cti->name, name, strlen(cti->name) + 1)) {
			mutex_unlock(&cti_lock);
			return cti;
		}
	}
	mutex_unlock(&cti_lock);

	return ERR_PTR(-EINVAL);
}
EXPORT_SYMBOL(coresight_cti_get);

void coresight_cti_put(struct coresight_cti *cti)
{
}
EXPORT_SYMBOL(coresight_cti_put);

static ssize_t cti_store_map_trigin(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val1, val2;
	int ret;

	if (sscanf(buf, "%lx %lx", &val1, &val2) != 2)
		return -EINVAL;

	ret = coresight_cti_map_trigin(&drvdata->cti, val1, val2);

	if (ret)
		return ret;
	return size;
}
static DEVICE_ATTR(map_trigin, S_IWUSR, NULL, cti_store_map_trigin);

static ssize_t cti_store_map_trigout(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val1, val2;
	int ret;

	if (sscanf(buf, "%lx %lx", &val1, &val2) != 2)
		return -EINVAL;

	ret = coresight_cti_map_trigout(&drvdata->cti, val1, val2);

	if (ret)
		return ret;
	return size;
}
static DEVICE_ATTR(map_trigout, S_IWUSR, NULL, cti_store_map_trigout);

static ssize_t cti_store_unmap_trigin(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val1, val2;

	if (sscanf(buf, "%lx %lx", &val1, &val2) != 2)
		return -EINVAL;

	coresight_cti_unmap_trigin(&drvdata->cti, val1, val2);

	return size;
}
static DEVICE_ATTR(unmap_trigin, S_IWUSR, NULL, cti_store_unmap_trigin);

static ssize_t cti_store_unmap_trigout(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	struct cti_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val1, val2;

	if (sscanf(buf, "%lx %lx", &val1, &val2) != 2)
		return -EINVAL;

	coresight_cti_unmap_trigout(&drvdata->cti, val1, val2);

	return size;
}
static DEVICE_ATTR(unmap_trigout, S_IWUSR, NULL, cti_store_unmap_trigout);

static struct attribute *cti_attrs[] = {
	&dev_attr_map_trigin.attr,
	&dev_attr_map_trigout.attr,
	&dev_attr_unmap_trigin.attr,
	&dev_attr_unmap_trigout.attr,
	NULL,
};

static struct attribute_group cti_attr_grp = {
	.attrs = cti_attrs,
};

static const struct attribute_group *cti_attr_grps[] = {
	&cti_attr_grp,
	NULL,
};

static int __devinit cti_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata;
	struct cti_drvdata *drvdata;
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
	/* Store the driver data pointer for use in exported functions */
	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cti-base");
	if (!res)
		return -ENODEV;

	drvdata->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!drvdata->base)
		return -ENOMEM;

	mutex_init(&drvdata->mutex);

	drvdata->clk = devm_clk_get(dev, "core_clk");
	if (IS_ERR(drvdata->clk))
		return PTR_ERR(drvdata->clk);

	ret = clk_set_rate(drvdata->clk, CORESIGHT_CLK_RATE_TRACE);
	if (ret)
		return ret;

	mutex_lock(&cti_lock);
	drvdata->cti.name = ((struct coresight_platform_data *)
			     (pdev->dev.platform_data))->name;
	list_add_tail(&drvdata->cti.link, &cti_list);
	mutex_unlock(&cti_lock);

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	desc->type = CORESIGHT_DEV_TYPE_NONE;
	desc->pdata = pdev->dev.platform_data;
	desc->dev = &pdev->dev;
	desc->groups = cti_attr_grps;
	desc->owner = THIS_MODULE;
	drvdata->csdev = coresight_register(desc);
	if (IS_ERR(drvdata->csdev))
		return PTR_ERR(drvdata->csdev);

	dev_info(dev, "CTI initialized\n");
	return 0;
}

static int __devexit cti_remove(struct platform_device *pdev)
{
	struct cti_drvdata *drvdata = platform_get_drvdata(pdev);

	coresight_unregister(drvdata->csdev);
	return 0;
}

static struct of_device_id cti_match[] = {
	{.compatible = "arm,coresight-cti"},
	{}
};

static struct platform_driver cti_driver = {
	.probe          = cti_probe,
	.remove         = __devexit_p(cti_remove),
	.driver         = {
		.name   = "coresight-cti",
		.owner	= THIS_MODULE,
		.of_match_table = cti_match,
	},
};

static int __init cti_init(void)
{
	return platform_driver_register(&cti_driver);
}
module_init(cti_init);

static void __exit cti_exit(void)
{
	platform_driver_unregister(&cti_driver);
}
module_exit(cti_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CoreSight CTI driver");
