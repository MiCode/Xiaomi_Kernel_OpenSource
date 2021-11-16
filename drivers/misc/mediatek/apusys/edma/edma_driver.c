/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: JB Tsai <jb.tsai@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/platform_device.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/interrupt.h>
#include <linux/syscalls.h>
#include <linux/cdev.h>
#include <linux/kthread.h>

#include "edma_dbgfs.h"
#include "edma_driver.h"
#include "edma_cmd_hnd.h"
#include "edma_queue.h"
#include "apusys_power.h"

#define EDMA_DEV_NAME		"edma"

static struct class *edma_class;

static int edma_init_queue_task(struct edma_sub *edma_sub)
{
#ifdef EDMA_IOCTRL

	edma_sub->enque_task = kthread_create(edma_enque_routine_loop,
					      edma_sub,
					      edma_sub->sub_name);
	if (IS_ERR(edma_sub->enque_task)) {
		edma_sub->enque_task = NULL;
		dev_notice(edma_sub->dev, "create enque_task kthread error.\n");
		return -ENOENT;
	}
	wake_up_process(edma_sub->enque_task);
#endif
	return 0;
}

int edma_initialize(struct edma_device *edma_device)
{
	int ret = 0, len = 0;
	int sub_id;

	init_waitqueue_head(&edma_device->req_wait);

	edma_device->power_timer.data = (unsigned long)edma_device;
	edma_device->power_timer.function =
				edma_power_time_up;
	init_timer(&edma_device->power_timer);

	INIT_WORK(&edma_device->power_off_work,
				edma_start_power_off);

	edma_device->dbg_cfg = 0;
	//real pwr state
	edma_device->power_state = EDMA_POWER_OFF;

	/* init hw and create task */
	for (sub_id = 0; sub_id < edma_device->edma_sub_num; sub_id++) {
		struct edma_sub *edma_sub = edma_device->edma_sub[sub_id];

		if (!edma_sub)
			continue;

		edma_sub->edma_device = edma_device;
		edma_sub->sub = sub_id;
		edma_sub->power_state = EDMA_POWER_OFF;
		mutex_init(&edma_sub->cmd_mutex);
		init_waitqueue_head(&edma_sub->cmd_wait);
		len = sprintf(edma_sub->sub_name, "edma%d", edma_sub->sub);
		if (len < 0)
			pr_notice("fail to copy to sub_name, lens = %d\n", len);

		ret = edma_init_queue_task(edma_sub);
	}

	return ret;
}

static int edma_open(struct inode *inode, struct file *flip)
{
	int ret = 0;
	struct edma_user *user = NULL;
	struct edma_device *edma_device;

	edma_device =
	    container_of(inode->i_cdev, struct edma_device, edma_chardev);

	edma_create_user(&user, edma_device);
	if (IS_ERR_OR_NULL(user)) {
		pr_notice("fail to create user\n");
		return -ENOMEM;
	}

	flip->private_data = user;

	return ret;
}

static int edma_release(struct inode *inode, struct file *flip)
{
	struct edma_user *user = flip->private_data;
	struct edma_device *edma_device;

	if (user) {
		edma_device = dev_get_drvdata(user->dev);
		edma_delete_user(user, edma_device);
	} else {
		pr_notice("delete empty user!\n");
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations edma_fops = {
	.owner = THIS_MODULE,
	.open = edma_open,
	.release = edma_release,
#ifdef EDMA_IOCTRL
#ifdef CONFIG_COMPAT
	.unlocked_ioctl = edma_ioctl
#endif
#endif
};

static inline void edma_unreg_chardev(struct edma_device *edma_device)
{
	cdev_del(&edma_device->edma_chardev);
	unregister_chrdev_region(edma_device->edma_devt, 1);
}

static inline int edma_reg_chardev(struct edma_device *edma_device)
{
	int ret = 0;

	ret = alloc_chrdev_region(&edma_device->edma_devt, 0, 1, EDMA_DEV_NAME);
	if ((ret) < 0) {
		pr_notice("alloc_chrdev_region failed, %d\n", ret);
		return ret;
	}

	/* Attatch file operation. */
	cdev_init(&edma_device->edma_chardev, &edma_fops);
	edma_device->edma_chardev.owner = THIS_MODULE;

	/* Add to system */
	ret = cdev_add(&edma_device->edma_chardev, edma_device->edma_devt, 1);
	if ((ret) < 0) {
		pr_notice("Attatch file operation failed, %d\n", ret);
		goto out;
	}

out:
	if (ret < 0)
		edma_unreg_chardev(edma_device);

	return ret;
}

static const struct of_device_id mtk_edma_sub_of_ids[] = {
	{.compatible = "mtk,edma-sub",    NULL},
	{}
};

int edma_send_cmd(int cmd, void *hnd, struct apusys_device *adev)
{
	struct edma_sub *edma_sub;
	int result = 0;

	if (adev == NULL)
		return -EINVAL;

	edma_sub = (struct edma_sub *)adev->private;

#ifdef DEBUG
		LOG_DBG("%s:cmd = %d, name = %s\n", __func__,
		cmd, edma_sub->sub_name);
#endif

	switch (cmd) {
	case APUSYS_CMD_POWERON:
		/*pre-power on*/
		return edma_power_on(edma_sub);
	case APUSYS_CMD_POWERDOWN:
		//return edma_power_off(edma_sub, 1);
		break;
	case APUSYS_CMD_RESUME:
		return result;
	case APUSYS_CMD_SUSPEND:
		return edma_power_off(edma_sub, 1);
	case APUSYS_CMD_EXECUTE:{
			struct apusys_cmd_hnd *cmd_hnd;
			struct edma_ext *edma_ext;

			if (hnd == NULL)
				break;

			cmd_hnd = (struct apusys_cmd_hnd *)hnd;
			if (cmd_hnd->kva == 0 ||
				cmd_hnd->size != sizeof(struct edma_ext))
				break;

			edma_ext = (struct edma_ext *)cmd_hnd->kva;

			result = edma_execute(edma_sub, edma_ext);

			cmd_hnd->ip_time =  edma_sub->ip_time;

			return result;
		}
	case APUSYS_CMD_PREEMPT:
		return result;
	default:
		break;
	}

	return -EINVAL;
}

static int mtk_edma_sub_probe(struct platform_device *pdev)
{
	int irq, ret;
	struct resource *mem;
	struct edma_sub *edma_sub;
	struct device *dev = &pdev->dev;

	edma_sub = devm_kzalloc(dev, sizeof(*edma_sub), GFP_KERNEL);
	if (!edma_sub)
		return -ENOMEM;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	edma_sub->base_addr = devm_ioremap_resource(dev, mem);
	if (IS_ERR((const void *)(edma_sub->base_addr))) {
		dev_notice(dev, "cannot get ioremap\n");
		return -ENOENT;
	}

	/* interrupt resource */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	ret = devm_request_irq(dev, irq, edma_isr_handler,
			       IRQF_TRIGGER_NONE,
			       dev_name(dev),
			       edma_sub);
	if (ret < 0) {
		dev_notice(dev, "Failed to request irq %d: %d\n", irq, ret);
		return ret;
	}

	edma_sub->dev = &pdev->dev;
	platform_set_drvdata(pdev, edma_sub);
	dev_set_drvdata(dev, edma_sub);

	return 0;
}

static int mtk_edma_sub_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver mtk_edma_sub_driver = {
	.probe = mtk_edma_sub_probe,
	.remove = mtk_edma_sub_remove,
	.driver = {
		   .name = "mtk,edma-sub",
		   .of_match_table = mtk_edma_sub_of_ids,
		   .pm = NULL,
	}
};

static int edma_setup_resource(struct platform_device *pdev,
			      struct edma_device *edma_device)
{
	struct device *dev = &pdev->dev;
	struct device_node *sub_node;
	struct platform_device *sub_pdev;
	int i, ret;

	ret = of_property_read_u32(dev->of_node, "sub_nr",
				   &edma_device->edma_sub_num);
	if (ret) {
		dev_notice(dev, "parsing sub_nr error: %d\n", ret);
		return -EINVAL;
	}

	if (edma_device->edma_sub_num > EDMA_SUB_NUM)
		return -EINVAL;

	for (i = 0; i < edma_device->edma_sub_num; i++) {
		struct edma_sub *edma_sub = NULL;

		sub_node = of_parse_phandle(dev->of_node,
					     "mediatek,edma-sub", i);
		if (!sub_node) {
			dev_notice(dev,
				"Missing <mediatek,edma-sub> phandle\n");
			return -EINVAL;
		}

		sub_pdev = of_find_device_by_node(sub_node);
		if (sub_pdev)
			edma_sub = platform_get_drvdata(sub_pdev);

		if (!edma_sub) {
			dev_notice(dev, "Waiting for edma sub %s\n",
				 sub_node->full_name);
			of_node_put(sub_node);
			return -EPROBE_DEFER;
		}
		of_node_put(sub_node);
		/* attach edma_sub */
		edma_device->edma_sub[i] = edma_sub;

		/* register device to APUSYS */
		edma_sub->adev.dev_type = APUSYS_DEVICE_EDMA;
		edma_sub->adev.preempt_type = APUSYS_PREEMPT_NONE;
		edma_sub->adev.preempt_level = 0;
		edma_sub->adev.private = edma_sub;
		edma_sub->adev.send_cmd = edma_send_cmd;
		edma_sub->adev.idx = i;
		ret = apusys_register_device(&edma_sub->adev);
		if (ret) {
			dev_notice(dev,
				"Failed to register apusys (%d)\n", ret);
			return -EPROBE_DEFER;
		}
	}
	apu_power_device_register(EDMA, pdev);

	return 0;
}

static int edma_probe(struct platform_device *pdev)
{
	struct edma_device *edma_device;
	struct device *dev = &pdev->dev;
	int ret;

	edma_device = devm_kzalloc(dev, sizeof(*edma_device), GFP_KERNEL);
	if (!edma_device)
		return -ENOMEM;

	ret = edma_setup_resource(pdev, edma_device);
	if (ret)
		return ret;

	INIT_LIST_HEAD(&edma_device->user_list);
	mutex_init(&edma_device->user_mutex);
	mutex_init(&edma_device->power_mutex);
	edma_device->edma_num_users = 0;
	edma_device->dev = &pdev->dev;
	edma_device->dbgfs_reg_core = 0;

	if (edma_reg_chardev(edma_device) == 0) {
		/* Create class register */
		edma_class = class_create(THIS_MODULE, EDMA_DEV_NAME);
		if (IS_ERR(edma_class)) {
			ret = PTR_ERR(edma_class);
			dev_notice(dev, "Unable to create class, err = %d\n",
									ret);
			goto dev_out;
		}

		dev = device_create(edma_class, NULL, edma_device->edma_devt,
				    NULL, EDMA_DEV_NAME);
		if (IS_ERR(dev)) {
			ret = PTR_ERR(dev);
			dev_notice(dev,
				"Failed to create device: /dev/%s, err = %d",
				EDMA_DEV_NAME, ret);
			goto dev_out;
		}

		platform_set_drvdata(pdev, edma_device);
		dev_set_drvdata(dev, edma_device);
		edma_create_sysfs(dev);
	}
	edma_initialize(edma_device);
	pr_notice("edma probe done\n");

	return 0;

dev_out:
	edma_unreg_chardev(edma_device);

	return ret;
}

static int edma_remove(struct platform_device *pdev)
{
	struct edma_device *edma_device = platform_get_drvdata(pdev);

	apu_power_device_unregister(EDMA);
	edma_unreg_chardev(edma_device);
	device_destroy(edma_class, edma_device->edma_devt);
	class_destroy(edma_class);

	edma_remove_sysfs(&pdev->dev);

	return 0;
}


static const struct of_device_id edma_of_ids[] = {
	{.compatible = "mtk,edma",},
	{}
};

static struct platform_driver edma_driver = {
	.probe = edma_probe,
	.remove = edma_remove,
	.driver = {
		   .name = EDMA_DEV_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = edma_of_ids,
	}
};

static int __init EDMA_INIT(void)
{
	int ret = 0;

	if (!apusys_power_check()) {
		pr_info("%s: edma is disabled by apusys\n", __func__);
		return -ENODEV;
	}

	ret = platform_driver_register(&mtk_edma_sub_driver);
	if (ret != 0) {
		pr_notice("Failed to register edma sub driver\n");
		return -ENODEV;
	}

	ret = platform_driver_register(&edma_driver);
	if (ret != 0) {
		pr_notice("failed to register edma driver");
		goto err_unreg_edma_sub;
	}

	return ret;
err_unreg_edma_sub:
	platform_driver_unregister(&mtk_edma_sub_driver);
	return ret;
}

static void __exit EDMA_EXIT(void)
{
	platform_driver_unregister(&edma_driver);
	platform_driver_unregister(&mtk_edma_sub_driver);
}

module_init(EDMA_INIT);
module_exit(EDMA_EXIT);
MODULE_LICENSE("GPL");
