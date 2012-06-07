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
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/crc-ccitt.h>
#include <mach/diag_bridge.h>

#define DRIVER_DESC	"USB host diag bridge driver test"
#define DRIVER_VERSION	"1.0"

#define RD_BUF_SIZE	2048
#define DIAG_TEST_CONNECTED	0

struct diag_test_dev {
	char *read_buf;
	struct work_struct read_w;
	unsigned long	flags;

	struct diag_bridge_ops	ops;
};
static struct diag_test_dev *__dev;
static struct dentry *dent;

static void
diag_test_read_complete_cb(void *d, char *buf, size_t size, size_t actual)
{
	if (actual < 0) {
		pr_err("%s: read complete err\n", __func__);
		return;
	}

	print_hex_dump(KERN_INFO, "to_host:", 0, 1, 1, buf, actual, false);
}
static void diag_test_read_work(struct work_struct *w)
{
	struct diag_test_dev *dev =
		container_of(w, struct diag_test_dev, read_w);

	memset(dev->read_buf, 0, RD_BUF_SIZE);
	diag_bridge_read(dev->read_buf, RD_BUF_SIZE);
}

static void
diag_test_write_complete_cb(void *d, char *buf, size_t size, size_t actual)
{
	struct diag_test_dev *dev = d;

	if (actual > 0)
		schedule_work(&dev->read_w);
}

#if defined(CONFIG_DEBUG_FS)
#define DEBUG_BUF_SIZE	1024
static ssize_t send_ping_cmd(struct file *file, const char __user *ubuf,
				 size_t count, loff_t *ppos)
{
	struct diag_test_dev	*dev = __dev;
	unsigned char		*buf;
	int			temp = sizeof(unsigned char) * 4;

	if (!dev)
		return -ENODEV;

	buf = kmalloc(temp, GFP_KERNEL);
	if (!buf) {
		pr_err("%s: unable to allocate mem for ping cmd\n",
				__func__);
		return -ENOMEM;
	}

	/* hdlc encoded ping command */
	buf[0] = 0x0C;
	buf[1] = 0x14;
	buf[2] = 0x3A;
	buf[3] = 0x7E;

	diag_bridge_write(buf, temp);

	return count;
}

const struct file_operations diag_test_ping_ops = {
	.write = send_ping_cmd,
};

static void diag_test_debug_init(void)
{
	struct dentry *dfile;

	dent = debugfs_create_dir("diag_test", 0);
	if (IS_ERR(dent))
		return;

	dfile = debugfs_create_file("send_ping", 0444, dent,
			0, &diag_test_ping_ops);
	if (!dfile || IS_ERR(dfile))
		debugfs_remove(dent);
}
#else
static void diag_test_debug_init(void) { }
#endif

static int diag_test_remove(struct platform_device *pdev)
{
	diag_bridge_close();

	if (dent) {
		debugfs_remove_recursive(dent);
		dent = NULL;
	}

	return 0;
}

static int diag_test_probe(struct platform_device *pdev)
{
	struct diag_test_dev	*dev = __dev;
	int			ret = 0;

	pr_info("%s:\n", __func__);

	ret = diag_bridge_open(&dev->ops);
	if (ret)
		pr_err("diag open failed: %d", ret);


	diag_test_debug_init();

	return ret;
}

static struct platform_driver diag_test = {
	.remove = diag_test_remove,
	.probe	= diag_test_probe,
	.driver = {
		.name = "diag_bridge",
		.owner = THIS_MODULE,
	},
};

static int __init diag_test_init(void)
{
	struct diag_test_dev	*dev;
	int ret = 0;

	pr_info("%s\n", __func__);

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	__dev = dev;

	dev->ops.read_complete_cb = diag_test_read_complete_cb;
	dev->ops.write_complete_cb = diag_test_write_complete_cb;
	dev->read_buf = kmalloc(RD_BUF_SIZE, GFP_KERNEL);
	if (!dev->read_buf) {
		pr_err("%s: unable to allocate read buffer\n", __func__);
		kfree(dev);
		return -ENOMEM;
	}

	dev->ops.ctxt = dev;
	INIT_WORK(&dev->read_w, diag_test_read_work);

	ret = platform_driver_register(&diag_test);
	if (ret)
		pr_err("%s: platform driver %s register failed %d\n",
				__func__, diag_test.driver.name, ret);

	return ret;
}

static void __exit diag_test_exit(void)
{
	struct diag_test_dev *dev = __dev;

	pr_info("%s:\n", __func__);

	if (test_bit(DIAG_TEST_CONNECTED, &dev->flags))
		diag_bridge_close();

	kfree(dev->read_buf);
	kfree(dev);

}

module_init(diag_test_init);
module_exit(diag_test_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL v2");
