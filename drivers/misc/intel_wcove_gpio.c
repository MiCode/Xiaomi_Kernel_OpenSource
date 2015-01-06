/*
 * intel_wcove_gpio.c - Intel WhiskeyCove GPIO(VBUS/VCONN/VCHRG) Control Driver
 *
 * Copyright (C) 2015 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Albin B <albin.bala.krishnan@intel.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/extcon.h>
#include <linux/gpio.h>
#include <linux/acpi.h>

#define WCOVE_GPIO_VCHGIN	"vchgin_desc"
#define WCOVE_GPIO_OTG		"otg_desc"
#define WCOVE_GPIO_VCONN	"vconn_desc"

struct wcove_gpio_info {
	struct platform_device *pdev;
	struct notifier_block nb;
	struct extcon_specific_cable_nb dc_cable_obj;
	struct extcon_specific_cable_nb sdp_cable_obj;
	struct extcon_specific_cable_nb otg_cable_obj;
	struct gpio_desc *gpio_vchgrin;
	struct gpio_desc *gpio_otg;
	struct gpio_desc *gpio_vconn;
	struct list_head gpio_queue;
	struct work_struct gpio_work;
	struct mutex lock;
	spinlock_t gpio_queue_lock;
};

struct wcove_gpio_event {
	struct list_head node;
	bool is_sdp_connected;
	bool is_otg_connected;
};

static void wcove_gpio_ctrl_worker(struct work_struct *work)
{
	struct wcove_gpio_info *info =
		container_of(work, struct wcove_gpio_info, gpio_work);
	struct wcove_gpio_event *evt, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&info->gpio_queue_lock, flags);
	list_for_each_entry_safe(evt, tmp, &info->gpio_queue, node) {
		list_del(&evt->node);
		spin_unlock_irqrestore(&info->gpio_queue_lock, flags);

		dev_info(&info->pdev->dev,
				"%s:%d state=%d\n", __FILE__, __LINE__,
				evt->is_sdp_connected || evt->is_otg_connected);

		mutex_lock(&info->lock);
		/* set high only when otg connected */
		if (!evt->is_sdp_connected)
			gpiod_set_value(info->gpio_otg, evt->is_otg_connected);
		/**
		 * set high when sdp/otg connected and set low when sdp/otg
		 * disconnected
		 */
		gpiod_set_value(info->gpio_vchgrin,
			(evt->is_sdp_connected ||
				evt->is_otg_connected) ? 1 : 0);
		mutex_unlock(&info->lock);
		spin_lock_irqsave(&info->gpio_queue_lock, flags);
		kfree(evt);

	}
	spin_unlock_irqrestore(&info->gpio_queue_lock, flags);

	return;
}

static int wcgpio_event_handler(struct notifier_block *nblock,
					unsigned long event, void *param)
{
	struct wcove_gpio_info *info =
			container_of(nblock, struct wcove_gpio_info, nb);
	struct extcon_dev *edev = param;
	struct wcove_gpio_event *evt;

	if (!edev)
		return NOTIFY_DONE;

	evt = kzalloc(sizeof(*evt), GFP_ATOMIC);
	if (!evt) {
		dev_err(&info->pdev->dev,
			"failed to allocate memory for SDP/OTG event\n");
		return NOTIFY_DONE;
	}

	evt->is_sdp_connected = extcon_get_cable_state(edev, "USB");
	evt->is_otg_connected = extcon_get_cable_state(edev, "USB-Host");
	dev_info(&info->pdev->dev,
			"[extcon notification] evt: SDP - %s OTG - %s\n",
			evt->is_sdp_connected ? "Connected" : "Disconnected",
			evt->is_otg_connected ? "Connected" : "Disconnected");

	INIT_LIST_HEAD(&evt->node);
	spin_lock(&info->gpio_queue_lock);
	list_add_tail(&evt->node, &info->gpio_queue);
	spin_unlock(&info->gpio_queue_lock);

	queue_work(system_nrt_wq, &info->gpio_work);
	return NOTIFY_OK;
}

static int wcove_gpio_probe(struct platform_device *pdev)
{
	struct wcove_gpio_info *info;
	int ret;

	info = devm_kzalloc(&pdev->dev,
			sizeof(struct wcove_gpio_info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "kzalloc failed\n");
		ret = -ENOMEM;
		goto error_mem;
	}

	info->pdev = pdev;
	platform_set_drvdata(pdev, info);
	mutex_init(&info->lock);
	INIT_LIST_HEAD(&info->gpio_queue);
	INIT_WORK(&info->gpio_work, wcove_gpio_ctrl_worker);
	spin_lock_init(&info->gpio_queue_lock);

	info->nb.notifier_call = wcgpio_event_handler;
	ret = extcon_register_interest(&info->sdp_cable_obj, NULL, "USB",
						&info->nb);
	if (ret) {
		dev_err(&pdev->dev,
			"failed to register extcon notifier for SDP charger\n");
		goto error_sdp;
	}

	ret = extcon_register_interest(&info->otg_cable_obj, NULL, "USB-Host",
						&info->nb);
	if (ret) {
		dev_err(&pdev->dev,
			"failed to register extcon notifier for otg\n");
		goto error_otg;
	}

	/* FIXME: hardcoding of the index 0, 1 & 2 should fix when upstreaming.
	 * However ACPI _DSD is not support in Gmin yet and we need to live
	 * with it.
	 */
	info->gpio_otg = devm_gpiod_get_index(&pdev->dev,
					WCOVE_GPIO_OTG, 0);
	if (IS_ERR(info->gpio_otg)) {
		dev_err(&pdev->dev, "Can't request gpio_otg\n");
		ret = PTR_ERR(info->gpio_otg);
		goto error_gpio;
	}

	info->gpio_vconn = devm_gpiod_get_index(&pdev->dev,
					WCOVE_GPIO_VCONN, 1);
	if (IS_ERR(info->gpio_vconn)) {
		dev_err(&pdev->dev, "Can't request gpio_vconn\n");
		ret = PTR_ERR(info->gpio_vconn);
		goto error_gpio;
	}

	info->gpio_vchgrin = devm_gpiod_get_index(&pdev->dev,
					WCOVE_GPIO_VCHGIN, 2);
	if (IS_ERR(info->gpio_vchgrin)) {
		dev_err(&pdev->dev, "Can't request gpio_vchgrin\n");
		ret = PTR_ERR(info->gpio_vchgrin);
		goto error_gpio;
	}

	ret = gpiod_direction_output(info->gpio_otg, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "cannot configure otg-gpio %d\n", ret);
		goto error_gpio;
	}

	ret = gpiod_direction_output(info->gpio_vconn, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "cannot configure vconn-gpio %d\n", ret);
		goto error_gpio;
	}

	ret = gpiod_direction_output(info->gpio_vchgrin, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "cannot configure vchgrin-gpio %d\n", ret);
		goto error_gpio;
	}
	dev_dbg(&pdev->dev, "wcove gpio probed\n");

	return 0;

error_gpio:
	extcon_unregister_interest(&info->otg_cable_obj);
error_otg:
	extcon_unregister_interest(&info->sdp_cable_obj);
error_sdp:
error_mem:
	return ret;
}

static int wcove_gpio_remove(struct platform_device *pdev)
{
	struct wcove_gpio_info *info =  dev_get_drvdata(&pdev->dev);

	if (info) {
		extcon_unregister_interest(&info->otg_cable_obj);
		extcon_unregister_interest(&info->sdp_cable_obj);
	}

	return 0;
}

static int wcove_gpio_suspend(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int wcove_gpio_resume(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int wcove_gpio_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int wcove_gpio_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int wcove_gpio_runtime_idle(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

#ifdef CONFIG_PM_SLEEP

static const struct dev_pm_ops wcove_gpio_driver_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(wcove_gpio_suspend,
				wcove_gpio_resume)

	SET_RUNTIME_PM_OPS(wcove_gpio_runtime_suspend,
				wcove_gpio_runtime_resume,
				wcove_gpio_runtime_idle)
};
#endif /* CONFIG_PM_SLEEP */

static struct acpi_device_id wcove_gpio_acpi_ids[] = {
	{"GPTC0001"},
	{}
};
MODULE_DEVICE_TABLE(acpi, wcove_gpio_acpi_ids);

static struct platform_device_id wcove_gpio_device_ids[] = {
	{"gptc0001", 0},
	{},
};

static struct platform_driver wcove_gpio_driver = {
	.driver = {
		.name = "gptc0001",
		.owner = THIS_MODULE,
		.acpi_match_table = ACPI_PTR(wcove_gpio_acpi_ids),
#ifdef CONFIG_PM_SLEEP
		.pm = &wcove_gpio_driver_pm_ops,
#endif /* CONFIG_PM_SLEEP */
	},
	.probe = wcove_gpio_probe,
	.remove = wcove_gpio_remove,
	.id_table = wcove_gpio_device_ids,
};

static int __init wcove_gpio_init(void)
{
	int ret;
	ret =  platform_driver_register(&wcove_gpio_driver);
	return ret;
}
late_initcall(wcove_gpio_init);

static void __exit wcove_gpio_exit(void)
{
	platform_driver_unregister(&wcove_gpio_driver);
}
module_exit(wcove_gpio_exit)

MODULE_AUTHOR("Albin B<albin.bala.krishnan@intel.com>");
MODULE_DESCRIPTION("Intel Whiskey Cove GPIO Driver");
MODULE_LICENSE("GPL");
