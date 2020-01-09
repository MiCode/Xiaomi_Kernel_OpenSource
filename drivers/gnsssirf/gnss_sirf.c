/*
 *
 * SiRF GNSS Driver
 *
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/aio.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/moduleparam.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>

#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/cdev.h>
#include <soc/qcom/boot_stats.h>
#include "gnss_sirf.h"

static int resetPin;
static int onOffPin;

static dev_t gnssDev;
static struct cdev c_dev;
static struct class *devClass;

static int configurePins(struct platform_device *pdev);
static int gnss_sirf_driver_open(struct inode *inode, struct file *filp);
static ssize_t gnss_sirf_driver_read(struct file *filp, char *buf,
						size_t count, loff_t *f_pos);
static ssize_t gnss_sirf_driver_write(struct file *filp, const char *buf,
						size_t count, loff_t *f_pos);
static int gnss_sirf_driver_release(struct inode *inode, struct file *filp);
static long gnss_sirf_driver_ioctl(struct file *file, unsigned int cmd,
						unsigned long arg);

static int gnss_sirf_probe(struct platform_device *pdev);
static int gnss_sirf_remove(struct platform_device *pdev);
static int gnss_sirf_suspend(struct platform_device *pdev, pm_message_t state);
static int gnss_sirf_resume(struct platform_device *pdev);

static const struct of_device_id gnss_sirf_match_table[] = {
	{ .compatible = "gnss_sirf" },
	{ }
};

static const struct file_operations gnss_sirf_fops = {
	.open		= gnss_sirf_driver_open,
	.read		= gnss_sirf_driver_read,
	.write		= gnss_sirf_driver_write,
	.release	= gnss_sirf_driver_release,
	.unlocked_ioctl = gnss_sirf_driver_ioctl,
	.owner      = THIS_MODULE
};

static struct platform_driver gnss_sirf_drv = {
	.driver = {
		.name           = "gnss_sirf",
		.of_match_table = gnss_sirf_match_table,
		.owner          = THIS_MODULE,
	},
	.probe   = gnss_sirf_probe,
	.remove  = gnss_sirf_remove,
	.suspend = gnss_sirf_suspend,
	.resume  = gnss_sirf_resume,
};

static int gnss_sirf_driver_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t gnss_sirf_driver_read(struct file *filp,
							 char *buf,
							 size_t count,
							 loff_t *f_pos)
{
	return 0;
}

static ssize_t gnss_sirf_driver_write(struct file *filp,
							 const char *buf,
							 size_t count,
							 loff_t *f_pos)
{
	return count;
}

static int gnss_sirf_driver_release(struct inode *inode,
						struct file *filp)
{
	return 0;
}

static long gnss_sirf_driver_ioctl(struct file *file,
						 unsigned int cmd,
						 unsigned long arg)
{
	switch (cmd) {
	case IO_CONTROL_SIRF_RESET_CLEAR:
		gpio_direction_output(resetPin, 0);
		break;
	case IO_CONTROL_SIRF_RESET_SET:
		gpio_direction_output(resetPin, 1);
		break;
	case IO_CONTROL_SIRF_ON_OFF_CLEAR:
		gpio_direction_output(onOffPin, 0);
		break;
	case IO_CONTROL_SIRF_ON_OFF_SET:
		gpio_direction_output(onOffPin, 1);
		break;
	default:
		break;
	}
	return 0;
}


static int gnss_sirf_init_ports(void)
{
	gpio_direction_output(resetPin, 0);
	gpio_direction_output(onOffPin, 0);
	return 0;
}

static int gnss_sirf_deInit_sirf_ports(void)
{
	gpio_direction_output(resetPin, 0);
	gpio_direction_output(onOffPin, 0);
	return 0;
}


static int gnss_sirf_cteate_device(void)
{
	if (alloc_chrdev_region(&gnssDev, 0, 1, "gnss_sirf") < 0)
		return -ENODEV;
	devClass = class_create(THIS_MODULE, "gnssdevClass");
	if (devClass == NULL) {
		unregister_chrdev_region(gnssDev, 1);
		return -ENODEV;
	}
	if (device_create(devClass, NULL, gnssDev, NULL, "gnss_sirf") == NULL) {
		class_destroy(devClass);
		unregister_chrdev_region(gnssDev, 1);
		return -ENODEV;
	}
	cdev_init(&c_dev, &gnss_sirf_fops);
	if (cdev_add(&c_dev, gnssDev, 1) == -1) {
		device_destroy(devClass, gnssDev);
		class_destroy(devClass);
		unregister_chrdev_region(gnssDev, 1);
		return -ENODEV;
	}
	return 0;
}

static int gnss_sirf_delete_device(void)
{
	/* Remove Char device */
	cdev_del(&c_dev);
	device_destroy(devClass, gnssDev);
	class_destroy(devClass);
	unregister_chrdev_region(gnssDev, 1);
	return 0;
}

static int configurePins(struct platform_device *pdev)
{
	int ret = -ENODEV;
	struct device *dev;

	dev = &pdev->dev;
	dev_info(dev, "%s Reset and InputOutput", __func__);

	if (gpio_is_valid(resetPin)) {
		ret = gpio_request(resetPin, "ssVreset-gpio");
		if (ret < 0) {
			pr_err("failed to request gpio %d: error:%d\n",
					 resetPin, ret);
			return ret;
		}
	}
	if (gpio_is_valid(onOffPin)) {
		ret = gpio_request(onOffPin, "ssVonoff-gpio");
		if (ret < 0) {
			pr_err("failed to request gpio %d: error:%d\n",
					 onOffPin, ret);
			return ret;
		}
	}
	gpio_direction_output(resetPin, 1);
	gpio_direction_output(onOffPin, 1);
	if (gnss_sirf_init_ports() < 0)
		pr_err("gnss_sirf_init_ports failed\n");
	else
		ret = 0;
	return ret;
}

static int gnss_sirf_probe(struct platform_device *pdev)
{
	int ret = -ENODEV;
	struct device *dev;
	char boot_marker[40];

	dev = &pdev->dev;
	dev_info(dev, "%s", __func__);
	if (pdev != NULL) {
		if (pdev->name) {
			resetPin = of_get_named_gpio(pdev->dev.of_node,
					 "ssVreset-gpio", 0);
			onOffPin = of_get_named_gpio(pdev->dev.of_node,
					 "ssVonoff-gpio", 0);
			ret = configurePins(pdev);
			if (ret == 0) {
				snprintf(boot_marker, sizeof(boot_marker),
						"M - DRIVER GNSS Ready");
				place_marker(boot_marker);
			}
		}
	}
	return ret;
}


static int gnss_sirf_remove(struct platform_device *pdev)
{
	struct device *dev;

	dev = &pdev->dev;
	dev_info(&pdev->dev, "%s", __func__);
	gnss_sirf_delete_device();

	if (gnss_sirf_deInit_sirf_ports() < 0) {
		pr_err("gnss_sirf_deInit_sirf_ports failed\n");
		return -ENODEV;
	}
	return 0;
}

static int gnss_sirf_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct device *dev;
	int ret = -ENODEV;

	dev = &pdev->dev;
	dev_info(dev, "%s Initial Freeing of resetPin & onOffPin", __func__);
	gpio_free(resetPin);
	ret = gpio_request_one(resetPin, 1, "ssVreset-gpio");
	if (ret < 0) {
		pr_err("failed to request gpio %d: error:%d\n",
				 resetPin, ret);
		return ret;
	}
	gpio_direction_input(resetPin);
	gpio_free(resetPin);

	gpio_free(onOffPin);
	ret = gpio_request(onOffPin, "ssVonoff-gpio");
	if (ret < 0) {
		pr_err("failed to request gpio %d: error:%d\n",
				 onOffPin, ret);
		return ret;
	}
	gpio_direction_input(onOffPin);
	gpio_free(onOffPin);
	dev_info(dev, "%s Final Free of resetPin & onOffPin", __func__);

	return 0;
}
static int gnss_sirf_resume(struct platform_device *pdev)
{
	struct device *dev;

	dev = &pdev->dev;
	dev_info(dev, "%s", __func__);
	configurePins(pdev);
	return 0;
}

static int __init gnss_sirf_init(void)
{
	int retVal;

	retVal = platform_driver_register(&gnss_sirf_drv);
	if (retVal) {
		pr_err("GNSS platform driver registration Failed !!!!\n");
		return retVal;
	}

	retVal = gnss_sirf_cteate_device();
	return retVal;
}


static void __exit gnss_sirf_exit(void)
{
	platform_driver_unregister(&gnss_sirf_drv);
}


module_init(gnss_sirf_init);
module_exit(gnss_sirf_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("SIRF GNSS receiver control driver");
