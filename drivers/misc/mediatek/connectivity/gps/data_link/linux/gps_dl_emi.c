/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include "gps_dl_config.h"

#if GPS_DL_HAS_PLAT_DRV
/*******************************************************************************
* Dependency
*******************************************************************************/
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

#include "gps_dl_log.h"
#include "gps_dl_linux_reserved_mem.h"
#include "gps_dl_emi.h"

/******************************************************************************
 * Definition
******************************************************************************/
/* device name and major number */
#define GPS_DL_EMI_DEVNAME            "gps_emi"
#define IOCTL_EMI_MEMORY_INIT        1
#define IOCTL_MNL_NVRAM_FILE_TO_MEM  2
#define IOCTL_MNL_NVRAM_MEM_TO_FILE  3
#define IOCTL_ADC_CAPTURE_ADDR_GET   4


/*******************************************************************************
* structure & enumeration
*******************************************************************************/
/*---------------------------------------------------------------------------*/
struct gps_icap_dev {
	struct class *cls;
	struct device *dev;
	dev_t devno;
	struct cdev chdev;
};

struct gps_icap_dev *gps_icap_dev_ptr;

char gps_icap_local_buf[GPS_ICAP_BUF_SIZE];


/*---------------------------------------------------------------------------*/
long gps_icap_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
#if 0
	unsigned int *tmp;
#endif

	GDL_LOGI("cmd (%d),arg(%ld)\n", cmd, arg);

	switch (cmd) {
	case IOCTL_EMI_MEMORY_INIT:
		GDL_LOGW("IOCTL_EMI_MEMORY_INIT");
		break;

	case IOCTL_MNL_NVRAM_FILE_TO_MEM:
		GDL_LOGW("IOCTL_MNL_NVRAM_FILE_TO_MEM");
		break;

	case IOCTL_MNL_NVRAM_MEM_TO_FILE:
		GDL_LOGW("IOCTL_MNL_NVRAM_MEM_TO_FILE");
		break;

	case IOCTL_ADC_CAPTURE_ADDR_GET:
#if 0
		tmp = (unsigned int *)&gGpsIcapPhyBase;
		GPS_DBG("gps_emi:gGpsIcapPhyBase (%x)\n", &gGpsIcapPhyBase);
		GPS_DBG("gps_emi:tmp  (%x)\n", tmp);
		if (copy_to_user((unsigned int __user *)arg, tmp, sizeof(unsigned int)))
			retval = -1;
#endif
		GDL_LOGW("IOCTL_ADC_CAPTURE_ADDR_GET,(%d)", retval);
		break;

	default:
		GDL_LOGW("unknown cmd (%d)", cmd);
		retval = 0;
		break;
	}
	return retval;

}

/******************************************************************************/
long gps_icap_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return gps_icap_unlocked_ioctl(filp, cmd, arg);
}

/*****************************************************************************/
static int gps_icap_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

/*****************************************************************************/


/*****************************************************************************/
static int gps_icap_release(struct inode *inode, struct file *file)
{
	return 0;
}

/******************************************************************************/
static ssize_t gps_icap_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	void *p_src;

	GDL_LOGI("begin");

	if (count > GPS_ICAP_BUF_SIZE)
		count = GPS_ICAP_BUF_SIZE;

	p_src = gps_dl_reserved_mem_icap_buf_get_vir_addr();

	if (p_src == NULL) {
		GDL_LOGW("src is null, return");
		return 0;
	}

	memcpy_fromio(&gps_icap_local_buf[0], p_src, GPS_ICAP_BUF_SIZE);

	if (copy_to_user(buf, (char *)&gps_icap_local_buf[0], count)) {
		GDL_LOGW("copy to user fail, return");
		return 0;
	}

	GDL_LOGI("finish, count = %ld", count);
	return count;
}

/******************************************************************************/
static ssize_t gps_icap_write(struct file *file, const char __user *buf, size_t count,
		loff_t *ppos)
{
	ssize_t ret = 0;

	GDL_LOGW("count = %ld", count);

	return ret;
}

/*****************************************************************************/
/* Kernel interface */
static const struct file_operations gps_icap_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = gps_icap_unlocked_ioctl,
	.compat_ioctl = gps_icap_compat_ioctl,
	.open = gps_icap_open,
	.read = gps_icap_read,
	.write = gps_icap_write,
	.release = gps_icap_release,
};

/*****************************************************************************/
void gps_icap_probe(void)
{
	int ret = 0, err = 0;

	GDL_LOGI("start");

	gps_icap_dev_ptr = kzalloc(sizeof(*gps_icap_dev_ptr), GFP_KERNEL);
	if (gps_icap_dev_ptr == NULL) {
		err = -ENOMEM;
		ret = -ENOMEM;
		goto err_out;
	}

	GDL_LOGD("registering chardev");
	ret = alloc_chrdev_region(&gps_icap_dev_ptr->devno, 0, 1, GPS_DL_EMI_DEVNAME);
	if (ret) {
		GDL_LOGE("alloc_chrdev_region fail: %d", ret);
		err = -ENOMEM;
		goto err_out;
	} else
		GDL_LOGI("major: %d, minor: %d",
			MAJOR(gps_icap_dev_ptr->devno), MINOR(gps_icap_dev_ptr->devno));

	cdev_init(&gps_icap_dev_ptr->chdev, &gps_icap_fops);
	gps_icap_dev_ptr->chdev.owner = THIS_MODULE;
	err = cdev_add(&gps_icap_dev_ptr->chdev, gps_icap_dev_ptr->devno, 1);
	if (err) {
		GDL_LOGE("cdev_add fail: %d", err);
		goto err_out;
	}

	gps_icap_dev_ptr->cls = class_create(THIS_MODULE, "gpsemi");
	if (IS_ERR(gps_icap_dev_ptr->cls)) {
		GDL_LOGE("unable to create class, err = %d\n", (int)PTR_ERR(gps_icap_dev_ptr->cls));
		goto err_out;
	}
	gps_icap_dev_ptr->dev = device_create(gps_icap_dev_ptr->cls,
		NULL, gps_icap_dev_ptr->devno, gps_icap_dev_ptr, "gps_emi");

	GDL_LOGI("done");
	return;

err_out:
	if (gps_icap_dev_ptr != NULL) {
		if (err == 0)
			cdev_del(&gps_icap_dev_ptr->chdev);
		if (ret == 0)
			unregister_chrdev_region(gps_icap_dev_ptr->devno, 1);

		kfree(gps_icap_dev_ptr);
		gps_icap_dev_ptr = NULL;
	}
}

#endif /* GPS_DL_HAS_PLAT_DRV */

