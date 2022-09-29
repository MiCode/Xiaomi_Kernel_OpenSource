// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 *
 * Author: ChenHung Yang <chenhung.yang@mediatek.com>
 */

#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/suspend.h>

#include "mtk-aov-config.h"
#include "mtk-aov-drv.h"
#include "mtk-aov-core.h"
#include "mtk-aov-aee.h"
#include "mtk-aov-data.h"
#include "mtk-aov-trace.h"

#include "mtk-vmm-notifier.h"
#include "mtk_mmdvfs.h"

static struct mtk_aov *query_aov_dev(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *aov_node;
	struct platform_device *aov_pdev;
	struct mtk_aov *aov_dev;

	dev_dbg(&pdev->dev, "%s+\n", __func__);

	aov_node = of_parse_phandle(dev->of_node, "mediatek,aov", 0);
	if (aov_node == NULL) {
		dev_info(&pdev->dev, "%s: failed to get aov node.\n", __func__);
		return NULL;
	}

	aov_pdev = of_find_device_by_node(aov_node);
	if (WARN_ON(aov_pdev == NULL) == true) {
		dev_info(&pdev->dev, "%s: failed to get aov pdev\n", __func__);
		of_node_put(aov_node);
		return NULL;
	}

	aov_dev = platform_get_drvdata(aov_pdev);
	if (aov_dev == NULL)
		dev_info(&pdev->dev, "%s aov dev is null\n", __func__);

	dev_dbg(&pdev->dev, "%s-\n", __func__);

	return aov_dev;
}

int mtk_aov_notify(struct platform_device *pdev, uint32_t notify, uint32_t status)
{
	struct mtk_aov *aov_dev;
	struct aov_notify info;
	int ret;

	AOV_TRACE_FORCE_BEGIN("AOV query device");
	aov_dev = query_aov_dev(pdev);
	AOV_TRACE_FORCE_END();
	if (aov_dev == NULL) {
		dev_info(&pdev->dev, "%s: invalid aov device\n", __func__);
		return -EIO;
	}

	info.notify = notify;
	info.status = status;

	AOV_TRACE_FORCE_BEGIN("AOV cmd notify");
	ret = aov_core_send_cmd(aov_dev, AOV_SCP_CMD_NOTIFY,
		(void *)&info, sizeof(struct aov_notify), true);
	AOV_TRACE_FORCE_END();

	return ret;
}
EXPORT_SYMBOL(mtk_aov_notify);

static inline bool mtk_aov_is_open(struct mtk_aov *aov_dev)
{
	return aov_dev->is_open;
}

static int mtk_aov_open(struct inode *inode, struct file *file)
{
	struct mtk_aov *aov_dev;

	pr_info("%s open aov driver+\n", __func__);

	aov_dev = container_of(inode->i_cdev, struct mtk_aov, aov_cdev);
	dev_dbg(aov_dev->dev, "open inode->i_cdev = 0x%p\n", inode->i_cdev);

	file->private_data = aov_dev;

	if (aov_dev->user_cnt == 0) {
		aov_dev->is_open = true;
	}
	aov_dev->user_cnt++;

	pr_info("%s open aov driver-\n", __func__);

	return 0;
}

static long mtk_aov_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	struct mtk_aov *aov_dev = (struct mtk_aov *)file->private_data;
	int ret;

	dev_dbg(aov_dev->dev, "%s ioctl aov driver(%d)+\n", __func__, cmd);

	switch (cmd) {
	case AOV_DEV_START: {
		dev_info(aov_dev->dev, "AOV start+\n");
		vmm_isp_ctrl_notify(1);
		mtk_mmdvfs_aov_enable(1);

		AOV_TRACE_FORCE_BEGIN("AOV start");
		ret = aov_core_send_cmd(aov_dev, AOV_SCP_CMD_START,
			(void *)arg, sizeof(struct aov_user), true);
		AOV_TRACE_END();
		if (ret < 0) {
			vmm_isp_ctrl_notify(0);
			mtk_mmdvfs_aov_enable(0);
		}

		dev_info(aov_dev->dev, "AOV start-(%d)\n", ret);
		break;
	}
	case AOV_DEV_SENSOR_ON:
		dev_dbg(aov_dev->dev, "AOV sensor on\n+");

		AOV_TRACE_FORCE_BEGIN("AOV sensor on");
		ret = aov_core_notify(aov_dev, (void *)arg, true);
		AOV_TRACE_FORCE_END();

		dev_dbg(aov_dev->dev, "AOV sensor on(%d)\n+", ret);
		break;
	case AOV_DEV_SENSOR_OFF:
		dev_dbg(aov_dev->dev, "AOV sensor off\n+");

		AOV_TRACE_FORCE_BEGIN("AOV sensor off");
		ret = aov_core_notify(aov_dev, (void *)arg, true);
		AOV_TRACE_FORCE_END();

		dev_dbg(aov_dev->dev, "AOV sensor off(%d)\n+", ret);
		break;
	case AOV_DEV_DQEVENT:
		dev_dbg(aov_dev->dev, "AOV dqevent+\n");

		AOV_TRACE_FORCE_BEGIN("AOV dqevent");
		ret = aov_core_copy(aov_dev, (struct aov_dqevent *)arg);
		AOV_TRACE_FORCE_END();

		dev_dbg(aov_dev->dev, "AOV dqevent-(%d)\n", ret);
		break;
	case AOV_DEV_STOP:
		dev_info(aov_dev->dev, "AOV stop+\n");

		AOV_TRACE_FORCE_BEGIN("AOV stop");
		ret = aov_core_send_cmd(aov_dev, AOV_SCP_CMD_STOP, NULL, 0, true);
		AOV_TRACE_FORCE_END();
		if (ret >= 0) {
			dev_info(aov_dev->dev, "AOV disable vmm+\n");
			vmm_isp_ctrl_notify(0);
			mtk_mmdvfs_aov_enable(0);
			dev_info(aov_dev->dev, "AOV disable vmm-\n");
		}
		dev_info(aov_dev->dev, "AOV stop-(%d)\n", ret);
		break;
	default:
		dev_info(aov_dev->dev, "unknown AOV control code(%d)\n", cmd);
		return -EINVAL;
	}

	dev_dbg(aov_dev->dev, "%s ioctl aov driver(cmd)-(%d)\n", __func__, cmd, ret);

	return ret;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long mtk_aov_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	struct mtk_aov *aov_dev = (struct mtk_aov *)file->private_data;
	long ret = -1;

	switch (cmd) {
	case COMPAT_AOV_DEV_START:
	case COMPAT_AOV_DEV_DQEVENT:
	case COMPAT_AOV_DEV_SENSOR_ON:
	case COMPAT_AOV_DEV_SENSOR_OFF:
	case COMPAT_AOV_DEV_STOP:
		ret = file->f_op->unlocked_ioctl(file, cmd, arg);
		break;
	default:
		dev_info(aov_dev->dev, "Invalid cmd_number 0x%x.\n", cmd);
		break;
	}

	return ret;
}
#endif

static unsigned int mtk_aov_poll(struct file *file, poll_table *wait)
{
	struct mtk_aov *aov_dev = (struct mtk_aov *)file->private_data;

	return aov_core_poll(aov_dev, file, wait);
}

static int mtk_aov_release(struct inode *inode, struct file *file)
{
	struct mtk_aov *aov_dev = (struct mtk_aov *)file->private_data;
	int ret;

	pr_info("%s release aov driver+\n", __func__);

	ret = aov_core_reset(aov_dev);
	if (ret > 0) {
		dev_info(aov_dev->dev, "AOV force disable vmm+\n");
		vmm_isp_ctrl_notify(0);
		mtk_mmdvfs_aov_enable(0);
		dev_info(aov_dev->dev, "AOV force disable vmm-\n");
	}

	aov_dev->user_cnt--;
	if (aov_dev->user_cnt == 0)
		aov_dev->is_open = false;

	pr_info("%s release aov driver-\n", __func__);

	return 0;
}

static const struct file_operations aov_fops = {
	.owner          = THIS_MODULE,
	.open           = mtk_aov_open,
	.unlocked_ioctl = mtk_aov_ioctl,
	.poll           = mtk_aov_poll,
	.release        = mtk_aov_release,

#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl   = mtk_aov_compat_ioctl,
#endif
};



static int mtk_aov_probe(struct platform_device *pdev)
{
	struct mtk_aov *aov_dev;
	int ret = 0;

	dev_info(&pdev->dev, "%s probe aov driver+\n", __func__);

	aov_dev = devm_kzalloc(&pdev->dev, sizeof(*aov_dev), GFP_KERNEL);
	if (aov_dev == NULL)
		return -ENOMEM;

	aov_dev->is_open = false;
	aov_dev->user_cnt = 0;

	aov_dev->dev = &pdev->dev;

	if (pdev->dev.of_node) {
		of_property_read_u32(pdev->dev.of_node, "op_mode", &(aov_dev->op_mode));

		dev_info(&pdev->dev, "%s aov mode(%d)\n", __func__, aov_dev->op_mode);
	} else {
		aov_dev->op_mode = 0;

		dev_info(&pdev->dev, "%s null of node\n", __func__);
	}

	aov_core_init(aov_dev);
	aov_aee_init(aov_dev);

	platform_set_drvdata(pdev, aov_dev);
	dev_set_drvdata(&pdev->dev, aov_dev);

	/* init character device */
	ret = alloc_chrdev_region(&aov_dev->aov_devno, 0, 1, AOV_DEVICE_NAME);
	if (ret < 0) {
		dev_info(&pdev->dev, "alloc_chrdev_region failed err= %d", ret);
		goto err_alloc;
	}

	cdev_init(&aov_dev->aov_cdev, &aov_fops);
	aov_dev->aov_cdev.owner = THIS_MODULE;

	ret = cdev_add(&aov_dev->aov_cdev, aov_dev->aov_devno, 1);
	if (ret < 0) {
		dev_info(&pdev->dev, "cdev_add fail  err= %d", ret);
		goto err_add;
	}

	aov_dev->aov_class = class_create(THIS_MODULE, "mtk_aov_driver");
	if (IS_ERR(aov_dev->aov_class) == true) {
		ret = (int)PTR_ERR(aov_dev->aov_class);
		dev_info(&pdev->dev, "class create fail  err= %d", ret);
		goto err_add;
	}

	aov_dev->aov_device = device_create(aov_dev->aov_class, NULL,
		aov_dev->aov_devno, NULL, AOV_DEVICE_NAME);
	if (IS_ERR(aov_dev->aov_device) == true) {
		ret = (int)PTR_ERR(aov_dev->aov_device);
		dev_info(&pdev->dev, "device create fail  err= %d", ret);
		goto err_device;
	}
	dev_info(&pdev->dev, "%s probe aov driver-\n", __func__);
	return 0;

err_device:
	class_destroy(aov_dev->aov_class);

err_add:
	cdev_del(&aov_dev->aov_cdev);

err_alloc:
	unregister_chrdev_region(aov_dev->aov_devno, 1);

	devm_kfree(&pdev->dev, aov_dev);

	dev_info(&pdev->dev, "- X. aov driver probe fail.\n");

	return ret;
}

static int mtk_aov_remove(struct platform_device *pdev)
{
	struct mtk_aov *aov_dev = platform_get_drvdata(pdev);

	pr_info("%s remove aov driver+\n", __func__);

	if (mtk_aov_is_open(aov_dev) == true) {
		aov_dev->is_open = false;
		dev_dbg(&pdev->dev, "%s: opened device found\n", __func__);
	}

	cdev_del(&aov_dev->aov_cdev);
	unregister_chrdev_region(aov_dev->aov_devno, 1);

	aov_aee_uninit(aov_dev);
	aov_core_uninit(aov_dev);

	devm_kfree(&pdev->dev, aov_dev);

	pr_info("%s remove aov driver-\n", __func__);

	return 0;
}

static int aov_runtime_suspend(struct device *dev)
{
	struct mtk_aov *aov_dev = dev_get_drvdata(dev);

	dev_dbg(aov_dev->dev, "%s runtime suspend+\n", __func__);

#if AOV_WAIT_POWER_ACK
	(void)aov_core_send_cmd(aov_dev, AOV_SCP_CMD_PWR_OFF, NULL, 0, true);
#else
	(void)aov_core_send_cmd(aov_dev, AOV_SCP_CMD_PWR_OFF, NULL, 0, false);
#endif  // AOV_WAIT_POWER_ACK

	dev_dbg(aov_dev->dev, "%s runtime suspend-\n", __func__);

	return 0;
}

static int aov_runtime_resume(struct device *dev)
{
	struct mtk_aov *aov_dev = dev_get_drvdata(dev);

	dev_dbg(aov_dev->dev, "%s runtime resume+\n", __func__);

#if AOV_WAIT_POWER_ACK
	(void)aov_core_send_cmd(aov_dev, AOV_SCP_CMD_PWR_ON, NULL, 0, true);
#else
	(void)aov_core_send_cmd(aov_dev, AOV_SCP_CMD_PWR_ON, NULL, 0, false);
#endif  // AOV_WAIT_POWER_ACK

	dev_dbg(aov_dev->dev, "%s runtime resume-\n", __func__);

	return 0;
}

static const struct dev_pm_ops mtk_aov_pm_ops = {
	.suspend_noirq = aov_runtime_suspend,
	.resume_noirq = aov_runtime_resume,

};

static const struct of_device_id mtk_aov_of_match[] = {
	{ .compatible = "mediatek,aov", },
	{}
};
MODULE_DEVICE_TABLE(of, mtk_aov_of_match);

static struct platform_driver mtk_aov_driver = {
	.probe  = mtk_aov_probe,
	.remove = mtk_aov_remove,
	.driver = {
		.name = AOV_DEVICE_NAME,
		.owner = THIS_MODULE,
		.pm = &mtk_aov_pm_ops,
		.of_match_table = mtk_aov_of_match,
	},
};

module_platform_driver(mtk_aov_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek AOV process driver");
