// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 *
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>

#include <leds-mtk.h>

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME " %s(%d) :" fmt, __func__, __LINE__

struct led_pwm {
	const char	*name;
	u8		active_low;
	unsigned int	max_brightness;
};

struct led_pwm_data {
	struct mt_led_data m_led;
	struct pwm_device	*pwm;
	struct pwm_state	pwmstate;
	unsigned int		active_low;
};

struct mt_leds_pwm {
	int num_leds;
	struct led_pwm_data leds[];
};

static int led_pwm_set(struct mt_led_data *mdev,
		       int brightness)
{
	struct led_pwm_data *led_dat =
		container_of(mdev, struct led_pwm_data, m_led);
	unsigned int max = mdev->conf.max_hw_brightness;
	unsigned long long duty = led_dat->pwmstate.period;

	duty *= brightness;
	do_div(duty, max);

	if (led_dat->active_low)
		duty = led_dat->pwmstate.period - duty;

	led_dat->pwmstate.duty_cycle = duty;
	led_dat->pwmstate.enabled = duty > 0;
	return pwm_apply_state(led_dat->pwm, &led_dat->pwmstate);
}

__attribute__((nonnull))
static int led_pwm_add(struct device *dev, struct mt_leds_pwm *priv,
		       struct led_pwm *led, struct fwnode_handle *fwnode)
{

	int ret;
	struct led_pwm_data *led_dat = &priv->leds[priv->num_leds];

	led_dat->pwm = devm_fwnode_pwm_get(dev, fwnode, NULL);
	if (IS_ERR(led_dat->pwm)) {
		ret = PTR_ERR(led_dat->pwm);
		dev_notice(dev,
			"unable to request PWM for %s: %d\n",
			led->name, ret);
		return ret;
	}

	pwm_init_state(led_dat->pwm, &led_dat->pwmstate);

	ret = mt_leds_classdev_register(dev, &led_dat->m_led);
	if (ret < 0) {
		dev_notice(dev, "failed to register PWM led for %s: %d\n",
			led->name, ret);
		return ret;
	}

	return 0;
}

static int led_pwm_create_fwnode(struct device *dev, struct mt_leds_pwm *priv)
{
	struct fwnode_handle *fwnode;
	struct led_pwm led;
	struct led_pwm_data *led_data;
	int ret = 0;

	pr_info("create fwnode begain +++");

	memset(&led, 0, sizeof(led));

	device_for_each_child_node(dev, fwnode) {
		led_data = &priv->leds[priv->num_leds];
		ret = mt_leds_parse_dt(&led_data->m_led, fwnode);
		if (ret < 0) {
			fwnode_handle_put(fwnode);
			return -EINVAL;
		}
		led.name = led_data->m_led.conf.cdev.name;
		led.max_brightness = led_data->m_led.conf.cdev.max_brightness;
		led_data->m_led.mtk_hw_brightness_set = led_pwm_set;

		ret = led_pwm_add(dev, priv, &led, fwnode);
		priv->num_leds++;
		if (ret) {
			fwnode_handle_put(fwnode);
			break;
		}
		pr_info("parse led: %s, num: %d, max: %d",
			led.name, priv->num_leds, led.max_brightness);
	}

	return ret;
}

static int led_pwm_probe(struct platform_device *pdev)
{
	struct mt_leds_pwm *priv;
	int ret = 0;
	int count;

	pr_info("probe begain +++");

	count = device_get_child_node_count(&pdev->dev);

	if (!count) {
		ret = -EINVAL;
		goto err;
	}

	priv = devm_kzalloc(&pdev->dev, struct_size(priv, leds, count),
			    GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto err;
	}

	ret = led_pwm_create_fwnode(&pdev->dev, priv);

	if (ret < 0)
		goto err;

	platform_set_drvdata(pdev, priv);

	pr_info("probe end +++");

	return 0;
err:
	pr_notice("Failed to probe: %d, %d!\n", ret, count);
	return ret;

}

static void __maybe_unused led_pwm_shutdown(struct platform_device *pdev)
{
	int i;
	struct mt_leds_pwm *m_leds = dev_get_platdata(&pdev->dev);

	pr_info("Turn off backlight\n");

	for (i = 0; m_leds && i < m_leds->num_leds; i++) {
		if (!&(m_leds->leds[i]))
			continue;

		led_pwm_set(&(m_leds->leds[i].m_led), 0);
		mt_leds_call_notifier(LED_STATUS_SHUTDOWN, &(m_leds->leds[i].m_led.conf));
	}
}

static const struct of_device_id of_pwm_leds_match[] = {
	{ .compatible = "mediatek,pwm-leds", },
	{},
};
MODULE_DEVICE_TABLE(of, of_pwm_leds_match);

static struct platform_driver led_pwm_driver = {
	.probe		= led_pwm_probe,
	.driver		= {
		.name	= "mtk_leds_pwm",
		.of_match_table = of_pwm_leds_match,
	},
	.shutdown = led_pwm_shutdown,
};

module_platform_driver(led_pwm_driver);

MODULE_AUTHOR("Mediatek Corporation");
MODULE_DESCRIPTION("MTK Disp PWM Backlight Driver");
MODULE_LICENSE("GPL");

