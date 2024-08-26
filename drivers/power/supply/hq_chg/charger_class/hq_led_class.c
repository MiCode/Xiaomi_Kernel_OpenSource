// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (c) 2023 Huaqin Technology(Shanghai) Co., Ltd.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/err.h>
#include <linux/of.h>

#include "hq_led_class.h"

static struct class *subpmic_led_class;

int subpmic_camera_set_led_flash_curr(struct subpmic_led_dev *subpmic_led, enum SUBPMIC_LED_ID id, int ma)
{
	if (!subpmic_led || !subpmic_led->ops)
		return -EINVAL;
	if (subpmic_led->ops->set_led_flash_curr == NULL)
		return -EOPNOTSUPP;
	return subpmic_led->ops->set_led_flash_curr(subpmic_led, id, ma);
}
EXPORT_SYMBOL(subpmic_camera_set_led_flash_curr);

int subpmic_camera_set_led_flash_time(struct subpmic_led_dev *subpmic_led, enum SUBPMIC_LED_ID id, int ms)
{
	if (!subpmic_led || !subpmic_led->ops)
		return -EINVAL;
	if (subpmic_led->ops->set_led_flash_time == NULL)
		return -EOPNOTSUPP;
	return subpmic_led->ops->set_led_flash_time(subpmic_led, id, ms);
}
EXPORT_SYMBOL(subpmic_camera_set_led_flash_time);

int subpmic_camera_set_led_flash_enable(struct subpmic_led_dev *subpmic_led, enum SUBPMIC_LED_ID id, bool en)
{
	if (!subpmic_led || !subpmic_led->ops)
		return -EINVAL;
	if (subpmic_led->ops->set_led_flash_enable == NULL)
		return -EOPNOTSUPP;
	return subpmic_led->ops->set_led_flash_enable(subpmic_led, id, en);
}
EXPORT_SYMBOL(subpmic_camera_set_led_flash_enable);

int subpmic_camera_set_led_torch_curr(struct subpmic_led_dev *subpmic_led, enum SUBPMIC_LED_ID id, int ma)
{
	if (!subpmic_led || !subpmic_led->ops)
		return -EINVAL;
	if (subpmic_led->ops->set_led_torch_curr == NULL)
		return -EOPNOTSUPP;
	return subpmic_led->ops->set_led_torch_curr(subpmic_led, id, ma);
}
EXPORT_SYMBOL(subpmic_camera_set_led_torch_curr);

int subpmic_camera_set_led_torch_enable(struct subpmic_led_dev *subpmic_led, enum SUBPMIC_LED_ID id, bool en)
{
	if (!subpmic_led || !subpmic_led->ops)
		return -EINVAL;
	if (subpmic_led->ops->set_led_torch_enable == NULL)
		return -EOPNOTSUPP;
	return subpmic_led->ops->set_led_torch_enable(subpmic_led, id, en);
}
EXPORT_SYMBOL(subpmic_camera_set_led_torch_enable);

static int subpmic_led_match_device_by_name(struct device *dev, const void *data)
{
	const char *name = data;
	struct subpmic_led_dev *subpmic_led = dev_get_drvdata(dev);

	return strcmp(subpmic_led->name, name) == 0;
}

struct subpmic_led_dev *subpmic_led_find_dev_by_name(const char *name)
{
	struct subpmic_led_dev *subpmic_led = NULL;
	struct device *dev = class_find_device(subpmic_led_class, NULL, name,
					subpmic_led_match_device_by_name);

	if (dev) {
		subpmic_led = dev_get_drvdata(dev);
	}

	return subpmic_led;
}
EXPORT_SYMBOL(subpmic_led_find_dev_by_name);

struct subpmic_led_dev *subpmic_led_register(char *name, struct device *parent,
							struct subpmic_led_ops *ops, void *private)
{
	struct subpmic_led_dev *subpmic_led;
	struct device *dev;
	int ret;

	if (!parent)
		pr_warn("%s: Expected proper parent device\n", __func__);

	if (!ops || !name)
		return ERR_PTR(-EINVAL);

	subpmic_led = kzalloc(sizeof(*subpmic_led), GFP_KERNEL);
	if (!subpmic_led)
		return ERR_PTR(-ENOMEM);

	dev = &(subpmic_led->dev);

	device_initialize(dev);

	dev->class = subpmic_led_class;
	dev->parent = parent;
	dev_set_drvdata(dev, subpmic_led);

	subpmic_led->private = private;

	ret = dev_set_name(dev, "%s", name);
	if (ret)
		goto dev_set_name_failed;

	ret = device_add(dev);
	if (ret)
		goto device_add_failed;

	subpmic_led->name = name;
	subpmic_led->ops = ops;

	return subpmic_led;

device_add_failed:
dev_set_name_failed:
	put_device(dev);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(subpmic_led_register);

void * subpmic_led_get_private(struct subpmic_led_dev *subpmic_led)
{
	if (!subpmic_led)
		return ERR_PTR(-EINVAL);
	return subpmic_led->private;
}
EXPORT_SYMBOL(subpmic_led_get_private);

int subpmic_led_unregister(struct subpmic_led_dev *subpmic_led)
{
	device_unregister(&subpmic_led->dev);
	kfree(subpmic_led);
	return 0;
}

static int __init subpmic_led_class_init(void)
{
	subpmic_led_class = class_create(THIS_MODULE, "subpmic_led_class");
	if (IS_ERR(subpmic_led_class)) {
		return PTR_ERR(subpmic_led_class);
	}

	subpmic_led_class->dev_uevent = NULL;

	return 0;
}

static void __exit subpmic_led_class_exit(void)
{
	class_destroy(subpmic_led_class);
}

subsys_initcall(subpmic_led_class_init);
module_exit(subpmic_led_class_exit);

MODULE_DESCRIPTION("Huaqin Led Class Core");
MODULE_LICENSE("GPL v2");
