// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>  /* Needed by all modules */
#include <linux/kernel.h>  /* Needed for KERN_ALERT */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>

#include <linux/init.h>
#include <linux/io.h>

#include "apusys_power.h"

#include "sample_drv.h"
#include "sample_cmn.h"
#include "sample_inf.h"

/* define */
#define APUSYS_SAMPLE_DEV_NAME "apusys_sample"

/* global variable */
static dev_t sample_devt;
static struct cdev *sample_cdev;
//static struct class *sample_class;

/* function declaration */
static int sample_open(struct inode *, struct file *);
static int sample_release(struct inode *, struct file *);
static long sample_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg);

static const struct file_operations sample_fops = {
	.open = sample_open,
	.release = sample_release,
	.unlocked_ioctl = sample_ioctl,
	.release = sample_release,
};

static int sample_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int sample_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int sample_probe(struct platform_device *pdev)
{
	int ret = 0;

	/* get major */
	ret = alloc_chrdev_region(&sample_devt, 0, 1, APUSYS_SAMPLE_DEV_NAME);
	if (ret < 0) {
		spl_drv_err("alloc_chrdev_region failed, %d\n", ret);
		return ret;
	}

	/* Allocate driver */
	sample_cdev = cdev_alloc();
	if (sample_cdev == NULL) {
		spl_drv_err("cdev_alloc failed\n");
		ret = -ENOMEM;
		goto out;
	}

	/* Attatch file operation. */
	cdev_init(sample_cdev, &sample_fops);
	sample_cdev->owner = THIS_MODULE;

	/* Add to system */
	ret = cdev_add(sample_cdev, sample_devt, 1);
	if (ret < 0) {
		spl_drv_err("Attatch file operation failed, %d\n", ret);
		goto out;
	}

	sample_device_init();

	return 0;
out:
	/* Release char driver */
	if (sample_cdev != NULL) {
		cdev_del(sample_cdev);
		sample_cdev = NULL;
	}
	unregister_chrdev_region(sample_devt, 1);
	return 0;
}

static int sample_remove(struct platform_device *pdev)
{
	sample_device_destroy();

	/* Release char driver */
	if (sample_cdev != NULL) {
		cdev_del(sample_cdev);
		sample_cdev = NULL;
	}
	unregister_chrdev_region(sample_devt, 1);

	return 0;
}

static int sample_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int sample_resume(struct platform_device *pdev)
{
	return 0;
}

static long sample_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static struct platform_driver sample_driver = {
	.probe = sample_probe,
	.remove = sample_remove,
	.suspend = sample_suspend,
	.resume  = sample_resume,
	//.pm = apusys_pm_qos,
	.driver = {
		.name = APUSYS_SAMPLE_DEV_NAME,
		.owner = THIS_MODULE,
	},
};

static struct platform_device sample_device = {
	.name = APUSYS_SAMPLE_DEV_NAME,
	.id = -1,
};

static int __init sample_init(void)
{
	if (!apusys_power_check()) {
		spl_drv_err("apusys disable\n");
		return -ENODEV;
	}

	if (platform_driver_register(&sample_driver)) {
		spl_drv_err("failed to register apusys sample driver)\n");
		return -ENODEV;
	}

	if (platform_device_register(&sample_device)) {
		spl_drv_err("failed to register apusys sample device\n");
		platform_driver_unregister(&sample_driver);
		return -ENODEV;
	}

	return 0;
}

static void __exit sample_destroy(void)
{
	platform_driver_unregister(&sample_driver);
	platform_device_unregister(&sample_device);
}

module_init(sample_init);
module_exit(sample_destroy);
MODULE_DESCRIPTION("MTK APUSYS SAMPLE Driver");
MODULE_AUTHOR("SS5");
MODULE_LICENSE("GPL");
