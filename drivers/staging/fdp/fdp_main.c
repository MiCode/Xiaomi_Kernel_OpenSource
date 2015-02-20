/* -------------------------------------------------------------------------
 * Copyright (C) 2013-2014 Inside Secure
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * ------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------
 * Copyright (C) 2014-2016, Intel Corporation
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * ------------------------------------------------------------------------- */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/device.h>

#include <linux/uaccess.h>

#include "fdp_main.h"
#include "porting_types.h"

MODULE_ALIAS("i2c:fdp");

MODULE_AUTHOR("Intel Corporation");
MODULE_LICENSE("GPL");

int fdp_major = FIELDSPEAK_MAJOR;
int fdp_minor;

static void fdp_cleanup_module(void);

static struct fdp_dev {

	/* common stuff, should be kept unchanged */

	struct cdev cdev;	/* char device structure */
	bool_t cdev_registered;

	struct class *class;	/* sysfs class */
	bool_t class_registered;

	struct device device;	/* sysfs device */
	bool_t device_registered;
} instance;

/*
 *  Function called when user opens the device
 */
int fdp_open(struct inode *inode, struct file *filp)
{
	return fdp_custom_open(inode, filp);
}

/*
 *  Function called when user closes the device
 */

int fdp_release(struct inode *inode, struct file *filp)
{
	return fdp_custom_release(inode, filp);
}

/*
 * function called when user performs a read operation
 */

ssize_t fdp_read(struct file *filp, char __user *buf, size_t count,
		 loff_t *f_pos)
{
	return fdp_custom_read(filp, buf, count, f_pos);
}

/*
 * function called when user performs a write operation
 */

ssize_t fdp_write(struct file *filp, const char __user *buf, size_t count,
		  loff_t *f_pos)
{
	return fdp_custom_write(filp, buf, count, f_pos);
}

/*
 * function called when user performs a ioctl operation
 */

long fdp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	int err = 0;

	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != FIELDSPEAK_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > FIELDSPEAK_MAX_IOCTL_VALUE)
		return -ENOTTY;

	/*
	 * the direction is a bitmask, and VERIFY_WRITE catches R/W
	 * transfers. `Type' is user-oriented, while
	 * access_ok is kernel-oriented, so the concept of "read" and
	 * "write" is reversed
	 */

	if (_IOC_DIR(cmd) & _IOC_READ)
		err =
		    !access_ok(VERIFY_WRITE, (void __user *)arg,
			       _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =
		    !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	switch (cmd) {

	case FIELDSPEAK_IOC_RESET:
		retval = fdp_custom_ioctl_reset(filp, cmd, arg);
		break;

	case FIELDSPEAK_IOC_CLK_REQ:
		retval = fdp_custom_ioctl_clk_req(filp, cmd, arg);
		break;

	}

	return retval;
}

unsigned int fdp_poll(struct file *filp, poll_table *wait)
{
	return fdp_custom_poll(filp, wait);
}

const struct file_operations fdp_fops = {
	.owner = THIS_MODULE,
	.read = fdp_read,
	.write = fdp_write,
	.unlocked_ioctl = fdp_ioctl,
	.open = fdp_open,
	.release = fdp_release,
	.poll = fdp_poll,
#ifdef CONFIG_COMPAT
	.compat_ioctl = fdp_ioctl
#endif
};

/*
 * function called when user loads the driver
 */

int fdp_init_module(void)
{
	int result;
	int devno;

	pr_debug("fdp_init_module\n");

	if (fdp_major) {

		/* user supplied major number, register the region */

		devno = MKDEV(fdp_major, fdp_minor);
		result = register_chrdev_region(devno, 1, "fieldspeak");
	} else {

		/* automatic major number, allocate a region */

		result =
		    alloc_chrdev_region(&devno, fdp_minor, 1, "fieldspeak");
		fdp_major = MAJOR(devno);
	}

	if (result < 0) {

		pr_err("fdp : can't get major %d\n", fdp_major);
		return result;
	}

	/* reset the instance */
	memset(&instance, 0, sizeof(instance));

	/* register the char device */
	cdev_init(&instance.cdev, &fdp_fops);
	instance.cdev.owner = THIS_MODULE;

	result = cdev_add(&instance.cdev, devno, 1);

	if (result) {
		pr_err("fdp : can't register device");

		fdp_cleanup_module();
		return result;
	}

	instance.cdev_registered = 1;

	/* register the nfc class */
	instance.class = class_create(THIS_MODULE, "nfc");

	if (IS_ERR(instance.class)) {

		pr_err("fdp : can't create class");

		result = PTR_ERR(instance.class);

		fdp_cleanup_module();
		return result;
	}

	fdp_custom_init();

	instance.class_registered = 1;

	/* add a device into the class */
	device_create(instance.class, NULL, devno, NULL, "nfcc");

	instance.device_registered = 1;

	/* succeeded */
	return 0;
}

/*
 * function called when user unloads the driver
 */

void fdp_cleanup_module(void)
{
	int devno = MKDEV(fdp_major, fdp_minor);

	pr_debug("fdp_cleanup_module\n");

	fdp_custom_exit();

	/* remove the device from the class if successfully added */
	if (instance.device_registered)
		device_destroy(instance.class, devno);

	/* unregister the device only if the registration succeeded */
	if (instance.class_registered)
		class_destroy(instance.class);

	/* unregister the character device */
	if (instance.cdev_registered)
		cdev_del(&instance.cdev);

	/* always unregister the region, since fdp_cleanup_module is not called
	 * if the registration failed */
	unregister_chrdev_region(devno, 1);
}

module_init(fdp_init_module);
module_exit(fdp_cleanup_module);

/* EOF */
