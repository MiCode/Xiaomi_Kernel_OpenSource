/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/usb/cdc.h>
#include <linux/uaccess.h>
#include <mach/usb_dun_bridge.h>

#define RD_BUF_SIZE		2048
#define DUN_TEST_CONNECTED	0


struct dun_bridge_test_dev {
	char *read_buf;
	size_t buflen;
	struct work_struct read_w;
	unsigned long	flags;

	struct dun_bridge_ops	ops;
};
static struct dun_bridge_test_dev *__dev;

static struct dentry *dfile;

static void
dun_bridge_test_read_complete(void *d, char *buf, size_t size, size_t actual)
{
	if (actual < 0) {
		pr_err("%s: read complete err\n", __func__);
		return;
	}

	__dev->buflen = actual;
	buf[actual] = 0;

	pr_info("%s: %s\n", __func__, buf);

	if (test_bit(DUN_TEST_CONNECTED, &__dev->flags))
		schedule_work(&__dev->read_w);
}

static void dun_bridge_test_read_work(struct work_struct *w)
{
	struct dun_bridge_test_dev *dev =
		container_of(w, struct dun_bridge_test_dev, read_w);

	dun_bridge_read(dev->read_buf, RD_BUF_SIZE);
}

static void
dun_bridge_test_write_complete(void *d, char *buf, size_t size, size_t actual)
{
	struct dun_bridge_test_dev *dev = d;

	if (actual > 0)
		schedule_work(&dev->read_w);

	kfree(buf);
}

#if defined(CONFIG_DEBUG_FS)
#define DEBUG_BUF_SIZE	1024

#define ACM_CTRL_DTR		0x01
#define ACM_CTRL_RTS		0x02

static int debug_open(struct inode *inode, struct file *file)
{
	struct dun_bridge_test_dev *dev = __dev;
	int ret = 0;

	if (!dev)
		return -ENODEV;

	if (!test_bit(DUN_TEST_CONNECTED, &dev->flags)) {
		ret = dun_bridge_open(&dev->ops);
		if (ret)
			return ret;
		set_bit(DUN_TEST_CONNECTED, &dev->flags);
		dun_bridge_send_ctrl_bits(ACM_CTRL_DTR | ACM_CTRL_RTS);
	}

	return ret;
}

static ssize_t debug_read(struct file *file, char __user *ubuf,
				 size_t count, loff_t *ppos)
{
	struct dun_bridge_test_dev	*dev = __dev;
	return simple_read_from_buffer(ubuf, count, ppos,
			dev->read_buf, dev->buflen);
}

static ssize_t debug_write(struct file *file, const char __user *ubuf,
				 size_t count, loff_t *ppos)
{
	struct dun_bridge_test_dev *dev = __dev;
	unsigned char *buf;
	int ret;

	if (!dev)
		return -ENODEV;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf) {
		pr_err("%s: unable to allocate mem for writing\n", __func__);
		return -ENOMEM;
	}

	if (!copy_from_user(buf, ubuf, count)) {
		ret = dun_bridge_write(buf, count);
		if (ret < 0) {
			pr_err("%s: error writing to dun_bridge\n", __func__);
			kfree(buf);
			return ret;
		}
	} else {
		pr_err("%s: error copying for writing\n", __func__);
		kfree(buf);
	}

	return count;
}

const struct file_operations dun_bridge_test_debug_ops = {
	.open = debug_open,
	.read = debug_read,
	.write = debug_write,
};

static void dun_bridge_test_debug_init(void)
{
	dfile = debugfs_create_file("dun_bridge_test", 0555, NULL,
			NULL, &dun_bridge_test_debug_ops);
}
#else
static void dun_bridge_test_debug_init(void) { }
#endif

static int __init dun_bridge_test_init(void)
{
	struct dun_bridge_test_dev	*dev;

	pr_info("%s\n", __func__);

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	__dev = dev;

	dev->ops.read_complete = dun_bridge_test_read_complete;
	dev->ops.write_complete = dun_bridge_test_write_complete;
	dev->read_buf = kmalloc(RD_BUF_SIZE, GFP_KERNEL);
	if (!dev->read_buf) {
		pr_err("%s: unable to allocate read buffer\n", __func__);
		kfree(dev);
		return -ENOMEM;
	}

	dev->ops.ctxt = dev;
	INIT_WORK(&dev->read_w, dun_bridge_test_read_work);

	dun_bridge_test_debug_init();

	return 0;
}

static void __exit dun_bridge_test_exit(void)
{
	struct dun_bridge_test_dev *dev = __dev;

	pr_info("%s:\n", __func__);

	if (test_bit(DUN_TEST_CONNECTED, &dev->flags))
		dun_bridge_close();

	debugfs_remove(dfile);

	kfree(dev->read_buf);
	kfree(dev);
}

module_init(dun_bridge_test_init);
module_exit(dun_bridge_test_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL V2");
