/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/time.h>

#define S2US (1000*1000)

struct mcu_time {
	struct device *dev;
	unsigned gpio;
	long long time_us;
};

static ssize_t mcu_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mcu_time *time = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n", time->time_us);
}

static DEVICE_ATTR_RO(mcu_time);

static int mcu_time_sync_probe(struct platform_device *pdev)
{
	int ret;
	struct mcu_time *mcu_time;
	struct timeval current_time;

	mcu_time = devm_kzalloc(&pdev->dev, sizeof(struct mcu_time),
			GFP_KERNEL);
	if (!mcu_time)
		return -ENOMEM;

	platform_set_drvdata(pdev, mcu_time);
	mcu_time->dev = &pdev->dev;

	mcu_time->gpio = of_get_named_gpio(pdev->dev.of_node,
			"qcom,mcu-link-gpio", 0);
	if (!gpio_is_valid(mcu_time->gpio)) {
		dev_err(&pdev->dev, "Invalid gpio number.\n");
		return -EIO;
	}

	ret = devm_gpio_request(&pdev->dev, mcu_time->gpio, "mcu_gpio");
	if (ret) {
		dev_err(&pdev->dev, "gpio request %d failed\n", mcu_time->gpio);
		return ret;
	}

	ret = gpio_direction_output(mcu_time->gpio, 0);
	if (ret) {
		dev_err(&pdev->dev, "Failed to set gpio direction output: %d\n",
			mcu_time->gpio);
		return ret;
	}

	do_gettimeofday(&current_time);
	mcu_time->time_us = current_time.tv_sec * S2US + current_time.tv_usec;

	device_create_file(&pdev->dev, &dev_attr_mcu_time);

	return 0;
}

static int mcu_time_sync_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_mcu_time);
	return 0;
}

static struct of_device_id mcu_time_sync_match_table[] =  {
	{.compatible = "qcom,mcu-time-sync"},
	{},
};

static struct platform_driver mcu_time_sync_driver = {
	.probe = mcu_time_sync_probe,
	.remove = mcu_time_sync_remove,
	.driver = {
		.name = "mcu-time-sync",
		.owner = THIS_MODULE,
		.of_match_table = mcu_time_sync_match_table,
	},
};

module_platform_driver(mcu_time_sync_driver);

MODULE_DESCRIPTION("MCU time sync driver");
MODULE_LICENSE("GPL v2");
