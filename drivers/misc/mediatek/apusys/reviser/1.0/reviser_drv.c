/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>  /* Needed by all modules */
#include <linux/kernel.h>  /* Needed for KERN_ALERT */
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>

#include <linux/init.h>
#include <linux/io.h>

#include "reviser_drv.h"
#include "reviser_ioctl.h"
#include "reviser_cmn.h"
#include "reviser_hw.h"
//#include "reviser_inf.h"

/* define */
#define APUSYS_DRV_NAME "apusys_drv_reviser"
#define APUSYS_DEV_NAME "apusys_reviser"

/* global variable */
static dev_t reviser_devt;
static struct cdev *reviser_cdev;
static struct class *reviser_class;

static struct reviser_dev_info g_reviser_info;

#define REVISER_DTS 0

#if REVISER_DTS
static int g_irq_reviser;
#endif

/* function declaration */
static int reviser_open(struct inode *, struct file *);
static int reviser_release(struct inode *, struct file *);
static long reviser_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg);
static long reviser_compat_ioctl(struct file *, unsigned int, unsigned long);

irqreturn_t reviser_interrupt(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}


static const struct file_operations reviser_fops = {
	.open = reviser_open,
	.unlocked_ioctl = reviser_ioctl,
	.release = reviser_release,
	.compat_ioctl = reviser_compat_ioctl,
};

static int reviser_open(struct inode *inode, struct file *file)
{
	DEBUG_TAG;
	return 0;
}

static int reviser_release(struct inode *inode, struct file *file)
{
	DEBUG_TAG;
	return 0;
}

static int reviser_probe(struct platform_device *pdev)
{
	//reviser_device_init();
	int ret = 0;
#if REVISER_DTS
	struct resource *apusys_reviser_irq; /* Interrupt resources */
#endif
	struct resource *apusys_reviser_ctl; /* IO mem resources */
	struct device *dev = &pdev->dev;

	DEBUG_TAG;


	memset(&g_reviser_info, 0, sizeof(struct reviser_dev_info));
	/* get major */
	ret = alloc_chrdev_region(&reviser_devt, 0, 1, APUSYS_DRV_NAME);
	if (ret < 0) {
		LOG_ERR("alloc_chrdev_region failed, %d\n", ret);
		goto out;
	}

	/* Allocate driver */
	reviser_cdev = cdev_alloc();
	if (reviser_cdev == NULL) {
		LOG_ERR("cdev_alloc failed\n");
		ret = -ENOMEM;
		goto free_chrdev_region;
	}

	/* Attatch file operation. */
	cdev_init(reviser_cdev, &reviser_fops);
	reviser_cdev->owner = THIS_MODULE;
	DEBUG_TAG;

	/* Add to system */
	ret = cdev_add(reviser_cdev, reviser_devt, 1);
	if (ret < 0) {
		LOG_ERR("Attatch file operation failed, %d\n", ret);
		goto free_chrdev_region;
	}

	/* Create class register */
	reviser_class = class_create(THIS_MODULE, APUSYS_DRV_NAME);
	if (IS_ERR(reviser_class)) {
		ret = PTR_ERR(reviser_class);
		LOG_ERR("Unable to create class, err = %d\n", ret);
		goto free_cdev_add;
	}

	dev = device_create(reviser_class, NULL, reviser_devt,
				NULL, APUSYS_DEV_NAME);
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		LOG_ERR("Failed to create device: /dev/%s, err = %d",
			APUSYS_DEV_NAME, ret);
		goto free_class;
	}
	DEBUG_TAG;

	apusys_reviser_ctl = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!apusys_reviser_ctl) {
		LOG_ERR("invalid address\n");
		ret = -ENODEV;
		goto free_device;
	}
	LOG_DEBUG("apusys_reviser_ctl->start = %p\n",
			apusys_reviser_ctl->start);
	g_reviser_info.pctrl_top = ioremap_nocache(apusys_reviser_ctl->start,
		apusys_reviser_ctl->end - apusys_reviser_ctl->start + 1);
	if (!g_reviser_info.pctrl_top) {
		LOG_ERR("Could not allocate iomem\n");
		ret = -EIO;
		goto free_device;
	}

	DEBUG_TAG;
#if REVISER_DTS
	/* Get IRQ for the device */
	apusys_reviser_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!apusys_reviser_irq) {
		LOG_ERR("invalid IRQ\n");
		ret = -ENODEV;
		goto free_map;
	}

	g_irq_reviser = apusys_reviser_irq->start;
	ret = request_irq(g_irq_reviser, reviser_interrupt, IRQF_TRIGGER_LOW,
			APUSYS_DRV_NAME, dev);
	if (ret) {
		LOG_ERR("invalid request IRQ\n");
		goto free_map;
	}

#endif
	DEBUG_TAG;
	return ret;
#if REVISER_DTS
free_map:
	iounmap(reviser_ctrl_top);
#endif
free_device:
	/* Release device */
	device_destroy(reviser_class, reviser_devt);

free_class:
	/* Release class */
	class_destroy(reviser_class);
free_cdev_add:
	/* Release char driver */
	if (reviser_cdev != NULL) {
		cdev_del(reviser_cdev);
		reviser_cdev = NULL;
	}
free_chrdev_region:
	unregister_chrdev_region(reviser_devt, 1);

out:
	return ret;
}

static int reviser_remove(struct platform_device *pdev)
{

	DEBUG_TAG;
#if REVISER_DTS
	struct device *dev;

	dev = &pdev->dev;

	DEBUG_TAG;
	//reviser_device_destroy();
	free_irq(g_irq_reviser, dev);
#endif
	iounmap(g_reviser_info.pctrl_top);



	/* Release device */
	device_destroy(reviser_class, reviser_devt);

	/* Release class */
	if (reviser_class != NULL)
		class_destroy(reviser_class);

	/* Release char driver */
	if (reviser_cdev != NULL) {
		cdev_del(reviser_cdev);
		reviser_cdev = NULL;
	}
	unregister_chrdev_region(reviser_devt, 1);
	return 0;
}

static int reviser_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int reviser_resume(struct platform_device *pdev)
{
	return 0;
}

static long reviser_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	int ret = 0;
	struct reviser_ioctl_info info;

	switch (cmd) {
	case REVISER_IOCTL_PRINT:
		DEBUG_TAG;
		reviser_print_private((void *)&g_reviser_info);
		reviser_print_boundary((void *)&g_reviser_info);
		reviser_print_context_ID((void *)&g_reviser_info);
		reviser_print_remap_table((void *)&g_reviser_info);
		break;
	case REVISER_IOCTL_SET_BOUNDARY:
		DEBUG_TAG;
		if (copy_from_user(&info,
				(void *)arg,
				sizeof(struct reviser_ioctl_info))) {
			LOG_ERR("copy info struct fail\n");
			ret = -EINVAL;
		}

		reviser_set_context_boundary(
				(void *)&g_reviser_info,
				info.bound.type,
				info.bound.index,
				info.bound.boundary);
		if (ret == 0) {
			if (copy_to_user((void *)arg,
				&info,
				sizeof(struct reviser_ioctl_info))) {
				LOG_ERR("copy info to user fail\n");
				ret = -EINVAL;
			}
		}
		break;
	case REVISER_IOCTL_SET_CONTEXT_ID:
		DEBUG_TAG;
		if (copy_from_user(&info, (void *)arg,
				sizeof(struct reviser_ioctl_info))) {
			LOG_ERR("copy info struct fail\n");
			ret = -EINVAL;
		}

		reviser_set_context_ID(
			(void *) &g_reviser_info,
			(enum REVISER_DEVICE_E) info.contex.type,
			info.contex.index,
			info.contex.ID);
		if (ret == 0) {
			if (copy_to_user((void *)arg, &info,
				sizeof(struct reviser_ioctl_info))) {
				LOG_ERR("copy info to user fail\n");
				ret = -EINVAL;
			}
		}
		break;
	case REVISER_IOCTL_SET_REMAP_TABLE:
		DEBUG_TAG;
		if (copy_from_user(&info, (void *)arg,
			sizeof(struct reviser_ioctl_info))) {
			LOG_ERR("copy info struct fail\n");
			ret = -EINVAL;
		}

		reviser_set_remap_talbe(
			(void *)&g_reviser_info, info.table.index,
			info.table.valid, info.table.ID,
			info.table.src_page, info.table.dst_page);
		if (ret == 0) {
			if (copy_to_user(
			(void *)arg, &info,
			sizeof(struct reviser_ioctl_info))) {
				LOG_ERR("copy info to user fail\n");
				ret = -EINVAL;
			}
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}
static long reviser_compat_ioctl(struct file *flip, unsigned int cmd,
	unsigned long arg)
{
	DEBUG_TAG;
	switch (cmd) {
	case REVISER_IOCTL_PRINT:
	case REVISER_IOCTL_SET_BOUNDARY:
	{
		return flip->f_op->unlocked_ioctl(flip, cmd,
					(unsigned long)compat_ptr(arg));
	}
	default:
		return -ENOIOCTLCMD;
		/*return vpu_ioctl(flip, cmd, arg);*/
	}
	return 0;
}

static const struct of_device_id reviser_of_match[] = {
	{.compatible = "mediatek,apusys_reviser",},
	{/* end of list */},
};

static struct platform_driver reviser_driver = {
	.probe = reviser_probe,
	.remove = reviser_remove,
	.suspend = reviser_suspend,
	.resume  = reviser_resume,
	//.pm = apusys_pm_qos,
	.driver = {
		.name = APUSYS_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = reviser_of_match,
	},
};

static int __init reviser_init(void)
{
	int ret = 0;
	//struct device *dev = NULL;


	DEBUG_TAG;

	if (platform_driver_register(&reviser_driver)) {
		LOG_ERR("failed to register APUSYS driver");
		return -ENODEV;
	}


	return ret;
}

static void __exit reviser_destroy(void)
{
	platform_driver_unregister(&reviser_driver);
}

module_init(reviser_init);
module_exit(reviser_destroy);
MODULE_DESCRIPTION("MTK APUSYS REVISER Driver");
MODULE_AUTHOR("SS5");
MODULE_LICENSE("GPL");
