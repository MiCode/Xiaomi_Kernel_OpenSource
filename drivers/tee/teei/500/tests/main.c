// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/unistd.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/proc_fs.h>

#include "main.h"
#define IMSG_TAG "[tz_test]"
#include <imsg_log.h>
#include <isee_kernel_api.h>

static int tz_test_open(struct inode *inode, struct file *file)
{
	if (!is_teei_ready())
		return -EBUSY;
	return 0;
}

static int tz_test_release(struct inode *inode, struct file *file)
{
	if (!is_teei_ready())
		return -EBUSY;
	return 0;
}

static long tz_test_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	int err = 0;
	struct tzdrv_test_data param;

	IMSG_TRACE("invoke command '%d' from '%s'\n",
				_IOC_NR(cmd), current->comm);

	if (!is_teei_ready())
		return -EBUSY;
	if (_IOC_TYPE(cmd) != TZDRV_IOC_MAGIC)
		return -EFAULT;
	if (_IOC_NR(cmd) > TZDRV_IOC_MAXNR)
		return -EFAULT;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE,
				(void __user *)arg, _IOC_SIZE(cmd));
	if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ,
				(void __user *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EPERM;

	if (IFASSIGN(err, copy_from_user(&param, (void *)arg, sizeof(param))))
		return -EINVAL;

	switch (cmd) {
	case TZDRV_CMD_KERNEL_CA_TEST:
		err = kernel_ca_test(&param);
		break;
	case TZDRV_CMD_SECURE_DRV_TEST:
		err = secure_drv_test(&param);
		break;
	default:
		return -ENODEV;
	}

	if (copy_to_user((void *)arg, &param, sizeof(param)))
		IMSG_ERROR("copy data to user failed!\n");

	return err;
}

static const struct file_operations tz_test_fops = {
	.owner = THIS_MODULE,
	.open = tz_test_open,
	.release = tz_test_release,
	.unlocked_ioctl = tz_test_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tz_test_ioctl,
#endif
	.write = NULL,
	.read = NULL,
};

static int __init tz_test_init(void)
{
#define TZ_PERMISSION 0666

	/* S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH */
	proc_create("tz_test0", TZ_PERMISSION, NULL, &tz_test_fops);
	return 0;
}
late_initcall(tz_test_init);
