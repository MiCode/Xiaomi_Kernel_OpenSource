// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/of_device.h>
#include <linux/types.h>

#include <apusys_device.h>

#include <common/mdla_driver.h>
#include <common/mdla_device.h>
#include <common/mdla_power_ctrl.h>
#include <common/mdla_cmd_proc.h>
#include <common/mdla_ioctl.h>

#include <utilities/mdla_profile.h>
#include <utilities/mdla_util.h>
#ifdef IS_KERNEL_4_14
#include <linux/dma-mapping.h>
#else
#include <linux/dma-direct.h>
#endif


static struct apusys_device *apusys_dev_mdla;
static struct apusys_device *apusys_dev_mdla_rt;

static bool apusys_mdla_rt_support(void)
{
	return DEVICE_MDLA != DEVICE_MDLA_RT;
}

static int apusys_mdla_handler(int type,
	void *hnd, struct apusys_device *dev)
{
	int ret = 0;
	struct mdla_dev *mdla_info;
	struct apusys_cmd_hnd *cmd_hnd;
	struct mdla_run_cmd_sync *cmd_data;

	if (unlikely(!dev || !(dev->private)))
		return -EINVAL;

	mdla_info = (struct mdla_dev *)dev->private;

	if (dev->dev_type != DEVICE_MDLA)
		return -EINVAL;

	if (unlikely(mdla_info->mdla_id >= mdla_util_get_core_num()))
		return -EINVAL;

	switch (type) {
	case APUSYS_CMD_POWERON:
		ret = mdla_pwr_ops_get()->on(mdla_info->mdla_id, true);
		break;
	case APUSYS_CMD_POWERDOWN:
		ret = mdla_pwr_ops_get()->off(mdla_info->mdla_id, 0, true);
		break;
	case APUSYS_CMD_RESUME:
		break;
	case APUSYS_CMD_SUSPEND:
		ret = mdla_pwr_ops_get()->off(mdla_info->mdla_id, 1, true);
		break;
	case APUSYS_CMD_EXECUTE:
		cmd_hnd = hnd;
		cmd_data = (struct mdla_run_cmd_sync *)cmd_hnd->kva;

		if (unlikely(!cmd_hnd || !cmd_data))
			return -ENODEV;

		ret = mdla_cmd_ops_get()->run_sync(
			cmd_data,
			mdla_info,
			cmd_hnd,
			MDLA_LOW_PRIORITY);
		break;
	case APUSYS_CMD_PREEMPT:
		ret = -EINVAL;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int apusys_mdla_rt_handler(int type,
	void *hnd, struct apusys_device *dev)
{
	int ret;
	struct apusys_cmd_hnd *cmd_hnd;
	struct mdla_dev *mdla_info;
	struct mdla_run_cmd_sync *cmd_data;

	if (type != APUSYS_CMD_EXECUTE)
		return 0;

	cmd_hnd = hnd;

	if (unlikely(!cmd_hnd || !dev))
		return -EINVAL;

	cmd_data = (struct mdla_run_cmd_sync *)cmd_hnd->kva;
	mdla_info = (struct mdla_dev *)dev->private;

	if (unlikely(!mdla_info || !cmd_data))
		return -ENODEV;

	if (dev->dev_type != DEVICE_MDLA_RT)
		return -EINVAL;

	if (unlikely(mdla_info->mdla_id >= mdla_util_get_core_num()))
		return -EINVAL;

	ret = mdla_cmd_ops_get()->run_sync(
			cmd_data,
			mdla_info,
			cmd_hnd,
			MDLA_HIGH_PRIORITY);

	return ret;
}

#define MDLA_DEVICE_NAME "mdlactl"
#define MDLA_CLASS_NAME  "mdla"

static dev_t mdlactl_dev_num;
static struct cdev *mdlactl_cdev;
static struct class *mdlactl_class;
static struct device *mdlactl_device;

static void mdla_drv_unregister_char_dev(void)
{
	cdev_del(mdlactl_cdev);
	unregister_chrdev_region(mdlactl_dev_num, 1);
}

static int mdla_drv_register_char_dev(struct device *dev)
{
	int ret = 0;

	/* Register a range of char device numbers */
	ret = alloc_chrdev_region(&mdlactl_dev_num, 0, 1, MDLA_DEVICE_NAME);
	if (ret < 0) {
		dev_info(dev, "alloc_chrdev_region failed, %d\n", ret);
		goto out;
	}

	/* Allocate a char device structure */
	mdlactl_cdev = cdev_alloc();
	if (!mdlactl_cdev) {
		dev_info(dev, "cdev_alloc failed\n");
		ret = -ENOMEM;
		goto err_cdev;
	}

	cdev_init(mdlactl_cdev, mdla_ioctl_get_fops());

	mdlactl_cdev->owner = THIS_MODULE;

	/* Add a char device to the system */
	ret = cdev_add(mdlactl_cdev, mdlactl_dev_num, 1);
	if (ret < 0) {
		dev_info(dev, "Attatch file operation failed, %d\n", ret);
		goto err_cdev_add;
	}

	dev_info(dev, "Registered cdev with major/minor number %d\n",
			mdlactl_dev_num);

	return 0;

err_cdev_add:
	cdev_del(mdlactl_cdev);
err_cdev:
	unregister_chrdev_region(mdlactl_dev_num, 1);
out:
	return ret;
}

int mdla_drv_create_device_node(struct device *dev)
{
	int ret = 0;

	if (mdlactl_cdev) {
		ret = -1;
		dev_info(dev, "%s() Has registered character device!\n",
					__func__);
		goto out;
	}

	/* 1. Register character driver */
	ret = mdla_drv_register_char_dev(dev);
	if (ret < 0)
		goto out;

	/* 2. Create a class structure. It's used in calls to device_create() */
	mdlactl_class = class_create(THIS_MODULE, MDLA_CLASS_NAME);
	if (IS_ERR(mdlactl_class)) {
		dev_info(dev, "Failed to register device class\n");
		ret = PTR_ERR(mdlactl_class);
		goto err_class;
	}

	/* 3. Creates a device and registers it with sysfs */
	mdlactl_device = device_create(mdlactl_class, NULL,
				mdlactl_dev_num, NULL, MDLA_DEVICE_NAME);
	if (IS_ERR(mdlactl_device)) {
		dev_info(dev, "Failed to create the device\n");
		ret = PTR_ERR(mdlactl_device);
		goto err_devive;
	}

	/* 4. Init DMA from of */
#ifdef IS_KERNEL_4_14
	of_dma_configure(mdlactl_device, NULL);
#else
	of_dma_configure(mdlactl_device, NULL, true);
#endif

	/* 5. Set DMA mask */
	ret = dma_get_mask(mdlactl_device);
	if (ret < 0 || ret != DMA_BIT_MASK(32)) {
		ret = dma_set_mask_and_coherent(mdlactl_device,
					DMA_BIT_MASK(32));
		if (ret)
			dev_info(dev, "MDLA: set DMA mask failed: %d\n", ret);
	}

	return 0;

err_devive:
	class_destroy(mdlactl_class);
err_class:
	mdla_drv_unregister_char_dev();
out:
	return ret;
}

void mdla_drv_destroy_device_node(void)
{
	if (mdlactl_device) {
		device_destroy(mdlactl_class, mdlactl_dev_num);
		mdlactl_device = NULL;
	}

	if (mdlactl_class) {
		class_destroy(mdlactl_class);
		mdlactl_class = NULL;
	}

	if (mdlactl_cdev) {
		mdla_drv_unregister_char_dev();
		mdlactl_cdev = NULL;
	}
}

static int mdla_probe(struct platform_device *pdev)
{
	int i, ret = 0;
	struct device *dev = &pdev->dev;
	struct apusys_device *adev_mdla;
	struct apusys_device *adev_mdla_rt;
	struct mdla_dev *mdev;

	if (mdla_pwr_apusys_disabled())
		return -1;

	/* Initialize platform to allocate mdla devices first. */
	ret = mdla_util_plat_init(pdev);

	if (ret < 0) {
		dev_info(dev, "platform init failed\n");
		return -EINVAL;
	} else if (ret == 1) {
		dev_info(dev, "%s: uP version done\n", __func__);
		return 0;
	}

	apusys_dev_mdla = kcalloc(mdla_util_get_core_num(),
					sizeof(struct apusys_device),
					GFP_KERNEL);
	if (!apusys_dev_mdla) {
		ret = -ENOMEM;
		goto err;
	}

	apusys_dev_mdla_rt = kcalloc(mdla_util_get_core_num(),
					sizeof(struct apusys_device),
					GFP_KERNEL);
	if (!apusys_dev_mdla_rt) {
		ret = -ENOMEM;
		goto err_dev_mdla_rt;
	}

	for_each_mdla_core(i) {
		mdev = mdla_get_device(i);
		adev_mdla = &apusys_dev_mdla[i];
		adev_mdla_rt = &apusys_dev_mdla_rt[i];

		adev_mdla->dev_type = DEVICE_MDLA;
		adev_mdla->preempt_type = APUSYS_PREEMPT_NONE;
		adev_mdla->private = mdev;
		adev_mdla->send_cmd = apusys_mdla_handler;

		ret = apusys_register_device(adev_mdla);
		if (ret) {
			dev_info(dev, "register apusys mdla %d fail\n", i);
			goto err_apusys;
		}

		if (!apusys_mdla_rt_support())
			continue;

		adev_mdla_rt->dev_type = DEVICE_MDLA_RT;
		adev_mdla_rt->preempt_type = APUSYS_PREEMPT_NONE;
		adev_mdla_rt->private = mdev;
		adev_mdla_rt->send_cmd = apusys_mdla_rt_handler;

		ret = apusys_register_device(adev_mdla_rt);

		if (ret) {
			dev_info(dev, "register apusys mdla RT %d fail\n", i);
			apusys_unregister_device(adev_mdla);
			goto err_apusys;
		}
	}

	dev_info(dev, "%s: done\n", __func__);

	return 0;


err_apusys:
	for (i = i - 1; i >= 0; i--) {
		apusys_unregister_device(&apusys_dev_mdla[i]);
		if (apusys_mdla_rt_support())
			apusys_unregister_device(&apusys_dev_mdla_rt[i]);
	}
	kfree(apusys_dev_mdla_rt);
err_dev_mdla_rt:
	kfree(apusys_dev_mdla);
err:
	mdla_util_plat_deinit(pdev);
	return ret;
}

static int mdla_remove(struct platform_device *pdev)
{
	int i;

	if (mdla_pwr_apusys_disabled())
		return 0;

	dev_info(&pdev->dev, "%s start -\n", __func__);

	for_each_mdla_core(i) {
		apusys_unregister_device(&apusys_dev_mdla[i]);
		if (apusys_mdla_rt_support())
			apusys_unregister_device(&apusys_dev_mdla_rt[i]);
	}

	kfree(apusys_dev_mdla_rt);
	kfree(apusys_dev_mdla);

	mdla_util_plat_deinit(pdev);

	platform_set_drvdata(pdev, NULL);

	dev_info(&pdev->dev, "%s done -\n", __func__);

	return 0;
}

static int mdla_resume(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s()\n", __func__);
	return 0;
}

static int mdla_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	int i;

	for_each_mdla_core(i)
		mdla_pwr_ops_get()->off(i, 1, true);

	dev_info(&pdev->dev, "%s()\n", __func__);
	return 0;
}

static struct platform_driver mdla_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
	},
	.probe = mdla_probe,
	.remove = mdla_remove,
	.suspend = mdla_suspend,
	.resume = mdla_resume,
};

int mdla_drv_init(void)
{
	mdla_driver.driver.of_match_table = mdla_util_get_device_id();

	return platform_driver_register(&mdla_driver);
}

void mdla_drv_exit(void)
{
	platform_driver_unregister(&mdla_driver);
}

