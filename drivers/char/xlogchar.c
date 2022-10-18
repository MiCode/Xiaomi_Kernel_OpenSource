/* Copyright (C) 2018 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/ratelimit.h>
#include <asm/current.h>
#include <asm/div64.h>
#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/poll.h>
#include "xlogchar.h"
#include <linux/string.h>

static struct xlogchar_dev *xlogdriver;

static int xlogchar_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int xlogchar_close(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t xlogchar_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	u64 temp = count;
	int err;
	size_t copy_bytes;
	if (do_div(temp, XLOGPKG_SIZE) || (count > XLOGBUF_SIZE)) {
		pr_err("xlog: invalide count %zu\n", count);
		return -EBADMSG;
	}
	if (!buf) {
		pr_err("xlog: bad address from user side\n");
		return -EFAULT;
	}
	mutex_lock(&xlogdriver->xlog_mutex);
	while ((XLOGBUF_SIZE - xlogdriver->free_size) < count) {
		mutex_unlock(&xlogdriver->xlog_mutex);
		pr_info("%s  goint to sleep\n", __func__);
		err = wait_event_interruptible(xlogdriver->wait_q, (XLOGBUF_SIZE - xlogdriver->free_size) >= count);
		if (err == -ERESTARTSYS) {
			return -ERESTARTSYS;
		}
		pr_info("%s  wakeup \n", __func__);
		mutex_lock(&xlogdriver->xlog_mutex);
	}
	if (XLOGBUF_SIZE < xlogdriver->readindex + count) {
		copy_bytes = XLOGBUF_SIZE - xlogdriver->readindex;
		err = copy_to_user(buf, (void *)(xlogdriver->buf + xlogdriver->readindex), copy_bytes);
		err = copy_to_user(buf + copy_bytes, (void *)(xlogdriver->buf), count - copy_bytes);
		xlogdriver->readindex = count - copy_bytes;
	} else {
		err = copy_to_user(buf, (void *)(xlogdriver->buf + xlogdriver->readindex), count);
		xlogdriver->readindex += count;
	}
	xlogdriver->free_size += count;
	mutex_unlock(&xlogdriver->xlog_mutex);
	return count;
}


static ssize_t xlogchar_write(struct file *file, const char __user *buf,
			      size_t count, loff_t *ppos)
{
	int err = 0;
	size_t copy_bytes;
	u64 temp = count;
	pr_info("%s: count is %zu\n", __func__, count);
	if (do_div(temp, XLOGPKG_SIZE) || (count > XLOGBUF_SIZE)) {
		pr_err("xlog: invalide count %zu\n", count);
		return -EBADMSG;
	}
	mutex_lock(&xlogdriver->xlog_mutex);
	if (xlogdriver->free_size < count) {
		pr_err("xlog: no more space to write free: %zu, count %zu\n", xlogdriver->free_size, count);
		mutex_unlock(&xlogdriver->xlog_mutex);
		return  -EIO;
	}
	if (XLOGBUF_SIZE < xlogdriver->writeindex + count) {
		copy_bytes = XLOGBUF_SIZE - xlogdriver->writeindex;
		err = copy_from_user(xlogdriver->buf + xlogdriver->writeindex, buf, copy_bytes);
		err = copy_from_user(xlogdriver->buf, buf + copy_bytes,  count - copy_bytes);
		xlogdriver->writeindex = count - copy_bytes;
	} else {
		err = copy_from_user(xlogdriver->buf + xlogdriver->writeindex, buf, count);
		xlogdriver->writeindex += count;
	}
	xlogdriver->free_size -= count;
	mutex_unlock(&xlogdriver->xlog_mutex);
	pr_info("%s  wakeup reader \n", __func__);
	wake_up_interruptible(&xlogdriver->wait_q);
	return count;
}

ssize_t xlogchar_kwrite(const char *buf, size_t count)
{
	size_t copy_bytes;
	u64 temp = count;
	pr_err("%s start\n", __func__);
	pr_info("%s: count is %zu\n", __func__, count);
	if (do_div(temp, XLOGPKG_SIZE) || (count > XLOGBUF_SIZE)) {
		pr_err("xlog: invalide count %zu\n", count);
		return -EBADMSG;
	}
	mutex_lock(&xlogdriver->xlog_mutex);
	if (xlogdriver->free_size < count) {
		pr_err("xlog: no more space to write free: %zu, count %zu\n", xlogdriver->free_size, count);
		mutex_unlock(&xlogdriver->xlog_mutex);
		return  -EIO;
	}
	if (XLOGBUF_SIZE < xlogdriver->writeindex + count) {
		copy_bytes = XLOGBUF_SIZE - xlogdriver->writeindex;
		memcpy(xlogdriver->buf + xlogdriver->writeindex, buf, copy_bytes);
		memcpy(xlogdriver->buf, buf + copy_bytes,  count - copy_bytes);
		xlogdriver->writeindex = count - copy_bytes;
	} else {
		memcpy(xlogdriver->buf + xlogdriver->writeindex, buf, count);
		xlogdriver->writeindex += count;
	}
	xlogdriver->free_size -= count;
	mutex_unlock(&xlogdriver->xlog_mutex);
	pr_info("%s  wakeup reader \n", __func__);
	wake_up_interruptible(&xlogdriver->wait_q);
	return count;
}

EXPORT_SYMBOL(xlogchar_kwrite);

static unsigned int xlogchar_poll(struct file *file, poll_table *wait)
{
	int masks = 0;

	return masks;
}


static const struct file_operations xlogcharfops = {
	.owner = THIS_MODULE,
	.read = xlogchar_read,
	.write = xlogchar_write,
	.poll = xlogchar_poll,
	.open = xlogchar_open,
	.release = xlogchar_close
};

static int xlogchar_setup_cdev(dev_t devno)
{

	int err;

	cdev_init(xlogdriver->cdev, &xlogcharfops);

	xlogdriver->cdev->owner = THIS_MODULE;
	xlogdriver->cdev->ops = &xlogcharfops;

	err = cdev_add(xlogdriver->cdev, devno, 1);

	if (err) {
		pr_info("xlog cdev registration failed !\n");
		return err;
	}

	xlogdriver->xlogchar_class = class_create(THIS_MODULE, "xlog");

	if (IS_ERR(xlogdriver->xlogchar_class)) {
		pr_err("Error creating xlogchar class.\n");
		return PTR_ERR(xlogdriver->xlogchar_class);
	}

	xlogdriver->xlog_dev = device_create(xlogdriver->xlogchar_class,
		NULL, devno, (void *)xlogdriver, "xlog");

	if (!xlogdriver->xlog_dev)
		return -EIO;

	return 0;

}

static int __init xlogchar_init(void)
{
	dev_t dev;
	int ret;

	pr_info("xlogchar_init\n");
	ret = 0;
	xlogdriver = kzalloc(sizeof(struct xlogchar_dev) + 5, GFP_KERNEL);
	if (!xlogdriver)
		return -ENOMEM;

	xlogdriver->buf = kzalloc(XLOGBUF_SIZE, GFP_KERNEL);
	if (!xlogdriver->buf)
		return -ENOMEM;

	mutex_init(&xlogdriver->xlog_mutex);
	init_waitqueue_head(&xlogdriver->wait_q);

	xlogdriver->num = 1;
	xlogdriver->name = ((void *)xlogdriver) + sizeof(struct xlogchar_dev);
	xlogdriver->free_size = XLOGBUF_SIZE;
	strlcpy(xlogdriver->name, "xlog", 4);
	/* Get major number from kernel and initialize */
	ret = alloc_chrdev_region(&dev, xlogdriver->minor_start,
				    xlogdriver->num, xlogdriver->name);
	if (!ret) {
		xlogdriver->major = MAJOR(dev);
		xlogdriver->minor_start = MINOR(dev);
	} else {
		pr_err("xlog: Major number not allocated\n");
		return ret;
	}
	xlogdriver->cdev = cdev_alloc();
	ret = xlogchar_setup_cdev(dev);
	if (ret)
		pr_err("xlogchar_setup_cdev failed\n");

	pr_info("xlogchar_init done\n");

	return ret;
}

static void xlogchar_exit(void)
{
	if (xlogdriver) {
		kfree(xlogdriver->buf);
		if (xlogdriver->cdev) {
			/* TODO - Check if device exists before deleting */
			device_destroy(xlogdriver->xlogchar_class,
				       MKDEV(xlogdriver->major,
					     xlogdriver->minor_start));
			cdev_del(xlogdriver->cdev);
		}
		if (!IS_ERR(xlogdriver->xlogchar_class))
			class_destroy(xlogdriver->xlogchar_class);
		kfree(xlogdriver);
	}
	pr_info("done xlogchar exit\n");
}

core_initcall(xlogchar_init);
module_exit(xlogchar_exit);

MODULE_DESCRIPTION("Xlog Char Driver");
MODULE_LICENSE("GPL v2");