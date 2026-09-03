// SPDX-License-Identifier: GPL-2.0
/**
 * phy-sprd-dummy.c - Unisoc USB Dummy PHY Glue layer
 *
 * Copyright (c) 2021 Unisoc Co., Ltd.
 *		http://www.unisoc.com
 *
 * Author: Surong Pang <surong.pang@unisoc.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/usb/phy.h>

struct sprd_dummy_phy {
	struct usb_phy		phy;
};

static int sprd_dummy_phy_init(struct usb_phy *x)
{
	return 0;
}

/* Turn off PHY and core */
static void sprd_dummy_phy_shutdown(struct usb_phy *x)
{
}

static int sprd_dummy_phy_set_vbus(struct usb_phy *x, int on)
{
	return 0;
}
static int sprd_dummy_phy_vbus_notify(struct notifier_block *nb,
				unsigned long event, void *data)
{
	return 0;
}

static enum usb_charger_type sprd_dummy_phy_charger_detect(struct usb_phy *x)
{
	return UNKNOWN_TYPE;
}

static int sprd_dummy_phy_notify_connect(struct usb_phy *x,
				enum usb_device_speed speed)
{
	return 0;
}

static int sprd_dummy_phy_notify_disconnect(struct usb_phy *x,
				enum usb_device_speed speed)
{
	return 0;
}

static int sprd_dummy_phy_probe(struct platform_device *pdev)
{
	struct sprd_dummy_phy *phy;
	struct device *dev = &pdev->dev;

	int ret;

	dev_info(dev, "%s enter\n", __func__);
	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	platform_set_drvdata(pdev, phy);
	phy->phy.dev				= dev;
	phy->phy.label				= "sprd-dummy-phy";
	phy->phy.init				= sprd_dummy_phy_init;
	phy->phy.shutdown			= sprd_dummy_phy_shutdown;
	phy->phy.set_vbus			= sprd_dummy_phy_set_vbus;
	phy->phy.type				= USB_PHY_TYPE_USB2;
	phy->phy.vbus_nb.notifier_call		= sprd_dummy_phy_vbus_notify;
	phy->phy.charger_detect			= sprd_dummy_phy_charger_detect;
	phy->phy.notify_connect			= sprd_dummy_phy_notify_connect;
	phy->phy.notify_disconnect		= sprd_dummy_phy_notify_disconnect;
	ret = usb_add_phy_dev(&phy->phy);
	if (ret) {
		dev_err(dev, "fail to add phy\n");
		return ret;
	}

	pm_runtime_enable(dev);

	return 0;
}

static int sprd_dummy_phy_remove(struct platform_device *pdev)
{
	struct sprd_dummy_phy *phy = platform_get_drvdata(pdev);

	usb_remove_phy(&phy->phy);
	return 0;
}

static const struct of_device_id sprd_dummy_phy_match[] = {
	{ .compatible = "sprd,usb-dummy-phy" },
	{},
};

MODULE_DEVICE_TABLE(of, sprd_dummy_phy_match);

static struct platform_driver sprd_dummy_phy_driver = {
	.probe		= sprd_dummy_phy_probe,
	.remove		= sprd_dummy_phy_remove,
	.driver		= {
		.name	= "sprd-dummy-phy",
		.of_match_table = sprd_dummy_phy_match,
	},
};

static int __init sprd_dummy_phy_driver_init(void)
{
	return platform_driver_register(&sprd_dummy_phy_driver);
}

static void __exit sprd_dummy_phy_driver_exit(void)
{
	platform_driver_unregister(&sprd_dummy_phy_driver);
}

late_initcall(sprd_dummy_phy_driver_init);
module_exit(sprd_dummy_phy_driver_exit);

MODULE_ALIAS("platform:sprd-dummy-phy");
MODULE_AUTHOR("Surong Pang <surong.pang@unisoc.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DesignWare USB SPRD Dummy PHY");
