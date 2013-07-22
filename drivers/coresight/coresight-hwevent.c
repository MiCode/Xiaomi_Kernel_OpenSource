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
#include <linux/of.h>

#include "coresight-priv.h"

struct hwevent_mux {
	phys_addr_t				start;
	phys_addr_t				end;
};

struct hwevent_drvdata {
	struct device				*dev;
	struct coresight_device			*csdev;
	struct clk				*clk;
	struct mutex				mutex;
	int					nr_hclk;
	struct clk				**hclk;
	int					nr_hmux;
	struct hwevent_mux			*hmux;
	bool					enable;
};

static int hwevent_enable(struct hwevent_drvdata *drvdata)
{
	int ret, i;

	mutex_lock(&drvdata->mutex);

	if (drvdata->enable)
		goto out;

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		goto err0;
	for (i = 0; i < drvdata->nr_hclk; i++) {
		ret = clk_prepare_enable(drvdata->hclk[i]);
		if (ret)
			goto err1;
	}
	drvdata->enable = true;
	dev_info(drvdata->dev, "Hardware Event driver enabled\n");
out:
	mutex_unlock(&drvdata->mutex);
	return 0;
err1:
	clk_disable_unprepare(drvdata->clk);
	for (i--; i >= 0; i--)
		clk_disable_unprepare(drvdata->hclk[i]);
err0:
	mutex_unlock(&drvdata->mutex);
	return ret;
}

static void hwevent_disable(struct hwevent_drvdata *drvdata)
{
	int i;

	mutex_lock(&drvdata->mutex);

	if (!drvdata->enable)
		goto out;

	drvdata->enable = false;
	clk_disable_unprepare(drvdata->clk);
	for (i = 0; i < drvdata->nr_hclk; i++)
		clk_disable_unprepare(drvdata->hclk[i]);
	dev_info(drvdata->dev, "Hardware Event driver disabled\n");
out:
	mutex_unlock(&drvdata->mutex);
}

static ssize_t hwevent_show_enable(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct hwevent_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->enable;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t hwevent_store_enable(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct hwevent_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	int ret = 0;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	if (val)
		ret = hwevent_enable(drvdata);
	else
		hwevent_disable(drvdata);

	if (ret)
		return ret;
	return size;
}
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, hwevent_show_enable,
		   hwevent_store_enable);

static ssize_t hwevent_store_setreg(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	struct hwevent_drvdata *drvdata = dev_get_drvdata(dev->parent);
	void *hwereg;
	unsigned long long addr;
	unsigned long val;
	int ret, i;

	if (sscanf(buf, "%llx %lx", &addr, &val) != 2)
		return -EINVAL;

	mutex_lock(&drvdata->mutex);

	if (!drvdata->enable) {
		dev_err(dev, "Hardware Event driver not enabled\n");
		ret = -EINVAL;
		goto err;
	}

	for (i = 0; i < drvdata->nr_hmux; i++) {
		if ((addr >= drvdata->hmux[i].start) &&
		    (addr < drvdata->hmux[i].end)) {
			hwereg = devm_ioremap(dev,
					      drvdata->hmux[i].start,
					      drvdata->hmux[i].end -
					      drvdata->hmux[i].start);
			if (!hwereg) {
				dev_err(dev, "unable to map address 0x%llx\n",
					addr);
				ret = -ENOMEM;
				goto err;
			}
			writel_relaxed(val, hwereg + addr -
				       drvdata->hmux[i].start);
			/* Ensure writes to hwevent control registers
			   are completed before unmapping the address
			*/
			mb();
			devm_iounmap(dev, hwereg);
			break;
		}
	}

	if (i == drvdata->nr_hmux) {
		ret = coresight_csr_hwctrl_set(addr, val);
		if (ret) {
			dev_err(dev, "invalid mux control register address\n");
			ret = -EINVAL;
			goto err;
		}
	}

	mutex_unlock(&drvdata->mutex);
	return size;
err:
	mutex_unlock(&drvdata->mutex);
	return ret;
}
static DEVICE_ATTR(setreg, S_IWUSR, NULL, hwevent_store_setreg);

static struct attribute *hwevent_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_setreg.attr,
	NULL,
};

static struct attribute_group hwevent_attr_grp = {
	.attrs = hwevent_attrs,
};

static const struct attribute_group *hwevent_attr_grps[] = {
	&hwevent_attr_grp,
	NULL,
};

static int hwevent_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct hwevent_drvdata *drvdata;
	struct coresight_desc *desc;
	struct coresight_platform_data *pdata;
	struct resource *res;
	int ret, i;
	const char *hmux_name, *hclk_name;

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

	if (pdev->dev.of_node)
		drvdata->nr_hmux = of_property_count_strings(pdev->dev.of_node,
							     "reg-names");

	if (drvdata->nr_hmux > 0) {
		drvdata->hmux = devm_kzalloc(dev, drvdata->nr_hmux *
					     sizeof(*drvdata->hmux),
					     GFP_KERNEL);
		if (!drvdata->hmux)
			return -ENOMEM;
		for (i = 0; i < drvdata->nr_hmux; i++) {
			ret = of_property_read_string_index(pdev->dev.of_node,
							    "reg-names", i,
							    &hmux_name);
			if (ret)
				return ret;
			res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							   hmux_name);
			if (!res)
				return -ENODEV;
			drvdata->hmux[i].start = res->start;
			drvdata->hmux[i].end = res->end;
		}
	} else if (drvdata->nr_hmux < 0) {
		return drvdata->nr_hmux;
	} else {
		/* return error if reg-names in dt node is empty string */
		return -ENODEV;
	}

	mutex_init(&drvdata->mutex);

	drvdata->clk = devm_clk_get(dev, "core_clk");
	if (IS_ERR(drvdata->clk))
		return PTR_ERR(drvdata->clk);

	ret = clk_set_rate(drvdata->clk, CORESIGHT_CLK_RATE_TRACE);
	if (ret)
		return ret;

	if (pdev->dev.of_node)
		drvdata->nr_hclk = of_property_count_strings(pdev->dev.of_node,
							     "qcom,hwevent-clks");
	if (drvdata->nr_hclk > 0) {
		drvdata->hclk = devm_kzalloc(dev, drvdata->nr_hclk *
					     sizeof(*drvdata->hclk),
					     GFP_KERNEL);
		if (!drvdata->hclk)
			return -ENOMEM;
		for (i = 0; i < drvdata->nr_hclk; i++) {
			ret = of_property_read_string_index(pdev->dev.of_node,
							    "qcom,hwevent-clks",
							    i, &hclk_name);
			if (ret)
				return ret;
			drvdata->hclk[i] = devm_clk_get(dev, hclk_name);
			if (IS_ERR(drvdata->hclk[i]))
				return PTR_ERR(drvdata->hclk[i]);
		}
	}

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	desc->type = CORESIGHT_DEV_TYPE_NONE;
	desc->pdata = pdev->dev.platform_data;
	desc->dev = &pdev->dev;
	desc->groups = hwevent_attr_grps;
	desc->owner = THIS_MODULE;
	drvdata->csdev = coresight_register(desc);
	if (IS_ERR(drvdata->csdev))
		return PTR_ERR(drvdata->csdev);

	dev_info(dev, "Hardware Event driver initialized\n");
	return 0;
}

static int hwevent_remove(struct platform_device *pdev)
{
	struct hwevent_drvdata *drvdata = platform_get_drvdata(pdev);

	coresight_unregister(drvdata->csdev);
	return 0;
}

static struct of_device_id hwevent_match[] = {
	{.compatible = "qcom,coresight-hwevent"},
	{}
};

static struct platform_driver hwevent_driver = {
	.probe		= hwevent_probe,
	.remove		= hwevent_remove,
	.driver		= {
		.name	= "coresight-hwevent",
		.owner	= THIS_MODULE,
		.of_match_table	= hwevent_match,
	},
};

static int __init hwevent_init(void)
{
	return platform_driver_register(&hwevent_driver);
}
module_init(hwevent_init);

static void __exit hwevent_exit(void)
{
	platform_driver_unregister(&hwevent_driver);
}
module_exit(hwevent_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CoreSight Hardware Event driver");
