/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/of_coresight.h>
#include <linux/coresight.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include "coresight-priv.h"

#define qpdi_writel(drvdata, val, off)	__raw_writel((val), drvdata->base + off)
#define qpdi_readl(drvdata, off)	__raw_readl(drvdata->base + off)

#define QPDI_DISABLE_CFG	(0x0)

static int boot_enable;
module_param_named(
	boot_enable, boot_enable, int, S_IRUGO
);

struct qpdi_drvdata {
	void __iomem		*base;
	struct device		*dev;
	struct coresight_device	*csdev;
	struct mutex		mutex;
	struct regulator	*reg;
	unsigned int		reg_low;
	unsigned int		reg_high;
	unsigned int		reg_lpm;
	unsigned int		reg_hpm;
	struct regulator	*reg_io;
	unsigned int		reg_low_io;
	unsigned int		reg_high_io;
	unsigned int		reg_lpm_io;
	unsigned int		reg_hpm_io;
	bool			enable;
};

static int qpdi_reg_set_optimum_mode(struct regulator *reg,
				     unsigned int reg_hpm)
{
	if (regulator_count_voltages(reg) <= 0)
		return 0;

	return regulator_set_optimum_mode(reg, reg_hpm);
}


static int qpdi_reg_set_voltage(struct regulator *reg, unsigned int reg_low,
				unsigned int reg_high)
{
	if (regulator_count_voltages(reg) <= 0)
		return 0;

	return regulator_set_voltage(reg, reg_low, reg_high);
}

static int __qpdi_enable(struct qpdi_drvdata *drvdata)
{
	int ret;

	if (!drvdata->reg || !drvdata->reg_io)
		return -EINVAL;

	ret = qpdi_reg_set_optimum_mode(drvdata->reg, drvdata->reg_hpm);
	if (ret < 0)
		return ret;
	ret = qpdi_reg_set_voltage(drvdata->reg, drvdata->reg_low,
				   drvdata->reg_high);
	if (ret)
		goto err0;
	ret = regulator_enable(drvdata->reg);
	if (ret)
		goto err1;
	ret = qpdi_reg_set_optimum_mode(drvdata->reg_io, drvdata->reg_hpm_io);
	if (ret < 0)
		goto err2;
	ret = qpdi_reg_set_voltage(drvdata->reg_io, drvdata->reg_low_io,
				   drvdata->reg_high_io);
	if (ret)
		goto err3;
	ret = regulator_enable(drvdata->reg_io);
	if (ret)
		goto err4;
	return 0;
err4:
	qpdi_reg_set_voltage(drvdata->reg_io, 0, drvdata->reg_high_io);
err3:
	qpdi_reg_set_optimum_mode(drvdata->reg_io, 0);
err2:
	regulator_disable(drvdata->reg);
err1:
	qpdi_reg_set_voltage(drvdata->reg, 0, drvdata->reg_high);
err0:
	qpdi_reg_set_optimum_mode(drvdata->reg, 0);
	return ret;
}

static int qpdi_enable(struct qpdi_drvdata *drvdata)
{
	int ret;

	mutex_lock(&drvdata->mutex);

	if (drvdata->enable)
		goto out;

	ret = __qpdi_enable(drvdata);
	if (ret)
		goto err;

	qpdi_writel(drvdata, 0, QPDI_DISABLE_CFG);

	drvdata->enable = true;
	dev_info(drvdata->dev, "qpdi enabled\n");
out:
	mutex_unlock(&drvdata->mutex);
	return 0;
err:
	mutex_unlock(&drvdata->mutex);
	return ret;
}

static void __qpdi_disable(struct qpdi_drvdata *drvdata)
{
	regulator_disable(drvdata->reg);
	qpdi_reg_set_voltage(drvdata->reg, 0, drvdata->reg_high);
	qpdi_reg_set_optimum_mode(drvdata->reg, 0);

	regulator_disable(drvdata->reg_io);
	qpdi_reg_set_voltage(drvdata->reg_io, 0, drvdata->reg_high_io);
	qpdi_reg_set_optimum_mode(drvdata->reg_io, 0);
}

static void qpdi_disable(struct qpdi_drvdata *drvdata)
{
	mutex_lock(&drvdata->mutex);

	if (!drvdata->enable) {
		mutex_unlock(&drvdata->mutex);
		return;
	}

	qpdi_writel(drvdata, 1, QPDI_DISABLE_CFG);

	__qpdi_disable(drvdata);

	drvdata->enable = false;
	mutex_unlock(&drvdata->mutex);
	dev_info(drvdata->dev, "qpdi disabled\n");
}

static ssize_t qpdi_show_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct qpdi_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = drvdata->enable;

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t qpdi_store_enable(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct qpdi_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	int ret = 0;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	if (val)
		ret = qpdi_enable(drvdata);
	else
		qpdi_disable(drvdata);

	if (ret)
		return ret;
	return size;
}
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, qpdi_show_enable,
		   qpdi_store_enable);

static struct attribute *qpdi_attrs[] = {
	&dev_attr_enable.attr,
	NULL,
};

static struct attribute_group qpdi_attr_grp = {
	.attrs = qpdi_attrs,
};

static const struct attribute_group *qpdi_attr_grps[] = {
	&qpdi_attr_grp,
	NULL,
};

static int qpdi_parse_of_data(struct platform_device *pdev,
			      struct qpdi_drvdata *drvdata)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *reg_node = NULL;
	struct device *dev = &pdev->dev;
	const __be32 *prop;
	int len;

	reg_node = of_parse_phandle(node, "vdd-supply", 0);
	if (reg_node) {
		drvdata->reg = devm_regulator_get(dev, "vdd");
		if (IS_ERR(drvdata->reg))
			return PTR_ERR(drvdata->reg);

		prop = of_get_property(node, "qcom,vdd-voltage-level", &len);
		if (!prop || (len != (2 * sizeof(__be32)))) {
			dev_err(dev, "sdc voltage levels not specified\n");
		} else {
			drvdata->reg_low = be32_to_cpup(&prop[0]);
			drvdata->reg_high = be32_to_cpup(&prop[1]);
		}

		prop = of_get_property(node, "qcom,vdd-current-level", &len);
		if (!prop || (len != (2 * sizeof(__be32)))) {
			dev_err(dev, "sdc current levels not specified\n");
		} else {
			drvdata->reg_lpm = be32_to_cpup(&prop[0]);
			drvdata->reg_hpm = be32_to_cpup(&prop[1]);
		}
		of_node_put(reg_node);
	} else {
		dev_err(dev, "sdc voltage supply not specified or available\n");
	}

	reg_node = of_parse_phandle(node, "vdd-io-supply", 0);
	if (reg_node) {
		drvdata->reg_io = devm_regulator_get(dev, "vdd-io");
		if (IS_ERR(drvdata->reg_io))
			return PTR_ERR(drvdata->reg_io);

		prop = of_get_property(node, "qcom,vdd-io-voltage-level", &len);
		if (!prop || (len != (2 * sizeof(__be32)))) {
			dev_err(dev, "sdc io voltage levels not specified\n");
		} else {
			drvdata->reg_low_io = be32_to_cpup(&prop[0]);
			drvdata->reg_high_io = be32_to_cpup(&prop[1]);
		}

		prop = of_get_property(node, "qcom,vdd-io-current-level", &len);
		if (!prop || (len != (2 * sizeof(__be32)))) {
			dev_err(dev, "sdc io current levels not specified\n");
		} else {
			drvdata->reg_lpm_io = be32_to_cpup(&prop[0]);
			drvdata->reg_hpm_io = be32_to_cpup(&prop[1]);
		}
		of_node_put(reg_node);
	} else {
		dev_err(dev,
			"sdc io voltage supply not specified or available\n");
	}
	return 0;
}

static int qpdi_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata;
	struct qpdi_drvdata *drvdata;
	struct resource *res;
	struct coresight_desc *desc;

	if (coresight_fuse_qpdi_access_disabled())
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

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "qpdi-base");
	if (!res)
		return -ENODEV;

	drvdata->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!drvdata->base)
		return -ENOMEM;

	mutex_init(&drvdata->mutex);

	if (pdev->dev.of_node) {
		ret = qpdi_parse_of_data(pdev, drvdata);
		if (ret)
			return ret;
	}

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	desc->type = CORESIGHT_DEV_TYPE_NONE;
	desc->pdata = pdev->dev.platform_data;
	desc->dev = &pdev->dev;
	desc->groups = qpdi_attr_grps;
	desc->owner = THIS_MODULE;
	drvdata->csdev = coresight_register(desc);
	if (IS_ERR(drvdata->csdev))
		return PTR_ERR(drvdata->csdev);

	if (boot_enable)
		qpdi_enable(drvdata);

	dev_info(dev, "CoreSight QPDI driver initialized\n");
	return 0;
}

static int qpdi_remove(struct platform_device *pdev)
{
	struct qpdi_drvdata *drvdata = platform_get_drvdata(pdev);

	coresight_unregister(drvdata->csdev);
	return 0;
}

static struct of_device_id qpdi_match[] = {
	{.compatible = "qcom,coresight-qpdi"},
	{}
};

static struct platform_driver qpdi_driver = {
	.probe		= qpdi_probe,
	.remove		= qpdi_remove,
	.driver		= {
		.name	= "coresight-qpdi",
		.owner	= THIS_MODULE,
		.of_match_table	= qpdi_match,
	},
};

static int __init qpdi_init(void)
{
	return platform_driver_register(&qpdi_driver);
}
module_init(qpdi_init);

static void __exit qpdi_exit(void)
{
	platform_driver_unregister(&qpdi_driver);
}
module_exit(qpdi_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CoreSight QPDI driver");
