// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2017-2020, The Linux Foundation. All rights reserved. */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/pinctrl/pinctrl.h>
#include <linux/errno.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/workqueue.h>

/*
 * Define vibration periods: default(5sec), min(50ms), max(15sec) and
 * overdrive(30ms).
 */
#define QPNP_VIB_MIN_PLAY_MS		50
#define QPNP_VIB_PLAY_MS		5000
#define QPNP_VIB_MAX_PLAY_MS		15000
#define QPNP_VIB_OVERDRIVE_PLAY_MS	30

#define VIB_LDO_ACTIVE    "vib_ldo_active"
#define VIB_LDO_SUSPEND    "vib_ldo_suspend"
struct pinctrl *pinctrl;
struct pinctrl_state *pins_active;
struct pinctrl_state *pins_suspend;

struct vib_ldo_chip {
	struct led_classdev	cdev;
	struct mutex		lock;
	struct hrtimer		stop_timer;
	struct hrtimer		overdrive_timer;
	struct work_struct	vib_work;
	struct work_struct	overdrive_work;

	int			state;
	u64			vib_play_ms;
	bool			vib_enabled;
	bool			disable_overdrive;
};

static inline int qpnp_vib_ldo_enable(struct vib_ldo_chip *chip, bool enable)
{
	int ret;

	if (chip->vib_enabled == enable)
		return 0;

	if (enable) {
		ret = pinctrl_select_state(pinctrl, pins_active);
		if (ret) {
			pr_err("Cannot select vibrator state pins_active");
		}
	} else {
		ret = pinctrl_select_state(pinctrl, pins_suspend);
		if (ret) {
			pr_err("Cannot select vibrator state pins_suspend");
		}
	}

	chip->vib_enabled = enable;

	return ret;
}

static int qpnp_vibrator_play_on(struct vib_ldo_chip *chip)
{
	int ret;

	ret = qpnp_vib_ldo_enable(chip, true);
	if (ret < 0) {
		pr_err("vibration enable failed, ret=%d\n", ret);
		return ret;
	}

	if (!chip->disable_overdrive)
		hrtimer_start(&chip->overdrive_timer,
			ms_to_ktime(QPNP_VIB_OVERDRIVE_PLAY_MS),
			HRTIMER_MODE_REL);

	return ret;
}

static void qpnp_vib_work(struct work_struct *work)
{
	struct vib_ldo_chip *chip = container_of(work, struct vib_ldo_chip,
						vib_work);
	int ret = 0;

	if (chip->state) {
		if (!chip->vib_enabled)
			ret = qpnp_vibrator_play_on(chip);

		if (ret == 0)
			hrtimer_start(&chip->stop_timer,
				      ms_to_ktime(chip->vib_play_ms),
				      HRTIMER_MODE_REL);
	} else {
		if (!chip->disable_overdrive) {
			hrtimer_cancel(&chip->overdrive_timer);
			cancel_work_sync(&chip->overdrive_work);
		}
		qpnp_vib_ldo_enable(chip, false);
	}
}

static enum hrtimer_restart vib_stop_timer(struct hrtimer *timer)
{
	struct vib_ldo_chip *chip = container_of(timer, struct vib_ldo_chip,
					     stop_timer);

	chip->state = 0;
	schedule_work(&chip->vib_work);
	return HRTIMER_NORESTART;
}

static void qpnp_vib_overdrive_work(struct work_struct *work)
{
	struct vib_ldo_chip *chip = container_of(work, struct vib_ldo_chip,
					     overdrive_work);

	mutex_lock(&chip->lock);

	/* LDO voltage update not required if Vibration disabled */
	if (!chip->vib_enabled)
		goto unlock;

unlock:
	mutex_unlock(&chip->lock);
}

static enum hrtimer_restart vib_overdrive_timer(struct hrtimer *timer)
{
	struct vib_ldo_chip *chip = container_of(timer, struct vib_ldo_chip,
					     overdrive_timer);
	schedule_work(&chip->overdrive_work);
	pr_debug("overdrive timer expired\n");
	return HRTIMER_NORESTART;
}

static ssize_t qpnp_vib_show_state(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct vib_ldo_chip *chip = container_of(cdev, struct vib_ldo_chip,
						cdev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", chip->vib_enabled);
}

static ssize_t qpnp_vib_store_state(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	/* At present, nothing to do with setting state */
	return count;
}

static ssize_t qpnp_vib_show_duration(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct vib_ldo_chip *chip = container_of(cdev, struct vib_ldo_chip,
						cdev);
	ktime_t time_rem;
	s64 time_ms = 0;

	if (hrtimer_active(&chip->stop_timer)) {
		time_rem = hrtimer_get_remaining(&chip->stop_timer);
		time_ms = ktime_to_ms(time_rem);
	}

	return scnprintf(buf, PAGE_SIZE, "%lld\n", time_ms);
}

static ssize_t qpnp_vib_store_duration(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct vib_ldo_chip *chip = container_of(cdev, struct vib_ldo_chip,
						cdev);
	u32 val;
	int ret;

	ret = kstrtouint(buf, 0, &val);
	if (ret < 0)
		return ret;

	/* setting 0 on duration is NOP for now */
	if (val <= 0)
		return count;

	if (val < QPNP_VIB_MIN_PLAY_MS)
		val = QPNP_VIB_MIN_PLAY_MS;

	if (val > QPNP_VIB_MAX_PLAY_MS)
		val = QPNP_VIB_MAX_PLAY_MS;

	mutex_lock(&chip->lock);
	chip->vib_play_ms = val;
	mutex_unlock(&chip->lock);

	return count;
}

static ssize_t qpnp_vib_show_activate(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	/* For now nothing to show */
	return scnprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t qpnp_vib_store_activate(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct vib_ldo_chip *chip = container_of(cdev, struct vib_ldo_chip,
						cdev);
	u32 val;
	int ret;

	ret = kstrtouint(buf, 0, &val);
	if (ret < 0)
		return ret;

	if (val != 0 && val != 1)
		return count;

	mutex_lock(&chip->lock);
	hrtimer_cancel(&chip->stop_timer);
	chip->state = val;
	pr_debug("state = %d, time = %llums\n", chip->state, chip->vib_play_ms);
	mutex_unlock(&chip->lock);
	schedule_work(&chip->vib_work);

	return count;
}

static struct device_attribute qpnp_vib_attrs[] = {
	__ATTR(state, 0664, qpnp_vib_show_state, qpnp_vib_store_state),
	__ATTR(duration, 0664, qpnp_vib_show_duration, qpnp_vib_store_duration),
	__ATTR(activate, 0664, qpnp_vib_show_activate, qpnp_vib_store_activate),
};

static int qpnp_vib_parse_dt(struct device *dev, struct vib_ldo_chip *chip)
{

	chip->disable_overdrive = of_property_read_bool(dev->of_node,
					"qcom,disable-overdrive");

	return 0;
}

/* Dummy functions for brightness */
static enum led_brightness qpnp_vib_brightness_get(struct led_classdev *cdev)
{
	return 0;
}

static void qpnp_vib_brightness_set(struct led_classdev *cdev,
			enum led_brightness level)
{
}

static int qpnp_vibrator_ldo_v2_suspend(struct device *dev)
{
	struct vib_ldo_chip *chip = dev_get_drvdata(dev);

	mutex_lock(&chip->lock);
	if (!chip->disable_overdrive) {
		hrtimer_cancel(&chip->overdrive_timer);
		cancel_work_sync(&chip->overdrive_work);
	}
	hrtimer_cancel(&chip->stop_timer);
	cancel_work_sync(&chip->vib_work);

	qpnp_vib_ldo_enable(chip, false);
	mutex_unlock(&chip->lock);

	return 0;
}
static SIMPLE_DEV_PM_OPS(qpnp_vibrator_ldo_v2_pm_ops, qpnp_vibrator_ldo_v2_suspend,
			NULL);

static int qpnp_vibrator_ldo_v2_probe(struct platform_device *pdev)
{

	struct vib_ldo_chip *chip;
	int i, ret;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		pr_err("Cannot get vibrator pinctrl");
		pinctrl = NULL;
		return 0;
	}

	pins_active = pinctrl_lookup_state(pinctrl, "vib_ldo_active");
	if (IS_ERR_OR_NULL(pins_active)) {
		pr_err("Cannot get vibrator pins_active");
		pins_active = NULL;
		return 0;
	}

	pins_suspend = pinctrl_lookup_state(pinctrl, "vib_ldo_suspend");
	if (IS_ERR_OR_NULL(pins_suspend)) {
		pr_err("Cannot get vibrator pins_suspend");
		pins_suspend = NULL;
		return 0;
	}

	ret = pinctrl_select_state(pinctrl, pins_suspend);
	if (ret) {
		pr_err("Cannot select vibrator state pins_suspend");
	}

	ret = qpnp_vib_parse_dt(&pdev->dev, chip);
	if (ret < 0) {
		pr_err("couldn't parse device tree, ret=%d\n", ret);
		return ret;
	}

	chip->vib_play_ms = QPNP_VIB_PLAY_MS;
	mutex_init(&chip->lock);
	INIT_WORK(&chip->vib_work, qpnp_vib_work);
	INIT_WORK(&chip->overdrive_work, qpnp_vib_overdrive_work);

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
		pr_err("Error in registering led class device, ret=%d\n", ret);
		goto fail;
	}

	for (i = 0; i < ARRAY_SIZE(qpnp_vib_attrs); i++) {
		ret = sysfs_create_file(&chip->cdev.dev->kobj,
				&qpnp_vib_attrs[i].attr);
		if (ret < 0) {
			dev_err(&pdev->dev, "Error in creating sysfs file, ret=%d\n",
				ret);
			goto sysfs_fail;
		}
	}

	pr_info("Vibrator LDO V2 successfully registered: overdrive = %s\n",
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

static int qpnp_vibrator_ldo_v2_remove(struct platform_device *pdev)
{
	struct vib_ldo_chip *chip = dev_get_drvdata(&pdev->dev);

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

static const struct of_device_id vibrator_ldo_v2_match_table[] = {
	{ .compatible = "qcom,qpnp-vibrator-ldo-v2" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, vibrator_ldo_v2_match_table);

static struct platform_driver qpnp_vibrator_ldo_v2_driver = {
	.driver	= {
		.name		= "qcom,qpnp-vibrator-ldo-v2",
		.of_match_table	= vibrator_ldo_v2_match_table,
		.pm		= &qpnp_vibrator_ldo_v2_pm_ops,
	},
	.probe	= qpnp_vibrator_ldo_v2_probe,
	.remove	= qpnp_vibrator_ldo_v2_remove,
};
module_platform_driver(qpnp_vibrator_ldo_v2_driver);

MODULE_DESCRIPTION("QPNP Vibrator-LDO V2 driver");
MODULE_LICENSE("GPL v2");
