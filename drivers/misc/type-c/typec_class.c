/* typec class
 *
 * Copyright (C) 2015 fengwei <fengwei@xiaomi.com	>
 * Copyright (c) 2015-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "typec: " fmt

#include <linux/module.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/err.h>

#include "typec_class.h"

static struct class *typec_class;
static atomic_t device_count;

static ssize_t status_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct typec_dev *tdev = dev_get_drvdata(dev);
	int mode = tdev->get_mode(tdev);

	return sprintf(buf, "%d\n", mode);
}

static ssize_t status_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct typec_dev *tdev = dev_get_drvdata(dev);
	int value;

	if (sscanf(buf, "%d", &value) != 1)
		return -EINVAL;

	if ((value < 0) || (value > 3))
		return -EINVAL;

	tdev->set_mode(tdev, value);

	return size;
}

static DEVICE_ATTR(status, S_IRUGO | S_IWUSR, status_show, status_store);

static ssize_t direction_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct typec_dev *tdev = dev_get_drvdata(dev);
	int direction = 0;
	if (tdev && tdev->get_direction)
		direction = tdev->get_direction(tdev);

	return sprintf(buf, "%d\n", direction);
}
static DEVICE_ATTR(direction, S_IRUGO , direction_show, NULL);


static int create_typec_class(void)
{
	if (!typec_class) {
		typec_class = class_create(THIS_MODULE, "typec");
		if (IS_ERR(typec_class))
			return PTR_ERR(typec_class);
		atomic_set(&device_count, 0);
	}

	return 0;
}

int typec_dev_register(struct typec_dev *tdev)
{
	int ret;

	if (!tdev || !tdev->name || !tdev->get_mode || !tdev->set_mode)
		return -EINVAL;

	if (atomic_inc_return(&device_count) > 1) {
		pr_err("Typec dev has registered,failed to register driver %s\n", tdev->name);
		return -EINVAL;
	}

	ret = create_typec_class();
	if (ret < 0)
		return ret;

	tdev->dev = device_create(typec_class, NULL,
		MKDEV(0, 0), NULL, tdev->name);
	if (IS_ERR(tdev->dev))
		return PTR_ERR(tdev->dev);

	ret = device_create_file(tdev->dev, &dev_attr_status);
	if (ret < 0)
		goto err_create_file;

	ret = device_create_file(tdev->dev, &dev_attr_direction);
	if (ret < 0)
		goto err_create_file;

	dev_set_drvdata(tdev->dev, tdev);
	return 0;

err_create_file:
	device_destroy(typec_class, MKDEV(0, 0));
	pr_err("failed to register driver %s\n",
			tdev->name);

	return ret;
}
EXPORT_SYMBOL_GPL(typec_dev_register);

void typec_dev_unregister(struct typec_dev *tdev)
{
	device_remove_file(tdev->dev, &dev_attr_status);
	dev_set_drvdata(tdev->dev, NULL);
	device_destroy(typec_class, MKDEV(0, 0));
	atomic_dec_return(&device_count);
}
EXPORT_SYMBOL_GPL(typec_dev_unregister);

static int __init typec_init(void)
{
	return create_typec_class();
}

static void __exit typec_exit(void)
{
	class_destroy(typec_class);
}

module_init(typec_init);
module_exit(typec_exit);

MODULE_AUTHOR("fengwei <fengwei@xiaomi.com>");
MODULE_DESCRIPTION("typec  class driver");
MODULE_LICENSE("GPL");
