/*
 * Implementation of the GPS EMI driver.
 *
 * Copyright (C) 2014 Mediatek
 * Authors:
 * Heiping <Heiping.Lei@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*******************************************************************************
* Dependency
*******************************************************************************/
#ifdef CONFIG_MTK_CONNSYS_DEDICATED_LOG_PATH
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/printk.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
#include <asm/mmu.h>
#else
#include <asm/memblock.h>
#endif
#include <linux/wait.h>
#include "gps.h"
#include "connsys_debug_utility.h"
#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt

/******************************************************************************
 * Definition
******************************************************************************/
/* device name and major number */
#define GPSFWLOG_DEVNAME            "fw_log_gps"
#define GPS_FW_LOG_IOC_MAGIC          (0xfc)
#define GPS_FW_LOG_IOCTL_ON_OFF      _IOW(GPS_FW_LOG_IOC_MAGIC, 0, int)
#define GPS_FW_LOG_IOCTL_SET_LEVEL   _IOW(GPS_FW_LOG_IOC_MAGIC, 1, int)
#define GPS_FW_LOG_IOCTL_GET_LEVEL   _IOW(GPS_FW_LOG_IOC_MAGIC, 2, int)

/*******************************************************************************
* structure & enumeration
*******************************************************************************/
/*---------------------------------------------------------------------------*/
struct gps_fw_log_dev {
	struct class *cls;
	struct device *dev;
	dev_t devno;
	struct cdev chdev;
};
/* typedef unsigned char   UINT8, *PUINT8, **PPUINT8; */

/******************************************************************************
 * local variables
******************************************************************************/
/* static int flag; */
struct gps_fw_log_dev *logdevobj;
static wait_queue_head_t GPS_log_wq;
bool fgGps_fw_log_ON;

/*---------------------------------------------------------------------------*/
static void log_event_cb(void);

long fw_log_gps_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long retval = 0;

	switch (cmd) {
	case GPS_FW_LOG_IOCTL_ON_OFF:
		pr_info("gps PS_FW_LOG_IOCTL_ON_OFF(%lu)\n", arg);
		GPS_fwlog_ctrl((bool)arg);
		break;

	case GPS_FW_LOG_IOCTL_SET_LEVEL:
		pr_info("gps GPS_FW_LOG_IOCTL_SET_LEVEL\n");
		break;
	case GPS_FW_LOG_IOCTL_GET_LEVEL:
		pr_info("gps GPS_FW_LOG_IOCTL_GET_LEVEL\n");
		break;

	default:
		pr_warn("gps unknown cmd (%d)\n", cmd);
		break;
	}
	return retval;
}

long fw_log_gps_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return fw_log_gps_unlocked_ioctl(filp, cmd, arg);
}

/******************************************************************************/
/*****************************************************************************/
static int fw_log_open(struct inode *inode, struct file *file)
{
	pr_info("%s: gps major %d minor %d (pid %d)\n", __func__, imajor(inode), iminor(inode), current->pid);
	return 0;
}

/*****************************************************************************/


/*****************************************************************************/
static int fw_log_close(struct inode *inode, struct file *file)
{
	pr_info("%s: gps major %d minor %d (pid %d)\n", __func__, imajor(inode), iminor(inode), current->pid);
	return 0;
}

/******************************************************************************/
static ssize_t fw_log_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	int retval;

	#if 0
	pr_info("GPS fw_log_read,len=%d\n", count);
	#endif

	retval = connsys_log_read_to_user(CONNLOG_TYPE_GPS, buf, count);
	return retval;
}
/******************************************************************************/
static unsigned int fw_log_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(file, &GPS_log_wq, wait);
	if (connsys_log_get_buf_size(CONNLOG_TYPE_GPS) > 0)
		mask = (POLLIN | POLLRDNORM);

	return mask;
}

/*****************************************************************************/
/* Kernel interface */
static const struct file_operations gps_fw_log_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = fw_log_gps_unlocked_ioctl,
	.compat_ioctl = fw_log_gps_compat_ioctl,
	.open = fw_log_open,
	.read = fw_log_read,
	.release = fw_log_close,
	.poll = fw_log_poll,
};

void log_event_cb(void)
{
	wake_up_interruptible(&GPS_log_wq);
}

static int __init gps_fw_log_init(void)
{
	int ret = 0;
	int err = 0;

	logdevobj = kzalloc(sizeof(*logdevobj), GFP_KERNEL);
	if (logdevobj == NULL) {
		err = -ENOMEM;
		ret = -ENOMEM;
		goto err_out;
	}

	pr_info("Registering chardev\n");
	ret = alloc_chrdev_region(&logdevobj->devno, 0, 1, GPSFWLOG_DEVNAME);
	if (ret) {
		pr_err("alloc_chrdev_region fail: %d\n", ret);
		err = -ENOMEM;
		goto err_out;
	} else {
		pr_info("major: %d, minor: %d\n", MAJOR(logdevobj->devno), MINOR(logdevobj->devno));
	}
	cdev_init(&logdevobj->chdev, &gps_fw_log_fops);
	logdevobj->chdev.owner = THIS_MODULE;
	err = cdev_add(&logdevobj->chdev, logdevobj->devno, 1);
	if (err) {
		pr_err("cdev_add fail: %d\n", err);
		goto err_out;
	}
	logdevobj->cls = class_create(THIS_MODULE, "gpsfwlog");
	if (IS_ERR(logdevobj->cls)) {
		pr_err("Unable to create class, err = %d\n", (int)PTR_ERR(logdevobj->cls));
	goto err_out;
	}
	logdevobj->dev = device_create(logdevobj->cls, NULL, logdevobj->devno, logdevobj, "fw_log_gps");

	connsys_log_init(CONNLOG_TYPE_GPS);
	init_waitqueue_head(&GPS_log_wq);
	connsys_log_register_event_cb(CONNLOG_TYPE_GPS, log_event_cb);

	pr_info("GPS FW LOG device init Done\n");
	return 0;

err_out:
	if (logdevobj != NULL) {
		if (err == 0)
			cdev_del(&logdevobj->chdev);
		if (ret == 0)
			unregister_chrdev_region(logdevobj->devno, 1);
		kfree(logdevobj);
		logdevobj = NULL;
	}
	return -1;
}

/*****************************************************************************/
static void __exit gps_fw_log_exit(void)
{
	if (!logdevobj) {
		pr_err("null pointer: %p\n", logdevobj);
		return;
	}

	pr_info("Unregistering chardev\n");
	connsys_log_deinit(CONNLOG_TYPE_GPS);
	cdev_del(&logdevobj->chdev);
	unregister_chrdev_region(logdevobj->devno, 1);
	device_destroy(logdevobj->cls, logdevobj->devno);
	class_destroy(logdevobj->cls);
	kfree(logdevobj);
	logdevobj = NULL;
	pr_info("Done\n");
}

int mtk_gps_fw_log_init(void)
{
	pr_info("gps fw log init begin");
	return gps_fw_log_init();
}

void mtk_gps_fw_log_exit(void)
{
	pr_info("gps fw log exit begin");
	return gps_fw_log_exit();
}

/*****************************************************************************/
#if 0
module_init(gps_emi_mod_init);
module_exit(gps_emi_mod_exit);
#endif
/*****************************************************************************/
MODULE_AUTHOR("Chaoran Zhang <Chaoran.Zhang@mediatek.com>");
MODULE_DESCRIPTION("GPS FW log driver");
MODULE_LICENSE("GPL");
#endif
