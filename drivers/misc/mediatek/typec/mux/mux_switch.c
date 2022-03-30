// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/usb/typec.h>
#include <linux/usb/typec_mux.h>
#include <linux/sysfs.h>
#include <mux.h>

#include "bus.h"
#include "mux_switch.h"

/* struct typec_mux_switch */
struct typec_mux_switch {
	struct device *dev;
	struct typec_switch *sw;
	struct typec_mux *mux;
	int orientation;
	struct typec_mux_state state;
};

/* struct mtk_typec_switch */
struct mtk_typec_switch {
	struct device *dev;
	struct typec_switch *sw;
	struct list_head list;
};

/* struct mtk_typec_mux */
struct mtk_typec_mux {
	struct device *dev;
	struct typec_mux *mux;
	struct list_head list;
};

static LIST_HEAD(mux_list);
static LIST_HEAD(switch_list);
static DEFINE_MUTEX(mux_lock);
static DEFINE_MUTEX(switch_lock);

/* MUX */
struct typec_mux *mtk_typec_mux_register(struct device *dev,
			const struct typec_mux_desc *desc)
{
	struct mtk_typec_mux *typec_mux;
	struct typec_mux *mux;

	mutex_lock(&mux_lock);
	list_for_each_entry(typec_mux, &mux_list, list) {
		if (typec_mux->dev == dev) {
			mux = ERR_PTR(-EEXIST);
			goto out;
		}
	}

	typec_mux = kzalloc(sizeof(*typec_mux), GFP_KERNEL);
	if (!typec_mux) {
		mux = ERR_PTR(-ENOMEM);
		goto out;
	}

	mux = typec_mux_register(dev, desc);
	if (IS_ERR(mux)) {
		kfree(typec_mux);
		mux = ERR_PTR(-EINVAL);
		goto out;
	}
	typec_mux->mux = mux;
	list_add_tail(&typec_mux->list, &mux_list);
out:
	mutex_unlock(&mux_lock);
	return mux;
}
EXPORT_SYMBOL_GPL(mtk_typec_mux_register);

void mtk_typec_mux_unregister(struct typec_mux *mux)
{
	struct mtk_typec_mux *typec_mux;

	mutex_lock(&mux_lock);
	list_for_each_entry(typec_mux, &mux_list, list) {
		if (typec_mux->mux == mux)
			break;
	}

	list_del(&typec_mux->list);
	kfree(typec_mux);
	mutex_unlock(&mux_lock);

	typec_mux_unregister(mux);
}
EXPORT_SYMBOL_GPL(mtk_typec_mux_unregister);

static int mtk_typec_mux_set(struct typec_mux *mux, struct typec_mux_state *state)
{
	struct typec_mux_switch *mux_sw = typec_mux_get_drvdata(mux);
	struct mtk_typec_mux *typec_mux;
	int ret = 0;

	dev_info(mux_sw->dev, "%s %d %d\n", __func__,
		 mux_sw->state.mode, state->mode);

	mutex_lock(&mux_lock);

	list_for_each_entry(typec_mux, &mux_list, list) {
		if (!IS_ERR_OR_NULL(typec_mux->mux))
			typec_mux->mux->set(typec_mux->mux, state);
	}

	mux_sw->state.alt = state->alt;
	mux_sw->state.mode = state->mode;
	mux_sw->state.data = state->data;

	mutex_unlock(&mux_lock);

	return ret;
}

static ssize_t mux_store(struct device *dev,
			 struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct typec_mux_switch *mux_sw =
		(struct typec_mux_switch *)dev->driver_data;
	u32 tmp;
	struct typec_mux_state state = {};

	if (kstrtouint(buf, 0, &tmp))
		return -EINVAL;

	if (tmp > 2) {
		dev_info(mux_sw->dev, "%s %d, INVALID: %d\n", __func__,
			 mux_sw->state.mode, tmp);
		return count;
	}

	state.mode = tmp;
	mtk_typec_mux_set(mux_sw->mux, &state);

	return count;
}

static ssize_t mux_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct typec_mux_switch *mux_sw =
		(struct typec_mux_switch *)dev->driver_data;

	return sprintf(buf, "%d\n", mux_sw->state.mode);
}
static DEVICE_ATTR_RW(mux);

/* SWITCH */
struct typec_switch *mtk_typec_switch_register(struct device *dev,
			const struct typec_switch_desc *desc)
{
	struct mtk_typec_switch *typec_sw;
	struct typec_switch *sw;

	mutex_lock(&switch_lock);
	list_for_each_entry(typec_sw, &switch_list, list) {
		if (typec_sw->dev == dev) {
			sw = ERR_PTR(-EEXIST);
			goto out;
		}
	}

	typec_sw = kzalloc(sizeof(*typec_sw), GFP_KERNEL);
	if (!typec_sw) {
		sw = ERR_PTR(-ENOMEM);
		goto out;
	}

	sw = typec_switch_register(dev, desc);
	if (IS_ERR(sw)) {
		kfree(typec_sw);
		sw = ERR_PTR(-EINVAL);
		goto out;
	}
	typec_sw->sw = sw;
	list_add_tail(&typec_sw->list, &switch_list);
out:
	mutex_unlock(&switch_lock);
	return sw;
}
EXPORT_SYMBOL_GPL(mtk_typec_switch_register);

void mtk_typec_switch_unregister(struct typec_switch *sw)
{
	struct mtk_typec_switch *typec_sw;

	mutex_lock(&switch_lock);
	list_for_each_entry(typec_sw, &switch_list, list) {
		if (typec_sw->sw == sw)
			break;
	}
	list_del(&typec_sw->list);
	kfree(typec_sw);
	mutex_unlock(&switch_lock);

	typec_switch_unregister(sw);
}
EXPORT_SYMBOL_GPL(mtk_typec_switch_unregister);

static int mtk_typec_switch_set(struct typec_switch *sw,
			      enum typec_orientation orientation)
{
	struct typec_mux_switch *mux_sw = typec_switch_get_drvdata(sw);
	struct mtk_typec_switch *typec_sw;
	int ret = 0;

	dev_info(mux_sw->dev, "%s %d %d\n", __func__,
		 mux_sw->orientation, orientation);

	if (mux_sw->orientation == orientation)
		return ret;

	mutex_lock(&switch_lock);

	list_for_each_entry(typec_sw, &switch_list, list) {
		if (!IS_ERR_OR_NULL(typec_sw->sw))
			typec_sw->sw->set(typec_sw->sw, orientation);
	}

	mux_sw->orientation = orientation;

	mutex_unlock(&switch_lock);

	return ret;
}

static ssize_t sw_store(struct device *dev,
			 struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct typec_mux_switch *mux_sw =
		(struct typec_mux_switch *)dev->driver_data;
	u32 tmp;

	if (kstrtouint(buf, 0, &tmp))
		return -EINVAL;

	if (tmp > 2) {
		dev_info(mux_sw->dev, "%s %d, INVALID: %d\n", __func__,
			 mux_sw->orientation, tmp);
		return count;
	}

	mtk_typec_switch_set(mux_sw->sw, tmp);

	return count;
}

static ssize_t sw_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct typec_mux_switch *mux_sw =
		(struct typec_mux_switch *)dev->driver_data;
	char str[16];

	switch (mux_sw->orientation) {
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

	dev_info(mux_sw->dev, "%s %d %s\n", __func__,
		 mux_sw->orientation, str);

	return sprintf(buf, "%d\n", mux_sw->orientation);
}
static DEVICE_ATTR_RW(sw);

static struct attribute *typec_mux_switch_attrs[] = {
	&dev_attr_sw.attr,
	&dev_attr_mux.attr,
	NULL
};

static const struct attribute_group typec_mux_switch_group = {
	.attrs = typec_mux_switch_attrs,
};

static int typec_mux_switch_sysfs_init(struct typec_mux_switch *mux_sw)
{
	struct device *dev = mux_sw->dev;
	int ret = 0;

	ret = sysfs_create_group(&dev->kobj, &typec_mux_switch_group);
	if (ret)
		dev_info(dev, "failed to creat sysfs attributes\n");

	return ret;
}

static int typec_mux_switch_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct typec_mux_switch *mux_sw;
	struct typec_switch_desc sw_desc;
	struct typec_mux_desc mux_desc;
	int ret = 0;

	dev_info(dev, "%s\n", __func__);

	mux_sw = kzalloc(sizeof(*mux_sw), GFP_KERNEL);
	if (!mux_sw)
		return -ENOMEM;

	mux_sw->dev = dev;

	sw_desc.drvdata = mux_sw;
	sw_desc.fwnode = dev->fwnode;
	sw_desc.set = mtk_typec_switch_set;

	mux_sw->sw = typec_switch_register(dev, &sw_desc);
	if (IS_ERR(mux_sw->sw)) {
		dev_info(dev, "error registering typec switch: %ld\n",
			PTR_ERR(mux_sw->sw));
		return PTR_ERR(mux_sw->sw);
	}

	mux_desc.drvdata = mux_sw;
	mux_desc.fwnode = dev->fwnode;
	mux_desc.set = mtk_typec_mux_set;

	mux_sw->mux = typec_mux_register(dev, &mux_desc);
	if (IS_ERR(mux_sw->mux)) {
		typec_switch_unregister(mux_sw->sw);
		dev_info(dev, "error registering typec mux: %ld\n",
			PTR_ERR(mux_sw->mux));
		return PTR_ERR(mux_sw->mux);
	}

	platform_set_drvdata(pdev, mux_sw);

	/* create sysfs for half-automation switch */
	typec_mux_switch_sysfs_init(mux_sw);

	dev_info(dev, "%s done\n", __func__);
	return ret;
}

static const struct of_device_id typec_mux_switch_ids[] = {
	{.compatible = "mediatek,typec_mux_switch",},
	{},
};

static struct platform_driver typec_mux_switch_driver = {
	.probe = typec_mux_switch_probe,
	.driver = {
		.name = "mtk-typec-mux-switch",
		.of_match_table = typec_mux_switch_ids,
	},
};

module_platform_driver(typec_mux_switch_driver);

MODULE_DESCRIPTION("Mediatek Type-C mux switch driver");
MODULE_LICENSE("GPL");
