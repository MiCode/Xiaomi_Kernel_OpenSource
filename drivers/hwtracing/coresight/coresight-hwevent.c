/* Copyright (c) 2013-2018, The Linux Foundation. All rights reserved.
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
#include <linux/coresight.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

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
	int					nr_hreg;
	struct regulator			**hreg;
	int					nr_hmux;
	struct hwevent_mux			*hmux;
	struct coresight_csr			*csr;
	const char				*csr_name;
};

static int hwevent_enable(struct hwevent_drvdata *drvdata)
{
	int ret, i, j;

	ret = clk_prepare_enable(drvdata->clk);
	if (ret)
		return ret;

	for (i = 0; i < drvdata->nr_hreg; i++) {
		ret = regulator_enable(drvdata->hreg[i]);
		if (ret)
			goto err0;
	}

	for (j = 0; j < drvdata->nr_hclk; j++) {
		ret = clk_prepare_enable(drvdata->hclk[j]);
		if (ret)
			goto err1;
	}
	return 0;
err1:
	for (j--; j >= 0; j--)
		clk_disable_unprepare(drvdata->hclk[j]);
err0:
	for (i--; i >= 0; i--)
		regulator_disable(drvdata->hreg[i]);

	clk_disable_unprepare(drvdata->clk);
	return ret;
}

static void hwevent_disable(struct hwevent_drvdata *drvdata)
{
	int i;

	clk_disable_unprepare(drvdata->clk);
	for (i = 0; i < drvdata->nr_hclk; i++)
		clk_disable_unprepare(drvdata->hclk[i]);
	for (i = 0; i < drvdata->nr_hreg; i++)
		regulator_disable(drvdata->hreg[i]);
}

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
	ret = hwevent_enable(drvdata);
	if (ret) {
		mutex_unlock(&drvdata->mutex);
		return ret;
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
			/*
			 * Ensure writes to hwevent control registers
			 * are completed before unmapping the address
			 */
			mb();
			devm_iounmap(dev, hwereg);
			break;
		}
	}

	if (i == drvdata->nr_hmux) {
		ret = coresight_csr_hwctrl_set(drvdata->csr, addr,  val);
		if (ret) {
			dev_err(dev, "invalid mux control register address\n");
			ret = -EINVAL;
			goto err;
		}
	}

	hwevent_disable(drvdata);
	mutex_unlock(&drvdata->mutex);
	return size;
err:
	hwevent_disable(drvdata);
	mutex_unlock(&drvdata->mutex);
	return ret;
}
static DEVICE_ATTR(setreg, 0200, NULL, hwevent_store_setreg);

static struct attribute *hwevent_attrs[] = {
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
	const char *hmux_name, *hclk_name, *hreg_name;

	pdata = of_get_coresight_platform_data(dev, pdev->dev.of_node);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);
	pdev->dev.platform_data = pdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;
	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	ret = of_get_coresight_csr_name(dev->of_node, &drvdata->csr_name);
	if (ret) {
		dev_err(dev, "No csr data\n");
	} else{
		drvdata->csr = coresight_csr_get(drvdata->csr_name);
		if (IS_ERR(drvdata->csr)) {
			dev_err(dev, "failed to get csr, defer probe\n");
			return -EPROBE_DEFER;
		}
	}

	drvdata->nr_hmux = of_property_count_strings(pdev->dev.of_node,
						     "reg-names");

	if (!drvdata->nr_hmux)
		return -ENODEV;

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
	} else {
		return drvdata->nr_hmux;
	}

	mutex_init(&drvdata->mutex);

	drvdata->clk = devm_clk_get(dev, "apb_pclk");
	if (IS_ERR(drvdata->clk))
		return PTR_ERR(drvdata->clk);

	drvdata->nr_hclk = of_property_count_strings(pdev->dev.of_node,
						     "qcom,hwevent-clks");
	drvdata->nr_hreg = of_property_count_strings(pdev->dev.of_node,
						     "qcom,hwevent-regs");

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
	if (drvdata->nr_hreg > 0) {
		drvdata->hreg = devm_kzalloc(dev, drvdata->nr_hreg *
					     sizeof(*drvdata->hreg),
					     GFP_KERNEL);
		if (!drvdata->hreg)
			return -ENOMEM;

		for (i = 0; i < drvdata->nr_hreg; i++) {
			ret = of_property_read_string_index(pdev->dev.of_node,
							    "qcom,hwevent-regs",
							    i, &hreg_name);
			if (ret)
				return ret;

			drvdata->hreg[i] = devm_regulator_get(dev, hreg_name);
			if (IS_ERR(drvdata->hreg[i]))
				return PTR_ERR(drvdata->hreg[i]);
		}
	}
	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	desc->type = CORESIGHT_DEV_TYPE_NONE;
	desc->pdata = pdev->dev.platform_data;
	desc->dev = &pdev->dev;
	desc->groups = hwevent_attr_grps;
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

static const struct of_device_id hwevent_match[] = {
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
