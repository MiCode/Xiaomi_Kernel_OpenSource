// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.

#include <linux/clk.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>

#define PINCTRL_ACTIVE		"active"
#define PINCTRL_SLEEP		"sleep"
#define CLK_DUTY_DEN		100

struct led_qcom_clk_priv {
	struct led_classdev cdev;
	struct clk *core;
	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_active;
	struct pinctrl_state *gpio_sleep;
	const char *name;
	const char *default_trigger;
	int duty_cycle, max_duty;
	bool clk_enabled;
};

static int qcom_clk_led_set(struct led_classdev *led_cdev,
		       enum led_brightness brightness)
{
	struct led_qcom_clk_priv *led_priv =
		container_of(led_cdev, struct led_qcom_clk_priv, cdev);
	int rc;

	if (brightness == LED_OFF) {
		if (led_priv->clk_enabled) {
			clk_disable_unprepare(led_priv->core);
			pinctrl_select_state(led_priv->pinctrl,
					led_priv->gpio_sleep);
			led_priv->clk_enabled = false;
		}
		return 0;
	}

	if (!led_priv->clk_enabled) {
		rc = pinctrl_select_state(led_priv->pinctrl,
				led_priv->gpio_active);
		if (rc < 0) {
			dev_err(led_cdev->dev, "Failed to select pinctrl state rc=%d\n",
					rc);
			return rc;
		}

		rc = clk_prepare_enable(led_priv->core);
		if (rc < 0) {
			dev_err(led_cdev->dev, "Failed to enable clock rc=%d\n",
					rc);
			goto err_enable;
		}

		led_priv->clk_enabled = true;
	}

	/*
	 * Duty cycle will be configured based on the brightness.
	 * Complete clock period/duty cycle is 100 and
	 * brightness is the period in which signal is active.
	 */
	led_priv->duty_cycle = brightness;
	rc = clk_set_duty_cycle(led_priv->core,
			led_priv->duty_cycle, CLK_DUTY_DEN);
	if (rc < 0) {
		dev_err(led_cdev->dev, "Failed to set duty cycle rc=%d\n",
				rc);
		goto err_set_duty;
	}

	return rc;

err_set_duty:
	clk_disable_unprepare(led_priv->core);
	led_priv->clk_enabled = false;
err_enable:
	pinctrl_select_state(led_priv->pinctrl, led_priv->gpio_sleep);

	return rc;
}

static int qcom_clk_led_parse_dt(struct device *dev,
		struct led_qcom_clk_priv *led)
{
	int ret;

	led->name = of_get_property(dev->of_node, "qcom,label", NULL) ? :
				dev->of_node->name;

	led->default_trigger = of_get_property(dev->of_node,
			"qcom,default-clk-trigger", NULL);

	ret = of_property_read_u32(dev->of_node, "qcom,max_duty",
			&led->max_duty);
	if (ret) {
		dev_err(dev, "Failed to read max_duty\n");
		return ret;
	}

	if (led->max_duty <= 0) {
		dev_err(dev, "Max duty cycle should not be <= 0\n");
		return -EINVAL;
	}

	led->core = devm_clk_get(dev, "core");
	if (IS_ERR(led->core)) {
		ret = PTR_ERR(led->core);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Failed to get core source\n");

		return ret;
	}

	led->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(led->pinctrl)) {
		dev_err(dev, "No pinctrl config specified!\n");
		return PTR_ERR(led->pinctrl);
	}

	led->gpio_active = pinctrl_lookup_state(led->pinctrl,
				PINCTRL_ACTIVE);
	if (IS_ERR_OR_NULL(led->gpio_active)) {
		dev_err(dev, "No default config specified!\n");
		return PTR_ERR(led->gpio_active);
	}

	led->gpio_sleep = pinctrl_lookup_state(led->pinctrl,
							PINCTRL_SLEEP);
	if (IS_ERR_OR_NULL(led->gpio_sleep)) {
		dev_err(dev, "No sleep config specified!\n");
		return  PTR_ERR(led->gpio_sleep);
	}

	return 0;
}

static int led_qcom_clk_probe(struct platform_device *pdev)
{
	struct led_qcom_clk_priv *priv;
	int ret;

	priv = devm_kzalloc(&pdev->dev, sizeof(struct led_qcom_clk_priv),
			    GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	ret = qcom_clk_led_parse_dt(&pdev->dev, priv);
	if (ret)
		return ret;

	priv->cdev.name = priv->name;
	priv->cdev.brightness = LED_OFF;
	priv->cdev.max_brightness = priv->max_duty;
	priv->cdev.flags = LED_CORE_SUSPENDRESUME;
	priv->cdev.default_trigger = priv->default_trigger;
	priv->cdev.brightness_set_blocking = qcom_clk_led_set;

	return devm_led_classdev_register(&pdev->dev, &priv->cdev);
}

static const struct of_device_id of_qcom_clk_leds_match[] = {
	{ .compatible = "qcom,clk-led-pwm", },
	{}
};
MODULE_DEVICE_TABLE(of, of_qcom_clk_leds_match);

static struct platform_driver led_qcom_clk_driver = {
	.probe		= led_qcom_clk_probe,
	.driver		= {
		.name		= "led_qcom_clk_pwm",
		.of_match_table	= of_qcom_clk_leds_match,
	},
};

module_platform_driver(led_qcom_clk_driver);

MODULE_DESCRIPTION("Generic clock driven LED driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:leds-qcom-clk");
