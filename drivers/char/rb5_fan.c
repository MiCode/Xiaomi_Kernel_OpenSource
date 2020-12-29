// SPDX-License-Identifier: GPL-2.0-only

/**
 * Driver for control fan on QRB5165.
 *
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/kernel.h>

#define DEV_NAME "rb5_fan"

static int major;
static struct class *fan_class;

static u32 pwr_enable_gpio;

static const struct file_operations fan_fops = {
	.owner		= THIS_MODULE,
};

static int fan_probe(struct platform_device *pdev)
{
	struct device *dev = NULL;
	struct device_node *np = pdev->dev.of_node;

	pr_debug(DEV_NAME ": probe\n");

	major = register_chrdev(0, DEV_NAME, &fan_fops);
	if (major < 0) {
		pr_warn(DEV_NAME ": unable to get major %d\n", major);
		return major;
	}

	fan_class = class_create(THIS_MODULE, DEV_NAME);
	if (IS_ERR(fan_class))
		return PTR_ERR(fan_class);

	dev = device_create(fan_class, NULL, MKDEV(major, 0), NULL, DEV_NAME);
	if (IS_ERR(dev)) {
		pr_err(DEV_NAME ": failed to create device %d\n", dev);
		return PTR_ERR(dev);
	}

	pwr_enable_gpio = of_get_named_gpio(np, "qcom,pwr-enable-gpio", 0);
	if (!gpio_is_valid(pwr_enable_gpio)) {
		pr_err("%s qcom,pwr-enable-gpio not specified\n", __func__);
		goto error;
	}
	if (gpio_request(pwr_enable_gpio, "qcom,pwr-enable-gpio")) {
		pr_err("qcom,pwr-enable-gpio request failed\n");
		goto error;
	}
	gpio_direction_output(pwr_enable_gpio, 0);
	gpio_set_value(pwr_enable_gpio, 1);
	pr_debug("%s gpio:%d set to high\n", __func__, pwr_enable_gpio);
	return 0;

error:
	gpio_free(pwr_enable_gpio);
	return -EINVAL;
}

static int fan_remove(struct platform_device *pdev)
{
	pr_debug(DEV_NAME ": remove\n");
	gpio_free(pwr_enable_gpio);
	device_destroy(fan_class, MKDEV(major, 0));
	class_destroy(fan_class);
	unregister_chrdev(major, DEV_NAME);
	return 0;
}

static const struct of_device_id of_fan_dt_match[] = {
	{.compatible	= "qcom,rb5_fan_controller"},
	{},
};

MODULE_DEVICE_TABLE(of, of_fan_dt_match);

static struct platform_driver fan_driver = {
	.probe	= fan_probe,
	.remove	= fan_remove,
	.driver	= {
		.name	= DEV_NAME,
		.of_match_table	= of_fan_dt_match,
	},
};

static int __init fan_init(void)
{
	pr_debug(DEV_NAME ": init\n");
	return platform_driver_register(&fan_driver);
}

static void __exit fan_exit(void)
{
	pr_debug(DEV_NAME ": exit\n");
	platform_driver_unregister(&fan_driver);
}

module_init(fan_init);
module_exit(fan_exit);

MODULE_DESCRIPTION("Driver to control fan");
MODULE_LICENSE("GPL v2");
