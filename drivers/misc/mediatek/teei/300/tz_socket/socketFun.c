/*
 * Copyright (c) 2015-2017 MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include "TEEI.h"
#include "socketFun.h"
#define SOCKET_SIZE	0x80000
#define MEM_CLEAR	0x1
#define SOCKET_MAJOR	253
#define SHMEM_ENABLE    0
#define SHMEM_DISABLE   1

#define IMSG_TAG "[tz_socket]"
#include <imsg_log.h>

static int socket_major = SOCKET_MAJOR;
static struct class *driver_class;
static dev_t devno;

struct socket_dev {
	struct cdev cdev;
	unsigned char mem[SOCKET_SIZE];
	struct semaphore sem;
};

struct semaphore daulOS_rd_sem;
EXPORT_SYMBOL_GPL(daulOS_rd_sem);
struct semaphore daulOS_wr_sem;
EXPORT_SYMBOL_GPL(daulOS_wr_sem);

struct socket_dev *socket_devp;

int socket_open(struct inode *inode, struct file *filp)
{
	filp->private_data = socket_devp;
	return 0;
}

int socket_release(struct inode *inode, struct file *filp)
{
	filp->private_data = NULL;
	return 0;
}

static long socket_ioctl(struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	struct socket_dev *dev = filp->private_data;

	switch (cmd) {
	case MEM_CLEAR:
		if (down_interruptible(&dev->sem))
			return -ERESTARTSYS;

		memset(dev->mem, 0, SOCKET_SIZE);
		up(&dev->sem);
		IMSG_INFO("Socket is set to zero.\n");
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static ssize_t socket_read(struct file *filp, char __user *buf,
			size_t size, loff_t *ppos)
{
	struct TEEI_socket_command *socket_p = NULL;
	int length = 0;
	int ret = 0;

	down(&daulOS_rd_sem);

	socket_p = (struct TEEI_socket_command *)daulOS_share_mem;

	if (socket_p->cmd_size > size)
		length = size;
	else
		length = socket_p->cmd_size;

	if (copy_to_user(buf, (void *)socket_p, length))
		ret = -EFAULT;
	else
		ret = length;

	return ret;
}

static ssize_t socket_write(struct file *filp, const char __user *buf,
				size_t size, loff_t *ppos)
{

	if (daulOS_shmem_flags == SHMEM_DISABLE) {
		IMSG_INFO("Socket write timeout\n");
		return -ETIME;
	}

	IMSG_INFO("Socket write function is running\n");

	memset((void *)daulOS_share_mem, 0, size);

	if (copy_from_user((void *)daulOS_share_mem, buf, size))
		return -EFAULT;

	up(&daulOS_wr_sem);
	return 0;
}

static loff_t socket_llseek(struct file *filp, loff_t offset, int orig)
{
	loff_t ret = 0;

	switch (orig) {
	case 0:
		if (offset < 0) {
			ret = -EINVAL;
			break;
		}

		if ((unsigned int)offset > SOCKET_SIZE) {
			ret = -EINVAL;
			break;
		}

		filp->f_pos = (unsigned int)offset;
		ret = filp->f_pos;
		break;

	case 1:
		if ((filp->f_pos + offset) > SOCKET_SIZE) {
			ret = -EINVAL;
			break;
		}

		if ((filp->f_pos + offset) < 0) {
			ret = -EINVAL;
			break;
		}

		filp->f_pos += offset;
		ret = filp->f_pos;
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct file_operations socket_fops = {
	.owner =		THIS_MODULE,
	.llseek =		socket_llseek,
	.read =			socket_read,
	.write =		socket_write,
	.unlocked_ioctl = socket_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = socket_ioctl,
#endif
	.open =			socket_open,
	.release =		socket_release,
};

static void socket_setup_cdev(struct socket_dev *dev, int index)
{
	int err = 0;
	int devno = MKDEV(socket_major, index);

	cdev_init(&dev->cdev, &socket_fops);
	dev->cdev.owner = socket_fops.owner;
	err = cdev_add(&dev->cdev, devno, 1);

	if (err)
		IMSG_ERROR("Error %d adding socket %d.\n", err, index);
}

int socket_init(void)
{
	int result = 0;
	struct device *class_dev = NULL;

	devno = MKDEV(socket_major, 0);

	result = alloc_chrdev_region(&devno, 0, 1, "tz_socket");
	socket_major = MAJOR(devno);

	if (result < 0)
		return result;

	driver_class = class_create(THIS_MODULE, "tz_socket");

	if (IS_ERR(driver_class)) {
		result = -ENOMEM;
		IMSG_ERROR("class_create failed %d.\n", result);
		goto unregister_chrdev_region;
	}

	class_dev = device_create(driver_class, NULL, devno, NULL, "tz_socket");

	if (!class_dev) {
		result = -ENOMEM;
		IMSG_ERROR("class_device_create failed %d.\n", result);
		goto class_destroy;
	}

	socket_devp = kmalloc(sizeof(struct socket_dev), GFP_KERNEL);

	if (socket_devp == NULL) {
		result = -ENOMEM;
		goto class_device_destroy;
	}

	memset(socket_devp, 0, sizeof(struct socket_dev));
	socket_setup_cdev(socket_devp, 0);
	TZ_SEMA_INIT_1(&socket_devp->sem);
	TZ_SEMA_INIT_0(&daulOS_rd_sem);
	TZ_SEMA_INIT_0(&daulOS_wr_sem);
	goto return_fn;

class_device_destroy:
	device_destroy(driver_class, devno);
class_destroy:
	class_destroy(driver_class);
unregister_chrdev_region:
	unregister_chrdev_region(devno, 1);
return_fn:
	return result;
}

void socket_exit(void)
{
	device_destroy(driver_class, devno);
	class_destroy(driver_class);
	cdev_del(&socket_devp->cdev);
	kfree(socket_devp);
	unregister_chrdev_region(MKDEV(socket_major, 0), 1);
}

MODULE_AUTHOR("microtrust");
MODULE_LICENSE("Dual BSD/GPL");

module_param(socket_major, int, 0444);

module_init(socket_init);
module_exit(socket_exit);
