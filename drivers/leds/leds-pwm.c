/*
 * linux/drivers/leds-pwm.c
 *
 * simple PWM based LED control
 *
 * Copyright 2009 Luotao Fu @ Pengutronix (l.fu@pengutronix.de)
 *
 * based on leds-gpio.c by Raphael Assenat <raph@8d.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/leds_pwm.h>
#include <linux/slab.h>

#define PWM_PERIOD_DEFAULT_NS 1000000

struct pwm_setting {
	u64	period_ns;
	u64	duty_ns;
};

struct led_setting {
	u64			on_ms;
	u64			off_ms;
	enum led_brightness	brightness;
	bool			blink;
};

struct led_pwm_data {
	struct led_classdev	cdev;
	struct pwm_device	*pwm;
	struct pwm_setting	pwm_setting;
	struct led_setting	led_setting;
	unsigned int		active_low;
	unsigned int		period;
	int			duty;
	bool			blinking;
};

struct led_pwm_priv {
	int num_leds;
	struct led_pwm_data leds[0];
};

static int __led_blink_config_pwm(struct led_pwm_data *led_data)
{
	struct pwm_state pstate;
	int rc;

	pwm_get_state(led_data->pwm, &pstate);
	pstate.enabled = !!(led_data->pwm_setting.duty_ns != 0);
	pstate.period = led_data->pwm_setting.period_ns;
	pstate.duty_cycle = led_data->pwm_setting.duty_ns;
	/* Use default pattern in PWM device */
	pstate.output_pattern = NULL;

	rc = pwm_apply_state(led_data->pwm, &pstate);
	if (rc < 0)
		pr_err("Apply PWM state for %s led failed, rc=%d\n",
				led_data->cdev.name, rc);

	return rc;
}

static int led_pwm_set_blink(struct led_pwm_data *led_data)
{
	u64 on_ms, off_ms, period_ns, duty_ns;
	enum led_brightness brightness = led_data->led_setting.brightness;
	int rc = 0;

	if (led_data->led_setting.blink) {
		on_ms = led_data->led_setting.on_ms;
		off_ms = led_data->led_setting.off_ms;

		duty_ns = on_ms * NSEC_PER_MSEC;
		period_ns = (on_ms + off_ms) * NSEC_PER_MSEC;

		if (period_ns < duty_ns && duty_ns != 0)
			period_ns = duty_ns + 1;

	} else {
		/* Use initial period if no blinking is required */
		period_ns = PWM_PERIOD_DEFAULT_NS;

		duty_ns = period_ns * brightness;
		do_div(duty_ns, LED_FULL);

		if (period_ns < duty_ns && duty_ns != 0)
			period_ns = duty_ns + 1;
	}

	pr_debug("BLINK: PWM settings for %s led: period = %lluns, duty = %lluns brightness = %d\n",
			led_data->cdev.name, period_ns, duty_ns, brightness);

	led_data->pwm_setting.duty_ns = duty_ns;
	led_data->pwm_setting.period_ns = period_ns;

	rc = __led_blink_config_pwm(led_data);
	if (rc < 0) {
		pr_err("failed to config pwm for blink %s failed, rc=%d\n",
				led_data->cdev.name, rc);
		return rc;
	}

	if (led_data->led_setting.blink) {
		led_data->cdev.brightness = LED_FULL;
		led_data->blinking = true;
	} else {
		led_data->cdev.brightness = led_data->led_setting.brightness;
		led_data->blinking = false;
	}

	return rc;
}

static int led_pwm_blink_set(struct led_classdev *led_cdev,
	unsigned long *on_ms, unsigned long *off_ms)
{
	struct led_pwm_data *led_data =
		container_of(led_cdev, struct led_pwm_data, cdev);
	int rc = 0;

	if (led_data->blinking && *on_ms == led_data->led_setting.on_ms &&
				*off_ms == led_data->led_setting.off_ms) {
		pr_debug("Ignore, on/off setting is not changed: on %lums, off %lums\n",
			*on_ms, *off_ms);
		return 0;
	}

	if (*on_ms == 0) {
		led_data->led_setting.blink = false;
		led_data->led_setting.brightness = LED_OFF;
	} else if (*off_ms == 0) {
		led_data->led_setting.blink = false;
		led_data->led_setting.brightness = led_data->cdev.brightness;
	} else {
		led_data->led_setting.on_ms = *on_ms;
		led_data->led_setting.off_ms = *off_ms;
		led_data->led_setting.blink = true;
	}

	rc = led_pwm_set_blink(led_data);
	if (rc < 0)
		pr_err("blink led failed for rc=%d\n", rc);

	return rc;
}

static void __led_pwm_set(struct led_pwm_data *led_data)
{
	int new_duty = led_data->duty;

	pwm_config(led_data->pwm, new_duty, led_data->period);

	if (new_duty == 0)
		pwm_disable(led_data->pwm);
	else
		pwm_enable(led_data->pwm);
}

static int led_pwm_set(struct led_classdev *led_cdev,
		       enum led_brightness brightness)
{
	struct led_pwm_data *led_data =
		container_of(led_cdev, struct led_pwm_data, cdev);
	unsigned int max = led_data->cdev.max_brightness;
	unsigned long long duty =  led_data->period;

	duty *= brightness;
	do_div(duty, max);

	if (led_data->active_low)
		duty = led_data->period - duty;

	led_data->duty = duty;
	led_data->blinking = false;

	__led_pwm_set(led_data);

	return 0;
}

static inline size_t sizeof_pwm_leds_priv(int num_leds)
{
	return sizeof(struct led_pwm_priv) +
		      (sizeof(struct led_pwm_data) * num_leds);
}

static void led_pwm_cleanup(struct led_pwm_priv *priv)
{
	while (priv->num_leds--)
		led_classdev_unregister(&priv->leds[priv->num_leds].cdev);
}

static int led_pwm_add(struct device *dev, struct led_pwm_priv *priv,
		       struct led_pwm *led, struct device_node *child)
{
	struct led_pwm_data *led_data = &priv->leds[priv->num_leds];
	struct pwm_args pargs;
	int ret;

	led_data->active_low = led->active_low;
	led_data->cdev.name = led->name;
	led_data->cdev.default_trigger = led->default_trigger;
	led_data->cdev.brightness = LED_OFF;
	led_data->cdev.max_brightness = led->max_brightness;
	/* Set a flag to keep the trigger always */
	led_data->cdev.flags |= LED_KEEP_TRIGGER;

	if (child)
		led_data->pwm = devm_of_pwm_get(dev, child, NULL);
	else
		led_data->pwm = devm_pwm_get(dev, led->name);
	if (IS_ERR(led_data->pwm)) {
		ret = PTR_ERR(led_data->pwm);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "unable to request PWM for %s: %d\n",
				led->name, ret);
		return ret;
	}

	led_data->cdev.brightness_set_blocking = led_pwm_set;
	led_data->cdev.blink_set = led_pwm_blink_set;

	/*
	 * FIXME: pwm_apply_args() should be removed when switching to the
	 * atomic PWM API.
	 */
	pwm_apply_args(led_data->pwm);

	pwm_get_args(led_data->pwm, &pargs);

	led_data->period = pargs.period;
	if (!led_data->period && (led->pwm_period_ns > 0))
		led_data->period = led->pwm_period_ns;

	ret = led_classdev_register(dev, &led_data->cdev);
	if (ret == 0) {
		priv->num_leds++;
		led_pwm_set(&led_data->cdev, led_data->cdev.brightness);
	} else {
		dev_err(dev, "failed to register PWM led for %s: %d\n",
			led->name, ret);
	}

	return ret;
}

static int led_pwm_create_of(struct device *dev, struct led_pwm_priv *priv)
{
	struct device_node *child;
	struct led_pwm led;
	int ret = 0;

	memset(&led, 0, sizeof(led));

	for_each_child_of_node(dev->of_node, child) {
		led.name = of_get_property(child, "label", NULL) ? :
			   child->name;

		led.default_trigger = of_get_property(child,
						"linux,default-trigger", NULL);
		led.active_low = of_property_read_bool(child, "active-low");
		of_property_read_u32(child, "max-brightness",
				     &led.max_brightness);

		ret = led_pwm_add(dev, priv, &led, child);
		if (ret) {
			of_node_put(child);
			break;
		}
	}

	return ret;
}

static int led_pwm_probe(struct platform_device *pdev)
{
	struct led_pwm_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct led_pwm_priv *priv;
	int count, i;
	int ret = 0;

	if (pdata)
		count = pdata->num_leds;
	else
		count = of_get_child_count(pdev->dev.of_node);

	if (!count)
		return -EINVAL;

	priv = devm_kzalloc(&pdev->dev, sizeof_pwm_leds_priv(count),
			    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	if (pdata) {
		for (i = 0; i < count; i++) {
			ret = led_pwm_add(&pdev->dev, priv, &pdata->leds[i],
					  NULL);
			if (ret)
				break;
		}
	} else {
		ret = led_pwm_create_of(&pdev->dev, priv);
	}

	if (ret) {
		led_pwm_cleanup(priv);
		return ret;
	}

	platform_set_drvdata(pdev, priv);

	return 0;
}

static int led_pwm_remove(struct platform_device *pdev)
{
	struct led_pwm_priv *priv = platform_get_drvdata(pdev);

	led_pwm_cleanup(priv);

	return 0;
}

static const struct of_device_id of_pwm_leds_match[] = {
	{ .compatible = "pwm-leds", },
	{},
};
MODULE_DEVICE_TABLE(of, of_pwm_leds_match);

static struct platform_driver led_pwm_driver = {
	.probe		= led_pwm_probe,
	.remove		= led_pwm_remove,
	.driver		= {
		.name	= "leds_pwm",
		.of_match_table = of_pwm_leds_match,
	},
};

module_platform_driver(led_pwm_driver);

MODULE_AUTHOR("Luotao Fu <l.fu@pengutronix.de>");
MODULE_DESCRIPTION("generic PWM LED driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:leds-pwm");
