// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/types.h>
#include <linux/dma-direct.h>
#include <linux/rpmsg.h>

#include "apusys_core.h"
#include "mdw_cmn.h"
#include "mdw_mem.h"

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

static int mdw_drv_misc_init(struct mdw_device *mdev)
{
	mdev->misc_dev.minor = MISC_DYNAMIC_MINOR;
	mdev->misc_dev.name = MDW_NAME;
	mdev->misc_dev.fops = &mdw_fops;

	return misc_register(&mdev->misc_dev);
}

static void mdw_drv_misc_deinit(struct mdw_device *mdev)
{
	return misc_deregister(&mdev->misc_dev);
}

static int mdw_probe(struct platform_device *pdev)
{
	struct mdw_device *mdev = NULL;
	int ret = 0;

	if (mdw_dev)
		return -EBUSY;

	mdev = kzalloc(sizeof(*mdev), GFP_KERNEL);
	if (!mdev)
		return -ENOMEM;

	/* get parameter from dts */
	of_property_read_u32(pdev->dev.of_node, "version", &mdev->version);
	of_property_read_u32(pdev->dev.of_node, "dsp_mask", &mdev->dsp_mask);
	of_property_read_u32(pdev->dev.of_node, "dla_mask", &mdev->dla_mask);
	of_property_read_u32(pdev->dev.of_node, "dma_mask", &mdev->dma_mask);
	mdev->pdev = pdev;
	mdev->driver_type = MDW_DRIVER_TYPE_PLATFORM;
	mdw_dev = mdev;
	platform_set_drvdata(pdev, mdev);

	/* register misc device */
	ret = mdw_drv_misc_init(mdev);
	if (ret) {
		pr_info("%s: register misc device fail\n");
		goto delete_mdw_dev;
	}

	/* init mdw device */
	ret = mdw_dev_init(mdev);
	if (ret)
		goto unregister_misc;

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
unregister_misc:
	mdw_drv_misc_deinit(mdev);
delete_mdw_dev:
	kfree(mdev);
	mdw_dev = NULL;
out:
	return ret;
}

static int mdw_remove(struct platform_device *pdev)
{
	struct mdw_device *mdev = platform_get_drvdata(pdev);

	mdev->dev_funcs->sw_deinit(mdev);
	mdw_dbg_deinit();
	mdw_sysfs_deinit(mdev);
	mdw_mem_deinit(mdev);
	mdw_dev_deinit(mdev);
	mdw_drv_misc_deinit(mdev);
	kfree(mdev);
	mdw_dev = NULL;
	pr_info("%s done\n", __func__);

	return 0;
}

static const struct of_device_id mdw_of_match[] = {
	{ .compatible = "mediatek, apu_mdw", },
	{},
};

static struct platform_driver mdw_driver = {
	.driver = {
		.name = "apusys",
		.owner = THIS_MODULE,
		.of_match_table = mdw_of_match,
	},
	.probe = mdw_probe,
	.remove = mdw_remove,
};

//----------------------------------------
static int mdw_rpmsg_probe(struct rpmsg_device *rpdev)
{
	struct device *dev = &rpdev->dev;
	struct mdw_device *mdev = NULL;
	int ret = 0;

	pr_info("%s+\n", __func__);

	if (mdw_dev) {
		pr_info("%s already probe\n", __func__);
		return -EBUSY;
	}
	mdev = kzalloc(sizeof(*mdev), GFP_KERNEL);
	if (!mdev)
		return -ENOMEM;

	mdev->driver_type = MDW_DRIVER_TYPE_RPMSG;
	mdev->version = 2;
	mdev->rpdev = rpdev;
	mdw_dev = mdev;
	dev_set_drvdata(dev, mdev);

	/* register misc device */
	ret = mdw_drv_misc_init(mdev);
	if (ret) {
		pr_info("%s: register misc device fail\n");
		goto delete_mdw_dev;
	}

	/* init mdw device */
	ret = mdw_dev_init(mdev);
	if (ret)
		goto unregister_misc;

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
unregister_misc:
	mdw_drv_misc_deinit(mdev);
delete_mdw_dev:
	kfree(mdev);
	mdw_dev = NULL;
out:
	return ret;
}

static void mdw_rpmsg_remove(struct rpmsg_device *rpdev)
{
	struct mdw_device *mdev = dev_get_drvdata(&rpdev->dev);

	mdev->dev_funcs->sw_deinit(mdev);
	mdw_dbg_deinit();
	mdw_sysfs_deinit(mdev);
	mdw_mem_deinit(mdev);
	mdw_dev_deinit(mdev);
	mdw_drv_misc_deinit(mdev);
	kfree(mdev);
	mdw_dev = NULL;
	pr_info("%s done\n", __func__);
}

static const struct of_device_id mdw_rpmsg_of_match[] = {
	{ .compatible = "mediatek,apu-mdw-rpmsg", },
	{ },
};

static struct rpmsg_driver mdw_rpmsg_driver = {
	.drv	= {
		.name	= "apu-mdw-rpmsg",
		.owner = THIS_MODULE,
		.of_match_table = mdw_rpmsg_of_match,
	},
	.probe = mdw_rpmsg_probe,
	.remove = mdw_rpmsg_remove,
};

//----------------------------------------
int mdw_init(struct apusys_core_info *info)
{
	int ret = 0;
	g_info = info;

	if (!mdw_pwr_check()) {
		pr_info("apusys mdw disable\n");
		return -ENODEV;
	}

	pr_info("%s register platorm...\n", __func__);
	ret = platform_driver_register(&mdw_driver);
	if (ret) {
		pr_info("failed to register apu mdw driver\n");
		goto out;
	}

	pr_info("%s register rpmsg...\n", __func__);
	ret = register_rpmsg_driver(&mdw_rpmsg_driver);
	if (ret) {
		pr_info("failed to register apu mdw rpmsg driver\n");
		platform_driver_unregister(&mdw_driver);
		goto unregister_platform_driver;
	}
	pr_info("%s init done\n", __func__);
	goto out;

unregister_platform_driver:
	platform_driver_unregister(&mdw_driver);
out:
	return ret;
}

void mdw_exit(void)
{
	unregister_rpmsg_driver(&mdw_rpmsg_driver);
	platform_driver_unregister(&mdw_driver);
	g_info = NULL;
}
