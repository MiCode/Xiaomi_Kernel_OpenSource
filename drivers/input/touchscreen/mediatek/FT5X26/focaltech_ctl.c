/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2010-2015, Focaltech Ltd. All rights reserved.
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

/************************************************************************
 *
 * File Name: focaltech_ctl.c
 *
 * Author:	  Xu YongFeng
 *
 * Created: 2015-01-01
 *
 * Modify by mshl on 2015-03-20
 *
 * Abstract: Function for old APK tool
 *
 ************************************************************************/

/*****************************************************************
 * Included header files
 *****************************************************************/
#include "focaltech_core.h"

/*****************************************************************
 * Private constant and macro definitions using #define
 *****************************************************************/
/* 预设的ft_rw_iic_drv的主设备号*/
#define FTS_RW_IIC_DRV "ft_rw_iic_drv"
#define FTS_RW_IIC_DRV_MAJOR 210
#define FTS_I2C_RDWR_MAX_QUEUE 36
#define FTS_I2C_SLAVEADDR 11
#define FTS_I2C_RW 12
/*****************************************************************
 * Private enumerations, structures and unions using typedef
 *****************************************************************/
struct fts_rw_i2c {
	u8 *buf;
	u8 flag;      /* 0-write 1-read */
	__u16 length; /* the length of data */
} *pfts_rw_i2c;

struct fts_rw_i2c_queue {
	struct fts_rw_i2c __user *i2c_queue;
	int queuenum;
} *pfts_rw_i2c_queue;

struct fts_rw_i2c_dev {
	struct cdev cdev;
	struct mutex fts_rw_i2c_mutex;
	struct i2c_client *client;
};

/*****************************************************************
 * Static variables
 *****************************************************************/
static int fts_rw_iic_drv_major = FTS_RW_IIC_DRV_MAJOR;
static struct class *fts_class;

/*****************************************************************
 * Global variable or extern global variabls/functions
 *****************************************************************/
struct fts_rw_i2c_dev *fts_rw_i2c_dev_tt;

/*****************************************************************
 * Static function prototypes
 *****************************************************************/

/************************************************************************
 * Name: fts_rw_iic_drv_myread
 * Brief: i2c read
 * Input: i2c info, data, length
 * Output: get data in buf
 * Return: fail <0
 ***********************************************************************/
static int fts_rw_iic_drv_myread(struct i2c_client *client, u8 *buf, int length)
{
	int ret = 0;

	ret = fts_i2c_read(client, NULL, 0, buf, length);

	if (ret < 0)
		dev_notice(&client->dev, "%s:IIC Read failed\n", __func__);
	return ret;
}

/************************************************************************
 * Name: fts_rw_iic_drv_mywrite
 * Brief: i2c write
 * Input: i2c info, data, length
 * Output: no
 * Return: fail <0
 ***********************************************************************/
static int fts_rw_iic_drv_mywrite(struct i2c_client *client, u8 *buf,
				  int length)
{
	int ret = 0;

	ret = fts_i2c_write(client, buf, length);
	if (ret < 0)
		dev_notice(&client->dev, "%s:IIC Write failed\n", __func__);
	return ret;
}

/************************************************************************
 * Name: fts_rw_iic_drv_RDWR
 * Brief: get package to i2c read/write
 * Input: i2c info, package
 * Output: put data in i2c_rw_msg.buf
 * Return: fail <0
 ***********************************************************************/
static int fts_rw_iic_drv_RDWR(struct i2c_client *client, unsigned long arg)
{
	struct fts_rw_i2c_queue i2c_rw_queue;
	struct fts_rw_i2c *i2c_rw_msg;
	u8 __user **data_ptrs;
	int ret = 0;
	int i;

	if (!access_ok(VERIFY_READ, (struct fts_rw_i2c_queue *)arg,
		       sizeof(struct fts_rw_i2c_queue)))
		return -EFAULT;

	if (copy_from_user(&i2c_rw_queue, (struct fts_rw_i2c_queue *)arg,
			   sizeof(struct fts_rw_i2c_queue)))
		return -EFAULT;

	if (i2c_rw_queue.queuenum > FTS_I2C_RDWR_MAX_QUEUE)
		return -EINVAL;

	i2c_rw_msg = kmalloc_array(
		i2c_rw_queue.queuenum, sizeof(struct fts_rw_i2c), GFP_KERNEL);
	if (!i2c_rw_msg)
		return -ENOMEM;

	if (copy_from_user(i2c_rw_msg, i2c_rw_queue.i2c_queue,
			   i2c_rw_queue.queuenum * sizeof(struct fts_rw_i2c))) {
		kfree(i2c_rw_msg);
		return -EFAULT;
	}

	data_ptrs = kmalloc_array(i2c_rw_queue.queuenum, sizeof(u8 __user *),
				  GFP_KERNEL);
	if (data_ptrs == NULL) {
		kfree(i2c_rw_msg);
		return -ENOMEM;
	}

	ret = 0;
	for (i = 0; i < i2c_rw_queue.queuenum; i++) {
		if ((i2c_rw_msg[i].length > 8192) ||
		    (i2c_rw_msg[i].flag & I2C_M_RECV_LEN)) {
			ret = -EINVAL;
			break;
		}
		data_ptrs[i] = (u8 __user *)i2c_rw_msg[i].buf;
		i2c_rw_msg[i].buf = kmalloc(i2c_rw_msg[i].length, GFP_KERNEL);
		if (i2c_rw_msg[i].buf == NULL) {
			ret = -ENOMEM;
			break;
		}

		if (copy_from_user(i2c_rw_msg[i].buf, data_ptrs[i],
				   i2c_rw_msg[i].length)) {
			++i;
			ret = -EFAULT;
			break;
		}
	}

	if (ret < 0) {
		int j;

		for (j = 0; j < i; ++j)
			kfree(i2c_rw_msg[j].buf);
		kfree(data_ptrs);
		kfree(i2c_rw_msg);
		return ret;
	}

	for (i = 0; i < i2c_rw_queue.queuenum; i++) {
		if (i2c_rw_msg[i].flag) {
			ret = fts_rw_iic_drv_myread(client, i2c_rw_msg[i].buf,
						    i2c_rw_msg[i].length);
			if (ret >= 0)
				ret = copy_to_user(data_ptrs[i],
						   i2c_rw_msg[i].buf,
						   i2c_rw_msg[i].length);
		} else {
			ret = fts_rw_iic_drv_mywrite(client, i2c_rw_msg[i].buf,
						     i2c_rw_msg[i].length);
		}
	}

	return ret;
}

/************************************************************************
 * Name: fts_rw_iic_drv_open
 * Brief: char device open function interface
 * Input: node, file point
 * Output: no
 * Return: 0
 ***********************************************************************/
static int fts_rw_iic_drv_open(struct inode *inode, struct file *filp)
{
	filp->private_data = fts_rw_i2c_dev_tt;
	return 0;
}

/************************************************************************
 * Name: fts_rw_iic_drv_release
 * Brief: char device close function interface
 * Input: node, file point
 * Output: no
 * Return: 0
 ***********************************************************************/
static int fts_rw_iic_drv_release(struct inode *inode, struct file *filp)
{

	return 0;
}

/************************************************************************
 * Name: fts_rw_iic_drv_ioctl
 * Brief: char device I/O control function interface
 * Input: file point, command, package
 * Output: no
 * Return: fail <0
 ***********************************************************************/
static long fts_rw_iic_drv_ioctl(struct file *filp, unsigned int cmd,
				 unsigned long arg)
{
	int ret = 0;
	struct fts_rw_i2c_dev *ftsdev = filp->private_data;

	ftsdev = filp->private_data;
#if FT_ESD_PROTECT
	esd_switch(0);
	apk_debug_flag = 1;
#endif
	mutex_lock(&fts_rw_i2c_dev_tt->fts_rw_i2c_mutex);
	switch (cmd) {
	case FTS_I2C_RW:
		ret = fts_rw_iic_drv_RDWR(ftsdev->client, arg);
		break;
	/* #if INTEL_EN */
	/* case FTS_RESET_TP: */
	/* fts_reset_tp((int)arg); */
	/* break; */
	/* #endif */
	default:
		ret = -ENOTTY;
		break;
	}
	mutex_unlock(&fts_rw_i2c_dev_tt->fts_rw_i2c_mutex);
#if FT_ESD_PROTECT
	esd_switch(1);
	apk_debug_flag = 0;
#endif
	return ret;
}

/*
 * char device file operation which will be put to register the char device
 */
static const struct file_operations fts_rw_iic_drv_fops = {
	.owner = THIS_MODULE,
	.open = fts_rw_iic_drv_open,
	.release = fts_rw_iic_drv_release,
	.unlocked_ioctl = fts_rw_iic_drv_ioctl,
};

/************************************************************************
 * Name: fts_rw_iic_drv_setup_cdev
 * Brief: setup char device
 * Input: device point, index number
 * Output: no
 * Return: no
 ***********************************************************************/
static void fts_rw_iic_drv_setup_cdev(struct fts_rw_i2c_dev *dev, int index)
{
	int err, devno = MKDEV(fts_rw_iic_drv_major, index);

	cdev_init(&dev->cdev, &fts_rw_iic_drv_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &fts_rw_iic_drv_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
		FTS_ERR("Error %d adding LED%d", err, index);
}

/************************************************************************
 * Name: fts_rw_iic_drv_myinitdev
 * Brief: initial char device
 * Input: i2c info
 * Output: no
 * Return: fail <0
 ***********************************************************************/
static int fts_rw_iic_drv_myinitdev(struct i2c_client *client)
{
	int err = 0;
	dev_t devno = MKDEV(fts_rw_iic_drv_major, 0);

	if (fts_rw_iic_drv_major)
		err = register_chrdev_region(devno, 1, FTS_RW_IIC_DRV);
	else {
		err = alloc_chrdev_region(&devno, 0, 1, FTS_RW_IIC_DRV);
		fts_rw_iic_drv_major = MAJOR(devno);
	}
	if (err < 0) {
		dev_notice(&client->dev,
			   "%s:ft_rw_iic_drv failed  error code=%d---\n",
			   __func__, err);
		return err;
	}

	fts_rw_i2c_dev_tt = kmalloc(sizeof(struct fts_rw_i2c_dev), GFP_KERNEL);
	if (!fts_rw_i2c_dev_tt) {
		err = -ENOMEM;
		unregister_chrdev_region(devno, 1);
		dev_notice(&client->dev, "%s:ft_rw_iic_drv failed\n", __func__);
		return err;
	}
	fts_rw_i2c_dev_tt->client = client;
	mutex_init(&fts_rw_i2c_dev_tt->fts_rw_i2c_mutex);
	fts_rw_iic_drv_setup_cdev(fts_rw_i2c_dev_tt, 0);

	fts_class = class_create(THIS_MODULE, "fts_class");
	if (IS_ERR(fts_class)) {
		dev_notice(&client->dev, "%s:failed in creating class.\n",
			   __func__);
		return -1;
	}
	/*create device node*/
	device_create(fts_class, NULL, MKDEV(fts_rw_iic_drv_major, 0), NULL,
		      FTS_RW_IIC_DRV);

	return 0;
}

/************************************************************************
 * Name: fts_rw_iic_drv_init
 * Brief: call initial char device
 * Input: i2c info
 * Output: no
 * Return: fail <0
 ***********************************************************************/
int fts_rw_iic_drv_init(struct i2c_client *client)
{
	dev_dbg(&client->dev, "[FTS]----ft_rw_iic_drv init ---\n");
	return fts_rw_iic_drv_myinitdev(client);
}

/************************************************************************
 * Name: fts_rw_iic_drv_exit
 * Brief: delete char device
 * Input: no
 * Output: no
 * Return: no
 ***********************************************************************/
void fts_rw_iic_drv_exit(void)
{
	device_destroy(fts_class, MKDEV(fts_rw_iic_drv_major, 0));
	/* delete class created by us */
	class_destroy(fts_class);
	/* delet the cdev */
	cdev_del(&fts_rw_i2c_dev_tt->cdev);
	kfree(fts_rw_i2c_dev_tt);
	unregister_chrdev_region(MKDEV(fts_rw_iic_drv_major, 0), 1);
}
