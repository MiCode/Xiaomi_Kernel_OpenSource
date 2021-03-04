/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/errno.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

/*
 * Define vibration periods: default(5sec), min(50ms), max(15sec) and
 * overdrive(30ms).
 */
#define QPNP_VIB_MIN_PLAY_MS		50
#define QPNP_VIB_PLAY_MS		1000
#define QPNP_VIB_MAX_PLAY_MS		15000
#define QPNP_VIB_OVERDRIVE_PLAY_MS	30

struct vib_pwm_chip {
	struct device		*dev;
	struct led_classdev	cdev;
	struct regmap		*regmap;
	struct mutex		lock;
	struct hrtimer		stop_timer;
	struct hrtimer		overdrive_timer;
	struct work_struct	vib_work;
	struct work_struct	overdrive_work;

	u16			base;
	int			state;
	int			effect_idx;
	u64			vib_play_ms;
	bool			vib_enabled;
	bool			disable_overdrive;

	struct pwm_device       *pwm_dev;
	struct pwm_device       *pwm_dir;

	u64	                pre_period_ns;
	u64	                period_ns;
	u64	                duty_ns;

	u32                     en_gpio;
	u32                     en_gpio_flags;

	int			pwm_nums;
	const char		*label;
	u8			id;
		
	
};

static int qpnp_vibrator_play_on(struct vib_pwm_chip *chip)
{
	struct pwm_state pstate;
	int err;

	printk("vib---qpnp_vibrator_play_on\n");

	if (chip->pwm_dev == NULL) {
		printk("vib---exit---qpnp_vibrator_play_on\n");
		return -ENOMEM;
	}

	pwm_get_state(chip->pwm_dev, &pstate);
	pstate.enabled = true;
	pstate.polarity = PWM_POLARITY_NORMAL;

	if (chip->effect_idx == 1) {
		pstate.period = 35 * 1000000;
		pstate.duty_cycle = 15 * 1000000;
	} else if (chip->effect_idx == 2) {
		pstate.period = 50 * 1000000;
		pstate.duty_cycle = 25 * 1000000;
	} else if (chip->effect_idx == 3) {
		pstate.period = 60 * 1000000;
		pstate.duty_cycle = 30 * 1000000;
	} else {
		pstate.period = 50000;
		pstate.duty_cycle = 42500;
	}

	printk("vib---pstate.period=%d\n", pstate.period);
	printk("vib---pstate.duty_cycle=%d\n", pstate.duty_cycle);

	if (gpio_is_valid(chip->en_gpio)) {
		err = gpio_direction_output(chip->en_gpio, 1);
		if (err)
			pr_err("vib---en fail, ret=%d\n", err);
	}

	err = pwm_apply_state(chip->pwm_dev, &pstate);
	if (err) {
		printk("vib---Apply PWM state for vib failed, err=%d\n", err);
	}

	return err;

}

static int qpnp_vibrator_play_off(struct vib_pwm_chip *chip)
{
	struct pwm_state pstate;
	int err;

	printk("vib---qpnp_vibrator_play_off\n");

	if (chip->pwm_dev == NULL) {
		printk("vib---exit---qpnp_vibrator_play_on\n");
		return -ENOMEM;
	}

	pwm_get_state(chip->pwm_dev, &pstate);
	pstate.enabled = false;
	//pstate.period = 10000;
	pstate.duty_cycle = 0;

	if (gpio_is_valid(chip->en_gpio)) {
		err = gpio_direction_output(chip->en_gpio, 0);
		if (err)
			pr_err("vib---en fail, ret=%d\n", err);
	}

	err = pwm_apply_state(chip->pwm_dev, &pstate);
	if (err) {
		printk("vib---Apply PWM state for vib failed, err=%d\n", err);
	}

	return err;

}

static void qpnp_vib_work(struct work_struct *work)
{
	struct vib_pwm_chip *chip = container_of(work, struct vib_pwm_chip,
						vib_work);
	int ret = 0;
	int en_time = 0;

	if (chip->state) {
		if (!chip->vib_enabled) {
			ret = qpnp_vibrator_play_on(chip);
		}
		if (ret == 0) {
			if (chip->effect_idx == 1) {
				en_time = 30;
			} else if (chip->effect_idx == 2) {
				en_time = 45;
			} else if (chip->effect_idx == 3) {
				en_time = 55;
			} else {
				en_time = chip->vib_play_ms;
			}
			printk("vib---en_time=%d\n", en_time);
			hrtimer_start(&chip->stop_timer,
				      ms_to_ktime(en_time),
				      HRTIMER_MODE_REL);
		}
	} else {
		if (!chip->disable_overdrive) {
			hrtimer_cancel(&chip->overdrive_timer);
			cancel_work_sync(&chip->overdrive_work);
		}
		ret = qpnp_vibrator_play_off(chip);
	}
}

static enum hrtimer_restart vib_stop_timer(struct hrtimer *timer)
{
	struct vib_pwm_chip *chip = container_of(timer, struct vib_pwm_chip,
					     stop_timer);

	chip->state = 0;
	schedule_work(&chip->vib_work);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart vib_overdrive_timer(struct hrtimer *timer)
{
	struct vib_pwm_chip *chip = container_of(timer, struct vib_pwm_chip,
					     overdrive_timer);
	schedule_work(&chip->overdrive_work);

	return HRTIMER_NORESTART;
}

static ssize_t qpnp_vib_show_state(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct vib_pwm_chip *chip = container_of(cdev, struct vib_pwm_chip,
						cdev);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->vib_enabled);
}

static ssize_t qpnp_vib_store_state(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	/* At present, nothing to do with setting state */
	return count;
}

static ssize_t qpnp_vib_show_effect(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct vib_pwm_chip *chip = container_of(cdev, struct vib_pwm_chip,
						cdev);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->effect_idx);
}

static ssize_t qpnp_vib_store_effect(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	/* At present, nothing to do with setting state */
	return count;
}

static ssize_t qpnp_vib_show_duration(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct vib_pwm_chip *chip = container_of(cdev, struct vib_pwm_chip,
						cdev);
	ktime_t time_rem;
	s64 time_ms = 0;

	if (hrtimer_active(&chip->stop_timer)) {
		time_rem = hrtimer_get_remaining(&chip->stop_timer);
		time_ms = ktime_to_ms(time_rem);
	}

	return snprintf(buf, PAGE_SIZE, "%lld\n", time_ms);
}

static ssize_t qpnp_vib_store_duration(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct vib_pwm_chip *chip = container_of(cdev, struct vib_pwm_chip,
						cdev);
	u32 val;
	int ret;

	ret = kstrtouint(buf, 0, &val);
	if (ret < 0)
		return ret;

	/* setting 0 on duration is NOP for now */
	if (val <= 0)
		return count;

	printk("vib---qpnp_vib_store_duration---val=%d\n", val);
	mutex_lock(&chip->lock);
	if (val < 40) {
		chip->effect_idx = 1; //short vib 1
	} else if (val >= 40 && val < 55) {
		chip->effect_idx = 2; //short vib 2
	} else if (val >= 55 && val < 70) {
		chip->effect_idx = 3; //short vib 3
	} else {
		chip->effect_idx = 0; //long vib
	}
	chip->vib_play_ms = val;
	mutex_unlock(&chip->lock);

	return count;
}

static ssize_t qpnp_vib_show_activate(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	/* For now nothing to show */
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t qpnp_vib_store_activate(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct vib_pwm_chip *chip = container_of(cdev, struct vib_pwm_chip,
						cdev);
	u32 val;
	int ret;

	printk("vib---qpnp_vib_store_activate\n");
	ret = kstrtouint(buf, 0, &val);
	if (ret < 0)
		return ret;

	if (val != 0 && val != 1)
		return count;

	mutex_lock(&chip->lock);
	hrtimer_cancel(&chip->stop_timer);
	chip->state = val;
	printk("vib---state = %d, time = %llums\n", chip->state, chip->vib_play_ms);
	mutex_unlock(&chip->lock);
	schedule_work(&chip->vib_work);

	return count;
}

static struct device_attribute qpnp_vib_attrs[] = {
	__ATTR(state, 0664, qpnp_vib_show_state, qpnp_vib_store_state),
	__ATTR(duration, 0664, qpnp_vib_show_duration, qpnp_vib_store_duration),
	__ATTR(effect, 0664, qpnp_vib_show_effect, qpnp_vib_store_effect),
	__ATTR(activate, 0664, qpnp_vib_show_activate, qpnp_vib_store_activate),
};

/* Dummy functions for brightness */
static enum led_brightness qpnp_vib_brightness_get(struct led_classdev *cdev)
{
	return 0;
}

static void qpnp_vib_brightness_set(struct led_classdev *cdev,
			enum led_brightness level)
{
}

static int qpnp_vibrator_pwm_suspend(struct device *dev)
{
	struct vib_pwm_chip *chip = dev_get_drvdata(dev);

	mutex_lock(&chip->lock);
	if (!chip->disable_overdrive) {
		hrtimer_cancel(&chip->overdrive_timer);
		cancel_work_sync(&chip->overdrive_work);
	}
	hrtimer_cancel(&chip->stop_timer);
	cancel_work_sync(&chip->vib_work);
	qpnp_vibrator_play_off(chip);
	mutex_unlock(&chip->lock);

	return 0;
}
static SIMPLE_DEV_PM_OPS(qpnp_vibrator_pwm_pm_ops, qpnp_vibrator_pwm_suspend,
			NULL);

static int qpnp_vib_parse_dt(struct vib_pwm_chip *chip)
{
	struct device_node *node = chip->dev->of_node, *child_node;
	int rc = 0, id = 0;

	chip->pwm_nums = of_get_available_child_count(node);
	if (chip->pwm_nums == 0) {
		dev_err(chip->dev, "No vib child node defined\n");
		return -ENODEV;
	}

	chip->en_gpio = of_get_named_gpio_flags(node, "vib,en-gpio", 0, &chip->en_gpio_flags);
	printk("vib---vib,en-gpio=%d\n", chip->en_gpio);

	for_each_available_child_of_node(node, child_node) {
		rc = of_property_read_u32(child_node, "pwm-sources", &id);
		if (rc) {
			dev_err(chip->dev, "Get pwm-sources failed, rc=%d\n", rc);
			return rc;
		}

		chip->id = id;
		chip->label = of_get_property(child_node, "label", NULL) ? : child_node->name;
		printk("chip->label=%s ", chip->label);

		chip->pwm_dev = devm_of_pwm_get(chip->dev, child_node, NULL);
		if (IS_ERR(chip->pwm_dev)) {
			rc = PTR_ERR(chip->pwm_dev);
			if (rc != -EPROBE_DEFER)
				dev_err(chip->dev, "Get pwm device for %s failed, rc=%d\n",
							chip->label, rc);
			return rc;
		}
	}
	return 0;
}

static int qpnp_vibrator_pwm_probe(struct platform_device *pdev)
{
	struct vib_pwm_chip *chip;
	int i, ret;
	u32 base;

	pr_err("vib---qpnp_vibrator_pwm_probe\n");

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &pdev->dev;

	ret = qpnp_vib_parse_dt(chip);
	if (ret < 0) {
		pr_err("vib---couldn't parse device tree, ret=%d\n", ret);
		return ret;
	}

	chip->base = (uint16_t)base;
	chip->vib_play_ms = QPNP_VIB_PLAY_MS;
	mutex_init(&chip->lock);
	INIT_WORK(&chip->vib_work, qpnp_vib_work);
	//INIT_WORK(&chip->overdrive_work, qpnp_vib_overdrive_work);

	hrtimer_init(&chip->stop_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	chip->stop_timer.function = vib_stop_timer;
	hrtimer_init(&chip->overdrive_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	chip->overdrive_timer.function = vib_overdrive_timer;
	dev_set_drvdata(&pdev->dev, chip);

	chip->cdev.name = "vibrator";
	chip->cdev.brightness_get = qpnp_vib_brightness_get;
	chip->cdev.brightness_set = qpnp_vib_brightness_set;
	chip->cdev.max_brightness = 100;
	ret = devm_led_classdev_register(&pdev->dev, &chip->cdev);
	if (ret < 0) {
		pr_err("vib---Error in registering led class device, ret=%d\n", ret);
		goto fail;
	}

	for (i = 0; i < ARRAY_SIZE(qpnp_vib_attrs); i++) {
		ret = sysfs_create_file(&chip->cdev.dev->kobj,
				&qpnp_vib_attrs[i].attr);
		if (ret < 0) {
			dev_err(&pdev->dev, "vib---Error in creating sysfs file, ret=%d\n",
				ret);
			goto sysfs_fail;
		}
	}

	printk("vib---Vibrator PWM successfully registered: overdrive = %s\n",
		chip->disable_overdrive ? "disabled" : "enabled");
	return 0;

sysfs_fail:
	for (--i; i >= 0; i--)
		sysfs_remove_file(&chip->cdev.dev->kobj,
				&qpnp_vib_attrs[i].attr);
fail:
	mutex_destroy(&chip->lock);
	dev_set_drvdata(&pdev->dev, NULL);
	return ret;
}

static int qpnp_vibrator_pwm_remove(struct platform_device *pdev)
{
	struct vib_pwm_chip *chip = dev_get_drvdata(&pdev->dev);

	pr_err("vib---qpnp_vibrator_pwm_remove\n");
	if (!chip->disable_overdrive) {
		hrtimer_cancel(&chip->overdrive_timer);
		cancel_work_sync(&chip->overdrive_work);
	}
	hrtimer_cancel(&chip->stop_timer);
	cancel_work_sync(&chip->vib_work);
	mutex_destroy(&chip->lock);
	dev_set_drvdata(&pdev->dev, NULL);

	return 0;
}

static const struct of_device_id vibrator_pwm_match_table[] = {
	{ .compatible = "qcom,lct-pwm-vibrator" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, vibrator_pwm_match_table);

static struct platform_driver qpnp_vibrator_pwm_driver = {
	.driver	= {
		.name		= "qcom,lct-pwm-vibrator",
		.of_match_table	= vibrator_pwm_match_table,
		.pm		= &qpnp_vibrator_pwm_pm_ops,
	},
	.probe	= qpnp_vibrator_pwm_probe,
	.remove	= qpnp_vibrator_pwm_remove,
};
module_platform_driver(qpnp_vibrator_pwm_driver);

MODULE_DESCRIPTION("QCOM QPNP Vibrator-PWM driver");
MODULE_LICENSE("GPL v2");
