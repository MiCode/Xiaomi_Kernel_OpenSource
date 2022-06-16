// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/of_device.h>
#include <linux/types.h>
#include <linux/dma-direct.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <mtk_heap.h>
#include <linux/completion.h>
#include <linux/rpmsg.h>

#if IS_ENABLED(CONFIG_COMPAT)
#include <linux/compat.h>
#endif

#include "sapu_driver.h"

static void sapu_rpm_lock_exit(void);

static struct mutex sapu_lock_rpm_mtx;
struct mutex *get_rpm_mtx(void)
{
	return &sapu_lock_rpm_mtx;
}

static struct sapu_lock_rpmsg_device sapu_lock_rpm_dev;
struct sapu_lock_rpmsg_device *get_rpm_dev(void)
{
	return &sapu_lock_rpm_dev;
}

static long
apusys_sapu_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	long ret;
	void __user *user_req = (void __user *)arg;

	ret = apusys_sapu_internal_ioctl(filep, cmd, user_req, 0);
	return ret;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long apusys_sapu_compat_ioctl(struct file *filep, unsigned int cmd,
	unsigned long arg)
{
	long ret;
	void __user *user_req = (void __user *)compat_ptr(arg);

	ret = apusys_sapu_internal_ioctl(filep, cmd, user_req, 1);
	return ret;
}
#endif


static int apusys_sapu_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int apusys_sapu_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t
apusys_sapu_read(struct file *filep, char __user *buf, size_t count,
					   loff_t *pos)
{
	return 0;
}

/*
 *static struct miscdevice apusys_sapu_device = {
 *	.minor = MISC_DYNAMIC_MINOR,
 *	.name = "apusys_sapu",
 *	.fops = &apusys_sapu_fops,
 *	.mod = 0x0660,
 *};
 *
 *struct apusys_sapu_data {
 *	struct platform_device *pdev;
 *	struct miscdevice mdev;
 *};
 */

static const struct file_operations apusys_sapu_fops = {
	.owner = THIS_MODULE,
	.open = apusys_sapu_open,
	.release = apusys_sapu_release,
	.unlocked_ioctl = apusys_sapu_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = apusys_sapu_compat_ioctl,
#endif
	.read = apusys_sapu_read,
};

static int apusys_sapu_probe(struct platform_device *pdev)
{
	int ret;
	struct apusys_sapu_data *data;
	struct device *dev = &pdev->dev;

	data = devm_kzalloc(dev, sizeof(struct apusys_sapu_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	platform_set_drvdata(pdev, data);
	data->pdev = pdev;

	data->mdev.minor  = MISC_DYNAMIC_MINOR;
	data->mdev.name   = "apusys_sapu";
	data->mdev.fops   = &apusys_sapu_fops;
	data->mdev.parent = NULL;
	data->mdev.mode   = 0x0660;

	kref_init(&data->lock_ref_cnt); // ref_count == 1

	//apusys_sapu_device.this_device = apusys_sapu_device->dev;
	ret = misc_register(&data->mdev);

	if (ret != 0) {
		pr_info("ERR: misc register failed.");
		devm_kfree(dev, data);
	}

	return ret;
}
static int apusys_sapu_remove(struct platform_device *pdev)
{
	struct apusys_sapu_data *data;
	struct device *dev = &pdev->dev;

	data = platform_get_drvdata(pdev);
	if (data != NULL) {
		misc_deregister(&data->mdev);
		devm_kfree(dev, data);
	}

	return 0;
}

static int sapu_lock_rpmsg_probe(struct rpmsg_device *rpdev)
{
	pr_info("%s: name=%s, src=%d\n",
			__func__, rpdev->id.name, rpdev->src);

	sapu_lock_rpm_dev.ept = rpdev->ept;
	sapu_lock_rpm_dev.rpdev = rpdev;

	pr_info("%s: rpdev->ept = %p\n", __func__, rpdev->ept);

	return 0;
}

static int sapu_lock_rpmsg_cb(struct rpmsg_device *rpdev, void *data,
		int len, void *priv, u32 src)
{
	struct PWRarg *d = data;

	pr_info("%s: lock = %d\n", __func__, d->lock);
	complete(&sapu_lock_rpm_dev.ack);

	return 0;
}

static void sapu_lock_rpmsg_remove(struct rpmsg_device *rpdev)
{
	sapu_rpm_lock_exit();
}

#define MODULE_NAME "apusys_sapu"
static const struct of_device_id apusys_sapu_of_match[] = {
	{ .compatible = "android,trusty-sapu", },
	{},
};
MODULE_DEVICE_TABLE(of, apusys_sapu_of_match);

struct platform_driver apusys_sapu_driver = {
	.probe = apusys_sapu_probe,
	.remove = apusys_sapu_remove,
	.driver	= {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = apusys_sapu_of_match,
	},
};

static const struct of_device_id sapu_lock_rpmsg_of_match[] = {
	{ .compatible = "mediatek,apu-lock-rv-rpmsg", },
	{},
};

struct rpmsg_driver sapu_lock_rpmsg_driver = {
	.drv = {
		.name = "apu-lock-rv-rpmsg",
		.owner = THIS_MODULE,
		.of_match_table = sapu_lock_rpmsg_of_match,
	},
	.probe = sapu_lock_rpmsg_probe,
	.callback = sapu_lock_rpmsg_cb,
	.remove = sapu_lock_rpmsg_remove,
};

static int sapu_rpm_lock_init(void)
{
	int ret = 0;

	init_completion(&sapu_lock_rpm_dev.ack);
	mutex_init(&sapu_lock_rpm_mtx);

	pr_info("%s: register rpmsg...\n", __func__);
	ret = register_rpmsg_driver(&sapu_lock_rpmsg_driver);
	if (ret) {
		pr_info("(%d)failed to register sapu lock rpmsg driver\n",
			ret);
		mutex_destroy(&sapu_lock_rpm_mtx);
		goto error;
	}
error:
	return ret;
}

static void sapu_rpm_lock_exit(void)
{
	unregister_rpmsg_driver(&sapu_lock_rpmsg_driver);
	mutex_destroy(&sapu_lock_rpm_mtx);
}

static int __init sapu_init(void)
{
	int ret;

	ret = platform_driver_register(&apusys_sapu_driver);

	if (ret) {
		pr_info("[%s] %s register fail\n",
			__func__, "apusys_sapu_driver");
		goto sapu_driver_quit;
	}

	ret = sapu_rpm_lock_init();

	if (ret) {
		pr_info("[%s] %s register fail\n",
			__func__,
			"sapu_lock_rpmsg_driver");
		goto sapu_rpmsg_quit;
	}
	return 0;

sapu_rpmsg_quit:
	unregister_rpmsg_driver(&sapu_lock_rpmsg_driver);
sapu_driver_quit:
	platform_driver_unregister(&apusys_sapu_driver);
	return ret;
}

void sapu_exit(void)
{
	unregister_rpmsg_driver(&sapu_lock_rpmsg_driver);
	platform_driver_unregister(&apusys_sapu_driver);
}

module_init(sapu_init);
module_exit(sapu_exit);
MODULE_LICENSE("GPL v2");
