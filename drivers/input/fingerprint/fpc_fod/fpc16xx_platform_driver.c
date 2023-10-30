// SPDX-License-Identifier: GPL-2.0
/*
 * FPC Fingerprint sensor device driver
 *
 * This driver will control the platform resources that the FPC fingerprint
 * sensor needs to operate. The major things are probing the sensor to check
 * that it is actually connected and let the Kernel know this and with that also
 * enabling and disabling of regulators, enabling and disabling of platform
 * clocks.
 * *
 * The driver will expose most of its available functionality in sysfs which
 * enables dynamic control of these features from eg. a user space process.
 *
 * Copyright (c) 2021 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */
#define DEBUG
//#define FPC_ENABLE_POWER_CTRL
//#define FPC_ENABLE_SCREEN_MONITOR
#define pr_fmt(fmt)     "fpc_fod: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/delay.h>

#ifdef FPC_ENABLE_SCREEN_MONITOR
#include <linux/workqueue.h>
#include <linux/soc/qcom/panel_event_notifier.h>
static struct drm_panel *active_panel;
static void *cookie = NULL;
#endif

struct fpc16xx_data {
	struct device *dev;
	bool clock_enabled;
	struct regulator *pwr_regulator;
	struct clk *clk;

	bool fb_black;
	struct mutex fb_lock;
	struct mutex lock;
	struct delayed_work screen_state_dw;
	struct workqueue_struct *screen_state_wq;
};

#ifdef FPC_ENABLE_POWER_CTRL
static int power_ctrl_init(struct fpc16xx_data *fpc16xx)
{
	int ret = 0;

	/* Init resources here */
	fpc16xx->pwr_regulator = regulator_get(fpc16xx->dev, "fpvdd");
	if (IS_ERR(fpc16xx->pwr_regulator)) {
		pr_err("%s: regulator_get() failed!", __func__);
		ret = PTR_ERR(fpc16xx->pwr_regulator);
		fpc16xx->pwr_regulator = NULL;
		goto err;
	}

	ret = regulator_set_voltage(fpc16xx->pwr_regulator, 3000000, 3000000);
	if (ret) {
		pr_err("%s: regulator_set_voltage() failed!", __func__);
		goto err;
	}

	ret = regulator_set_load(fpc16xx->pwr_regulator, 200000);
	if (ret) {
		pr_err("%s: regulator_set_load failed!", __func__);
		goto err;
	}

	return 0;

err:
	if (fpc16xx->pwr_regulator) {
		regulator_put(fpc16xx->pwr_regulator);
	}

	return ret;
}

static ssize_t power_ctrl_get(struct device *dev,
		struct device_attribute *attribute, char *buffer)
{
	int enabled;
	struct fpc16xx_data *fpc16xx = dev_get_drvdata(dev);

	mutex_lock(&fpc16xx->lock);
	enabled = regulator_is_enabled(fpc16xx->pwr_regulator);
	mutex_unlock(&fpc16xx->lock);

	return scnprintf(buffer, PAGE_SIZE, "%i\n", enabled);
}

static ssize_t power_ctrl_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0, voltage = 0, enabled = 0;
	struct regulator *vreg = NULL;
	struct fpc16xx_data *fpc16xx = dev_get_drvdata(dev);

	mutex_lock(&fpc16xx->lock);

	if (fpc16xx->pwr_regulator == NULL) {
		ret = power_ctrl_init(fpc16xx);
		if (ret || !fpc16xx->pwr_regulator) {
			pr_err("%s: fpc power_init_cfg failed!", __func__);
			goto err;
		}
	}

	vreg = fpc16xx->pwr_regulator;

	if (!strncmp(buf, "enable", strlen("enable"))) {
		enabled = regulator_is_enabled(vreg);
		if (enabled <= 0) {
			ret = regulator_enable(vreg);
			usleep_range(100*1000, 200*1000);
		}
	} else if (!strncmp(buf, "disable", strlen("disable"))) {
		enabled = regulator_is_enabled(vreg);
		if (enabled > 0) {
			ret = regulator_disable(vreg);
		}
	} else if (!strncmp(buf, "force_disable", strlen("force_disable"))) {
		ret = regulator_force_disable(vreg);
	} else {
		ret = -EINVAL;
	}

	voltage = regulator_get_voltage(vreg);
	enabled = regulator_is_enabled(vreg);

err:
	pr_info("%s, finish cmd:%s, ret:%d, enabled:%d, voltage:%d\n", __func__,
			buf, ret, enabled, voltage);

	mutex_unlock(&fpc16xx->lock);

	return ret ? ret : count;
}
#else
static ssize_t power_ctrl_get(struct device *dev,
		struct device_attribute *attribute, char *buffer)
{
	(void)dev;
	(void)attribute;
	(void)buffer;
	return scnprintf(buffer, PAGE_SIZE, "%i\n", 1);
}

static ssize_t power_ctrl_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	(void)dev;
	(void)attr;
	(void)buf;
	return count;
}
#endif

static DEVICE_ATTR(power_ctrl, S_IWUSR, power_ctrl_get, power_ctrl_set);

static ssize_t clock_ctrl_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct fpc16xx_data *fpc16xx = dev_get_drvdata(dev);

	if (!strncmp(buf, "enable", strlen("enable")) && fpc16xx->clock_enabled == false) {

		/* Enable clock here */

		fpc16xx->clock_enabled = true;
		pr_info("%s: fpc16xx clock enabled!", __func__);
		return count;
	} else if (!strncmp(buf, "disable", strlen("disable")) && fpc16xx->clock_enabled == true) {

		/* Disable clock here */

		fpc16xx->clock_enabled = false;
		pr_info("%s: fpc16xx clock disabled!", __func__);
		return count;
	} else {
		pr_err("%s: invalid value or state!", __func__);
		return -EINVAL;
	}
}

static DEVICE_ATTR_WO(clock_ctrl);

static ssize_t screen_state_show(struct device *device,
	    struct device_attribute *attribute,
	    char *buffer)
{
	char value;
	struct fpc16xx_data *fpc16xx = dev_get_drvdata(device);

	mutex_lock(&fpc16xx->fb_lock);
	value = fpc16xx->fb_black ? '0' : '1';
	mutex_unlock(&fpc16xx->fb_lock);

	*buffer = value;
	return 1;
}

static DEVICE_ATTR_RO(screen_state);

static struct attribute *fpc16xx_attrs[] = {
	&dev_attr_power_ctrl.attr,
	&dev_attr_clock_ctrl.attr,
	&dev_attr_screen_state.attr,
	NULL
};

ATTRIBUTE_GROUPS(fpc16xx);

#ifdef FPC_ENABLE_SCREEN_MONITOR
static int fpc_check_panel(struct device_node *np)
{
	int i;
	int count;
	struct device_node *node;
	struct drm_panel *panel;

	if(!np) {
		pr_err("device is null,failed to find active panel");
		return -ENODEV;
	}
	count = of_count_phandle_with_args(np, "panel", NULL);
	pr_info("%s:of_count_phandle_with_args:count=%d", __func__,count);
	if (count <= 0)
		return -ENODEV;
	for (i = 0; i < count; i++) {
		node = of_parse_phandle(np, "panel", i);
		panel = of_drm_find_panel(node);
		of_node_put(node);
		if (!IS_ERR(panel)) {
			active_panel = panel;
			pr_info("%s:active_panel = panel", __func__);
			return 0;
		} else {
			active_panel = NULL;
			pr_info("%s:active_panel = NULL", __func__);
		}
	}
	return PTR_ERR(panel);
}

static void fpc_screen_state_for_fingerprint_callback(enum panel_event_notifier_tag notifier_tag,
			struct panel_event_notification *notification, void *client_data)
{

	struct fpc16xx_data *fpc16xx = client_data;
	if (!fpc16xx) {
		pr_err("%s:Invalid client data", __func__);
		return;
	}

	if (!notification) {
		pr_err("%s:Invalid notification", __func__);
		return;
	}

	if(notification->notif_data.early_trigger) {
		return;
	}
	if(notifier_tag == PANEL_EVENT_NOTIFICATION_PRIMARY){
		switch (notification->notif_type) {
			case DRM_PANEL_EVENT_UNBLANK:
				pr_info("%s:DRM_PANEL_EVENT_UNBLANK", __func__);
				mutex_lock(&fpc16xx->fb_lock);
				fpc16xx->fb_black = false;
				mutex_unlock(&fpc16xx->fb_lock);
				sysfs_notify(&fpc16xx->dev->kobj, NULL, dev_attr_screen_state.attr.name);
				pr_info("%s:exit", __func__);
				break;
			case DRM_PANEL_EVENT_BLANK:
				pr_info("%s:DRM_PANEL_EVENT_BLANK", __func__);
				mutex_lock(&fpc16xx->fb_lock);
				fpc16xx->fb_black = true;
				mutex_unlock(&fpc16xx->fb_lock);
				pr_info("%s:exit", __func__);
				break;
			default:
				break;
		}
	}
}

static void fpc_register_panel_notifier_work(struct work_struct *work)
{
	int error = 0;
	static retry_count = 0;
	struct device_node *node;
	struct fpc16xx_data *fpc16xx = container_of(work, struct fpc16xx_data, screen_state_dw.work);
	node = of_find_node_by_name(NULL, "fingerprint-screen");
	if (!node) {
		pr_err("%s ERROR: Cannot find node with panel!", __func__);
		return;
	}

	error = fpc_check_panel(node);
	if (active_panel) {
		pr_info("success to get active panel, retry times = %d",retry_count);
		if (!cookie) {
			cookie = panel_event_notifier_register(PANEL_EVENT_NOTIFICATION_PRIMARY,
					PANEL_EVENT_NOTIFIER_CLIENT_FINGERPRINT, active_panel,
					fpc_screen_state_for_fingerprint_callback, (void*)fpc16xx);
			if (IS_ERR(cookie))
				pr_err("%s:Failed to register for active_panel events", __func__);
			else
				pr_info("%s:active_panel_event_notifier_register register succeed", __func__);
		}
	} else {
		if (retry_count++ < 5) {
			pr_err("Failed to register panel notifier %d times, try again 5s later", retry_count);
			queue_delayed_work(fpc16xx->screen_state_wq, &fpc16xx->screen_state_dw, msecs_to_jiffies(5000));
		} else {
			pr_err("Failed to register panel notifier, not try");
		}
		return;
	}
}
#endif

static const struct of_device_id fpc16xx_of_match[] = {
	{ .compatible = "fpc,fpc16xx", },
	{}
};

MODULE_DEVICE_TABLE(of, fpc16xx_of_match);

static int fpc16xx_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct fpc16xx_data *fpc16xx;
	int ret = 0;

	pr_info("fpc16xx probe start!");
	fpc16xx = devm_kzalloc(dev, sizeof(struct fpc16xx_data), GFP_KERNEL);
	if (!fpc16xx) {
		return -ENOMEM;
	}

	fpc16xx->dev = dev;
	fpc16xx->fb_black = false;
	fpc16xx->clock_enabled = false;
	mutex_init(&fpc16xx->lock);
	mutex_init(&fpc16xx->fb_lock);
	platform_set_drvdata(pdev, fpc16xx);

	ret = devm_device_add_groups(dev, fpc16xx_groups);
	if (ret < 0) {
		pr_err("%s:fpc16xx probe failed: %d", __func__, ret);
		return ret;
	}

#ifdef FPC_ENABLE_SCREEN_MONITOR
	fpc16xx->screen_state_wq = create_singlethread_workqueue("screen_state_wq");
	if (fpc16xx->screen_state_wq) {
		INIT_DELAYED_WORK(&fpc16xx->screen_state_dw, fpc_register_panel_notifier_work);
		pr_info("fpc16xx screen monitor will start 5s later!");
		queue_delayed_work(fpc16xx->screen_state_wq, &fpc16xx->screen_state_dw, msecs_to_jiffies(5000));
	}
#endif
	pr_info("fpc16xx probe is done!");

	return 0;
}

static int fpc16xx_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct fpc16xx_data *fpc16xx = dev_get_drvdata(dev);

#ifdef FPC_ENABLE_POWER_CTRL
	if (fpc16xx->pwr_regulator) {
		if (regulator_is_enabled(fpc16xx->pwr_regulator)) {
			regulator_disable(fpc16xx->pwr_regulator);
		}
	}
#endif

#ifdef FPC_ENABLE_SCREEN_MONITOR
	if (fpc16xx->screen_state_wq) {
		destroy_workqueue(fpc16xx->screen_state_wq);
	}

	if (active_panel && !IS_ERR(cookie)) {
		panel_event_notifier_unregister(cookie);
		pr_info("%s:panel_event_notifier_unregister", __func__);
	} else {
		pr_err("%s:active_panel_event_notifier_unregister falt", __func__);
	}
#endif
	mutex_destroy(&fpc16xx->lock);
	mutex_destroy(&fpc16xx->fb_lock);
	pr_info("fpc16xx is down!");
	return 0;
}

static struct platform_driver fpc16xx_driver = {
	.driver = {
		.name           = "fpc16xx",
		.of_match_table = fpc16xx_of_match,
	},
	.probe  = fpc16xx_probe,
	.remove = fpc16xx_remove
};

module_platform_driver(fpc16xx_driver);

MODULE_AUTHOR("Liang Meng <liang.meng@fingerprints.com>");
MODULE_DESCRIPTION("fingerprint optical device driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
