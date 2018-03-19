/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include <linux/err.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <soc/qcom/subsystem_notif.h>

static void __iomem *base_reg;

struct state_notifier_block {
	const char *subsystem;
	struct notifier_block nb;
	u32 offset;
	void *handle;
	struct list_head notifier_list;
};

static LIST_HEAD(notifier_block_list);

static int subsys_state_callback(struct notifier_block *this,
		unsigned long value, void *priv)
{
	struct state_notifier_block *notifier =
		container_of(this, struct state_notifier_block, nb);

	writel_relaxed(value, base_reg + notifier->offset);

	return NOTIFY_OK;
}

static int subsys_notif_virt_probe(struct platform_device *pdev)
{
	struct device_node *node;
	struct device_node *child = NULL;
	struct resource *res;
	struct state_notifier_block *notif_block;
	int ret = 0;

	if (!pdev) {
		dev_err(&pdev->dev, "pdev is NULL\n");
		return -EINVAL;
	}

	node = pdev->dev.of_node;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vdev_base");
	base_reg = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR_OR_NULL(base_reg)) {
		dev_err(&pdev->dev, "Memory mapping failed\n");
		return -ENOMEM;
	}

	for_each_child_of_node(node, child) {

		notif_block = devm_kmalloc(&pdev->dev,
				sizeof(struct state_notifier_block),
				GFP_KERNEL);
		if (!notif_block)
			return -ENOMEM;

		notif_block->subsystem =
				of_get_property(child, "subsys-name", NULL);
		if (IS_ERR_OR_NULL(notif_block->subsystem)) {
			dev_err(&pdev->dev, "Could not find subsystem name\n");
			ret = -EINVAL;
			goto err_nb;
		}

		notif_block->nb.notifier_call = subsys_state_callback;

		notif_block->handle =
			subsys_notif_register_notifier(notif_block->subsystem,
				&notif_block->nb);
		if (IS_ERR_OR_NULL(notif_block->handle)) {
			dev_err(&pdev->dev, "Could not register SSR notifier cb\n");
			ret = -EINVAL;
			goto err_nb;
		}

		ret = of_property_read_u32(child, "offset",
				&notif_block->offset);
		if (ret) {
			dev_err(&pdev->dev, "offset reading for %s failed\n",
				notif_block->subsystem);
			ret = -EINVAL;
			goto err_offset;
		}

		list_add_tail(&notif_block->notifier_list,
			&notifier_block_list);

	}
	return 0;

err_offset:
	subsys_notif_unregister_notifier(notif_block->handle,
		&notif_block->nb);
err_nb:
	kfree(notif_block);
	return ret;
}

static int subsys_notif_virt_remove(struct platform_device *pdev)
{
	struct state_notifier_block *notif_block;

	list_for_each_entry(notif_block, &notifier_block_list,
			notifier_list) {
		subsys_notif_unregister_notifier(notif_block->handle,
			&notif_block->nb);
		list_del(&notif_block->notifier_list);
	}
	return 0;
}

static const struct of_device_id match_table[] = {
	{ .compatible = "qcom,subsys-notif-virt" },
	{},
};

static struct platform_driver subsys_notif_virt_driver = {
	.probe = subsys_notif_virt_probe,
	.remove = subsys_notif_virt_remove,
	.driver = {
		.name = "subsys_notif_virt",
		.owner = THIS_MODULE,
		.of_match_table = match_table,
	},
};

static int __init subsys_notif_virt_init(void)
{
	return platform_driver_register(&subsys_notif_virt_driver);
}
module_init(subsys_notif_virt_init);

static void __exit subsys_notif_virt_exit(void)
{
	platform_driver_unregister(&subsys_notif_virt_driver);
}
module_exit(subsys_notif_virt_exit);

MODULE_DESCRIPTION("Subsystem Notification Virtual Driver");
MODULE_LICENSE("GPL v2");
