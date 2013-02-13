/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

/* add additional information to our printk's */
#define pr_fmt(fmt) "%s: " fmt "\n", __func__

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#include "hsic_sysmon.h"
#include "sysmon.h"

#define DRIVER_DESC	"HSIC System monitor driver test"

#define RD_BUF_SIZE	4096

struct sysmon_test_dev {
	int			buflen;
	char			buf[RD_BUF_SIZE];
};
static struct sysmon_test_dev *sysmon_dev;

static ssize_t sysmon_test_read(struct file *file, char __user *ubuf,
				 size_t count, loff_t *ppos)
{
	struct sysmon_test_dev *dev = sysmon_dev;
	enum hsic_sysmon_device_id id =
				(enum hsic_sysmon_device_id)file->private_data;
	int ret;

	if (!dev)
		return -ENODEV;

	ret = hsic_sysmon_read(id, dev->buf, RD_BUF_SIZE, &dev->buflen, 3000);
	if (!ret)
		return simple_read_from_buffer(ubuf, count, ppos,
					dev->buf, dev->buflen);

	return 0;
}

static ssize_t sysmon_test_write(struct file *file, const char __user *ubuf,
				 size_t count, loff_t *ppos)
{
	struct sysmon_test_dev *dev = sysmon_dev;
	enum hsic_sysmon_device_id id =
				(enum hsic_sysmon_device_id)file->private_data;
	int ret;

	if (!dev)
		return -ENODEV;

	if (copy_from_user(dev->buf, ubuf, count)) {
		pr_err("error copying for writing");
		return 0;
	}

	ret = hsic_sysmon_write(id, dev->buf, count, 1000);
	if (ret < 0) {
		pr_err("error writing to hsic_sysmon");
		return ret;
	}

	return count;
}

static int sysmon_test_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return hsic_sysmon_open((enum hsic_sysmon_device_id)inode->i_private);
}

static int sysmon_test_release(struct inode *inode, struct file *file)
{
	hsic_sysmon_close((enum hsic_sysmon_device_id)inode->i_private);
	return 0;
}

static const struct file_operations sysmon_test_ops = {
	.read = sysmon_test_read,
	.write = sysmon_test_write,
	.open = sysmon_test_open,
	.release = sysmon_test_release
};

static struct dentry *dfile0, *dfile1;

static int __init sysmon_test_init(void)
{
	sysmon_dev = kzalloc(sizeof(*sysmon_dev), GFP_KERNEL);
	if (!sysmon_dev)
		return -ENOMEM;

	dfile0 = debugfs_create_file("hsic_sysmon_test.0", 0666, NULL,
			(void *)HSIC_SYSMON_DEV_EXT_MODEM, &sysmon_test_ops);
	dfile1 = debugfs_create_file("hsic_sysmon_test.1", 0666, NULL,
			(void *)HSIC_SYSMON_DEV_EXT_MODEM_2, &sysmon_test_ops);
	return 0;
}

static void __exit sysmon_test_exit(void)
{
	if (dfile0)
		debugfs_remove(dfile0);
	if (dfile1)
		debugfs_remove(dfile1);
	kfree(sysmon_dev);
}

module_init(sysmon_test_init);
module_exit(sysmon_test_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL v2");
