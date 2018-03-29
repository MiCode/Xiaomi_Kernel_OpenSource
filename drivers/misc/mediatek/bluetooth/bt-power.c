/* Copyright (c) 2009-2010, 2013-2014 The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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
/*
 * Bluetooth Power Switch Module
 * controls power to external Bluetooth device
 * with interface to power management device
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <mt-plat/mtk_rtc.h>

#define BT_PWR_DBG(fmt, arg...)  pr_debug("%s: " fmt "\n" , __func__ , ## arg)
#define BT_PWR_INFO(fmt, arg...) pr_info("%s: " fmt "\n" , __func__ , ## arg)
#define BT_PWR_ERR(fmt, arg...)  pr_err("%s: " fmt "\n" , __func__ , ## arg)

struct bluetooth_power_platform_data {
/* Bluetooth reset gpio */
int bt_gpio_sys_rst;
/* Optional: Bluetooth power setup function */
int (*bt_power_setup) (int);
};


static struct of_device_id bt_power_match_table[] = {
	{	.compatible = "bcm,bcm4356-bt" },
	{}
};

static struct bluetooth_power_platform_data *bt_power_pdata;
static struct platform_device *btpdev;
static bool previous;

static int bt_configure_gpios(int on)
{
	int rc = 0;
	int bt_reset_gpio = bt_power_pdata->bt_gpio_sys_rst;

	BT_PWR_INFO("%s  bt_gpio=%d on: %d", __func__, bt_reset_gpio, on);

	if (on) {
		rc = gpio_request(bt_reset_gpio, "bt_sys_rst_n");
		if (rc) {
			BT_PWR_ERR("unable to request gpio %d (%d)\n",
					bt_reset_gpio, rc);
			return rc;
		}

		/* Enable RTC_GPIO 32k clock */
		rtc_gpio_enable_32k(RTC_GPIO_USER_WIFI);

		rc = gpio_direction_output(bt_reset_gpio, 0);
		if (rc) {
			BT_PWR_ERR("Unable to set direction\n");
			return rc;
		}
		msleep(50);
		rc = gpio_direction_output(bt_reset_gpio, 1);
		if (rc) {
			BT_PWR_ERR("Unable to set direction\n");
			return rc;
		}
		msleep(50);
	} else {
		gpio_set_value(bt_reset_gpio, 0);
		msleep(100);
	}
	return rc;
}

static int bluetooth_power(int on)
{
	int rc = 0;

	if (on) {
		if (bt_power_pdata->bt_gpio_sys_rst) {
			rc = bt_configure_gpios(on);
			if (rc < 0) {
				BT_PWR_ERR("bt_power gpio config failed");
				goto gpio_fail;
			}
		}
	} else {
		bt_configure_gpios(on);

gpio_fail:
	if (bt_power_pdata->bt_gpio_sys_rst)
		gpio_free(bt_power_pdata->bt_gpio_sys_rst);
	}

	return rc;
}

static int bluetooth_toggle_radio(void *data, bool blocked)
{
	int ret = 0;
	int (*power_control)(int enable);

	power_control =
		((struct bluetooth_power_platform_data *)data)->bt_power_setup;

	if (previous != blocked)
		ret = (*power_control)(!blocked);
	if (!ret)
		previous = blocked;
	return ret;
}

static const struct rfkill_ops bluetooth_power_rfkill_ops = {
	.set_block = bluetooth_toggle_radio,
};


static ssize_t enable_extldo(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, 6, "%s", "false");
}

static DEVICE_ATTR(extldo, S_IRUGO, enable_extldo, NULL);

static int bluetooth_power_rfkill_probe(struct platform_device *pdev)
{
	struct rfkill *rfkill;
	int ret;

	rfkill = rfkill_alloc("bt_power", &pdev->dev, RFKILL_TYPE_BLUETOOTH,
			      &bluetooth_power_rfkill_ops,
			      pdev->dev.platform_data);
	if (!rfkill) {
		dev_err(&pdev->dev, "rfkill allocate failed\n");
		return -ENOMEM;
	}

	/* add file into rfkill0 to handle LDO27 */
	ret = device_create_file(&pdev->dev, &dev_attr_extldo);

	/* force Bluetooth off during init to allow for user control */
	rfkill_init_sw_state(rfkill, 1);
	previous = 1;

	ret = rfkill_register(rfkill);
	if (ret) {
		dev_err(&pdev->dev, "rfkill register failed=%d\n", ret);
		rfkill_destroy(rfkill);
		return ret;
	}

	platform_set_drvdata(pdev, rfkill);

	return 0;
}

static void bluetooth_power_rfkill_remove(struct platform_device *pdev)
{
	struct rfkill *rfkill;

	dev_dbg(&pdev->dev, "%s\n", __func__);

	rfkill = platform_get_drvdata(pdev);
	if (rfkill)
		rfkill_unregister(rfkill);
	rfkill_destroy(rfkill);
	platform_set_drvdata(pdev, NULL);
}

static int bt_power_populate_dt_pinfo(struct platform_device *pdev)
{
	int ret;

	if (!bt_power_pdata)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		bt_power_pdata->bt_gpio_sys_rst =
			of_get_named_gpio(pdev->dev.of_node,
						"bcm_bt_reg_on", 0);
		if (bt_power_pdata->bt_gpio_sys_rst < 0) {
			BT_PWR_ERR("bt reg on gpio not provided in device tree");
			return bt_power_pdata->bt_gpio_sys_rst;
		}

		ret = gpio_direction_output(bt_power_pdata->bt_gpio_sys_rst, 0);
		if (ret < 0) {
			BT_PWR_ERR("Unable to set int direction for gpio %d\n",
				bt_power_pdata->bt_gpio_sys_rst);
			return ret;
		}
	}

	bt_power_pdata->bt_power_setup = bluetooth_power;

	return 0;
}

static int bt_power_probe(struct platform_device *pdev)
{
	int ret = 0;
	dev_dbg(&pdev->dev, "%s\n", __func__);

	bt_power_pdata =
		kzalloc(sizeof(struct bluetooth_power_platform_data),
			GFP_KERNEL);

	if (!bt_power_pdata) {
		BT_PWR_ERR("Failed to allocate memory");
		return -ENOMEM;
	}

	if (pdev->dev.of_node) {
		ret = bt_power_populate_dt_pinfo(pdev);
		if (ret < 0) {
			BT_PWR_ERR("Failed to populate device tree info");
			goto free_pdata;
		}
		pdev->dev.platform_data = bt_power_pdata;
	} else if (pdev->dev.platform_data) {
		/* Optional data set to default if not provided */
		if (!((struct bluetooth_power_platform_data *)
			(pdev->dev.platform_data))->bt_power_setup)
			((struct bluetooth_power_platform_data *)
				(pdev->dev.platform_data))->bt_power_setup =
						bluetooth_power;

		memcpy(bt_power_pdata, pdev->dev.platform_data,
			sizeof(struct bluetooth_power_platform_data));
	} else {
		BT_PWR_ERR("Failed to get platform data");
		goto free_pdata;
	}

	if (bluetooth_power_rfkill_probe(pdev) < 0)
		goto free_pdata;

	btpdev = pdev;

	return 0;

free_pdata:
	kfree(bt_power_pdata);

	return ret;
}

static int bt_power_remove(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "%s\n", __func__);

	bluetooth_power_rfkill_remove(pdev);

	kfree(bt_power_pdata);

	return 0;
}

static struct platform_driver bt_power_driver = {
	.probe = bt_power_probe,
	.remove = bt_power_remove,
	.driver = {
		.name = "bt_power",
		.owner = THIS_MODULE,
		.of_match_table = bt_power_match_table,
	},
};

static int __init bluetooth_power_init(void)
{
	int ret;

	ret = platform_driver_register(&bt_power_driver);

	return ret;
}

static void __exit bluetooth_power_exit(void)
{
	platform_driver_unregister(&bt_power_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("BCM Bluetooth power control driver");
MODULE_VERSION("1.40");

module_init(bluetooth_power_init);
module_exit(bluetooth_power_exit);
