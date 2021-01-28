// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>

#include "../inc/mt_led_trigger.h"

const char *mt_led_trigger_mode_name[MT_LED_MODE_MAX] = {
	"cc_mode",
	"pwm_mode",
	"breath_mode",
};

static ssize_t mt_led_cc_attr_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t mt_led_cc_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt);

static const struct device_attribute mt_led_cc_mode_attrs[] = {
	__ATTR(soft_start_step, 0644,
		mt_led_cc_attr_show, mt_led_cc_attr_store),
};

static ssize_t mt_led_cc_attr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct mt_led_info *info = (struct mt_led_info *)led_cdev;
	int ret = 0;
	ptrdiff_t cmd;

	cmd = attr - mt_led_cc_mode_attrs;
	buf[0] = '\0';

	switch (cmd) {
	case CC_MODE_ATTR_SFSTR:
		ret = info->ops->get_soft_start_step(info);
		break;
	default:
		return -EINVAL;
	}

	snprintf(buf, PAGE_SIZE, "%d\n", ret);
	return strlen(buf);
}

static ssize_t mt_led_cc_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct mt_led_info *info = (struct mt_led_info *)led_cdev;
	int ret = 0;
	unsigned long value = 0;
	ptrdiff_t cmd;

	cmd = attr - mt_led_cc_mode_attrs;

	ret = kstrtoul(buf, 10, &value);
	if (ret < 0)
		return ret;

	switch (cmd) {
	case CC_MODE_ATTR_SFSTR:
		ret = info->ops->set_soft_start_step(info, value);
		if (ret < 0)
			return ret;
		break;
	default:
		break;
	}

	return cnt;
}

static int mt_led_cc_activate(struct led_classdev *led)
{
	struct mt_led_info *info = (struct mt_led_info *)led;
	int i = 0, ret = 0;

	if ((info->magic_code & MT_LED_MAGIC_MASK) == MT_LED_MAGIC_CODE &&
		(info->magic_code & MT_LED_MAGIC_CC_MODE) != 0) {

		for (i = 0; i < ARRAY_SIZE(mt_led_cc_mode_attrs); i++) {
			ret = device_create_file(led->dev,
					mt_led_cc_mode_attrs + i);
			if (ret < 0) {
				dev_err(led->dev,
					"%s: create file fail %d\n",
					__func__, i);
				goto out_create_file;
			}
		}
		ret = info->ops->change_mode(led, MT_LED_CC_MODE);
		if (ret < 0) {
			dev_err(led->dev, "%s: change mode fail\n", __func__);
			goto out_change_mode;
		}
		return;
out_change_mode:
		i = ARRAY_SIZE(mt_led_cc_mode_attrs);
out_create_file:
		while (--i > 0)
			device_remove_file(led->dev, mt_led_cc_mode_attrs + i);
	} else
		dev_err(led->dev, "not Support MT CC Trigger\n");
	return 0;
}

static void mt_led_cc_deactivate(struct led_classdev *led)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(mt_led_cc_mode_attrs); i++)
		device_remove_file(led->dev, mt_led_cc_mode_attrs + i);
}

static ssize_t mt_led_pwm_attr_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t mt_led_pwm_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt);

static const struct device_attribute mt_led_pwm_mode_attrs[] = {
	__ATTR(pwm_dim_duty, 0644, mt_led_pwm_attr_show, mt_led_pwm_attr_store),
	__ATTR(pwm_dim_freq, 0644, mt_led_pwm_attr_show, mt_led_pwm_attr_store),
	__ATTR(list_duty, 0444, mt_led_pwm_attr_show, NULL),
	__ATTR(list_freq, 0444, mt_led_pwm_attr_show, NULL),
};

static ssize_t mt_led_pwm_attr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct mt_led_info *info = (struct mt_led_info *)led_cdev;
	int ret = 0;
	ptrdiff_t cmd = attr - mt_led_pwm_mode_attrs;

	buf[0] = '\0';

	dev_dbg(led_cdev->dev, "%s cmd = %d\n", __func__, (int)cmd);
	switch (cmd) {
	case PWM_MODE_ATTR_DIM_DUTY:
		ret = info->ops->get_pwm_dim_duty(info);
		snprintf(buf, PAGE_SIZE, "%d (0~255)\n", ret);
		break;
	case PWM_MODE_ATTR_DIM_FREQ:
		ret = info->ops->get_pwm_dim_freq(info);
		snprintf(buf, PAGE_SIZE, "%d.%dHz\n", ret/1000, ret%1000);
		break;
	case PWM_MODE_ATTR_LIST_DUTY:
		ret = info->ops->list_pwm_duty(info, buf);
		break;
	case PWM_MODE_ATTR_LIST_FREQ:
		ret = info->ops->list_pwm_freq(info, buf);
		break;
	default:
		return -EINVAL;
	}
	if (ret < 0)
		return ret;

	return strlen(buf);
}

static ssize_t mt_led_pwm_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct mt_led_info *info = (struct mt_led_info *)led_cdev;
	int ret = 0;
	unsigned long value = 0;
	ptrdiff_t cmd = attr - mt_led_pwm_mode_attrs;

	ret = kstrtoul(buf, 10, &value);
	if (ret < 0)
		return ret;

	switch (cmd) {
	case PWM_MODE_ATTR_DIM_DUTY:
		ret = info->ops->set_pwm_dim_duty(info, value);
		break;
	case PWM_MODE_ATTR_DIM_FREQ:
		ret = info->ops->set_pwm_dim_freq(info, value);
		break;
	default:
		return -EINVAL;
	}
	if (ret < 0)
		return ret;

	return cnt;
}

static int mt_led_pwm_activate(struct led_classdev *led)
{
	struct mt_led_info *info = (struct mt_led_info *)led;
	int i = 0, ret = 0;

	if ((info->magic_code & MT_LED_MAGIC_MASK) == MT_LED_MAGIC_CODE &&
		(info->magic_code & MT_LED_MAGIC_PWM_MODE) != 0) {

		for (i = 0; i < ARRAY_SIZE(mt_led_pwm_mode_attrs); i++) {
			ret = device_create_file(led->dev,
					mt_led_pwm_mode_attrs + i);
			if (ret < 0) {
				dev_err(led->dev,
					"%s: create file fail %d\n",
					__func__, i);
				goto out_create_file;
			}
		}

		ret = info->ops->change_mode(led, MT_LED_PWM_MODE);
		if (ret < 0) {
			dev_err(led->dev, "%s change mode fail\n", __func__);
			goto out_change_mode;
		}

		return;
out_change_mode:
		i = ARRAY_SIZE(mt_led_pwm_mode_attrs);
out_create_file:
		while (--i > 0)
			device_remove_file(led->dev, mt_led_pwm_mode_attrs + i);
	} else
		dev_err(led->dev, "Not Support MT PWM Trigger\n");
	return 0;
}

static void mt_led_pwm_deactivate(struct led_classdev *led)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(mt_led_pwm_mode_attrs); i++)
		device_remove_file(led->dev, mt_led_pwm_mode_attrs + i);
}

static ssize_t mt_led_breath_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt);
static ssize_t mt_led_breath_attr_show(struct device *dev,
	struct device_attribute *attr, char *buf);

static const struct device_attribute mt_led_breath_mode_attrs[] = {
	__ATTR(tr1, 0644, mt_led_breath_attr_show, mt_led_breath_attr_store),
	__ATTR(tr2, 0644, mt_led_breath_attr_show, mt_led_breath_attr_store),
	__ATTR(tf1, 0644, mt_led_breath_attr_show, mt_led_breath_attr_store),
	__ATTR(tf2, 0644, mt_led_breath_attr_show, mt_led_breath_attr_store),
	__ATTR(ton, 0644, mt_led_breath_attr_show, mt_led_breath_attr_store),
	__ATTR(toff, 0644, mt_led_breath_attr_show, mt_led_breath_attr_store),
	__ATTR(list_time, 0444, mt_led_breath_attr_show, NULL),
};

static ssize_t mt_led_breath_attr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ptrdiff_t cmd = attr - mt_led_breath_mode_attrs;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct mt_led_info *info = (struct mt_led_info *)led_cdev;

	buf[0] = '\0';


	switch (cmd) {
	case BREATH_MODE_ATTR_TR1:
		ret = info->ops->get_breath_tr1(info);
		break;
	case BREATH_MODE_ATTR_TR2:
		ret = info->ops->get_breath_tr2(info);
		break;
	case BREATH_MODE_ATTR_TF1:
		ret = info->ops->get_breath_tf1(info);
		break;
	case BREATH_MODE_ATTR_TF2:
		ret = info->ops->get_breath_tf2(info);
		break;
	case BREATH_MODE_ATTR_TON:
		ret = info->ops->get_breath_ton(info);
		break;
	case BREATH_MODE_ATTR_TOFF:
		ret = info->ops->get_breath_toff(info);
		break;
	case BREATH_MODE_ATTR_LIST_TIME:
		ret = info->ops->list_breath_time(info, buf);
		return strlen(buf);
	default:
		return -EINVAL;
	}
	if (ret < 0)
		return ret;

	snprintf(buf, PAGE_SIZE, "%d.%d s\n", ret/1000, ret%1000);
	return strlen(buf);
}

static ssize_t mt_led_breath_attr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct mt_led_info *info = (struct mt_led_info *)led_cdev;
	int ret = 0;
	unsigned long value = 0;
	ptrdiff_t cmd = attr - mt_led_breath_mode_attrs;

	ret = kstrtoul(buf, 10, &value);
	if (ret < 0)
		return ret;

	switch (cmd) {
	case BREATH_MODE_ATTR_TR1:
		ret = info->ops->set_breath_tr1(info, value);
		break;
	case BREATH_MODE_ATTR_TR2:
		ret = info->ops->set_breath_tr2(info, value);
		break;
	case BREATH_MODE_ATTR_TF1:
		ret = info->ops->set_breath_tf1(info, value);
		break;
	case BREATH_MODE_ATTR_TF2:
		ret = info->ops->set_breath_tf2(info, value);
		break;
	case BREATH_MODE_ATTR_TON:
		ret = info->ops->set_breath_ton(info, value);
		break;
	case BREATH_MODE_ATTR_TOFF:
		ret = info->ops->set_breath_toff(info, value);
		break;
	default:
		return -EINVAL;
	}

	if (ret < 0)
		return ret;

	return cnt;
}

static int mt_led_breath_activate(struct led_classdev *led)
{
	struct mt_led_info *info = (struct mt_led_info *)led;
	int i = 0, ret = 0;

	if ((info->magic_code & MT_LED_MAGIC_MASK) == MT_LED_MAGIC_CODE &&
		(info->magic_code & MT_LED_MAGIC_BREATH_MODE) != 0) {

		for (i = 0; i < ARRAY_SIZE(mt_led_breath_mode_attrs); i++) {
			ret = device_create_file(led->dev,
					mt_led_breath_mode_attrs + i);
			if (ret < 0) {
				dev_err(led->dev,
					"%s: create file fail %d\n",
					__func__, i);
				goto out_create_file;
			}
		}

		ret = info->ops->change_mode(led, MT_LED_BREATH_MODE);
		if (ret < 0) {
			dev_err(led->dev, "%s change mode fail\n", __func__);
			goto out_change_mode;
		}
		return;
out_change_mode:
		i = ARRAY_SIZE(mt_led_breath_mode_attrs);
out_create_file:
		while (--i > 0)
			device_remove_file(
				led->dev, mt_led_breath_mode_attrs + i);
	} else
		dev_err(led->dev, "Not Support MT Breath Trigger\n");
	return 0;
}

static void mt_led_breath_deactivate(struct led_classdev *led)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(mt_led_breath_mode_attrs); i++)
		device_remove_file(led->dev, mt_led_breath_mode_attrs + i);
}


static struct led_trigger mt_led_trigger[] = {
	{
		.name = "cc_mode",
		.activate = mt_led_cc_activate,
		.deactivate = mt_led_cc_deactivate,
	},
	{
		.name = "pwm_mode",
		.activate = mt_led_pwm_activate,
		.deactivate = mt_led_pwm_deactivate,
	},
	{
		.name = "breath_mode",
		.activate = mt_led_breath_activate,
		.deactivate = mt_led_breath_deactivate,
	},
};

int mt_led_check_ops(struct mt_led_ops *ops)
{
	if (!ops->get_soft_start_step)
		return -EINVAL;
	if (!ops->set_soft_start_step)
		return -EINVAL;
	if (!ops->get_pwm_dim_duty)
		return -EINVAL;
	if (!ops->set_pwm_dim_duty)
		return -EINVAL;
	if (!ops->get_pwm_dim_freq)
		return -EINVAL;
	if (!ops->set_pwm_dim_freq)
		return -EINVAL;
	if (!ops->get_breath_tr1)
		return -EINVAL;
	if (!ops->set_breath_tr1)
		return -EINVAL;
	if (!ops->get_breath_tr2)
		return -EINVAL;
	if (!ops->set_breath_tr2)
		return -EINVAL;
	if (!ops->get_breath_tf1)
		return -EINVAL;
	if (!ops->set_breath_tf1)
		return -EINVAL;
	if (!ops->get_breath_tf2)
		return -EINVAL;
	if (!ops->set_breath_tf2)
		return -EINVAL;
	if (!ops->get_breath_ton)
		return -EINVAL;
	if (!ops->set_breath_ton)
		return -EINVAL;
	if (!ops->get_breath_toff)
		return -EINVAL;
	if (!ops->set_breath_toff)
		return -EINVAL;
	if (!ops->list_pwm_duty)
		return -EINVAL;
	if (!ops->list_pwm_freq)
		return -EINVAL;
	if (!ops->list_breath_time)
		return -EINVAL;
	return 0;
}

int mt_led_trigger_register(struct mt_led_ops *ops)
{
	int i = 0, ret = 0;

	ret = mt_led_check_ops(ops);
	if (ret < 0) {
		pr_err("%s all ops need to be implment\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(mt_led_trigger); i++) {
		ret = led_trigger_register(&mt_led_trigger[i]);
		if (ret < 0) {
			pr_err("%s register led %d fail\n", __func__, i);
			goto out_led_trigger;
		}
	}

	return 0;

out_led_trigger:
	while (--i > 0)
		led_trigger_unregister(&mt_led_trigger[i]);

	return -EINVAL;
}

void mt_led_trigger_unregister(void)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(mt_led_trigger); i++)
		led_trigger_unregister(&mt_led_trigger[i]);
}


MODULE_AUTHOR("CY_Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6360 PMU RGBLED Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
