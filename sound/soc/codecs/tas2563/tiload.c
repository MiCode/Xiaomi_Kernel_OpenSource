/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
** Copyright (C) 2019 XiaoMi, Inc.
**
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; version 2.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
**
** File:
**     tiload.c
**
** Description:
**     utility for TAS2563 Android in-system tuning
**
** =============================================================================
*/


#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>

#include "tiload.h"

/* enable debug prints in the driver */
#define DEBUG

static struct cdev *tiload_cdev;
static int tiload_major; /* Dynamic allocation of Mjr No. */
static int tiload_opened; /* Dynamic allocation of Mjr No. */
static struct tas2563_priv *g_TAS2563;
struct class *tiload_class;
static unsigned int magic_num;

static char gPage;
static char gBook;
/******************************** Debug section *****************************/


/*----------------------------------------------------------------------------
 * Function : tiload_open
 *
 * Purpose  : open method for tiload programming interface
 *----------------------------------------------------------------------------
 */
static int tiload_open(struct inode *in, struct file *filp)
{
	struct tas2563_priv *pTAS2563 = g_TAS2563;

	dev_info(pTAS2563->dev, "%s\n", __func__);

	if (tiload_opened) {
		dev_info(pTAS2563->dev, "%s device is already opened\n", "tiload");
		return -EINVAL;
	}
	filp->private_data = (void *)pTAS2563;
	tiload_opened++;
	return 0;
}

/*----------------------------------------------------------------------------
 * Function : tiload_release
 *
 * Purpose  : close method for tiload programming interface
 *----------------------------------------------------------------------------
 */
static int tiload_release(struct inode *in, struct file *filp)
{
	struct tas2563_priv *pTAS2563 = (struct tas2563_priv *)filp->private_data;

	dev_info(pTAS2563->dev, "%s\n", __func__);
	filp->private_data = NULL;
	tiload_opened--;
	return 0;
}

#define MAX_LENGTH 128
/*----------------------------------------------------------------------------
 * Function : tiload_read
 *
 * Purpose  : read from codec
 *----------------------------------------------------------------------------
 */
static ssize_t tiload_read(struct file *filp, char __user *buf,
	size_t count, loff_t *offset)
{
	struct tas2563_priv *pTAS2563 = (struct tas2563_priv *)filp->private_data;
	static char rd_data[MAX_LENGTH + 1];
	unsigned int nCompositeRegister = 0, Value = 0;
	char reg_addr;
	size_t size;
	int ret = 0;
#ifdef DEBUG
	/* int i; */
#endif

	dev_info(pTAS2563->dev, "%s\n", __func__);
	if (count > MAX_LENGTH) {
		dev_err(pTAS2563->dev, "Max %d bytes can be read\n", MAX_LENGTH);
		return -EINVAL;
	}

	/* copy register address from user space  */
	size = copy_from_user(&reg_addr, buf, 1);
	if (size != 0) {
		dev_err(pTAS2563->dev, "read: copy_from_user failure\n");
		return -EINVAL;
	}

	size = count;

	nCompositeRegister = BPR_REG(gBook, gPage, reg_addr);
	dev_dbg(pTAS2563->dev, " nCompositeRegister:0x%x  gBook:0x%x gpage:0x%x \n", nCompositeRegister, gBook, gPage);
	if (count == 1) {
		ret =
			pTAS2563->read(pTAS2563, 0x80000000 | nCompositeRegister, &Value);
		if (ret >= 0)
			rd_data[0] = (char) Value;
	} else if (count > 1) {
		ret =
			pTAS2563->bulk_read(pTAS2563, 0x80000000 | nCompositeRegister,
			rd_data, size);
	}
	if (ret < 0)
		dev_err(pTAS2563->dev, "%s, %d, ret=%d, count=%zu error happen!\n",
			__func__, __LINE__, ret, count);

#ifdef DEBUG
	dev_info(pTAS2563->dev, "read size = %d, reg_addr= %x , count = %d\n",
		(int) size, reg_addr, (int) count);
/*	for (i = 0; i < (int) size; i++) {
		dev_dbg(pTAS2563->dev, "rd_data[%d]=%x\n", i, rd_data[i]);
	}
*/
#endif
	if (size != count)
		dev_err(pTAS2563->dev, "read %d registers from the codec\n", (int) size);

	if (copy_to_user(buf, rd_data, size) != 0) {
		dev_err(pTAS2563->dev, "copy_to_user failed\n");
		return -EINVAL;
	}

	return size;
}

/*
 *----------------------------------------------------------------------------
 * Function : tiload_write
 *
 * Purpose  : write to codec
 *----------------------------------------------------------------------------
 */
static ssize_t tiload_write(struct file *filp, const char __user *buf,
	size_t count, loff_t *offset)
{
	struct tas2563_priv *pTAS2563 = (struct tas2563_priv *)filp->private_data;
	static char wr_data[MAX_LENGTH + 1];
	char *pData = wr_data;
	size_t size;
	unsigned int nCompositeRegister = 0;
	unsigned int nRegister;
	int ret = 0;
#ifdef DEBUG
	/* int i; */
#endif
	dev_info(pTAS2563->dev, "%s\n", __func__);

	if (count > MAX_LENGTH) {
		dev_err(pTAS2563->dev, "Max %d bytes can be read\n", MAX_LENGTH);
		return -EINVAL;
	}

	/* copy buffer from user space  */
	size = copy_from_user(wr_data, buf, count);
	if (size != 0) {
		dev_err(pTAS2563->dev, "copy_from_user failure %d\n", (int) size);
		return -EINVAL;
	}
#ifdef DEBUG
	dev_info(pTAS2563->dev, "write size = %zu\n", count);
/* for (i = 0; i < (int) count; i++) {
*		dev_info(pTAS2563->dev, "wr_data[%d]=%x\n", i, wr_data[i]);
*	}
*/
#endif
	nRegister = wr_data[0];
	size = count;
	if ((nRegister == 127) && (gPage == 0)) {
		gBook = wr_data[1];
		return size;
	}

	if (nRegister == 0) {
		gPage = wr_data[1];
		pData++;
		count--;
	}

	nCompositeRegister = BPR_REG(gBook, gPage, nRegister);
	if (count == 2) {
		ret =
			pTAS2563->write(pTAS2563, 0x80000000 | nCompositeRegister,
			pData[1]);
	} else if (count > 2) {
		ret =
			pTAS2563->bulk_write(pTAS2563, 0x80000000 | nCompositeRegister,
			&pData[1], count - 1);
	}

	if (ret < 0)
		dev_err(pTAS2563->dev, "%s, %d, ret=%d, count=%zu, ERROR Happen\n", __func__,
			__LINE__, ret, count);

	return size;
}

static void tiload_route_IO(struct tas2563_priv *pTAS2563, unsigned int bLock)
{
	if (bLock)
		pTAS2563->write(pTAS2563, 0xAFFEAFFE, 0xBABEBABE);
	else
		pTAS2563->write(pTAS2563, 0xBABEBABE, 0xAFFEAFFE);
}

static long tiload_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct tas2563_priv *pTAS2563 = (struct tas2563_priv *)filp->private_data;
	long num = 0;
	void __user *argp = (void __user *) arg;
	int val;
	struct BPR bpr;

	dev_info(pTAS2563->dev, "%s, cmd=0x%x\n", __func__, cmd);
/*  if (_IOC_TYPE(cmd) != TILOAD_IOC_MAGIC)
 *      return -ENOTTY;
 */

	switch (cmd) {
	case TILOAD_IOMAGICNUM_GET:
		num = copy_to_user(argp, &magic_num, sizeof(int));
		break;
	case TILOAD_IOMAGICNUM_SET:
		num = copy_from_user(&magic_num, argp, sizeof(int));
		dev_info(pTAS2563->dev, "TILOAD_IOMAGICNUM_SET\n");
		tiload_route_IO(pTAS2563, magic_num);
		break;
	case TILOAD_BPR_READ:
		break;
	case TILOAD_BPR_WRITE:
		num = copy_from_user(&bpr, argp, sizeof(struct BPR));
		dev_info(pTAS2563->dev, "TILOAD_BPR_WRITE: 0x%02X, 0x%02X, 0x%02X\n\r", bpr.nBook,
			bpr.nPage, bpr.nRegister);
		break;
	case TILOAD_IOCTL_SET_CHL:
		break;

	case TILOAD_IOCTL_SET_CONFIG:
		num = copy_from_user(&val, argp, sizeof(val));
		pTAS2563->set_config(pTAS2563, val);
		break;
	case TILOAD_IOCTL_SET_CALIBRATION:
		num = copy_from_user(&val, argp, sizeof(val));
		pTAS2563->set_calibration(pTAS2563, val);
		break;

	default:
		break;
	}
	return num;
}

#ifdef CONFIG_COMPAT
static long tiload_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct tas2563_priv *pTAS2563 = (struct tas2563_priv *)filp->private_data;
	long nResult = 0;

	switch (cmd) {
	case TILOAD_COMPAT_IOMAGICNUM_GET:
		dev_info(pTAS2563->dev, "%s, TILOAD_COMPAT_IOMAGICNUM_GET=0x%x\n",
			__func__, cmd);
		nResult = tiload_ioctl(filp, TILOAD_IOMAGICNUM_GET,
			(unsigned long) compat_ptr(arg));
		break;

	case TILOAD_COMPAT_IOMAGICNUM_SET:
		dev_info(pTAS2563->dev, "%s, TILOAD_COMPAT_IOMAGICNUM_SET=0x%x\n",
			__func__, cmd);
		nResult = tiload_ioctl(filp, TILOAD_IOMAGICNUM_SET,
			(unsigned long) compat_ptr(arg));
		break;

	case TILOAD_COMPAT_BPR_READ:
		dev_info(pTAS2563->dev, "%s, TILOAD_COMPAT_BPR_READ=0x%x\n",
			__func__, cmd);
		nResult = tiload_ioctl(filp, TILOAD_BPR_READ,
			(unsigned long) compat_ptr(arg));
		break;

	case TILOAD_COMPAT_BPR_WRITE:
		dev_info(pTAS2563->dev, "%s, TILOAD_COMPAT_BPR_WRITE=0x%x\n",
			__func__, cmd);
		nResult = tiload_ioctl(filp, TILOAD_BPR_WRITE,
			(unsigned long) compat_ptr(arg));
		break;

	case TILOAD_COMPAT_IOCTL_SET_CHL:
		dev_info(pTAS2563->dev, "%s, TILOAD_COMPAT_IOCTL_SET_CHL=0x%x\n",
			__func__, cmd);
		nResult = tiload_ioctl(filp, TILOAD_IOCTL_SET_CHL,
			(unsigned long) compat_ptr(arg));
		break;

	case TILOAD_COMPAT_IOCTL_SET_CONFIG:
		dev_info(pTAS2563->dev, "%s, TILOAD_COMPAT_IOCTL_SET_CONFIG=0x%x\n",
			__func__, cmd);
		nResult = tiload_ioctl(filp, TILOAD_IOCTL_SET_CONFIG,
			(unsigned long) compat_ptr(arg));
		break;

	case TILOAD_COMPAT_IOCTL_SET_CALIBRATION:
		dev_info(pTAS2563->dev, "%s, TILOAD_COMPAT_IOCTL_SET_CALIBRATION=0x%x\n",
			__func__, cmd);
		nResult = tiload_ioctl(filp, TILOAD_IOCTL_SET_CALIBRATION,
			(unsigned long) compat_ptr(arg));
		break;

	default:
		dev_err(pTAS2563->dev, "%s, unsupport compat ioctl=0x%x\n",
			__func__, cmd);
		break;
	}

	return nResult;
}
#endif

/*********** File operations structure for tiload *************/
static const struct file_operations tiload_fops = {
	.owner = THIS_MODULE,
	.open = tiload_open,
	.release = tiload_release,
	.read = tiload_read,
	.write = tiload_write,
	.unlocked_ioctl = tiload_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tiload_compat_ioctl,
#endif
};

/*----------------------------------------------------------------------------
 * Function : tiload_driver_init
 *
 * Purpose  : Register a char driver for dynamic tiload programming
 *----------------------------------------------------------------------------
 */
int tiload_driver_init(struct tas2563_priv *pTAS2563)
{
	int result;
	dev_t dev = MKDEV(tiload_major, 0);

	g_TAS2563 = pTAS2563;

	dev_info(pTAS2563->dev, "%s\n", __func__);

	result = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
	if (result < 0) {
		dev_err(pTAS2563->dev, "cannot allocate major number %d\n", tiload_major);
		return result;
	}
	tiload_class = class_create(THIS_MODULE, DEVICE_NAME);
	tiload_major = MAJOR(dev);
	dev_info(pTAS2563->dev, "allocated Major Number: %d\n", tiload_major);

	tiload_cdev = cdev_alloc();
	cdev_init(tiload_cdev, &tiload_fops);
	tiload_cdev->owner = THIS_MODULE;
	tiload_cdev->ops = &tiload_fops;

	if (device_create(tiload_class, NULL, dev, NULL, "tiload_node") == NULL)
		dev_err(pTAS2563->dev, "Device creation failed\n");

	if (cdev_add(tiload_cdev, dev, 1) < 0) {
		dev_err(pTAS2563->dev, "tiload_driver: cdev_add failed\n");
		unregister_chrdev_region(dev, 1);
		tiload_cdev = NULL;
		return 1;
	}
	dev_info(pTAS2563->dev, "Registered TiLoad driver, Major number: %d\n", tiload_major);
	/* class_device_create(tiload_class, NULL, dev, NULL, DEVICE_NAME, 0); */
	return 0;
}

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("Utility for TAS2563 Android in-system tuning");
MODULE_LICENSE("GPL v2");
