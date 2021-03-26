// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2015, 2018-2019, 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/leds.h>
#include <linux/device.h>
#include <linux/pwm.h>

#define QPNP_VIB_VTG_CTL(base)		((base) + 0x41)
#define QPNP_VIB_EN_CTL(base)		((base) + 0x46)

#define QPNP_VIB_MAX_LEVEL		31
#define QPNP_VIB_MIN_LEVEL		12

#define QPNP_VIB_DEFAULT_TIMEOUT	15000
#define QPNP_VIB_DEFAULT_VTG_LVL	3100

#define QPNP_VIB_EN			BIT(7)
#define QPNP_VIB_VTG_SET_MASK		0x1F
#define QPNP_VIB_LOGIC_SHIFT		4

enum qpnp_vib_mode {
	QPNP_VIB_MANUAL,
	QPNP_VIB_DTEST1,
	QPNP_VIB_DTEST2,
	QPNP_VIB_DTEST3,
};

struct qpnp_pwm_info {
	struct pwm_device *pwm_dev;
	u32 pwm_channel;
	u32 duty_us;
	u32 period_us;
};

struct qpnp_vib {
	struct platform_device *pdev;
	struct regmap *regmap;
	struct hrtimer vib_timer;
	struct led_classdev cdev;
	struct work_struct work;
	struct qpnp_pwm_info pwm_info;
	enum   qpnp_vib_mode mode;
	u8  reg_vtg_ctl;
	u8  reg_en_ctl;
	u8  active_low;
	u16 base;
	int state;
	int vtg_level;
	int timeout;
	u32 vib_play_ms;
	struct mutex lock;
};

static int qpnp_vib_read_u8(struct qpnp_vib *vib, u8 *data, u16 reg)
{
	int rc;

	rc = regmap_read(vib->regmap, reg, (unsigned int *)data);
	if (rc < 0)
		dev_err(&vib->pdev->dev,
			"Error reading address: %X - ret %X\n", reg, rc);

	return rc;
}

static int qpnp_vib_write_u8(struct qpnp_vib *vib, u8 *data, u16 reg)
{
	int rc;

	rc = regmap_write(vib->regmap, reg, (unsigned int)*data);
	if (rc < 0)
		dev_err(&vib->pdev->dev,
			"Error writing address: %X - ret %X\n", reg, rc);

	return rc;
}

static int qpnp_vibrator_config(struct qpnp_vib *vib)
{
	u8 reg = 0;
	int rc;

	/* Configure the VTG CTL regiser */
	rc = qpnp_vib_read_u8(vib, &reg, QPNP_VIB_VTG_CTL(vib->base));
	if (rc < 0)
		return rc;

	reg &= ~QPNP_VIB_VTG_SET_MASK;
	reg |= (vib->vtg_level & QPNP_VIB_VTG_SET_MASK);
	rc = qpnp_vib_write_u8(vib, &reg, QPNP_VIB_VTG_CTL(vib->base));
	if (rc)
		return rc;

	vib->reg_vtg_ctl = reg;

	/* Configure the VIB ENABLE regiser */
	rc = qpnp_vib_read_u8(vib, &reg, QPNP_VIB_EN_CTL(vib->base));
	if (rc < 0)
		return rc;

	reg |= (!!vib->active_low) << QPNP_VIB_LOGIC_SHIFT;
	if (vib->mode != QPNP_VIB_MANUAL) {
		vib->pwm_info.pwm_dev = pwm_request(vib->pwm_info.pwm_channel,
								 "qpnp-vib");
		if (IS_ERR_OR_NULL(vib->pwm_info.pwm_dev)) {
			dev_err(&vib->pdev->dev, "vib pwm request failed\n");
			return -ENODEV;
		}

		rc = pwm_config(vib->pwm_info.pwm_dev, vib->pwm_info.duty_us,
						vib->pwm_info.period_us);
		if (rc < 0) {
			dev_err(&vib->pdev->dev, "vib pwm config failed\n");
			pwm_free(vib->pwm_info.pwm_dev);
			return -ENODEV;
		}

		reg |= BIT(vib->mode - 1);
	}

	rc = qpnp_vib_write_u8(vib, &reg, QPNP_VIB_EN_CTL(vib->base));
	if (rc < 0)
		return rc;

	vib->reg_en_ctl = reg;

	return rc;
}

static int qpnp_vib_set(struct qpnp_vib *vib, int on)
{
	int rc;
	u8 val;

	if (on) {
		if (vib->mode != QPNP_VIB_MANUAL) {
			pwm_enable(vib->pwm_info.pwm_dev);
		} else {
			val = vib->reg_en_ctl;
			val |= QPNP_VIB_EN;
			rc = qpnp_vib_write_u8(vib, &val,
					QPNP_VIB_EN_CTL(vib->base));
			if (rc < 0)
				return rc;
			vib->reg_en_ctl = val;
		}
	} else {
		if (vib->mode != QPNP_VIB_MANUAL) {
			pwm_disable(vib->pwm_info.pwm_dev);
		} else {
			val = vib->reg_en_ctl;
			val &= ~QPNP_VIB_EN;
			rc = qpnp_vib_write_u8(vib, &val,
					QPNP_VIB_EN_CTL(vib->base));
			if (rc < 0)
				return rc;
			vib->reg_en_ctl = val;
		}
	}

	return 0;
}

static void qpnp_vib_update(struct work_struct *work)
{
	struct qpnp_vib *vib = container_of(work, struct qpnp_vib,
					 work);
	qpnp_vib_set(vib, vib->state);
}

static enum hrtimer_restart qpnp_vib_timer_func(struct hrtimer *timer)
{
	struct qpnp_vib *vib = container_of(timer, struct qpnp_vib,
							 vib_timer);

	vib->state = 0;
	schedule_work(&vib->work);

	return HRTIMER_NORESTART;
}

#ifdef CONFIG_PM
static int qpnp_vibrator_suspend(struct device *dev)
{
	struct qpnp_vib *vib = dev_get_drvdata(dev);

	hrtimer_cancel(&vib->vib_timer);
	cancel_work_sync(&vib->work);
	/* turn-off vibrator */
	qpnp_vib_set(vib, 0);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(qpnp_vibrator_pm_ops, qpnp_vibrator_suspend, NULL);

static int qpnp_vib_parse_dt(struct qpnp_vib *vib)
{
	struct platform_device *pdev = vib->pdev;
	struct device_node *node = pdev->dev.of_node;
	const char *mode;
	u32 temp_val;
	int rc;

	vib->timeout = QPNP_VIB_DEFAULT_TIMEOUT;

	rc = of_property_read_u32(node, "reg", &temp_val);
	if (rc < 0) {
		dev_err(&pdev->dev,
			"Couldn't find reg in node = %s rc = %d\n",
			node->full_name, rc);
		return rc;
	}
	vib->base = temp_val;

	rc = of_property_read_u32(node,
			"qcom,vib-timeout-ms", &temp_val);
	if (!rc) {
		vib->timeout = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&pdev->dev, "Unable to read vib timeout\n");
		return rc;
	}

	vib->vtg_level = QPNP_VIB_DEFAULT_VTG_LVL;
	rc = of_property_read_u32(node,
			"qcom,vib-vtg-level-mV", &temp_val);
	if (!rc) {
		vib->vtg_level = temp_val;
	} else if (rc != -EINVAL) {
		dev_err(&pdev->dev, "Unable to read vtg level\n");
		return rc;
	}

	vib->vtg_level /= 100;
	if (vib->vtg_level < QPNP_VIB_MIN_LEVEL)
		vib->vtg_level = QPNP_VIB_MIN_LEVEL;
	else if (vib->vtg_level > QPNP_VIB_MAX_LEVEL)
		vib->vtg_level = QPNP_VIB_MAX_LEVEL;

	vib->mode = QPNP_VIB_MANUAL;
	rc = of_property_read_string(node, "qcom,mode", &mode);
	if (!rc) {
		if (strcmp(mode, "manual") == 0) {
			vib->mode = QPNP_VIB_MANUAL;
		} else if (strcmp(mode, "dtest1") == 0) {
			vib->mode = QPNP_VIB_DTEST1;
		} else if (strcmp(mode, "dtest2") == 0) {
			vib->mode = QPNP_VIB_DTEST2;
		} else if (strcmp(mode, "dtest3") == 0) {
			vib->mode = QPNP_VIB_DTEST3;
		} else {
			dev_err(&pdev->dev, "Invalid mode\n");
			return -EINVAL;
		}
	} else if (rc != -EINVAL) {
		dev_err(&pdev->dev, "Unable to read mode\n");
		return rc;
	}

	if (vib->mode != QPNP_VIB_MANUAL) {
		rc = of_property_read_u32(node,
				"qcom,pwm-channel", &temp_val);
		if (!rc)
			vib->pwm_info.pwm_channel = temp_val;
		else
			return rc;

		rc = of_property_read_u32(node,
				"qcom,period-us", &temp_val);
		if (!rc)
			vib->pwm_info.period_us = temp_val;
		else
			return rc;

		rc = of_property_read_u32(node,
				"qcom,duty-us", &temp_val);
		if (!rc)
			vib->pwm_info.duty_us = temp_val;
		else
			return rc;
	}

	vib->active_low = of_property_read_bool(node,
				"qcom,active-low");

	return 0;
}

static ssize_t qpnp_vib_get_state(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct qpnp_vib *chip = container_of(cdev, struct qpnp_vib, cdev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", !!chip->reg_en_ctl);
}

static ssize_t qpnp_vib_set_state(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
		/* At present, nothing to do with setting state */
		return count;
}

static ssize_t qpnp_vib_get_duration(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct qpnp_vib *vib = container_of(cdev, struct qpnp_vib, cdev);
	ktime_t time_rem;
	s64 time_us = 0;

	if (hrtimer_active(&vib->vib_timer)) {
		time_rem = hrtimer_get_remaining(&vib->vib_timer);
		time_us = ktime_to_us(time_rem);
	}

	return scnprintf(buf, PAGE_SIZE, "%lld\n", div_s64(time_us, 1000));
}

static ssize_t qpnp_vib_set_duration(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct qpnp_vib *vib = container_of(cdev, struct qpnp_vib, cdev);
	u32 value;
	int rc;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	/* setting 0 on duration is NOP for now */
	if (value <= 0)
		return count;

	if (value > vib->timeout)
		value = vib->timeout;

	mutex_lock(&vib->lock);
	vib->vib_play_ms = value;
	mutex_unlock(&vib->lock);
	return count;
}

static ssize_t qpnp_vib_get_activate(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct qpnp_vib *vib = container_of(cdev, struct qpnp_vib, cdev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", vib->state);
}

static ssize_t qpnp_vib_set_activate(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct qpnp_vib *vib = container_of(cdev, struct qpnp_vib, cdev);
	u32 value;
	int rc;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (value != 0 && value != 1)
		return count;

	vib->state = value;
	pr_debug("state = %d, time = %ums\n", vib->state,
						vib->vib_play_ms);
	mutex_lock(&vib->lock);
	if (vib->state) {
		hrtimer_cancel(&vib->vib_timer);
		hrtimer_start(&vib->vib_timer,
			ktime_set(vib->vib_play_ms / 1000,
				 (vib->vib_play_ms % 1000) * 1000000),
					HRTIMER_MODE_REL);
	}
	mutex_unlock(&vib->lock);
	schedule_work(&vib->work);

	return count;
}

static struct device_attribute qpnp_vib_attrs[] = {
	__ATTR(state, 0664, qpnp_vib_get_state, qpnp_vib_set_state),
	__ATTR(duration, 0664, qpnp_vib_get_duration, qpnp_vib_set_duration),
	__ATTR(activate, 0664, qpnp_vib_get_activate, qpnp_vib_set_activate),
};

/* Dummy functions for brightness */
static
enum led_brightness qpnp_brightness_get(struct led_classdev *cdev)
{
	return 0;
}

static void qpnp_brightness_set(struct led_classdev *cdev,
					enum led_brightness level)
{

}

static int qpnp_vibrator_probe(struct platform_device *pdev)
{
	struct qpnp_vib *vib;
	int rc;
	int i;

	vib = devm_kzalloc(&pdev->dev, sizeof(*vib), GFP_KERNEL);
	if (!vib)
		return -ENOMEM;

	vib->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!vib->regmap) {
		dev_err(&pdev->dev, "Couldn't get parent's regmap\n");
		return -EINVAL;
	}

	vib->pdev = pdev;

	rc = qpnp_vib_parse_dt(vib);
	if (rc) {
		dev_err(&pdev->dev, "DT parsing failed\n");
		return rc;
	}

	rc = qpnp_vibrator_config(vib);
	if (rc) {
		dev_err(&pdev->dev, "vib config failed\n");
		return rc;
	}

	mutex_init(&vib->lock);
	INIT_WORK(&vib->work, qpnp_vib_update);

	hrtimer_init(&vib->vib_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vib->vib_timer.function = qpnp_vib_timer_func;

	vib->cdev.name = "vibrator";
	vib->cdev.brightness_get = qpnp_brightness_get;
	vib->cdev.brightness_set = qpnp_brightness_set;
	vib->cdev.max_brightness = 100;
	rc = devm_led_classdev_register(&pdev->dev, &vib->cdev);
	if (rc < 0) {
		dev_err(&pdev->dev, "Error in registering led class device, rc=%d\n",
			rc);
		goto led_reg_fail;
	}

	/* Enabling sysfs entries */
	for (i = 0; i < ARRAY_SIZE(qpnp_vib_attrs); i++) {
		rc = sysfs_create_file(&vib->cdev.dev->kobj,
				&qpnp_vib_attrs[i].attr);
		if (rc < 0) {
			dev_err(&pdev->dev, "Error in creating sysfs file, rc=%d\n",
				rc);
			goto sysfs_fail;
		}
	}

	dev_set_drvdata(&pdev->dev, vib);
	return rc;

sysfs_fail:
	dev_set_drvdata(&pdev->dev, NULL);
led_reg_fail:
	hrtimer_cancel(&vib->vib_timer);
	cancel_work_sync(&vib->work);
	mutex_destroy(&vib->lock);
	return -EINVAL;
}

static int qpnp_vibrator_remove(struct platform_device *pdev)
{
	struct qpnp_vib *vib = dev_get_drvdata(&pdev->dev);
	int i;

	/* Removing sysfs entries */
	for (i = 0; i < ARRAY_SIZE(qpnp_vib_attrs); i++)
		sysfs_remove_file(&vib->cdev.dev->kobj,
				&qpnp_vib_attrs[i].attr);

	dev_set_drvdata(&pdev->dev, NULL);
	hrtimer_cancel(&vib->vib_timer);
	cancel_work_sync(&vib->work);
	mutex_destroy(&vib->lock);
	return 0;
}

static const struct of_device_id spmi_match_table[] = {
	{	.compatible = "qcom,qpnp-vibrator",
	},
	{}
};

static struct platform_driver qpnp_vibrator_driver = {
	.driver	= {
		.name	= "qcom,qpnp-vibrator",
		.of_match_table = spmi_match_table,
		.pm	= &qpnp_vibrator_pm_ops,
	},
	.probe	= qpnp_vibrator_probe,
	.remove	= qpnp_vibrator_remove,
};

module_platform_driver(qpnp_vibrator_driver);

MODULE_DESCRIPTION("qpnp vibrator driver");
MODULE_LICENSE("GPL v2");
