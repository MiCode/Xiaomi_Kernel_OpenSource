/* Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
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
#include <linux/of.h>
#include <linux/of_coresight.h>
#include <linux/coresight.h>
#include <linux/spinlock.h>
#include <linux/notifier.h>
#include <soc/qcom/jtag.h>

#include "coresight-priv.h"

#define funnel_writel(drvdata, val, off)	\
			__raw_writel((val), drvdata->base + off)
#define funnel_readl(drvdata, off)		\
			__raw_readl(drvdata->base + off)

#define FUNNEL_LOCK(drvdata)						\
do {									\
	mb();								\
	funnel_writel(drvdata, 0x0, CORESIGHT_LAR);			\
} while (0)
#define FUNNEL_UNLOCK(drvdata)						\
do {									\
	funnel_writel(drvdata, CORESIGHT_UNLOCK, CORESIGHT_LAR);	\
	mb();								\
} while (0)

#define FUNNEL_FUNCTL		(0x000)
#define FUNNEL_PRICTL		(0x004)
#define FUNNEL_ITATBDATA0	(0xEEC)
#define FUNNEL_ITATBCTR2	(0xEF0)
#define FUNNEL_ITATBCTR1	(0xEF4)
#define FUNNEL_ITATBCTR0	(0xEF8)

#define FUNNEL_HOLDTIME_MASK	(0xF00)
#define FUNNEL_HOLDTIME_SHFT	(0x8)
#define FUNNEL_HOLDTIME		(0x7 << FUNNEL_HOLDTIME_SHFT)
#define FUNNEL_NR_PORTS		8

struct funnel_drvdata {
	void __iomem		*base;
	struct device		*dev;
	struct coresight_device	*csdev;
	struct clk		*clk;
	uint32_t		priority;
	spinlock_t		spinlock;
	DECLARE_BITMAP(inport, FUNNEL_NR_PORTS);
	bool			notify;
	struct notifier_block	jtag_save_blk;
	struct notifier_block	jtag_restore_blk;
};

static void __funnel_enable(struct funnel_drvdata *drvdata, int port)
{
	uint32_t functl;

	FUNNEL_UNLOCK(drvdata);

	functl = funnel_readl(drvdata, FUNNEL_FUNCTL);
	functl &= ~FUNNEL_HOLDTIME_MASK;
	functl |= FUNNEL_HOLDTIME;
	functl |= (1 << port);
	funnel_writel(drvdata, functl, FUNNEL_FUNCTL);
	funnel_writel(drvdata, drvdata->priority, FUNNEL_PRICTL);

	FUNNEL_LOCK(drvdata);
}

static int funnel_enable(struct coresight_device *csdev, int inport,
			 int outport)
{
	struct funnel_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	int ret;

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	spin_lock(&drvdata->spinlock);
	__funnel_enable(drvdata, inport);
	__set_bit(inport, drvdata->inport);
	spin_unlock(&drvdata->spinlock);

	dev_info(drvdata->dev, "FUNNEL inport %d enabled\n", inport);
	return 0;
}

static int funnel_enable_cpu_port(struct notifier_block *this,
				  unsigned long event, void *ptr)
{
	struct funnel_drvdata *drvdata = container_of(this,
						      struct funnel_drvdata,
						      jtag_restore_blk);
	int port;
	int cpu = raw_smp_processor_id();

	port = coresight_etm_get_funnel_port(cpu);
	if (port < 0) {
		pr_warn_ratelimited("error retrieving ETM funnel port: %#x\n",
				     port);
		return notifier_from_errno(port);
	}
	spin_lock(&drvdata->spinlock);
	if (!test_bit(port, drvdata->inport))
		goto out;
	__funnel_enable(drvdata, port);
out:
	spin_unlock(&drvdata->spinlock);
	return NOTIFY_OK;
}

static void __funnel_disable(struct funnel_drvdata *drvdata, int inport)
{
	uint32_t functl;

	FUNNEL_UNLOCK(drvdata);

	functl = funnel_readl(drvdata, FUNNEL_FUNCTL);
	functl &= ~(1 << inport);
	funnel_writel(drvdata, functl, FUNNEL_FUNCTL);

	FUNNEL_LOCK(drvdata);
}

static void funnel_disable(struct coresight_device *csdev, int inport,
			   int outport)
{
	struct funnel_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	spin_lock(&drvdata->spinlock);
	__funnel_disable(drvdata, inport);
	__clear_bit(inport, drvdata->inport);
	spin_unlock(&drvdata->spinlock);

	clk_disable_unprepare(drvdata->clk);

	dev_info(drvdata->dev, "FUNNEL inport %d disabled\n", inport);
}

static int funnel_disable_cpu_port(struct notifier_block *this,
				   unsigned long event, void *ptr)
{
	struct funnel_drvdata *drvdata = container_of(this,
						      struct funnel_drvdata,
						      jtag_save_blk);
	int port;
	int cpu = raw_smp_processor_id();

	port = coresight_etm_get_funnel_port(cpu);
	if (port < 0) {
		pr_warn_ratelimited("error retrieving ETM funnel port: %#x\n",
				     port);
		return notifier_from_errno(port);
	}
	spin_lock(&drvdata->spinlock);
	if (!test_bit(port, drvdata->inport))
		goto out;
	__funnel_disable(drvdata, port);
out:
	spin_unlock(&drvdata->spinlock);
	return NOTIFY_OK;
}

static const struct coresight_ops_link funnel_link_ops = {
	.enable		= funnel_enable,
	.disable	= funnel_disable,
};

static const struct coresight_ops funnel_cs_ops = {
	.link_ops	= &funnel_link_ops,
};

static ssize_t funnel_show_priority(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct funnel_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->priority;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t funnel_store_priority(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct funnel_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	drvdata->priority = val;
	return size;
}
static DEVICE_ATTR(priority, S_IRUGO | S_IWUSR, funnel_show_priority,
		   funnel_store_priority);

static struct attribute *funnel_attrs[] = {
	&dev_attr_priority.attr,
	NULL,
};

static struct attribute_group funnel_attr_grp = {
	.attrs = funnel_attrs,
};

static const struct attribute_group *funnel_attr_grps[] = {
	&funnel_attr_grp,
	NULL,
};

static int funnel_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata;
	struct funnel_drvdata *drvdata;
	struct resource *res;
	struct coresight_desc *desc;

	if (coresight_fuse_access_disabled())
		return -EPERM;

	pdata = of_get_coresight_platform_data(dev, pdev->dev.of_node);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);
	pdev->dev.platform_data = pdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;
	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "funnel-base");
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

	spin_lock_init(&drvdata->spinlock);

	drvdata->notify = of_property_read_bool(pdev->dev.of_node,
						"qcom,funnel-save-restore");

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	desc->type = CORESIGHT_DEV_TYPE_LINK;
	desc->subtype.link_subtype = CORESIGHT_DEV_SUBTYPE_LINK_MERG;
	desc->ops = &funnel_cs_ops;
	desc->pdata = pdev->dev.platform_data;
	desc->dev = &pdev->dev;
	desc->groups = funnel_attr_grps;
	desc->owner = THIS_MODULE;
	drvdata->csdev = coresight_register(desc);
	if (IS_ERR(drvdata->csdev))
		return PTR_ERR(drvdata->csdev);

	if (drvdata->notify) {
		drvdata->jtag_save_blk.notifier_call = funnel_disable_cpu_port;
		ret = msm_jtag_save_register(&drvdata->jtag_save_blk);
		if (ret) {
			dev_err(dev,
				"Jtag save notifier register failed:%d\n",
				ret);
			drvdata->notify = false;
			goto out;
		}

		drvdata->jtag_restore_blk.notifier_call =
							funnel_enable_cpu_port;
		ret = msm_jtag_restore_register(&drvdata->jtag_restore_blk);
		if (ret) {
			dev_err(dev,
				"Jtag restore notifier register failed:%d\n",
				ret);
			msm_jtag_save_unregister(&drvdata->jtag_save_blk);
			drvdata->notify = false;
		}
	}
out:
	dev_dbg(dev, "FUNNEL initialized\n");
	return 0;
}

static int funnel_remove(struct platform_device *pdev)
{
	struct funnel_drvdata *drvdata = platform_get_drvdata(pdev);

	if (drvdata->notify) {
		msm_jtag_restore_unregister(&drvdata->jtag_restore_blk);
		msm_jtag_save_unregister(&drvdata->jtag_save_blk);
	}
	coresight_unregister(drvdata->csdev);
	return 0;
}

static struct of_device_id funnel_match[] = {
	{.compatible = "arm,coresight-funnel"},
	{}
};

static struct platform_driver funnel_driver = {
	.probe          = funnel_probe,
	.remove         = funnel_remove,
	.driver         = {
		.name   = "coresight-funnel",
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
