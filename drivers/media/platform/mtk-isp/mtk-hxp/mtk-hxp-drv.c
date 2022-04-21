// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/suspend.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>

#include "mtk-hxp-config.h"
#include "mtk-hxp-drv.h"
#include "mtk-hxp-core.h"
#include "mtk-hxp-aee.h"
#include "mtk-hxp-aov.h"

static inline bool mtk_hxp_is_open(struct mtk_hxp *hxp_dev)
{
	return hxp_dev->is_open;
}

struct platform_device *mtk_hxp_get_plat_device(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *hxp_node;
	struct platform_device *hxp_pdev;

	dev_dbg(&pdev->dev, "- E. hxp get platform device.\n");

	hxp_node = of_parse_phandle(dev->of_node, "mediatek,hxp", 0);
	if (hxp_node == NULL) {
		dev_info(&pdev->dev, "%s can't get hxp node.\n", __func__);
		return NULL;
	}

	hxp_pdev = of_find_device_by_node(hxp_node);
	if (WARN_ON(hxp_pdev == NULL) == true) {
		dev_info(&pdev->dev, "%s hxp pdev failed.\n", __func__);
		of_node_put(hxp_node);
		return NULL;
	}

	return hxp_pdev;
}
EXPORT_SYMBOL(mtk_hxp_get_plat_device);

static int mtk_hxp_open(struct inode *inode, struct file *file)
{
	struct mtk_hxp *hxp_dev;

	pr_info("%s open hxp driver+\n", __func__);

	hxp_dev = container_of(inode->i_cdev, struct mtk_hxp, hcp_cdev);
	dev_dbg(hxp_dev->dev, "open inode->i_cdev = 0x%p\n", inode->i_cdev);

	file->private_data = hxp_dev;

	hxp_dev->is_open = true;

	pr_info("%s open hxp driver-\n", __func__);

	return 0;
}

static long mtk_hxp_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	struct mtk_hxp *hxp_dev = (struct mtk_hxp *)file->private_data;
	int ret;

	dev_info(hxp_dev->dev, "%s ioctl hxp driver(%d)+\n", __func__, cmd);

	switch (cmd) {
	case HXP_AOV_INIT: {
		dev_info(hxp_dev->dev, "AOV init+\n");
		ret = hxp_core_send_cmd(hxp_dev, HXP_AOV_CMD_INIT,
			(void *)arg, sizeof(struct aov_user), false);
		dev_info(hxp_dev->dev, "AOV init-(%d)\n", ret);
		break;
	}
	case HXP_AOV_DQEVENT:
		dev_info(hxp_dev->dev, "AOV dqevent+\n");
		ret = hxp_core_copy(hxp_dev, (struct aov_dqevent *)arg);
		dev_info(hxp_dev->dev, "AOV dqevent-(%d)\n", ret);
		break;
	case HXP_AOV_DEINIT: {
		dev_info(hxp_dev->dev, "AOV deinit+\n");
		ret = hxp_core_send_cmd(hxp_dev, HXP_AOV_CMD_DEINIT, NULL, 0, false);
		dev_info(hxp_dev->dev, "AOV deinit-(%d)\n", ret);
		break;
	}
	default:
		dev_info(hxp_dev->dev, "Unknown AOV control code(%d)\n", cmd);
		return -EINVAL;
	}

	dev_info(hxp_dev->dev, "%s ioctl hxp driver(cmd)-(%d)\n", __func__, cmd, ret);

	return ret;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long mtk_hxp_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	struct mtk_hxp *hxp_dev = (struct mtk_hxp *)file->private_data;
	long ret = -1;

	switch (cmd) {
	case COMPAT_HXP_AOV_INIT:
	case COMPAT_HXP_AOV_DQEVENT:
	case COMPAT_HXP_AOV_DEINIT:
		ret = file->f_op->unlocked_ioctl(file, cmd, arg);
		break;
	default:
		dev_info(hxp_dev->dev, "Invalid cmd_number 0x%x.\n", cmd);
		break;
	}

	return ret;
}
#endif

static unsigned int mtk_hxp_poll(struct file *file, poll_table *wait)
{
	struct mtk_hxp *hxp_dev = (struct mtk_hxp *)file->private_data;

	return hxp_core_poll(hxp_dev, file, wait);
}

static int mtk_hxp_release(struct inode *inode, struct file *file)
{
	struct mtk_hxp *hxp_dev = (struct mtk_hxp *)file->private_data;

	pr_info("%s release hxp driver+\n", __func__);

	hxp_dev->is_open = false;

	//hxp_ctrl_reset(hxp_dev);

	pr_info("%s release hxp driver-\n", __func__);

	return 0;
}

static const struct file_operations hxp_fops = {
	.owner          = THIS_MODULE,
	.open           = mtk_hxp_open,
	.unlocked_ioctl = mtk_hxp_ioctl,
	.poll           = mtk_hxp_poll,
	.release        = mtk_hxp_release,

#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl   = mtk_hxp_compat_ioctl,
#endif
};

static int mtk_hxp_probe(struct platform_device *pdev)
{
	struct mtk_hxp *hxp_dev;
	int ret = 0;

	dev_info(&pdev->dev, "%s probe hxp driver+\n", __func__);

	hxp_dev = devm_kzalloc(&pdev->dev, sizeof(*hxp_dev), GFP_KERNEL);
	if (hxp_dev == NULL)
		return -ENOMEM;

	hxp_dev->is_open = false;

	hxp_dev->dev = &pdev->dev;

	hxp_core_init(hxp_dev);

	//hxp_aee_init(hxp_dev);

	//hxp_ctrl_init(hxp_dev);

	platform_set_drvdata(pdev, hxp_dev);
	dev_set_drvdata(&pdev->dev, hxp_dev);

	/* init character device */
	ret = alloc_chrdev_region(&hxp_dev->hcp_devno, 0, 1, HXP_DEVICE_NAME);
	if (ret < 0) {
		dev_info(&pdev->dev, "alloc_chrdev_region failed err= %d", ret);
		goto err_alloc;
	}

	cdev_init(&hxp_dev->hcp_cdev, &hxp_fops);
	hxp_dev->hcp_cdev.owner = THIS_MODULE;

	ret = cdev_add(&hxp_dev->hcp_cdev, hxp_dev->hcp_devno, 1);
	if (ret < 0) {
		dev_info(&pdev->dev, "cdev_add fail  err= %d", ret);
		goto err_add;
	}

	hxp_dev->hcp_class = class_create(THIS_MODULE, "mtk_hxp_driver");
	if (IS_ERR(hxp_dev->hcp_class) == true) {
		ret = (int)PTR_ERR(hxp_dev->hcp_class);
		dev_info(&pdev->dev, "class create fail  err= %d", ret);
		goto err_add;
	}

	hxp_dev->hcp_device = device_create(hxp_dev->hcp_class, NULL,
		hxp_dev->hcp_devno, NULL, HXP_DEVICE_NAME);
	if (IS_ERR(hxp_dev->hcp_device) == true) {
		ret = (int)PTR_ERR(hxp_dev->hcp_device);
		dev_info(&pdev->dev, "device create fail  err= %d", ret);
		goto err_device;
	}

	dev_info(&pdev->dev, "%s probe hxp driver-\n", __func__);

	return 0;

err_device:
	class_destroy(hxp_dev->hcp_class);

err_add:
	cdev_del(&hxp_dev->hcp_cdev);

err_alloc:
	unregister_chrdev_region(hxp_dev->hcp_devno, 1);

	devm_kfree(&pdev->dev, hxp_dev);

	dev_info(&pdev->dev, "- X. hcp driver probe fail.\n");

	return ret;
}

static int mtk_hxp_remove(struct platform_device *pdev)
{
	struct mtk_hxp *hxp_dev = platform_get_drvdata(pdev);

	pr_info("%s remove hxp driver+\n", __func__);

	if (mtk_hxp_is_open(hxp_dev) == true) {
		hxp_dev->is_open = false;
		dev_dbg(&pdev->dev, "%s: opened device found\n", __func__);
	}

	cdev_del(&hxp_dev->hcp_cdev);
	unregister_chrdev_region(hxp_dev->hcp_devno, 1);

	//hxp_ctrl_uninit(hxp_dev);

	//hxp_aee_uninit(hxp_dev);

	hxp_core_uninit(hxp_dev);

	devm_kfree(&pdev->dev, hxp_dev);

	pr_info("%s remove hxp driver-\n", __func__);

	return 0;
}

static int hxp_runtime_suspend(struct device *dev)
{
	struct mtk_hxp *hxp_dev = dev_get_drvdata(dev);
	//int i, ret;

	dev_info(hxp_dev->dev, "%s runtime suspend+\n", __func__);

#if HXP_WAIT_POWER_ACK
	(void)hxp_core_send_cmd(hxp_dev, HXP_AOV_CMD_PWR_OFF, NULL, 0, true);
#else
	(void)hxp_core_send_cmd(hxp_dev, HXP_AOV_CMD_PWR_OFF, NULL, 0, false);
#endif  // HXP_WAIT_POWER_ACK

	dev_info(hxp_dev->dev, "%s runtime suspend-\n", __func__);

	return 0;
}

static int hxp_runtime_resume(struct device *dev)
{
	struct mtk_hxp *hxp_dev = dev_get_drvdata(dev);

	dev_info(hxp_dev->dev, "%s runtime resume+\n", __func__);

#if HXP_WAIT_POWER_ACK
	(void)hxp_core_send_cmd(hxp_dev, HXP_AOV_CMD_PWR_ON, NULL, 0, true);
#else
	(void)hxp_core_send_cmd(hxp_dev, HXP_AOV_CMD_PWR_ON, NULL, 0, false);
#endif  // HXP_WAIT_POWER_ACK

	dev_info(hxp_dev->dev, "%s runtime resume-\n", __func__);

	return 0;
}

static const struct dev_pm_ops mtk_hxp_pm_ops = {
	.suspend_late = hxp_runtime_suspend,
	.resume_early = hxp_runtime_resume,
};

static const struct of_device_id mtk_hxp_of_match[] = {
	{ .compatible = "mediatek,hxp", },
	{}
};
MODULE_DEVICE_TABLE(of, mtk_hxp_of_match);

static struct platform_driver mtk_hxp_driver = {
	.probe  = mtk_hxp_probe,
	.remove = mtk_hxp_remove,
	.driver = {
		.name = HXP_DEVICE_NAME,
		.owner = THIS_MODULE,
		.pm = &mtk_hxp_pm_ops,
		.of_match_table = mtk_hxp_of_match,
	},
};

// module_platform_driver(mtk_hxp_driver);

#if HXP_BUILD_FOR_FPGA
static int __init mtk_hxp_init(void)
{
	pr_info("%s+", __func__);

	if (platform_driver_register(&mtk_hxp_driver)) {
		pr_notice("%s: failed to register hxp driver\n", __func__);
		return -1;
	}

	pr_info("%s-", __func__);

	return 0;
}

static void __exit mtk_hxp_deinit(void)
{
	platform_driver_unregister(&mtk_hxp_driver);
}
#endif // HXP_BUILD_FOR_FPGA

late_initcall_sync(mtk_hxp_init);
module_exit(mtk_hxp_deinit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek hetero control process driver");
