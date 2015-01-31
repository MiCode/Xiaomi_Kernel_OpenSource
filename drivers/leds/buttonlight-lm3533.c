/*
 * leds-lm3533.c -- LM3533 LED driver
 *
 * Copyright (C) 2011-2012 Texas Instruments
 * Author: Johan Hovold <jhovold@gmail.com>
 * Copyright (C) 2015 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under  the terms of the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/mfd/core.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/delay.h>

#include <linux/mfd/lm3533.h>

#define LM3533_LVCTRLBANK_MIN		2
#define LM3533_LVCTRLBANK_MAX		5
#define LM3533_LVCTRLBANK_COUNT		4
#define LM3533_RISEFALLTIME_MAX		7
#define LM3533_ALS_CHANNEL_LV_MIN	1
#define LM3533_ALS_CHANNEL_LV_MAX	2

#define LM3533_REG_CTRLBANK_BCONF_BASE		0x1b
#define LM3533_REG_PATTERN_ENABLE		0x28
#define LM3533_REG_PATTERN_LOW_TIME_BASE	0x71
#define LM3533_REG_PATTERN_HIGH_TIME_BASE	0x72
#define LM3533_REG_PATTERN_RISETIME_BASE	0x74
#define LM3533_REG_PATTERN_FALLTIME_BASE	0x75

#define LM3533_REG_PATTERN_STEP			0x10

#define LM3533_REG_CTRLBANK_BCONF_MAPPING_MASK		0x04
#define LM3533_REG_CTRLBANK_BCONF_ALS_EN_MASK		0x02
#define LM3533_REG_CTRLBANK_BCONF_ALS_CHANNEL_MASK	0x01

#define LM3533_LED_FLAG_PATTERN_ENABLE		1

bool button_bl_open_flag = false;

struct lm3533_led {
	struct lm3533 *lm3533;
	struct lm3533_ctrlbank cb;
	struct led_classdev cdev;
	int id;

	struct mutex mutex;
	unsigned long flags;

	struct work_struct work;
	u8 new_brightness;
};

static inline struct lm3533_led *to_lm3533_led(struct led_classdev *cdev)
{
	return container_of(cdev, struct lm3533_led, cdev);
}

static inline int lm3533_led_get_ctrlbank_id(struct lm3533_led *led)
{
	return led->id + 2;
}

static inline u8 lm3533_led_get_lv_reg(struct lm3533_led *led, u8 base)
{
	return base + led->id;
}

static inline u8 lm3533_led_get_pattern(struct lm3533_led *led)
{
	return led->id;
}

static inline u8 lm3533_led_get_pattern_reg(struct lm3533_led *led,
								u8 base)
{
	return base + lm3533_led_get_pattern(led) * LM3533_REG_PATTERN_STEP;
}

static void lm3533_led_update(struct lm3533_led *led)
{
	static int old_brightness = -1;
	dev_info(led->cdev.dev, "%s - %u\n", __func__, led->new_brightness);
	if(0 == led->new_brightness)
	{
		if(old_brightness == led->new_brightness)
		{
			return ;
		}
		lm3533_ctrlbank_set_brightness(&led->cb, led->new_brightness);
		mutex_lock(&(led->lm3533->lock));
		if((!lcd_bl_open_flag)&&button_bl_open_flag)
		{
			lm3533_disable(led->lm3533);
		}
		button_bl_open_flag = false;
		mutex_unlock(&(led->lm3533->lock));
	}
	else
	{
		mutex_lock(&(led->lm3533->lock));
		if((!button_bl_open_flag)&&(!lcd_bl_open_flag))
		{
			lm3533_enable(led->lm3533);
			mdelay(2);
			lm3533_init(led->lm3533);
		}
		button_bl_open_flag = true;
		mutex_unlock(&(led->lm3533->lock));
		lm3533_ctrlbank_set_brightness(&led->cb, led->new_brightness);
	}
	old_brightness = led->new_brightness;
}

static void lm3533_led_work(struct work_struct *work)
{
	struct lm3533_led *led = container_of(work, struct lm3533_led, work);

	lm3533_led_update(led);
}

static void lm3533_led_set(struct led_classdev *cdev,
						enum led_brightness value)
{
	struct lm3533_led *led = to_lm3533_led(cdev);

	dev_info(led->cdev.dev, "%s - %d\n", __func__, value);

	led->new_brightness = value;
	schedule_work(&led->work);
}

static enum led_brightness lm3533_led_get(struct led_classdev *cdev)
{
	struct lm3533_led *led = to_lm3533_led(cdev);
	u8 val;
	int ret;

	ret = lm3533_ctrlbank_get_brightness(&led->cb, &val);
	if (ret)
		return ret;

	dev_dbg(led->cdev.dev, "%s - %u\n", __func__, val);

	return val;
}

static ssize_t show_id(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3533_led *led = to_lm3533_led(led_cdev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", led->id);
}

/*
 * Pattern generator rise/fall times:
 *
 *   0 - 2048 us (default)
 *   1 - 262 ms
 *   2 - 524 ms
 *   3 - 1.049 s
 *   4 - 2.097 s
 *   5 - 4.194 s
 *   6 - 8.389 s
 *   7 - 16.78 s
 */
static ssize_t show_risefalltime(struct device *dev,
					struct device_attribute *attr,
					char *buf, u8 base)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3533_led *led = to_lm3533_led(led_cdev);
	ssize_t ret;
	u8 reg;
	u8 val;

	reg = lm3533_led_get_pattern_reg(led, base);
	ret = lm3533_read(led->lm3533, reg, &val);
	if (ret)
		return ret;

	return scnprintf(buf, PAGE_SIZE, "%x\n", val);
}

static ssize_t show_risetime(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_risefalltime(dev, attr, buf,
					LM3533_REG_PATTERN_RISETIME_BASE);
}

static ssize_t show_falltime(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return show_risefalltime(dev, attr, buf,
					LM3533_REG_PATTERN_FALLTIME_BASE);
}

static ssize_t store_risefalltime(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len, u8 base)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3533_led *led = to_lm3533_led(led_cdev);
	u8 val;
	u8 reg;
	int ret;

	if (kstrtou8(buf, 0, &val) || val > LM3533_RISEFALLTIME_MAX)
		return -EINVAL;

	reg = lm3533_led_get_pattern_reg(led, base);
	ret = lm3533_write(led->lm3533, reg, val);
	if (ret)
		return ret;

	return len;
}

static ssize_t store_risetime(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
	return store_risefalltime(dev, attr, buf, len,
					LM3533_REG_PATTERN_RISETIME_BASE);
}

static ssize_t store_falltime(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
	return store_risefalltime(dev, attr, buf, len,
					LM3533_REG_PATTERN_FALLTIME_BASE);
}

static ssize_t show_als_channel(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3533_led *led = to_lm3533_led(led_cdev);
	unsigned channel;
	u8 reg;
	u8 val;
	int ret;

	reg = lm3533_led_get_lv_reg(led, LM3533_REG_CTRLBANK_BCONF_BASE);
	ret = lm3533_read(led->lm3533, reg, &val);
	if (ret)
		return ret;

	channel = (val & LM3533_REG_CTRLBANK_BCONF_ALS_CHANNEL_MASK) + 1;

	return scnprintf(buf, PAGE_SIZE, "%u\n", channel);
}

static ssize_t store_als_channel(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3533_led *led = to_lm3533_led(led_cdev);
	unsigned channel;
	u8 reg;
	u8 val;
	u8 mask;
	int ret;

	if (kstrtouint(buf, 0, &channel))
		return -EINVAL;

	if (channel < LM3533_ALS_CHANNEL_LV_MIN ||
					channel > LM3533_ALS_CHANNEL_LV_MAX)
		return -EINVAL;

	reg = lm3533_led_get_lv_reg(led, LM3533_REG_CTRLBANK_BCONF_BASE);
	mask = LM3533_REG_CTRLBANK_BCONF_ALS_CHANNEL_MASK;
	val = channel - 1;

	ret = lm3533_update(led->lm3533, reg, val, mask);
	if (ret)
		return ret;

	return len;
}

static ssize_t show_als_en(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3533_led *led = to_lm3533_led(led_cdev);
	bool enable;
	u8 reg;
	u8 val;
	int ret;

	reg = lm3533_led_get_lv_reg(led, LM3533_REG_CTRLBANK_BCONF_BASE);
	ret = lm3533_read(led->lm3533, reg, &val);
	if (ret)
		return ret;

	enable = val & LM3533_REG_CTRLBANK_BCONF_ALS_EN_MASK;

	return scnprintf(buf, PAGE_SIZE, "%d\n", enable);
}

static ssize_t store_als_en(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3533_led *led = to_lm3533_led(led_cdev);
	unsigned enable;
	u8 reg;
	u8 mask;
	u8 val;
	int ret;

	if (kstrtouint(buf, 0, &enable))
		return -EINVAL;

	reg = lm3533_led_get_lv_reg(led, LM3533_REG_CTRLBANK_BCONF_BASE);
	mask = LM3533_REG_CTRLBANK_BCONF_ALS_EN_MASK;

	if (enable)
		val = mask;
	else
		val = 0;

	ret = lm3533_update(led->lm3533, reg, val, mask);
	if (ret)
		return ret;

	return len;
}

static ssize_t show_linear(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3533_led *led = to_lm3533_led(led_cdev);
	u8 reg;
	u8 val;
	int linear;
	int ret;

	reg = lm3533_led_get_lv_reg(led, LM3533_REG_CTRLBANK_BCONF_BASE);
	ret = lm3533_read(led->lm3533, reg, &val);
	if (ret)
		return ret;

	if (val & LM3533_REG_CTRLBANK_BCONF_MAPPING_MASK)
		linear = 1;
	else
		linear = 0;

	return scnprintf(buf, PAGE_SIZE, "%x\n", linear);
}

static ssize_t store_linear(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3533_led *led = to_lm3533_led(led_cdev);
	unsigned long linear;
	u8 reg;
	u8 mask;
	u8 val;
	int ret;

	if (kstrtoul(buf, 0, &linear))
		return -EINVAL;

	reg = lm3533_led_get_lv_reg(led, LM3533_REG_CTRLBANK_BCONF_BASE);
	mask = LM3533_REG_CTRLBANK_BCONF_MAPPING_MASK;

	if (linear)
		val = mask;
	else
		val = 0;

	ret = lm3533_update(led->lm3533, reg, val, mask);
	if (ret)
		return ret;

	return len;
}

static ssize_t show_pwm(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3533_led *led = to_lm3533_led(led_cdev);
	u8 val;
	int ret;

	ret = lm3533_ctrlbank_get_pwm(&led->cb, &val);
	if (ret)
		return ret;

	return scnprintf(buf, PAGE_SIZE, "%u\n", val);
}

static ssize_t store_pwm(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3533_led *led = to_lm3533_led(led_cdev);
	u8 val;
	int ret;

	if (kstrtou8(buf, 0, &val))
		return -EINVAL;

	ret = lm3533_ctrlbank_set_pwm(&led->cb, val);
	if (ret)
		return ret;

	return len;
}

static ssize_t show_update(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	return 1;//scnprintf(buf, PAGE_SIZE, "");
}

static ssize_t store_update(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t len)
{
	u8 val;

	if (kstrtou8(buf, 0, &val))
		return -EINVAL;

	return len;
}

static LM3533_ATTR_RW(als_channel);
static LM3533_ATTR_RW(als_en);
static LM3533_ATTR_RW(falltime);
static LM3533_ATTR_RO(id);
static LM3533_ATTR_RW(linear);
static LM3533_ATTR_RW(pwm);
static LM3533_ATTR_RW(risetime);
static LM3533_ATTR_RW(update);

static struct attribute *lm3533_led_attributes[] = {
	&dev_attr_als_channel.attr,
	&dev_attr_als_en.attr,
	&dev_attr_falltime.attr,
	&dev_attr_id.attr,
	&dev_attr_linear.attr,
	&dev_attr_pwm.attr,
	&dev_attr_risetime.attr,
	&dev_attr_update.attr,
	NULL,
};

static umode_t lm3533_led_attr_is_visible(struct kobject *kobj,
					     struct attribute *attr, int n)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct lm3533_led *led = to_lm3533_led(led_cdev);
	umode_t mode = attr->mode;

	if (attr == &dev_attr_als_channel.attr ||
					attr == &dev_attr_als_en.attr) {
		if (!led->lm3533->have_als)
			mode = 0;
	}

	if (attr == &dev_attr_update.attr) {
		if (led->id)
			mode = 0;
	}
	return mode;
};

static struct attribute_group lm3533_led_attribute_group = {
	.is_visible	= lm3533_led_attr_is_visible,
	.attrs		= lm3533_led_attributes
};

static int __devinit lm3533_led_setup(struct lm3533_led *led,
					struct lm3533_led_platform_data *pdata)
{
	int ret;

	ret = lm3533_ctrlbank_set_max_current(&led->cb, pdata->max_current);
	if (ret)
		return ret;

	return lm3533_ctrlbank_set_pwm(&led->cb, pdata->pwm);
}

static int __devinit lm3533_led_probe(struct platform_device *pdev)
{
	struct lm3533 *lm3533;
	struct lm3533_led_platform_data *pdata;
	struct lm3533_led *led;
	int ret;

	dev_dbg(&pdev->dev, "%s\n", __func__);

	lm3533 = dev_get_drvdata(pdev->dev.parent);
	if (!lm3533)
		return -EINVAL;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "no platform data\n");
		return -EINVAL;
	}

	if (pdev->id < 0 || pdev->id >= LM3533_LVCTRLBANK_COUNT) {
		dev_err(&pdev->dev, "illegal LED id %d\n", pdev->id);
		return -EINVAL;
	}

	led = devm_kzalloc(&pdev->dev, sizeof(*led), GFP_KERNEL);
	if (!led)
		return -ENOMEM;

	led->lm3533 = lm3533;
	led->cdev.name = pdata->name;
	led->cdev.default_trigger = pdata->default_trigger;
	led->cdev.brightness_set = lm3533_led_set;
	led->cdev.brightness_get = lm3533_led_get;
	led->cdev.brightness = LED_OFF;
	led->id = pdev->id;

	mutex_init(&led->mutex);
	INIT_WORK(&led->work, lm3533_led_work);

	/* The class framework makes a callback to get brightness during
	 * registration so use parent device (for error reporting) until
	 * registered.
	 */
	led->cb.lm3533 = lm3533;
	led->cb.id = lm3533_led_get_ctrlbank_id(led);
	led->cb.dev = lm3533->dev;

	platform_set_drvdata(pdev, led);

	ret = led_classdev_register(pdev->dev.parent, &led->cdev);
	if (ret) {
		dev_err(&pdev->dev, "failed to register LED %d\n", pdev->id);
		return ret;
	}

	led->cb.dev = led->cdev.dev;

	ret = sysfs_create_group(&led->cdev.dev->kobj,
						&lm3533_led_attribute_group);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to create sysfs attributes\n");
		goto err_unregister;
	}

	ret = lm3533_led_setup(led, pdata);
	if (ret)
		goto err_sysfs_remove;

	ret = lm3533_ctrlbank_enable(&led->cb);
	if (ret)
		goto err_sysfs_remove;

	return 0;

err_sysfs_remove:
	sysfs_remove_group(&led->cdev.dev->kobj, &lm3533_led_attribute_group);
err_unregister:
	led_classdev_unregister(&led->cdev);
	flush_work(&led->work);

	return ret;
}

static int __devexit lm3533_led_remove(struct platform_device *pdev)
{
	struct lm3533_led *led = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s\n", __func__);

	lm3533_ctrlbank_disable(&led->cb);
	sysfs_remove_group(&led->cdev.dev->kobj, &lm3533_led_attribute_group);
	led_classdev_unregister(&led->cdev);
	flush_work(&led->work);

	return 0;
}

static void lm3533_led_shutdown(struct platform_device *pdev)
{

	struct lm3533_led *led = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s\n", __func__);

	lm3533_ctrlbank_disable(&led->cb);
	lm3533_led_set(&led->cdev, LED_OFF);		/* disable blink */
	flush_work(&led->work);
}

static struct platform_driver lm3533_led_driver = {
	.driver = {
		.name = "lm3533-leds",
		.owner = THIS_MODULE,
	},
	.probe		= lm3533_led_probe,
	.remove		= __devexit_p(lm3533_led_remove),
	.shutdown	= lm3533_led_shutdown,
};
module_platform_driver(lm3533_led_driver);

MODULE_AUTHOR("feng wei <fengwei84@gmail.com>");
MODULE_DESCRIPTION("LM3533 LED driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:lm3533-leds");
