// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/of_device.h>
#include <linux/types.h>
#include <linux/dma-direct.h>

#include "apusys_core.h"
#include "mdw_cmn.h"
#include "mdw_mem.h"

static struct class *mdw_class;
static dev_t mdw_devt;
static int mdw_major;
struct mdw_device *mdw_dev;
struct apusys_core_info *g_info;
static atomic_t g_inited;

static int mdw_dev_open(struct inode *inode, struct file *filp)
{
	struct mdw_fpriv *mpriv = NULL;

	mpriv = kzalloc(sizeof(*mpriv), GFP_KERNEL);
	if (!mpriv)
		return -ENOMEM;

	mpriv->mdev = mdw_dev;
	filp->private_data = mpriv;
	mutex_init(&mpriv->mtx);
	idr_init(&mpriv->cmds_idr);
	INIT_LIST_HEAD(&mpriv->mems);

	if (!atomic_read(&g_inited)) {
		mdw_dev->dev_funcs->sw_init(mdw_dev);
		atomic_inc(&g_inited);
	}

	return 0;
}

static int mdw_dev_release(struct inode *inode, struct file *filp)
{
	struct mdw_fpriv *mpriv = NULL;

	mpriv = filp->private_data;
	mutex_lock(&mpriv->mtx);
	mdw_cmd_mpriv_release(mpriv);
	mdw_mem_mpriv_release(mpriv);
	mutex_unlock(&mpriv->mtx);
	kfree(mpriv);

	return 0;
}

static const struct file_operations mdw_fops = {
	.owner = THIS_MODULE,
	.open = mdw_dev_open,
	.release = mdw_dev_release,
	.unlocked_ioctl = mdw_ioctl,
	.compat_ioctl = mdw_ioctl,
};

static void mdw_dev_release_func(struct device *dev)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	kfree(dev);
}

static int mdw_drv_cdev_init(struct mdw_device *mdev)
{
	int ret = 0;

	mdev->dev = kzalloc(sizeof(*mdev->dev), GFP_KERNEL);
	if (!mdev->dev) {
		ret = -ENOMEM;
		goto out;
	}

	cdev_init(&mdev->cdev, &mdw_fops);
	mdev->cdev.owner = THIS_MODULE;

	device_initialize(mdev->dev);
	mdev->dev->devt = MKDEV(mdev->major, 0);
	mdev->dev->class = mdw_class;
	mdev->dev->release = mdw_dev_release_func;
	dev_set_drvdata(mdev->dev, mdev);
	dev_set_name(mdev->dev, MDW_NAME);
	ret = cdev_device_add(&mdev->cdev, mdev->dev);
	if (ret) {
		pr_info("[error] %s: fail to add cdev(%d)\n",
			__func__, ret);
	}

out:
	return ret;
}

static void mdw_drv_cdev_deinit(struct mdw_device *mdev)
{
	pr_info("%s\n", __func__);
	cdev_device_del(&mdev->cdev, mdev->dev);
}

static struct mdw_device *mdw_drv_create_dev(struct platform_device *pdev)
{
	struct mdw_device *mdev = NULL;

	if (mdw_dev)
		return NULL;

	mdev = kzalloc(sizeof(*mdev), GFP_KERNEL);
	if (!mdev)
		return NULL;

	/* get parameter from dts */
	of_property_read_u32(pdev->dev.of_node, "version", &mdev->version);
	of_property_read_u32(pdev->dev.of_node, "dsp_mask", &mdev->dsp_mask);
	of_property_read_u32(pdev->dev.of_node, "dla_mask", &mdev->dla_mask);
	of_property_read_u32(pdev->dev.of_node, "dma_mask", &mdev->dma_mask);

	mdev->pdev = pdev;
	mdev->major = mdw_major;
	mdw_dev = mdev;

	return mdev;
}

static void mdw_drv_delete_dev(struct mdw_device *mdev)
{
	kfree(mdev);
	mdw_dev = NULL;
}

static int mdw_probe(struct platform_device *pdev)
{
	struct mdw_device *mdev = NULL;
	int ret = 0;

	g_mdw_klog = 0xff;

	/* create mdw device with fetal parameters */
	mdev = mdw_drv_create_dev(pdev);
	if (!mdev) {
		ret = -ENOMEM;
		goto out;
	}

	/* register char device */
	ret = mdw_drv_cdev_init(mdev);
	if (ret)
		goto delete_mdw_dev;

	platform_set_drvdata(pdev, mdev);

	/* init mdw device */
	ret = mdw_dev_init(mdev);
	if (ret)
		goto deinit_cdev;

	ret = mdw_mem_init(mdev);
	if (ret)
		goto deinit_mdev;

	ret = mdw_sysfs_init(mdev);
	if (ret)
		goto deinit_mem;

	mdw_dbg_init(g_info);

	pr_info("%s done\n", __func__);

	goto out;

deinit_mem:
	mdw_mem_deinit(mdev);
deinit_mdev:
	mdw_dev_deinit(mdev);
deinit_cdev:
	mdw_drv_cdev_deinit(mdev);
delete_mdw_dev:
	mdw_drv_delete_dev(mdev);
out:
	return ret;
}

static int mdw_remove(struct platform_device *pdev)
{
	struct mdw_device *mdev;

	mdev = platform_get_drvdata(pdev);
	if (!mdev)
		return -EINVAL;

	mdev->dev_funcs->sw_deinit(mdev);
	mdw_dbg_deinit();
	mdw_sysfs_deinit(mdev);
	mdw_mem_deinit(mdev);
	mdw_dev_deinit(mdev);
	mdw_drv_cdev_deinit(mdev);
	mdw_drv_delete_dev(mdev);
	pr_info("%s done\n", __func__);

	return 0;
}

static struct platform_driver mdw_driver = {
	.driver = {
		.name = MDW_NAME,
		.owner = THIS_MODULE,
	},
	.probe = mdw_probe,
	.remove = mdw_remove,
};

static const struct of_device_id mdw_of_match[] = {
	{ .compatible = "mediatek, apu_mdw", },
	{},
};

int mdw_init(struct apusys_core_info *info)
{
	int ret = 0;

	g_info = info;

	/* get major */
	ret = alloc_chrdev_region(&mdw_devt, 0, 1, MDW_NAME);
	if (ret < 0) {
		pr_info("[error] unable to get major\n");
		goto out;
	}
	mdw_major = MAJOR(mdw_devt);

	/* create class */
	mdw_class = class_create(THIS_MODULE, MDW_NAME);
	if (IS_ERR(mdw_class)) {
		pr_info("[error] failed to allocate class\n");
		ret = PTR_ERR(mdw_class);
		goto remove_major;
	}

	mdw_driver.driver.of_match_table = mdw_of_match;

	ret =  platform_driver_register(&mdw_driver);
	if (ret) {
		pr_info("failed to register apu mdw driver\n");
		goto delete_class;
	}

	pr_info("%s:%d\n", __func__, __LINE__);

	goto out;

delete_class:
	class_destroy(mdw_class);
remove_major:
	unregister_chrdev_region(MKDEV(mdw_major, 0), 1);
out:
	return ret;
}

void mdw_exit(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	platform_driver_unregister(&mdw_driver);
	class_destroy(mdw_class);
	unregister_chrdev_region(MKDEV(mdw_major, 0), 1);
	g_info = NULL;
}
