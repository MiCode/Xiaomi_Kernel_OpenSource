// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2022 Spreadtrum Communications Inc.

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "core.h"

#define AUTOTEST_DEV_MAX 10

struct autotest_device {
	struct device dev;
	struct cdev char_dev;
	struct list_head list;
	struct mutex lock;
};

struct autotest_device *autotest;
struct class *autotest_class;
static dev_t autotest_devt;

static struct autotest_handler *
sprd_autotest_find_handler(unsigned int type)
{
	struct autotest_handler *handler;

	mutex_lock(&autotest->lock);
	list_for_each_entry(handler, &autotest->list, node) {
		if (handler->type == type) {
			mutex_unlock(&autotest->lock);
			return handler;
		}
	}
	mutex_unlock(&autotest->lock);

	return NULL;
}

static long sprd_autotest_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	struct autotest_handler *handler;

	pr_info("autotest cmd = %x\n", cmd);

	handler = sprd_autotest_find_handler(cmd);
	if (!handler)
		return -EINVAL;

	return handler->start_test(handler, (void *)arg);
}

static int sprd_autotest_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int sprd_autotest_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static const struct file_operations sprd_autotest_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = sprd_autotest_ioctl,
	.compat_ioctl = sprd_autotest_ioctl,
	.open = sprd_autotest_open,
	.release = sprd_autotest_release,
};

int sprd_autotest_register_handler(struct autotest_handler *handler)
{
	int ret;

	if (!handler || !handler->start_test)
		return -EINVAL;

	if (sprd_autotest_find_handler(handler->type)) {
		pr_err("try to add dupicated autotest handler.\n");
		return -EEXIST;
	}

	if (handler->pre_test) {
		ret = handler->pre_test(handler, handler->data);
		if (ret)
			return ret;
	}

	INIT_LIST_HEAD(&handler->node);
	mutex_lock(&autotest->lock);
	list_add_tail(&handler->node, &autotest->list);
	mutex_unlock(&autotest->lock);

	return 0;
}
EXPORT_SYMBOL_GPL(sprd_autotest_register_handler);

void sprd_autotest_unregister_handler(struct autotest_handler *handler)
{
	if (!handler)
		return;

	if (handler->post_test)
		handler->post_test(handler, handler->data);

	mutex_lock(&autotest->lock);
	list_del(&handler->node);
	mutex_unlock(&autotest->lock);
}
EXPORT_SYMBOL_GPL(sprd_autotest_unregister_handler);

static int __init sprd_autotest_init(void)
{
	int ret;

	autotest = kzalloc(sizeof(*autotest), GFP_KERNEL);
	if (!autotest)
		return -ENOMEM;

	autotest_class = class_create(THIS_MODULE, "autotest");
	if (IS_ERR(autotest_class)) {
		pr_err("failed to create autotest class.\n");
		ret = PTR_ERR(autotest_class);
		goto class_err;
	}

	ret = alloc_chrdev_region(&autotest_devt, 0,
				  AUTOTEST_DEV_MAX, "autotest");
	if (ret < 0) {
		pr_err("failed to allocate chardev region.\n");
		goto alloc_chrdev_err;
	}

	mutex_init(&autotest->lock);
	INIT_LIST_HEAD(&autotest->list);

	cdev_init(&autotest->char_dev, &sprd_autotest_fops);
	autotest->char_dev.owner = THIS_MODULE;
	autotest->dev.devt = MKDEV(MAJOR(autotest_devt), 0);
	autotest->dev.class = autotest_class;
	dev_set_name(&autotest->dev, "autotest%d", 0);
	device_initialize(&autotest->dev);
	dev_set_drvdata(&autotest->dev, autotest);

	ret = cdev_device_add(&autotest->char_dev, &autotest->dev);
	if (ret) {
		pr_err("failed to add autotest device.\n");
		goto cdev_add_err;
	}

	return 0;

cdev_add_err:
	unregister_chrdev_region(autotest_devt, AUTOTEST_DEV_MAX);
alloc_chrdev_err:
	class_destroy(autotest_class);
class_err:
	kfree(autotest);

	return ret;
}

static void __exit sprd_autotest_exit(void)
{
	cdev_device_del(&autotest->char_dev, &autotest->dev);
	unregister_chrdev_region(autotest_devt, AUTOTEST_DEV_MAX);
	class_destroy(autotest_class);
	kfree(autotest);
}

subsys_initcall(sprd_autotest_init);
module_exit(sprd_autotest_exit);

MODULE_DESCRIPTION("sprd autotest core driver");
MODULE_LICENSE("GPL v2");
