/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
**
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; version 2.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License along with
** this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
** Street, Fifth Floor, Boston, MA 02110-1301, USA.
**
** File:
**     tiload.c
**
** Description:
**     utility for TAS2555 Android in-system tuning
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
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>

#include "tiload.h"

/* enable debug prints in the driver */
#define DEBUG

#ifdef DEBUG
#define dprintk(x...)   printk(x)
#else
#define dprintk(x...)
#endif

/* Function prototypes */
#ifdef REG_DUMP
static void dump_page(struct i2c_client *i2c, u8 page);
#endif

/* externs */
static struct cdev *tiload_cdev;
static int tiload_major;	/* Dynamic allocation of Mjr No. */
static int tiload_opened;	/* Dynamic allocation of Mjr No. */
static struct tas2555_priv *g_TAS2555;
struct class *tiload_class;
static unsigned int magic_num = 0x00;

static char gPage;
static char gBook;
/******************************** Debug section *****************************/

#ifdef REG_DUMP
/*
 *----------------------------------------------------------------------------
 * Function : dump_page
 * Purpose  : Read and display one codec register page, for debugging purpose
 *----------------------------------------------------------------------------
 */
static void dump_page(struct i2c_client *i2c, u8 page)
{
}
#endif

/*
 *----------------------------------------------------------------------------
 * Function : tiload_open
 *
 * Purpose  : open method for tiload programming interface
 *----------------------------------------------------------------------------
 */
static int tiload_open(struct inode *in, struct file *filp)
{
	dprintk("TiLoad DRIVER : %s\n", __FUNCTION__);
	if (tiload_opened) {
		printk("%s device is already opened\n", "tiload");
		printk("%s: only one instance of driver is allowed\n", "tiload");
		return -EPERM;
	}
	tiload_opened++;
	return 0;
}

/*
 *----------------------------------------------------------------------------
 * Function : tiload_release
 *
 * Purpose  : close method for tiload programming interface
 *----------------------------------------------------------------------------
 */
static int tiload_release(struct inode *in, struct file *filp)
{
	dprintk("TiLoad DRIVER : %s\n", __FUNCTION__);
	tiload_opened--;
	return 0;
}

#define MAX_LENGTH 128
/*
 *----------------------------------------------------------------------------
 * Function : tiload_read
 *
 * Purpose  : read from codec
 *----------------------------------------------------------------------------
 */
static ssize_t tiload_read(struct file *file, char __user *buf,
	size_t count, loff_t *offset)
{
	static char rd_data[MAX_LENGTH + 1];
	unsigned int nCompositeRegister = 0, Value;
	char reg_addr;
	size_t size;
	int ret = 0;
#ifdef DEBUG
	int i;
#endif

	dprintk("TiLoad DRIVER : %s\n", __FUNCTION__);
	if (count > MAX_LENGTH) {
		printk("Max %d bytes can be read\n", MAX_LENGTH);
		return -EPERM;
	}

	/* copy register address from user space  */
	size = copy_from_user(&reg_addr, buf, 1);
	if (size != 0) {
		printk("read: copy_from_user failure\n");
		return -EPERM;
	}

	size = count;

	nCompositeRegister = BPR_REG(gBook, gPage, reg_addr);
	if (count == 1) {
		ret =
			g_TAS2555->read(g_TAS2555, 0x80000000 | nCompositeRegister, &Value);
		if (ret >= 0)
			rd_data[0] = (char) Value;
	} else if (count > 1) {
		ret =
			g_TAS2555->bulk_read(g_TAS2555, 0x80000000 | nCompositeRegister,
			rd_data, size);
	}
	if (ret < 0)
		printk("%s, %d, ret=%d, count=%zu error happen!\n", __FUNCTION__,
			__LINE__, ret, count);
#ifdef DEBUG
	printk(KERN_ERR "read size = %d, reg_addr= %x , count = %d\n",
		(int) size, reg_addr, (int) count);
	for (i = 0; i < (int) size; i++) {
		printk(KERN_ERR "rd_data[%d]=%x\n", i, rd_data[i]);
	}
#endif
	if (size != count) {
		printk("read %d registers from the codec\n", (int) size);
	}

	if (copy_to_user(buf, rd_data, size) != 0) {
		dprintk("copy_to_user failed\n");
		return -EPERM;
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
static ssize_t tiload_write(struct file *file, const char __user *buf,
	size_t count, loff_t *offset)
{
	static char wr_data[MAX_LENGTH + 1];
	char *pData = wr_data;
	size_t size;
	unsigned int nCompositeRegister = 0;
	unsigned int nRegister;
	int ret = 0;
#ifdef DEBUG
	int i;
#endif

	dprintk("TiLoad DRIVER : %s\n", __FUNCTION__);

	if (count > MAX_LENGTH) {
		printk("Max %d bytes can be read\n", MAX_LENGTH);
		return -EPERM;
	}

	/* copy buffer from user space  */
	size = copy_from_user(wr_data, buf, count);
	if (size != 0) {
		printk("copy_from_user failure %d\n", (int) size);
		return -EPERM;
	}
#ifdef DEBUG
	printk(KERN_ERR "write size = %zu\n", count);
	for (i = 0; i < (int) count; i++) {

		printk(KERN_INFO "\nwr_data[%d]=%x\n", i, wr_data[i]);
	}
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
#if 1
	nCompositeRegister = BPR_REG(gBook, gPage, nRegister);
	if (count == 2) {
		ret =
			g_TAS2555->write(g_TAS2555, 0x80000000 | nCompositeRegister,
			pData[1]);
	} else if (count > 2) {
		ret =
			g_TAS2555->bulk_write(g_TAS2555, 0x80000000 | nCompositeRegister,
			&pData[1], count - 1);
	}
	if (ret < 0)
		printk("%s, %d, ret=%d, count=%zu, ERROR Happen\n", __FUNCTION__,
			__LINE__, ret, count);
#else
	for (n = 1; n < count; n++) {
		nCompositeRegister = BPR_REG(gBook, gPage, nRegister + n - 1);
		g_codec->driver->write(g_codec, 0x80000000 | nCompositeRegister,
			pData[n]);
	}
#endif

	return size;
}

static void tiload_route_IO(unsigned int bLock)
{
	if (bLock) {
		g_TAS2555->write(g_TAS2555, 0xAFFEAFFE, 0xBABEBABE);
	} else {
		g_TAS2555->write(g_TAS2555, 0xBABEBABE, 0xAFFEAFFE);
	}
}

static long tiload_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long num = 0;
	void __user *argp = (void __user *) arg;
	int val;

	BPR bpr;

	printk(KERN_ERR "tiload_ioctl\n\r");

	dprintk("TiLoad DRIVER : %s\n", __FUNCTION__);
	switch (cmd) {
	case TILOAD_IOMAGICNUM_GET:
		num = copy_to_user(argp, &magic_num, sizeof(int));
		break;
	case TILOAD_IOMAGICNUM_SET:
		num = copy_from_user(&magic_num, argp, sizeof(int));
		tiload_route_IO(magic_num);
		break;
	case TILOAD_BPR_READ:
		break;
	case TILOAD_BPR_WRITE:
		num = copy_from_user(&bpr, argp, sizeof(BPR));
		printk("TILOAD_BPR_WRITE: 0x%02X, 0x%02X, 0x%02X\n\r", bpr.nBook,
			bpr.nPage, bpr.nRegister);
		break;
	case TILOAD_IOCTL_SET_MODE:
		num = copy_from_user(&val, argp, sizeof(val));
		mutex_lock(&g_TAS2555->codec_lock);
		g_TAS2555->set_mode(g_TAS2555, val);
		mutex_unlock(&g_TAS2555->codec_lock);
		break;
	case TILOAD_IOCTL_SET_CALIBRATION:
		mutex_lock(&g_TAS2555->codec_lock);
		g_TAS2555->set_calibration(g_TAS2555, 0xFF);
		mutex_unlock(&g_TAS2555->codec_lock);
		break;
	default:
		break;
	}
	return num;
}

/*********** File operations structure for tiload *************/
static struct file_operations tiload_fops = {
	.owner = THIS_MODULE,
	.open = tiload_open,
	.release = tiload_release,
	.read = tiload_read,
	.write = tiload_write,
	.unlocked_ioctl = tiload_ioctl,
};

/*
 *----------------------------------------------------------------------------
 * Function : tiload_driver_init
 *
 * Purpose  : Register a char driver for dynamic tiload programming
 *----------------------------------------------------------------------------
 */
int tiload_driver_init(struct tas2555_priv *pTAS2555)
{
	int result;

	dev_t dev = MKDEV(tiload_major, 0);
	dprintk("TiLoad DRIVER : %s\n", __FUNCTION__);
	g_TAS2555 = pTAS2555;

	dprintk("allocating dynamic major number\n");

	result = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
	if (result < 0) {
		dprintk("cannot allocate major number %d\n", tiload_major);
		return result;
	}
	tiload_class = class_create(THIS_MODULE, DEVICE_NAME);
	tiload_major = MAJOR(dev);
	dprintk("allocated Major Number: %d\n", tiload_major);

	tiload_cdev = cdev_alloc();
	cdev_init(tiload_cdev, &tiload_fops);
	tiload_cdev->owner = THIS_MODULE;
	tiload_cdev->ops = &tiload_fops;

	if (device_create(tiload_class, NULL, dev, NULL, "tiload_node") == NULL)
		dprintk(KERN_ERR "Device creation failed\n");

	if (cdev_add(tiload_cdev, dev, 1) < 0) {
		dprintk("tiload_driver: cdev_add failed \n");
		unregister_chrdev_region(dev, 1);
		tiload_cdev = NULL;
		return 1;
	}
	printk("Registered TiLoad driver, Major number: %d \n", tiload_major);
	return 0;
}

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("Utility for TAS2555 Android in-system tuning");
MODULE_LICENSE("GPLv2");
