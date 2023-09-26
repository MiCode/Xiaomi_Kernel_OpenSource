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
#include <linux/power_supply.h>

#if CONFIG_MTK_GAUGE_VERSION == 30
#include <mtk_gauge_time_service.h>
#include <mt-plat/charger_class.h>
#include <linux/alarmtimer.h>
#endif

struct usbotg_boost {
	struct platform_device *pdev;
	struct charger_device *bbc_charger;
	struct charger_device *xmusb350;
	struct charger_device *cp_master;
	struct power_supply *bms_psy;
	struct delayed_work disable_otg_work;
#if CONFIG_MTK_GAUGE_VERSION == 30
	struct alarm otg_timer;
	struct timespec endtime;
	struct workqueue_struct *boost_workq;
	struct work_struct kick_work;
	unsigned int polling_interval;
	bool polling_enabled;
#endif
};
static struct usbotg_boost *g_info;

static struct pinctrl *drvvbus;
static struct pinctrl_state *drvvbus_high;
static struct pinctrl_state *drvvbus_low;

#if CONFIG_MTK_GAUGE_VERSION == 30
static void usbotg_alarm_start_timer(struct usbotg_boost *info)
{
	struct timespec time, time_now;
	ktime_t ktime;

	get_monotonic_boottime(&time_now);
	time.tv_sec = info->polling_interval;
	time.tv_nsec = 0;
	info->endtime = timespec_add(time_now, time);

	ktime = ktime_set(info->endtime.tv_sec, info->endtime.tv_nsec);

	pr_info("%s: alarm timer start\n", __func__);
	alarm_start(&info->otg_timer, ktime);
}

static void enable_boost_polling(bool poll_en)
{
	if (g_info) {
		if (poll_en) {
			usbotg_alarm_start_timer(g_info);
			g_info->polling_enabled = true;
		} else {
			g_info->polling_enabled = false;
			alarm_try_to_cancel(&g_info->otg_timer);
		}
	}
}

static void usbotg_boost_kick_work(struct work_struct *work)
{
	struct usbotg_boost *usb_boost_manager =
		container_of(work, struct usbotg_boost, kick_work);

	pr_info("%s\n", __func__);

	charger_dev_kick_wdt(usb_boost_manager->bbc_charger);

	if (usb_boost_manager->polling_enabled == true)
		usbotg_alarm_start_timer(usb_boost_manager);
}

static enum alarmtimer_restart
	usbotg_alarm_timer_func(struct alarm *alarm, ktime_t now)
{
	struct usbotg_boost *usb_boost_manager =
		container_of(alarm, struct usbotg_boost, otg_timer);
	union power_supply_propval val = {0,};

	pr_info("%s\n", __func__);

	queue_work(usb_boost_manager->boost_workq,
		&usb_boost_manager->kick_work);

	if (g_info && g_info->bms_psy) {
		power_supply_get_property(g_info->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
		if (val.intval <= 3) {
			pr_info("%s: low soc, disable OTG\n", __func__);
			schedule_delayed_work(&g_info->disable_otg_work, msecs_to_jiffies(200));
		}
	}

	return ALARMTIMER_NORESTART;
}
#endif

static void usbotg_disable_otg(struct work_struct *work)
{
	struct usbotg_boost *info = container_of(work, struct usbotg_boost, disable_otg_work.work);

	if (!info)
		return;

	charger_dev_enable_otg(g_info->xmusb350, false);
	charger_dev_enable_otg(g_info->bbc_charger, false);
	charger_dev_enable_otg(g_info->cp_master, false);
	enable_boost_polling(false);

	return;
}

int usb_otg_set_vbus(int is_on)
{
	union power_supply_propval val = {0,};

	if (is_on && g_info && g_info->bms_psy) {
		power_supply_get_property(g_info->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
		if (val.intval <= 3) {
			pr_info("%s: low soc don't enable OTG\n", __func__);
			return 0;
		}
	}

	if (!IS_ERR(drvvbus)) {
		if (is_on)
			pinctrl_select_state(drvvbus, drvvbus_high);
		else
			pinctrl_select_state(drvvbus, drvvbus_low);

		return 0;
	}

	if (!g_info)
		return -1;

#if CONFIG_MTK_GAUGE_VERSION == 30
	if (is_on) {
		charger_dev_enable_otg(g_info->xmusb350, true);
		charger_dev_enable_otg(g_info->cp_master, true);
		charger_dev_enable_otg(g_info->bbc_charger, true);
		charger_dev_set_boost_current_limit(g_info->bbc_charger, 1500000);
		charger_dev_kick_wdt(g_info->bbc_charger);
		enable_boost_polling(true);
	} else {
		charger_dev_enable_otg(g_info->xmusb350, false);
		charger_dev_enable_otg(g_info->bbc_charger, false);
		charger_dev_enable_otg(g_info->cp_master, false);
		enable_boost_polling(false);
	}
#else
	if (is_on) {
		charger_dev_enable_otg(g_info->xmusb350, true);
		charger_dev_enable_otg(g_info->cp_master, true);
		charger_dev_enable_otg(g_info->bbc_charger, true);
		charger_dev_set_boost_current_limit(g_info->bbc_charger, 1500000);
	} else {
		charger_dev_enable_otg(g_info->xmusb350, false);
		charger_dev_enable_otg(g_info->bbc_charger, false);
		charger_dev_enable_otg(g_info->cp_master, false);
	}
#endif
	return 0;
}

static int usbotg_boost_probe(struct platform_device *pdev)
{
	struct usbotg_boost *info = NULL;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;

	drvvbus = devm_pinctrl_get(dev);
	if (IS_ERR(drvvbus)) {
		pr_notice("Cannot find usb pinctrl!\n");
	} else {
		drvvbus_high = pinctrl_lookup_state(drvvbus, "drvvbus_high");
		if (IS_ERR(drvvbus_high)) {
			pr_notice("Cannot find usb pinctrl drvvbus_high\n");
			return -EINVAL;
		}
		drvvbus_low = pinctrl_lookup_state(drvvbus, "drvvbus_low");
		if (IS_ERR(drvvbus_low)) {
			pr_notice("Cannot find usb pinctrl drvvbus_low\n");
			return -EINVAL;
		}
		return 0;
	}

	info = devm_kzalloc(&pdev->dev, sizeof(struct usbotg_boost),
		GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	platform_set_drvdata(pdev, info);
	info->pdev = pdev;
	info->bbc_charger = get_charger_by_name("bbc");
	if (!info->bbc_charger) {
		pr_info("%s: get BBC charger device failed\n", __func__);
		return -ENODEV;
	}

	info->xmusb350 = get_charger_by_name("xmusb350");
	if (!info->xmusb350) {
		pr_info("%s: get xmusb350 device failed\n", __func__);
		return -ENODEV;
	}

	info->cp_master = get_charger_by_name("cp_master");
	if (!info->cp_master) {
		pr_info("%s: get cp_master device failed\n", __func__);
		return -ENODEV;
	}

	info->bms_psy = power_supply_get_by_name("bms");
	if (!info->bms_psy) {
		pr_info("%s: get bms_psy failed\n", __func__);
		return -ENODEV;
	}

	INIT_DELAYED_WORK(&info->disable_otg_work, usbotg_disable_otg);

#if CONFIG_MTK_GAUGE_VERSION == 30
	alarm_init(&info->otg_timer, ALARM_BOOTTIME,
		usbotg_alarm_timer_func);
	if (of_property_read_u32(node, "boost_period",
		(u32 *) &info->polling_interval))
		return -EINVAL;

	info->polling_interval = 15;
	info->boost_workq = create_singlethread_workqueue("boost_workq");
	INIT_WORK(&info->kick_work, usbotg_boost_kick_work);
#endif
	g_info = info;
	return 0;
}

static int usbotg_boost_remove(struct platform_device *pdev)
{
	if (g_info)
		cancel_delayed_work_sync(&g_info->disable_otg_work);

	return 0;
}

static const struct of_device_id usb_boost_of_match[] = {
	{.compatible = "mediatek,usb_boost"},
	{},
};

MODULE_DEVICE_TABLE(of, usb_boost_of_match);
static struct platform_driver usb_boost_driver = {
	.remove = usbotg_boost_remove,
	.probe = usbotg_boost_probe,
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



