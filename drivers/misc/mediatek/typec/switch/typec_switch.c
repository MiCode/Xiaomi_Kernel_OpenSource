// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/usb/typec.h>
#include <linux/usb/typec_mux.h>
#include <linux/sysfs.h>

#include "typec_switch.h"

static int mtk_typec_mux_set(struct typec_mux *mux, int state)
{
	struct mtk_typec_switch *typec_switch = typec_mux_get_drvdata(mux);
	int ret = 0;

	mutex_lock(&typec_switch->lock);

	/* do mux set */

	mutex_unlock(&typec_switch->lock);

	return ret;
}

static int mtk_typec_switch_set(struct typec_switch *sw,
			      enum typec_orientation orientation)
{
	struct mtk_typec_switch *typec_switch = typec_switch_get_drvdata(sw);
	int ret = 0;

	dev_info(typec_switch->dev, "%s %d %d\n", __func__,
		 typec_switch->orientation, orientation);

	if (typec_switch->orientation == orientation)
		return ret;

	mutex_lock(&typec_switch->lock);

	if (typec_switch->fusb)
		ret = fusb304_set_conf(typec_switch->fusb, orientation);
	if (ret)
		dev_err(typec_switch->dev, "fusb304 set fail %d\n", ret);

	if (typec_switch->ptn)
		ret = ptn36241g_set_conf(typec_switch->ptn, orientation);
	if (ret)
		dev_err(typec_switch->dev, "ptn36241g set fail %d\n", ret);

	typec_switch->orientation = orientation;

	mutex_unlock(&typec_switch->lock);

	return ret;
}

static ssize_t sw_store(struct device *dev,
			 struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct mtk_typec_switch *typec_switch =
		(struct mtk_typec_switch *)dev->driver_data;
	u32 tmp;

	if (kstrtouint(buf, 0, &tmp))
		return -EINVAL;

	if (tmp > 2) {
		dev_info(typec_switch->dev, "%s %d, INVALID: %d\n", __func__,
			 typec_switch->orientation, tmp);
		return count;
	}

	mtk_typec_switch_set(typec_switch->sw, tmp);

	return count;
}

static ssize_t sw_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct mtk_typec_switch *typec_switch =
		(struct mtk_typec_switch *)dev->driver_data;
	char str[16];

	switch (typec_switch->orientation) {
	case TYPEC_ORIENTATION_NONE:
		strncpy(str, "NONE\0", 5);
		break;
	case TYPEC_ORIENTATION_NORMAL:
		strncpy(str, "NORMAL\0", 7);
		break;
	case TYPEC_ORIENTATION_REVERSE:
		strncpy(str, "REVERSE\0", 8);
		break;
	default:
		strncpy(str, "INVALID\0", 8);
	}

	dev_info(typec_switch->dev, "%s %d %s\n", __func__,
		 typec_switch->orientation, str);

	return sprintf(buf, "%d\n", typec_switch->orientation);
}
static DEVICE_ATTR_RW(sw);

static struct attribute *mtk_typec_switch_attrs[] = {
	&dev_attr_sw.attr,
	NULL
};

static const struct attribute_group mtk_typec_switch_group = {
	.attrs = mtk_typec_switch_attrs,
};

static int mtk_typec_switch_sysfs_init(struct mtk_typec_switch *typec_switch)
{
	struct device *dev = typec_switch->dev;
	int ret;

	ret = sysfs_create_group(&dev->kobj, &mtk_typec_switch_group);
	if (ret)
		dev_info(dev, "failed to creat sysfs attributes\n");
	return ret;
}

static int mtk_typec_switch_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct mtk_typec_switch *typec_switch;
	struct typec_switch_desc sw_desc;
	struct typec_mux_desc mux_desc;
	int index;
	int ret = 0;

	dev_info(dev, "%s\n", __func__);

	typec_switch = kzalloc(sizeof(*typec_switch), GFP_KERNEL);
	if (!typec_switch)
		return -ENOMEM;

	typec_switch->dev = dev;

	index = of_property_match_string(np,
					"switch-names", "ptn36241g");
	if (index >= 0) {
		typec_switch->ptn = devm_kzalloc(dev,
			sizeof(*typec_switch->ptn), GFP_KERNEL);

		if (!typec_switch->ptn) {
			dev_err(dev, "ptn alloc fail\n");
			return -ENOMEM;
		}

		typec_switch->ptn->dev = dev;

		if (ptn36241g_init(typec_switch->ptn)) {
			devm_kfree(dev, typec_switch->ptn);
			dev_err(dev, "ptn36241g init fail\n");
		}
	}

	index = of_property_match_string(np,
					"switch-names", "fusb304");
	if (index >= 0) {
		typec_switch->fusb = devm_kzalloc(dev,
			sizeof(*typec_switch->fusb), GFP_KERNEL);

		if (!typec_switch->fusb) {
			dev_err(dev, "fusb alloc fail\n");
			return -ENOMEM;
		}

		typec_switch->fusb->dev = dev;

		if (fusb304_init(typec_switch->fusb)) {
			devm_kfree(dev, typec_switch->fusb);
			dev_err(dev, "fusb304 init fail\n");
		}
	}

	sw_desc.drvdata = typec_switch;
	sw_desc.fwnode = dev->fwnode;
	sw_desc.set = mtk_typec_switch_set;

	typec_switch->sw = typec_switch_register(dev, &sw_desc);
	if (IS_ERR(typec_switch->sw)) {
		dev_err(dev, "error registering typec switch: %ld\n",
			PTR_ERR(typec_switch->sw));
		return PTR_ERR(typec_switch->sw);
	}

	mux_desc.drvdata = typec_switch;
	mux_desc.fwnode = dev->fwnode;
	mux_desc.set = mtk_typec_mux_set;

	typec_switch->mux = typec_mux_register(dev, &mux_desc);
	if (IS_ERR(typec_switch->mux)) {
		typec_switch_unregister(typec_switch->sw);
		dev_err(dev, "error registering typec mux: %ld\n",
			PTR_ERR(typec_switch->mux));
		return PTR_ERR(typec_switch->mux);
	}

	platform_set_drvdata(pdev, typec_switch);

	/* create sysfs for half-automation switch */
	mtk_typec_switch_sysfs_init(typec_switch);

	dev_info(dev, "%s done\n", __func__);
	return ret;
}

static const struct of_device_id mtk_typec_switch_ids[] = {
	{.compatible = "mediatek,typec_switch",},
	{},
};

static struct platform_driver mtk_typec_switch_driver = {
	.probe = mtk_typec_switch_probe,
	.driver = {
		.name = "mtk-typec-switch",
		.of_match_table = mtk_typec_switch_ids,
	},
};

module_platform_driver(mtk_typec_switch_driver);
