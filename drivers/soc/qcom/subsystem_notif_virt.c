/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <soc/qcom/subsystem_notif.h>
#ifdef CONFIG_GHS_VMM
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/spinlock.h>
#include <linux/rbtree.h>
#include <linux/idr.h>
#include <linux/uaccess.h>
#include <linux/dma-direction.h>
#include <linux/dma-mapping.h>
#include <linux/jiffies.h>
#include <linux/reboot.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <ghs_vmm/kgipc.h>
#endif

#define CLIENT_STATE_OFFSET 4
#define SUBSYS_STATE_OFFSET 8
#define SUBSYS_NAME_MAX_LEN 64
#define GIPC_RECV_BUFF_SIZE_BYTES (32*1024)
#define SSR_VIRT_DT_PATHLEN 100
#define SUBSYS_STATE_STRLEN 100

enum subsystem_type {
	VIRTUAL,
	NATIVE,
};

struct subsystem_descriptor {
	const char *name;
	u32 offset;
	enum subsystem_type type;
	struct notifier_block nb;
	void *handle;
	int ssr_irq;
	struct list_head subsystem_list;
	struct work_struct work;
	void *commdev;
};

static LIST_HEAD(subsystem_descriptor_list);
static struct workqueue_struct *ssr_wq;


#ifdef CONFIG_GHS_VMM
enum subsys_payload_type {
	SUBSYS_CHANNEL_SEND = 100,
	SUBSYS_CHANNEL_RECV,
	SUBSYS_CHANNEL_SEND_ACK,
	SUBSYS_CHANNEL_RECV_ACK
};

struct ghs_vdev {
	void *read_data; /* buffer to receive from gipc */
	size_t read_size;
	int read_offset;
	GIPC_Endpoint endpoint;
	spinlock_t io_lock;
	wait_queue_head_t rx_queue;
	int xfer_state;
	char name[32];
};

static char dt_gipc_path_name[SSR_VIRT_DT_PATHLEN];

static int ssrvirt_channel_ack(struct ghs_vdev *dev)
{
	int ret = 0;

	ret = wait_event_interruptible(dev->rx_queue, ((dev->read_size != 0) ||
				(dev->xfer_state == SUBSYS_CHANNEL_SEND_ACK)));

	return ret;
}

static int ssrvirt_channel_send(struct ghs_vdev *dev, void *payload,
		size_t size)
{
	GIPC_Result result;
	uint8_t *msg;

	spin_lock_bh(&dev->io_lock);

	result = GIPC_PrepareMessage(dev->endpoint, size,
			(void **)&msg);
	if (result == GIPC_Full) {
		spin_unlock_bh(&dev->io_lock);
		pr_err("Failed to reserve send msg for %zd bytes\n",
				size);
		return -EBUSY;
	} else if (result != GIPC_Success) {
		spin_unlock_bh(&dev->io_lock);
		pr_err("Failed to send due to error %d\n", result);
		return -ENOMEM;
	}

	dev->xfer_state = SUBSYS_CHANNEL_SEND;

	if (size)
		memcpy(msg, payload, size);

	result = GIPC_IssueMessage(dev->endpoint, size, 0);

	spin_unlock_bh(&dev->io_lock);

	if (result != GIPC_Success) {
		pr_err("Send error %d, size %zd, protocol %x\n",
				result, size, 0);
		return -EAGAIN;
	}

	ssrvirt_channel_ack(dev);

	return 0;
}

static int subsystem_state_callback(struct notifier_block *this,
		unsigned long value, void *priv)
{
	struct subsystem_descriptor *subsystem =
		container_of(this, struct subsystem_descriptor, nb);
	char buf[SUBSYS_STATE_STRLEN];

	memset(buf, 0, SUBSYS_STATE_STRLEN);
	snprintf(buf, SUBSYS_STATE_STRLEN, "%ld", value);
	ssrvirt_channel_send(subsystem->commdev, buf, (strlen(buf) + 1));

	return NOTIFY_OK;
}

static void ssrvirt_channel_rx_dispatch(struct subsystem_descriptor *subsystem,
		struct ghs_vdev *dev)
{
	GIPC_Result result;
	uint32_t events;
	uint32_t id_type_size;
	void *subsystem_handle;
	int state;
	char subsystem_name[SUBSYS_NAME_MAX_LEN];

	events = kgipc_dequeue_events(dev->endpoint);

	if (events & (GIPC_EVENT_RECEIVEREADY)) {
		do {
			dev->read_size = 0;
			dev->read_offset = 0;
			result = GIPC_ReceiveMessage(dev->endpoint,
					dev->read_data,
					GIPC_RECV_BUFF_SIZE_BYTES,
					&dev->read_size,
					&id_type_size);

			if (result == GIPC_Success || dev->read_size > 0) {
				if (sscanf(dev->read_data, "%s %d",
						subsystem_name, &state) != 2) {
					pr_err("%s:return error", __func__);
					break;
				}

				subsystem_handle =
					subsys_notif_add_subsys(
							subsystem->name);

				if (state == SUBSYS_CHANNEL_SEND_ACK) {
					dev->xfer_state =
						SUBSYS_CHANNEL_SEND_ACK;
					wake_up_interruptible(&dev->rx_queue);
					continue;
				}

				subsys_notif_queue_notification(
						subsystem_handle, state, NULL);
			}
		} while (result == GIPC_Success);
	}
}

static void ghs_irq_handler(void *cookie)
{
	struct subsystem_descriptor *subsystem =
		(struct subsystem_descriptor *)cookie;

	queue_work(ssr_wq, &subsystem->work);
}

static int ssrvirt_commdev_alloc(void *dev_id, const char *name)
{
	struct ghs_vdev *dev = NULL;
	struct device_node *gvh_dn;
	struct subsystem_descriptor *subsystem =
		(struct subsystem_descriptor *)dev_id;
	int ret = 0;

	memset(dt_gipc_path_name, 0, SSR_VIRT_DT_PATHLEN);
	snprintf(dt_gipc_path_name, SSR_VIRT_DT_PATHLEN, "ssrvirt_%s", name);

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		ret = -ENOMEM;
		pr_err("Allocate struct ghs_vdev failed %zu bytes on subsystem %s\n",
				sizeof(*dev), name);
		goto err;
	}

	subsystem->commdev = dev;

	memset(dev, 0, sizeof(*dev));
	spin_lock_init(&dev->io_lock);
	init_waitqueue_head(&dev->rx_queue);

	gvh_dn = of_find_node_by_path("/aliases");
	if (gvh_dn) {
		const char *ep_path = NULL;
		struct device_node *ep_dn;

		ret = of_property_read_string(gvh_dn, dt_gipc_path_name,
				&ep_path);
		if (ret) {
			pr_err("Failed to read endpoint string ret %d\n",
					ret);
			goto err;
		}

		of_node_put(gvh_dn);

		ep_dn = of_find_node_by_path(ep_path);
		if (ep_dn) {
			dev->endpoint = kgipc_endpoint_alloc(ep_dn);
			of_node_put(ep_dn);
			if (IS_ERR(dev->endpoint)) {
				ret = PTR_ERR(dev->endpoint);
				pr_err("KGIPC alloc failed id: %s, ret: %d\n",
						dt_gipc_path_name, ret);
				goto err;
			} else {
				pr_debug("gipc ep found for %s\n",
						dt_gipc_path_name);
			}
		} else {
			pr_err("of_parse_phandle failed for : %s\n",
					dt_gipc_path_name);
			ret = -ENOENT;
			goto err;
		}
	} else {
		pr_err("of_find_compatible_node failed for : %s\n",
				dt_gipc_path_name);
		ret = -ENOENT;
		goto err;
	}

	strlcpy(dev->name, name, sizeof(dev->name));
	dev->read_data = kmalloc(GIPC_RECV_BUFF_SIZE_BYTES, GFP_KERNEL);
	if (!dev->read_data) {
		ret = -ENOMEM;
		goto err;
	}

	ret = kgipc_endpoint_start_with_irq_callback(dev->endpoint,
			ghs_irq_handler,
			subsystem);
	if (ret) {
		pr_err("irq alloc failed : %s, ret: %d\n", name, ret);
		kfree(dev->read_data);
		goto err;
	}

	return 0;
err:
	kfree(dev);
	return ret;
}

static int ssrvirt_commdev_dealloc(void *dev_id)
{
	struct subsystem_descriptor *subsystem =
		(struct subsystem_descriptor *)dev_id;
	struct ghs_vdev *dev = (struct ghs_vdev *)subsystem->commdev;

	kgipc_endpoint_free(dev->endpoint);
	kfree(dev->read_data);
	kfree(dev);
	return 0;
}

static void subsystem_notif_wq_func(struct work_struct *work)
{
	struct subsystem_descriptor *subsystem =
		container_of(work, struct subsystem_descriptor, work);

	ssrvirt_channel_rx_dispatch(subsystem, subsystem->commdev);
}

static int get_resources(struct platform_device *pdev)
{
	struct device_node *node;
	struct device_node *child = NULL;
	const char *ss_type;
	struct subsystem_descriptor *subsystem = NULL;
	int ret = 0;

	node = pdev->dev.of_node;

	for_each_child_of_node(node, child) {
		subsystem = devm_kmalloc(&pdev->dev,
				sizeof(struct subsystem_descriptor),
				GFP_KERNEL);
		if (!subsystem)
			return -ENOMEM;

		subsystem->name =
			of_get_property(child, "subsys-name", NULL);
		if (IS_ERR_OR_NULL(subsystem->name)) {
			dev_err(&pdev->dev, "Could not find subsystem name\n");
			return -EINVAL;
		}

		ret = of_property_read_string(child, "type",
				&ss_type);
		if (ret) {
			dev_err(&pdev->dev, "type reading for %s failed\n",
					subsystem->name);
			return -EINVAL;
		}

		if (!strcmp(ss_type, "virtual"))
			subsystem->type = VIRTUAL;

		if (!strcmp(ss_type, "native"))
			subsystem->type = NATIVE;

		INIT_WORK(&subsystem->work, subsystem_notif_wq_func);

		if (subsystem->type == NATIVE) {
			subsystem->nb.notifier_call =
				subsystem_state_callback;

			subsystem->handle =
				subsys_notif_register_notifier(
					subsystem->name, &subsystem->nb);
			if (IS_ERR_OR_NULL(subsystem->handle)) {
				dev_err(&pdev->dev,
					"Could not register SSR notifier cb\n");
				return -EINVAL;
			}
		}

		ssrvirt_commdev_alloc(subsystem, subsystem->name);
		list_add_tail(&subsystem->subsystem_list,
			&subsystem_descriptor_list);
	}

	return 0;
}

static void release_resources(void)
{
	struct subsystem_descriptor *subsystem, *node;

	list_for_each_entry_safe(subsystem, node, &subsystem_descriptor_list,
			subsystem_list) {
		if (subsystem->type == NATIVE)
			subsys_notif_unregister_notifier(subsystem->handle,
					&subsystem->nb);
		ssrvirt_commdev_dealloc(subsystem->commdev);
		list_del(&subsystem->subsystem_list);
	}

}
#endif

#ifndef CONFIG_GHS_VMM
#ifdef CONFIG_MSM_GVM_QUIN

static void __iomem *base_reg;

static void subsystem_notif_wq_func(struct work_struct *work)
{
	struct subsystem_descriptor *subsystem =
		container_of(work, struct subsystem_descriptor, work);
	void *subsystem_handle;
	int state, ret;

	state = readl_relaxed(base_reg + subsystem->offset);
	subsystem_handle = subsys_notif_add_subsys(subsystem->name);
	ret = subsys_notif_queue_notification(subsystem_handle, state, NULL);
	writel_relaxed(ret, base_reg + subsystem->offset + CLIENT_STATE_OFFSET);
}

static int subsystem_state_callback(struct notifier_block *this,
		unsigned long value, void *priv)
{
	struct subsystem_descriptor *subsystem =
		container_of(this, struct subsystem_descriptor, nb);

	writel_relaxed(value, base_reg + subsystem->offset +
			SUBSYS_STATE_OFFSET);

	return NOTIFY_OK;
}

static irqreturn_t subsystem_restart_irq_handler(int irq, void *dev_id)
{
	struct subsystem_descriptor *subsystem = dev_id;

	queue_work(ssr_wq, &subsystem->work);

	return IRQ_HANDLED;
}

static int get_resources(struct platform_device *pdev)
{
	struct device_node *node;
	struct device_node *child = NULL;
	const char *ss_type;
	struct resource *res;
	struct subsystem_descriptor *subsystem = NULL;
	int ret = 0;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vdev_base");
	base_reg = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR_OR_NULL(base_reg)) {
		dev_err(&pdev->dev, "Memory mapping failed\n");
		return -ENOMEM;
	}

	node = pdev->dev.of_node;
	for_each_child_of_node(node, child) {

		subsystem = devm_kmalloc(&pdev->dev,
				sizeof(struct subsystem_descriptor),
				GFP_KERNEL);
		if (!subsystem)
			return -ENOMEM;

		subsystem->name =
			of_get_property(child, "subsys-name", NULL);
		if (IS_ERR_OR_NULL(subsystem->name)) {
			dev_err(&pdev->dev, "Could not find subsystem name\n");
			return -EINVAL;
		}

		ret = of_property_read_u32(child, "offset",
				&subsystem->offset);
		if (ret) {
			dev_err(&pdev->dev, "offset reading for %s failed\n",
					subsystem->name);
			return -EINVAL;
		}

		ret = of_property_read_string(child, "type",
				&ss_type);
		if (ret) {
			dev_err(&pdev->dev, "type reading for %s failed\n",
					subsystem->name);
			return -EINVAL;
		}

		if (!strcmp(ss_type, "virtual"))
			subsystem->type = VIRTUAL;

		if (!strcmp(ss_type, "native"))
			subsystem->type = NATIVE;

		switch (subsystem->type) {
		case NATIVE:
			subsystem->nb.notifier_call =
				subsystem_state_callback;

			subsystem->handle =
				subsys_notif_register_notifier(
					subsystem->name, &subsystem->nb);
			if (IS_ERR_OR_NULL(subsystem->handle)) {
				dev_err(&pdev->dev,
					"Could not register SSR notifier cb\n");
				return -EINVAL;
			}
			list_add_tail(&subsystem->subsystem_list,
					&subsystem_descriptor_list);
			break;
		case VIRTUAL:
			subsystem->ssr_irq =
				of_irq_get_byname(child, "state-irq");
			if (subsystem->ssr_irq < 0) {
				dev_err(&pdev->dev, "Could not find IRQ\n");
				return -EINVAL;
			}
			ret = devm_request_threaded_irq(&pdev->dev,
					subsystem->ssr_irq, NULL,
					subsystem_restart_irq_handler,
					IRQF_ONESHOT | IRQF_TRIGGER_RISING,
					subsystem->name, subsystem);
			INIT_WORK(&subsystem->work, subsystem_notif_wq_func);
			break;
		default:
			dev_err(&pdev->dev, "Unsupported type %d\n",
				subsystem->type);
		}
	}

	return 0;
}

static void release_resources(void)
{
	struct subsystem_descriptor *subsystem, *node;

	list_for_each_entry_safe(subsystem, node, &subsystem_descriptor_list,
			subsystem_list) {
		subsys_notif_unregister_notifier(subsystem->handle,
				&subsystem->nb);
		list_del(&subsystem->subsystem_list);
	}
}
#endif
#endif

static int subsys_notif_virt_probe(struct platform_device *pdev)
{
	int ret = 0;

	if (!pdev) {
		dev_err(&pdev->dev, "pdev is NULL\n");
		return -EINVAL;
	}

	ssr_wq = create_singlethread_workqueue("ssr_wq");
	if (!ssr_wq) {
		dev_err(&pdev->dev, "Workqueue creation failed\n");
		return -ENOMEM;
	}

	ret = get_resources(pdev);
	if (ret)
		destroy_workqueue(ssr_wq);

	return ret;
}

static int subsys_notif_virt_remove(struct platform_device *pdev)
{
	destroy_workqueue(ssr_wq);
	release_resources();

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
