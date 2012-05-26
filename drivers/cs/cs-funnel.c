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
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/cs.h>

#include "cs-priv.h"

#define funnel_writel(funnel, id, val, off)	\
			__raw_writel((val), funnel.base + (SZ_4K * id) + off)
#define funnel_readl(funnel, id, off)		\
			__raw_readl(funnel.base + (SZ_4K * id) + off)

#define FUNNEL_FUNCTL			(0x000)
#define FUNNEL_PRICTL			(0x004)
#define FUNNEL_ITATBDATA0		(0xEEC)
#define FUNNEL_ITATBCTR2		(0xEF0)
#define FUNNEL_ITATBCTR1		(0xEF4)
#define FUNNEL_ITATBCTR0		(0xEF8)


#define FUNNEL_LOCK(id)							\
do {									\
	mb();								\
	funnel_writel(funnel, id, 0x0, CS_LAR);				\
} while (0)
#define FUNNEL_UNLOCK(id)						\
do {									\
	funnel_writel(funnel, id, CS_UNLOCK_MAGIC, CS_LAR);		\
	mb();								\
} while (0)

#define FUNNEL_HOLDTIME_MASK		(0xF00)
#define FUNNEL_HOLDTIME_SHFT		(0x8)
#define FUNNEL_HOLDTIME			(0x7 << FUNNEL_HOLDTIME_SHFT)

struct funnel_ctx {
	void __iomem	*base;
	bool		enabled;
	struct mutex	mutex;
	struct device	*dev;
	struct kobject	*kobj;
	struct clk	*clk;
	uint32_t	priority;
};

static struct funnel_ctx funnel;

static void __funnel_enable(uint8_t id, uint32_t port_mask)
{
	uint32_t functl;

	FUNNEL_UNLOCK(id);

	functl = funnel_readl(funnel, id, FUNNEL_FUNCTL);
	functl &= ~FUNNEL_HOLDTIME_MASK;
	functl |= FUNNEL_HOLDTIME;
	functl |= port_mask;
	funnel_writel(funnel, id, functl, FUNNEL_FUNCTL);
	funnel_writel(funnel, id, funnel.priority, FUNNEL_PRICTL);

	FUNNEL_LOCK(id);
}

int funnel_enable(uint8_t id, uint32_t port_mask)
{
	int ret;

	ret = clk_prepare_enable(funnel.clk);
	if (ret)
		return ret;

	mutex_lock(&funnel.mutex);
	__funnel_enable(id, port_mask);
	funnel.enabled = true;
	dev_info(funnel.dev, "FUNNEL port mask 0x%lx enabled\n",
					(unsigned long) port_mask);
	mutex_unlock(&funnel.mutex);

	return 0;
}

static void __funnel_disable(uint8_t id, uint32_t port_mask)
{
	uint32_t functl;

	FUNNEL_UNLOCK(id);

	functl = funnel_readl(funnel, id, FUNNEL_FUNCTL);
	functl &= ~port_mask;
	funnel_writel(funnel, id, functl, FUNNEL_FUNCTL);

	FUNNEL_LOCK(id);
}

void funnel_disable(uint8_t id, uint32_t port_mask)
{
	mutex_lock(&funnel.mutex);
	__funnel_disable(id, port_mask);
	funnel.enabled = false;
	dev_info(funnel.dev, "FUNNEL port mask 0x%lx disabled\n",
					(unsigned long) port_mask);
	mutex_unlock(&funnel.mutex);

	clk_disable_unprepare(funnel.clk);
}

#define FUNNEL_ATTR(__name)						\
static struct kobj_attribute __name##_attr =				\
	__ATTR(__name, S_IRUGO | S_IWUSR, __name##_show, __name##_store)

static ssize_t priority_store(struct kobject *kobj,
			struct kobj_attribute *attr,
			const char *buf, size_t n)
{
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	funnel.priority = val;
	return n;
}
static ssize_t priority_show(struct kobject *kobj,
			struct kobj_attribute *attr,
			char *buf)
{
	unsigned long val = funnel.priority;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
FUNNEL_ATTR(priority);

static int funnel_sysfs_init(void)
{
	int ret;

	funnel.kobj = kobject_create_and_add("funnel", qdss_get_modulekobj());
	if (!funnel.kobj) {
		dev_err(funnel.dev, "failed to create FUNNEL sysfs kobject\n");
		ret = -ENOMEM;
		goto err_create;
	}

	ret = sysfs_create_file(funnel.kobj, &priority_attr.attr);
	if (ret) {
		dev_err(funnel.dev, "failed to create FUNNEL sysfs priority"
		" attribute\n");
		goto err_file;
	}

	return 0;
err_file:
	kobject_put(funnel.kobj);
err_create:
	return ret;
}

static void funnel_sysfs_exit(void)
{
	sysfs_remove_file(funnel.kobj, &priority_attr.attr);
	kobject_put(funnel.kobj);
}

static int funnel_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -EINVAL;
		goto err_res;
	}

	funnel.base = ioremap_nocache(res->start, resource_size(res));
	if (!funnel.base) {
		ret = -EINVAL;
		goto err_ioremap;
	}

	funnel.dev = &pdev->dev;

	mutex_init(&funnel.mutex);

	funnel.clk = clk_get(funnel.dev, "core_clk");
	if (IS_ERR(funnel.clk)) {
		ret = PTR_ERR(funnel.clk);
		goto err_clk_get;
	}

	ret = clk_set_rate(funnel.clk, CS_CLK_RATE_TRACE);
	if (ret)
		goto err_clk_rate;

	funnel_sysfs_init();

	dev_info(funnel.dev, "FUNNEL initialized\n");
	return 0;

err_clk_rate:
	clk_put(funnel.clk);
err_clk_get:
	mutex_destroy(&funnel.mutex);
	iounmap(funnel.base);
err_ioremap:
err_res:
	dev_err(funnel.dev, "FUNNEL init failed\n");
	return ret;
}

static int funnel_remove(struct platform_device *pdev)
{
	if (funnel.enabled)
		funnel_disable(0x0, 0xFF);
	funnel_sysfs_exit();
	clk_put(funnel.clk);
	mutex_destroy(&funnel.mutex);
	iounmap(funnel.base);

	return 0;
}

static struct of_device_id funnel_match[] = {
	{.compatible = "qcom,msm-funnel"},
	{}
};

static struct platform_driver funnel_driver = {
	.probe          = funnel_probe,
	.remove         = funnel_remove,
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
