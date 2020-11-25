// SPDX-License-Identifier: GPL-2.0-only

/**
 * Driver for enable gpio 140/145,116/117 on QRB5165.
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

#define DEV_NAME "rb5_gpio_enable"

static int major;
static struct class *gp_en_class;

static u32 enable_gpio;

static const struct file_operations rb5_gp_en_fops = {
	.owner		= THIS_MODULE,
};

static int rb5_gpios_enable_probe(struct platform_device *pdev)
{
	struct device *dev = NULL;
	struct device_node *np = pdev->dev.of_node;

	pr_debug(DEV_NAME " probe\n");
	major = register_chrdev(0, DEV_NAME, &rb5_gp_en_fops);
	if (major < 0) {
		pr_warn(DEV_NAME ": unable to get major %d\n", major);
		return major;
	}

	gp_en_class = class_create(THIS_MODULE, DEV_NAME);
	if (IS_ERR(gp_en_class))
		return PTR_ERR(gp_en_class);

	dev = device_create(gp_en_class, NULL, MKDEV(major, 0), NULL, DEV_NAME);
	if (IS_ERR(dev)) {
		pr_err(DEV_NAME ": failed to create device %d\n", dev);
		return PTR_ERR(dev);
	}

	enable_gpio = of_get_named_gpio(np, "qcom,enable-gpio", 0);
	if (!gpio_is_valid(enable_gpio)) {
		pr_err("%s qcom,enable-gpio not specified\n", __func__);
		goto error;
	}
	if (gpio_request(enable_gpio, "qcom,enable-gpio")) {
		pr_err("qcom,enable-gpio request failed\n");
		goto error;
	}
	gpio_direction_output(enable_gpio, 1);
	gpio_set_value(enable_gpio, 0);
	pr_debug("%s gpio:%d set to low\n", __func__, enable_gpio);
	return 0;

error:
	gpio_free(enable_gpio);
	return -EINVAL;
}

static int rb5_gpios_enable_remove(struct platform_device *pdev)
{
	pr_debug(DEV_NAME " remove\n");
	gpio_free(enable_gpio);
	device_destroy(gp_en_class, MKDEV(major, 0));
	class_destroy(gp_en_class);
	unregister_chrdev(major, DEV_NAME);
	return 0;
}

static const struct of_device_id of_rb5_gpios_enable_dt_match[] = {
	{.compatible	= "qcom,rb5_gpios_enable"},
	{},
};

MODULE_DEVICE_TABLE(of, of_rb5_gpios_enable_dt_match);

static struct platform_driver rb5_gpios_enable_driver = {
	.probe	= rb5_gpios_enable_probe,
	.remove	= rb5_gpios_enable_remove,
	.driver	= {
		.name	= "rb5_gpios_enable",
		.of_match_table	= of_rb5_gpios_enable_dt_match,
	},
};

static int __init rb5_gpios_enable_init(void)
{
	pr_debug(DEV_NAME " init\n");
	return platform_driver_register(&rb5_gpios_enable_driver);
}

static void __exit rb5_gpios_enable_exit(void)
{
	pr_debug(DEV_NAME " exit\n");
	platform_driver_unregister(&rb5_gpios_enable_driver);
}

module_init(rb5_gpios_enable_init);
module_exit(rb5_gpios_enable_exit);

MODULE_DESCRIPTION("Driver to enable rb5 gpio 140/145,116/117");
MODULE_LICENSE("GPL v2");
