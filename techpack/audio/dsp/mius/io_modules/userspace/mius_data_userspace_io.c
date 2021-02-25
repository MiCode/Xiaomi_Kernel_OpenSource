/**
 * Copyright MI
 * Copyright (C) 2021 XiaoMi, Inc.
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

#include <mius/mius_data_io.h>
#include <mius/mius_device.h>


static dev_t mius_userspace_major;
#define USERSPACE_IO_DEVICE_NAME "mius_us_io"
struct mius_userspace_device {
	struct cdev cdev;
	struct semaphore sem;
};

static struct mius_userspace_device io_device;

static int device_open(struct inode *inode, struct file *filp)
{
	if (inode->i_cdev != &io_device.cdev) {
		pr_warn("mius : dev pointer mismatch\n");
		return -ENODEV; /* No such device */
	}

	if (down_interruptible(&io_device.sem) != 0)
		return -EEXIST;
	MI_PRINT_I("Opened device %s", USERSPACE_IO_DEVICE_NAME);
	return 0;
}

static ssize_t device_write(struct file *fp, const char __user *buff,
	size_t length, loff_t *ppos)
{
	int push_result;

	push_result = mius_data_push(
		MIUS_ALL_DEVICES, buff, length, MIUS_DATA_PUSH_FROM_USERSPACE);

	return push_result == 0 ? (ssize_t)length : (ssize_t)(-1);
}

static int device_close(struct inode *inode, struct file *filp)
{
	up(&io_device.sem);
	MI_PRINT_I("Closed device %s", USERSPACE_IO_DEVICE_NAME);
	return 0;
}

static const struct file_operations
mius_userspace_fops = {
	.owner		= THIS_MODULE,
	.open		= device_open,
	.write		= device_write,
	.release	= device_close,
};

int mius_userspace_io_driver_init(void)
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

	mius_userspace_major = MAJOR(device_number);

	device_number = MKDEV(mius_userspace_major, 0);
	device = device_create(
		mius_class, NULL, device_number,
		NULL, USERSPACE_IO_DEVICE_NAME);

	if (IS_ERR(device)) {
		unregister_chrdev(
			mius_userspace_major, USERSPACE_IO_DEVICE_NAME);
		pr_err("Failed to create the device\n");
		return PTR_ERR(device);
	}

	cdev_init(&io_device.cdev, &mius_userspace_fops);
	io_device.cdev.owner = THIS_MODULE;
	err = cdev_add(&io_device.cdev, device_number, 1);
	if (err) {
		MI_PRINT_W("error %d while trying to add %s%d",
			err, MIUS_DEVICENAME, 0);
		return err;
	}

	sema_init(&io_device.sem, 1);
	return 0;
}

void mius_userspace_io_driver_exit(void)
{
	BUG_ON(mius_class == NULL);
	device_destroy(mius_class, MKDEV(mius_userspace_major, 0));
	cdev_del(&io_device.cdev);
	unregister_chrdev(mius_userspace_major, USERSPACE_IO_DEVICE_NAME);
	up(&io_device.sem);
}


