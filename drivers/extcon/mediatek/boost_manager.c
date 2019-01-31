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

#include <linux/debugfs.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/pinctrl/consumer.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#ifdef CONFIG_MTK_CHARGER
#if CONFIG_MTK_GAUGE_VERSION == 30
#include <mtk_gauge_time_service.h>
#include <mt-plat/charger_class.h>
#endif
#endif

struct usbotg_boost {
	struct platform_device *pdev;
	struct charger_device *primary_charger;
#ifdef CONFIG_MTK_CHARGER
#if CONFIG_MTK_GAUGE_VERSION == 30
	struct gtimer otg_kthread_gtimer;
	struct workqueue_struct *boost_workq;
	struct work_struct kick_work;
	unsigned int polling_interval;
	bool polling_enabled;
#endif
#endif
};
static struct usbotg_boost *g_info;
static struct mutex otg_pwr_lock;
static bool vbus_on;

#ifdef CONFIG_MTK_CHARGER
#if CONFIG_MTK_GAUGE_VERSION == 30
static void enable_boost_polling(bool poll_en)
{
	if (g_info) {
		if (poll_en) {
			gtimer_start(&g_info->otg_kthread_gtimer,
				g_info->polling_interval);

			g_info->polling_enabled = true;
		} else {
			g_info->polling_enabled = false;
			gtimer_stop(&g_info->otg_kthread_gtimer);
		}
	}
}

static void usbotg_boost_kick_work(struct work_struct *work)
{
	struct usbotg_boost *usb_boost_manager =
		container_of(work, struct usbotg_boost, kick_work);

	pr_info("usbotg_boost_kick_work\n");

	charger_dev_kick_wdt(usb_boost_manager->primary_charger);

	if (usb_boost_manager->polling_enabled == true) {
		gtimer_start(&usb_boost_manager->otg_kthread_gtimer,
			usb_boost_manager->polling_interval);
	}
}

static int usbotg_gtimer_func(struct gtimer *data)
{
	struct usbotg_boost *usb_boost_manager =
		container_of(data, struct usbotg_boost, otg_kthread_gtimer);

	queue_work(usb_boost_manager->boost_workq,
		&usb_boost_manager->kick_work);

	return 0;
}
#endif
#endif

int usb_otg_set_vbus(int is_on)
{
	if (!g_info)
		return -1;

	mutex_lock(&otg_pwr_lock);
	if (is_on && !vbus_on) {
		vbus_on = true;
#ifdef CONFIG_MTK_CHARGER
		charger_dev_enable_otg(g_info->primary_charger, true);
		charger_dev_set_boost_current_limit(g_info->primary_charger,
			1500000);
#if CONFIG_MTK_GAUGE_VERSION == 30
		charger_dev_kick_wdt(g_info->primary_charger);
		enable_boost_polling(true);
#endif
#endif
	} else if (!is_on && vbus_on) {
#ifdef CONFIG_MTK_CHARGER
		charger_dev_enable_otg(g_info->primary_charger, false);
#if CONFIG_MTK_GAUGE_VERSION == 30
		enable_boost_polling(false);
#endif
#endif
		vbus_on = false;
	}
	mutex_unlock(&otg_pwr_lock);

	return 0;
}

static int usbotg_boost_probe(struct platform_device *pdev)
{
	struct usbotg_boost *info = NULL;
#ifdef CONFIG_MTK_CHARGER
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
#endif

	info = devm_kzalloc(&pdev->dev, sizeof(struct usbotg_boost),
		GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	platform_set_drvdata(pdev, info);
	info->pdev = pdev;
#ifdef CONFIG_MTK_CHARGER
	info->primary_charger = get_charger_by_name("primary_chg");
	if (!info->primary_charger) {
		pr_info("%s: get primary charger device failed\n", __func__);
		return -ENODEV;
	}

#if CONFIG_MTK_GAUGE_VERSION == 30
	gtimer_init(&info->otg_kthread_gtimer, &info->pdev->dev, "otg_boost");
	info->otg_kthread_gtimer.callback = usbotg_gtimer_func;
	if (of_property_read_u32(node, "boost_period",
		(u32 *) &info->polling_interval))
		return -EINVAL;

	info->polling_interval = 30;
	info->boost_workq = create_singlethread_workqueue("boost_workq");
	INIT_WORK(&info->kick_work, usbotg_boost_kick_work);
#endif
#endif
	mutex_init(&otg_pwr_lock);

	g_info = info;
	return 0;
}

static int usbotg_boost_remove(struct platform_device *pdev)
{
	usb_otg_set_vbus(false);

	return 0;
}

static void usbotg_boost_shutdown(struct platform_device *pdev)
{
	usb_otg_set_vbus(false);
}


static const struct of_device_id usb_boost_of_match[] = {
	{.compatible = "mediatek,usb_boost"},
	{},
};

MODULE_DEVICE_TABLE(of, usb_boost_of_match);
static struct platform_driver usb_boost_driver = {
	.remove = usbotg_boost_remove,
	.probe = usbotg_boost_probe,
	.shutdown = usbotg_boost_shutdown,
	.driver = {
		   .name = "mediatek,usb_boost",
		   .of_match_table = usb_boost_of_match,
		   },
};

static int __init usb_boost_init(void)
{
	platform_driver_register(&usb_boost_driver);
	return 0;
}

late_initcall(usb_boost_init);

static void __exit usb_boost_init_cleanup(void)
{
}

module_exit(usb_boost_init_cleanup);



