/*
 * drivers/misc/max1749.c
 *
 * Driver for MAX1749, vibrator motor driver.
 *
 * Copyright (c) 2011-2013 NVIDIA Corporation, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>

#include <linux/slab.h>

#include "../staging/android/timed_output.h"

struct vibrator_data {
	struct timed_output_dev dev;
	struct hrtimer timer;
	struct regulator *regulator;
	struct work_struct work;
	bool vibrator_on;
};

static struct vibrator_data *data;


static void vibrator_start(void)
{
	if (!data->vibrator_on) {
		regulator_enable(data->regulator);
		data->vibrator_on = true;
	}
}

static void vibrator_stop(void)
{
	int ret;
	if (data->vibrator_on) {
		ret = regulator_is_enabled(data->regulator);
		if (ret > 0) {
			regulator_disable(data->regulator);
			data->vibrator_on = false;
		}
	}
}

static void vibrator_work_func(struct work_struct *work)
{
	vibrator_stop();
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	schedule_work(&data->work);
	return HRTIMER_NORESTART;
}

/*
 * Timeout value can be changed from sysfs entry
 * created by timed_output_dev.
 * echo 100 > /sys/class/timed_output/vibrator/enable
 */
static void vibrator_enable(struct timed_output_dev *dev, int value)
{
	hrtimer_cancel(&data->timer);
	if (value > 0) {
		vibrator_start();
		hrtimer_start(&data->timer,
			  ktime_set(value / 1000, (value % 1000) * 1000000),
			  HRTIMER_MODE_REL);
	} else
		vibrator_stop();
	return;
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	if (hrtimer_active(&data->timer)) {
		ktime_t r = hrtimer_get_remaining(&data->timer);
		struct timeval t = ktime_to_timeval(r);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	} else
		return 0;
}

static struct timed_output_dev vibrator_dev = {
	.name		= "vibrator",
	.get_time	= vibrator_get_time,
	.enable		= vibrator_enable,
};

static int vibrator_probe(struct platform_device *pdev)
{
	int ret;
	data = kzalloc(sizeof(struct vibrator_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->regulator = regulator_get(NULL, "vdd_vbrtr");
	if (IS_ERR_OR_NULL(data->regulator)) {
		pr_err("vibrator_init:Couldn't get regulator vdd_vbrtr\n");
		data->regulator = NULL;
		ret = PTR_ERR(data->regulator);
		goto err;
	}
	hrtimer_init(&data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	/* Intialize the work queue */
	INIT_WORK(&data->work, vibrator_work_func);
	data->timer.function = vibrator_timer_func;
	data->dev = vibrator_dev;
	data->vibrator_on = false;

	ret = timed_output_dev_register(&data->dev);
	if (ret)
		goto err2;

	return 0;

err2:
	regulator_put(data->regulator);
err:
	kfree(data);
	return ret;
}

static int vibrator_remove(struct platform_device *pdev)
{
	timed_output_dev_unregister(&data->dev);
	regulator_put(data->regulator);
	kfree(data);

	return 0;
}

static struct platform_driver vibrator_driver = {
	.probe = vibrator_probe,
	.remove = vibrator_remove,
	.driver = {
		.name = "tegra-vibrator",
		.owner = THIS_MODULE,
	},
};

static int __init vibrator_init(void)
{
	return platform_driver_register(&vibrator_driver);
}

static void __exit vibrator_exit(void)
{
	platform_driver_unregister(&vibrator_driver);
}

MODULE_DESCRIPTION("timed output vibrator device");
MODULE_AUTHOR("GPL");

module_init(vibrator_init);
module_exit(vibrator_exit);
