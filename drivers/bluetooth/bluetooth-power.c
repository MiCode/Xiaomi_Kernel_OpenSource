/* Copyright (c) 2009-2010, 2013 The Linux Foundation. All rights reserved.
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

static struct of_device_id ar3002_match_table[] = {
	{	.compatible = "qca,ar3002" },
	{}
};

static int bt_reset_gpio;

static bool previous;

static int bluetooth_power(int on)
{
	int rc;

	pr_debug("%s  bt_gpio= %d\n", __func__, bt_reset_gpio);
	if (on) {
		rc = gpio_direction_output(bt_reset_gpio, 1);
		if (rc) {
			pr_err("%s: Unable to set direction\n", __func__);
			return rc;
		}
		msleep(100);
	} else {
		gpio_set_value(bt_reset_gpio, 0);
		rc = gpio_direction_input(bt_reset_gpio);
		if (rc) {
			pr_err("%s: Unable to set direction\n", __func__);
			return rc;
		}
		msleep(100);
	}
	return 0;
}

static int bluetooth_toggle_radio(void *data, bool blocked)
{
	int ret = 0;
	int (*power_control)(int enable);

	power_control = data;
	if (previous != blocked)
		ret = (*power_control)(!blocked);
	if (!ret)
		previous = blocked;
	return ret;
}

static const struct rfkill_ops bluetooth_power_rfkill_ops = {
	.set_block = bluetooth_toggle_radio,
};

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

static int __devinit bt_power_probe(struct platform_device *pdev)
{
	int ret = 0;

	dev_dbg(&pdev->dev, "%s\n", __func__);

	if (!pdev->dev.platform_data) {
		/* Update the platform data if the
		device node exists as part of device tree.*/
		if (pdev->dev.of_node) {
			pdev->dev.platform_data = bluetooth_power;
		} else {
			dev_err(&pdev->dev, "device node not set\n");
			return -ENOSYS;
		}
	}
	if (pdev->dev.of_node) {
		bt_reset_gpio = of_get_named_gpio(pdev->dev.of_node,
							"qca,bt-reset-gpio", 0);
		if (bt_reset_gpio < 0) {
			pr_err("bt-reset-gpio not available");
			return bt_reset_gpio;
		}
	}

	ret = gpio_request(bt_reset_gpio, "bt sys_rst_n");
	if (ret) {
		pr_err("%s: unable to request gpio %d (%d)\n",
			__func__, bt_reset_gpio, ret);
		return ret;
	}

	/* When booting up, de-assert BT reset pin */
	ret = gpio_direction_output(bt_reset_gpio, 0);
	if (ret) {
		pr_err("%s: Unable to set direction\n", __func__);
		return ret;
	}

	ret = bluetooth_power_rfkill_probe(pdev);

	return ret;
}

static int __devexit bt_power_remove(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "%s\n", __func__);

	bluetooth_power_rfkill_remove(pdev);

	return 0;
}

static struct platform_driver bt_power_driver = {
	.probe = bt_power_probe,
	.remove = __devexit_p(bt_power_remove),
	.driver = {
		.name = "bt_power",
		.owner = THIS_MODULE,
		.of_match_table = ar3002_match_table,
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
MODULE_DESCRIPTION("MSM Bluetooth power control driver");
MODULE_VERSION("1.40");

module_init(bluetooth_power_init);
module_exit(bluetooth_power_exit);
