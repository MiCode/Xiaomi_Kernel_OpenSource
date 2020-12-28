/**
 * Copyright Elliptic Labs
 * Copyright (C) 2020 XiaoMi, Inc.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/mm.h>


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
/*  includes the file structure, that is, file open read close */
#include <linux/fs.h>

/* include the character device, makes cdev avilable */
#include <linux/cdev.h>
#include <linux/semaphore.h>

/* includes copy_user vice versa */
#include <linux/uaccess.h>

#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/device.h>


#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>
#include <linux/errno.h>

#include <elliptic/elliptic_data_io.h>
#include <elliptic/elliptic_device.h>


static dev_t elliptic_userspace_major;
#define USERSPACE_IO_DEVICE_NAME "elliptic_us_io"
struct elliptic_userspace_device {
	struct cdev cdev;
	struct semaphore sem;
};

static struct elliptic_userspace_device io_device;

static int device_open(struct inode *inode, struct file *filp)
{
	if (inode->i_cdev != &io_device.cdev) {
		pr_warn("elliptic : dev pointer mismatch\n");
		return -ENODEV; /* No such device */
	}

	if (down_interruptible(&io_device.sem) != 0)
		return -EEXIST;
	EL_PRINT_I("Opened device %s", USERSPACE_IO_DEVICE_NAME);
	return 0;
}

static ssize_t device_write(struct file *fp, const char __user *buff,
	size_t length, loff_t *ppos)
{
	int push_result;

	push_result = elliptic_data_push(
		ELLIPTIC_ALL_DEVICES, buff, length, ELLIPTIC_DATA_PUSH_FROM_USERSPACE);

	return push_result == 0 ? (ssize_t)length : (ssize_t)(-1);
}

static int device_close(struct inode *inode, struct file *filp)
{
	up(&io_device.sem);
	EL_PRINT_I("Closed device %s", USERSPACE_IO_DEVICE_NAME);
	return 0;
}

static const struct file_operations
elliptic_userspace_fops = {
	.owner		= THIS_MODULE,
	.open		= device_open,
	.write		= device_write,
	.release	= device_close,
};

int elliptic_userspace_io_driver_init(void)
{
	struct device *device;
	dev_t device_number;
	int err;

	err = alloc_chrdev_region(
		&device_number, 0, 1, USERSPACE_IO_DEVICE_NAME);

	if (err < 0) {
		pr_err("failed to allocate chrdev region\n");
		return err;
	}

	elliptic_userspace_major = MAJOR(device_number);

	device_number = MKDEV(elliptic_userspace_major, 0);
	device = device_create(
		elliptic_class, NULL, device_number,
		NULL, USERSPACE_IO_DEVICE_NAME);

	if (IS_ERR(device)) {
		unregister_chrdev(
			elliptic_userspace_major, USERSPACE_IO_DEVICE_NAME);
		pr_err("Failed to create the device\n");
		return PTR_ERR(device);
	}

	cdev_init(&io_device.cdev, &elliptic_userspace_fops);
	io_device.cdev.owner = THIS_MODULE;
	err = cdev_add(&io_device.cdev, device_number, 1);
	if (err) {
		EL_PRINT_W("error %d while trying to add %s%d",
			err, ELLIPTIC_DEVICENAME, 0);
		return err;
	}

	sema_init(&io_device.sem, 1);
	return 0;
}

void elliptic_userspace_io_driver_exit(void)
{
	BUG_ON(elliptic_class == NULL);
	device_destroy(elliptic_class, MKDEV(elliptic_userspace_major, 0));
	cdev_del(&io_device.cdev);
	unregister_chrdev(elliptic_userspace_major, USERSPACE_IO_DEVICE_NAME);
	up(&io_device.sem);
}


