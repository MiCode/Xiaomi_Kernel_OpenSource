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
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/coresight.h>

#include "coresight-priv.h"

#define funnel_writel(drvdata, id, val, off)	\
			__raw_writel((val), drvdata->base + (SZ_4K * id) + off)
#define funnel_readl(drvdata, id, off)		\
			__raw_readl(drvdata->base + (SZ_4K * id) + off)

#define FUNNEL_FUNCTL			(0x000)
#define FUNNEL_PRICTL			(0x004)
#define FUNNEL_ITATBDATA0		(0xEEC)
#define FUNNEL_ITATBCTR2		(0xEF0)
#define FUNNEL_ITATBCTR1		(0xEF4)
#define FUNNEL_ITATBCTR0		(0xEF8)


#define FUNNEL_LOCK(id)							\
do {									\
	mb();								\
	funnel_writel(drvdata, id, 0x0, CORESIGHT_LAR);			\
} while (0)
#define FUNNEL_UNLOCK(id)						\
do {									\
	funnel_writel(drvdata, id, CORESIGHT_UNLOCK, CORESIGHT_LAR);	\
	mb();								\
} while (0)

#define FUNNEL_HOLDTIME_MASK		(0xF00)
#define FUNNEL_HOLDTIME_SHFT		(0x8)
#define FUNNEL_HOLDTIME			(0x7 << FUNNEL_HOLDTIME_SHFT)

struct funnel_drvdata {
	void __iomem	*base;
	bool		enabled;
	struct mutex	mutex;
	struct device	*dev;
	struct kobject	*kobj;
	struct clk	*clk;
	uint32_t	priority;
};

static struct funnel_drvdata *drvdata;

static void __funnel_enable(uint8_t id, uint32_t port_mask)
{
	uint32_t functl;

	FUNNEL_UNLOCK(id);

	functl = funnel_readl(drvdata, id, FUNNEL_FUNCTL);
	functl &= ~FUNNEL_HOLDTIME_MASK;
	functl |= FUNNEL_HOLDTIME;
	functl |= port_mask;
	funnel_writel(drvdata, id, functl, FUNNEL_FUNCTL);
	funnel_writel(drvdata, id, drvdata->priority, FUNNEL_PRICTL);

	FUNNEL_LOCK(id);
}

int funnel_enable(uint8_t id, uint32_t port_mask)
{
	int ret;

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	mutex_lock(&drvdata->mutex);
	__funnel_enable(id, port_mask);
	drvdata->enabled = true;
	dev_info(drvdata->dev, "FUNNEL port mask 0x%lx enabled\n",
					(unsigned long) port_mask);
	mutex_unlock(&drvdata->mutex);

	return 0;
}

static void __funnel_disable(uint8_t id, uint32_t port_mask)
{
	uint32_t functl;

	FUNNEL_UNLOCK(id);

	functl = funnel_readl(drvdata, id, FUNNEL_FUNCTL);
	functl &= ~port_mask;
	funnel_writel(drvdata, id, functl, FUNNEL_FUNCTL);

	FUNNEL_LOCK(id);
}

void funnel_disable(uint8_t id, uint32_t port_mask)
{
	mutex_lock(&drvdata->mutex);
	__funnel_disable(id, port_mask);
	drvdata->enabled = false;
	dev_info(drvdata->dev, "FUNNEL port mask 0x%lx disabled\n",
					(unsigned long) port_mask);
	mutex_unlock(&drvdata->mutex);

	clk_disable_unprepare(drvdata->clk);
}

static ssize_t funnel_show_priority(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	unsigned long val = drvdata->priority;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t funnel_store_priority(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	drvdata->priority = val;
	return size;
}
static DEVICE_ATTR(priority, S_IRUGO | S_IWUSR, funnel_show_priority,
		   funnel_store_priority);

static int __devinit funnel_sysfs_init(void)
{
	int ret;

	drvdata->kobj = kobject_create_and_add("funnel", qdss_get_modulekobj());
	if (!drvdata->kobj) {
		dev_err(drvdata->dev, "failed to create FUNNEL sysfs kobject\n");
		ret = -ENOMEM;
		goto err_create;
	}

	ret = sysfs_create_file(drvdata->kobj, &dev_attr_priority.attr);
	if (ret) {
		dev_err(drvdata->dev, "failed to create FUNNEL sysfs priority"
		" attribute\n");
		goto err_file;
	}

	return 0;
err_file:
	kobject_put(drvdata->kobj);
err_create:
	return ret;
}

static void __devexit funnel_sysfs_exit(void)
{
	sysfs_remove_file(drvdata->kobj, &dev_attr_priority.attr);
	kobject_put(drvdata->kobj);
}

static int __devinit funnel_probe(struct platform_device *pdev)
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

	mutex_init(&drvdata->mutex);

	drvdata->clk = clk_get(drvdata->dev, "core_clk");
	if (IS_ERR(drvdata->clk)) {
		ret = PTR_ERR(drvdata->clk);
		goto err_clk_get;
	}

	ret = clk_set_rate(drvdata->clk, CORESIGHT_CLK_RATE_TRACE);
	if (ret)
		goto err_clk_rate;

	funnel_sysfs_init();

	dev_info(drvdata->dev, "FUNNEL initialized\n");
	return 0;

err_clk_rate:
	clk_put(drvdata->clk);
err_clk_get:
	mutex_destroy(&drvdata->mutex);
	iounmap(drvdata->base);
err_ioremap:
err_res:
	kfree(drvdata);
err_kzalloc_drvdata:
	dev_err(drvdata->dev, "FUNNEL init failed\n");
	return ret;
}

static int __devexit funnel_remove(struct platform_device *pdev)
{
	if (drvdata->enabled)
		funnel_disable(0x0, 0xFF);
	funnel_sysfs_exit();
	clk_put(drvdata->clk);
	mutex_destroy(&drvdata->mutex);
	iounmap(drvdata->base);
	kfree(drvdata);

	return 0;
}

static struct of_device_id funnel_match[] = {
	{.compatible = "qcom,msm-funnel"},
	{}
};

static struct platform_driver funnel_driver = {
	.probe          = funnel_probe,
	.remove         = __devexit_p(funnel_remove),
	.driver         = {
		.name   = "msm_funnel",
		.owner	= THIS_MODULE,
		.of_match_table = funnel_match,
	},
};

static int __init funnel_init(void)
{
	return platform_driver_register(&funnel_driver);
}
module_init(funnel_init);

static void __exit funnel_exit(void)
{
	platform_driver_unregister(&funnel_driver);
}
module_exit(funnel_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CoreSight Funnel driver");
