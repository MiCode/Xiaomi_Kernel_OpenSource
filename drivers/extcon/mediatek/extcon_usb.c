/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/extcon.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include "extcon_usb.h"

struct usb_extcon_info {
	struct device *dev;
	struct extcon_dev *edev;
	unsigned int dr; /* data role */
	struct delayed_work wq_detcable;
	struct workqueue_struct *extcon_workq;
};

static const unsigned int usb_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

enum {
	DUAL_PROP_MODE_UFP = 0,
	DUAL_PROP_MODE_DFP,
	DUAL_PROP_MODE_NONE,
};

enum {
	DUAL_PROP_PR_SRC = 0,
	DUAL_PROP_PR_SNK,
	DUAL_PROP_PR_NONE,
};

enum {
	DUAL_PROP_DR_HOST = 0,
	DUAL_PROP_DR_DEVICE,
	DUAL_PROP_DR_NONE,
};

static struct usb_extcon_info *g_extcon_info;
static unsigned int curr_dr = DUAL_PROP_DR_NONE;

static unsigned int extcon_get_dr(void)
{
	return curr_dr;
}

static void usb_extcon_detect_cable(struct work_struct *work)
{
	struct usb_extcon_info *info = container_of(to_delayed_work(work),
						    struct usb_extcon_info,
						    wq_detcable);

	unsigned int dr;

	dr = extcon_get_dr();

	if (info->dr != dr) {
		bool host_connected = false;
		bool device_connected = false;

		info->dr = dr;

		if (dr == DUAL_PROP_DR_DEVICE)
			device_connected = true;
		else if (dr == DUAL_PROP_DR_HOST)
			host_connected = true;

		pr_info("usb_extcon_detect_cable change =%d %d\n",
			host_connected, device_connected);

		extcon_set_state_sync(info->edev, EXTCON_USB_HOST,
					host_connected);
		extcon_set_state_sync(info->edev, EXTCON_USB,
					device_connected);
	}
}

void mt_usb_connect(void)
{
	curr_dr = DUAL_PROP_DR_DEVICE;

	if (g_extcon_info) {
		queue_delayed_work(g_extcon_info->extcon_workq,
			&g_extcon_info->wq_detcable, 0);
	}
}
EXPORT_SYMBOL_GPL(mt_usb_connect);

void mt_usb_disconnect(void)
{
	curr_dr = DUAL_PROP_DR_NONE;

	if (g_extcon_info) {
		queue_delayed_work(g_extcon_info->extcon_workq,
			&g_extcon_info->wq_detcable, 0);
	}
}
EXPORT_SYMBOL_GPL(mt_usb_disconnect);

void mt_usbhost_connect(void)
{
	curr_dr = DUAL_PROP_DR_HOST;

	if (g_extcon_info) {
		queue_delayed_work(g_extcon_info->extcon_workq,
			&g_extcon_info->wq_detcable, 0);
	}
}
EXPORT_SYMBOL_GPL(mt_usbhost_connect);

void mt_usbhost_disconnect(void)
{
	curr_dr = DUAL_PROP_DR_NONE;
	if (g_extcon_info) {
		queue_delayed_work(g_extcon_info->extcon_workq,
			&g_extcon_info->wq_detcable, 0);
	}
}
EXPORT_SYMBOL_GPL(mt_usbhost_disconnect);

void mt_vbus_on(void)
{
	usb_otg_set_vbus(true);
}
EXPORT_SYMBOL_GPL(mt_vbus_on);

void mt_vbus_off(void)
{
	usb_otg_set_vbus(false);
}
EXPORT_SYMBOL_GPL(mt_vbus_off);



static int usb_extcon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct usb_extcon_info *info;
	int ret;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = dev;

	info->edev = devm_extcon_dev_allocate(dev, usb_extcon_cable);
	if (IS_ERR(info->edev)) {
		dev_err(dev, "failed to allocate extcon device\n");
		return -ENOMEM;
	}

	ret = devm_extcon_dev_register(dev, info->edev);
	if (ret < 0) {
		dev_err(dev, "failed to register extcon device\n");
		return ret;
	}
	platform_set_drvdata(pdev, info);
	info->dr = DUAL_PROP_DR_NONE;
	info->extcon_workq = create_singlethread_workqueue("usb_extcon_workq");
	INIT_DELAYED_WORK(&info->wq_detcable, usb_extcon_detect_cable);

	/* Perform initial detection */
	usb_extcon_detect_cable(&info->wq_detcable.work);
	g_extcon_info = info;

	return 0;
}

static int usb_extcon_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id usb_extcon_dt_match[] = {
	{ .compatible = "mediatek,extcon-usb", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, usb_extcon_dt_match);

static struct platform_driver usb_extcon_driver = {
	.probe		= usb_extcon_probe,
	.remove		= usb_extcon_remove,
	.driver		= {
		.name	= "mediatek,extcon-usb",
		.of_match_table = usb_extcon_dt_match,
	},
};

module_platform_driver(usb_extcon_driver);

