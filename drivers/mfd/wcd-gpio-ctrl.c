/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/mfd/wcd9xxx/wcd-gpio-ctrl.h>

struct wcd_gpio_pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *pinctrl_active;
	struct pinctrl_state *pinctrl_sleep;
};

static struct wcd_gpio_pinctrl_info *wcd_gpio_get_gpiodata(
						struct device_node *np)
{
	struct platform_device *pdev;
	struct wcd_gpio_pinctrl_info *gpio_data;

	if (!np) {
		pr_err("%s: device node is null\n", __func__);
		return NULL;
	}

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		pr_err("%s: platform device not found!\n", __func__);
		return NULL;
	}

	gpio_data = dev_get_drvdata(&pdev->dev);
	if (!gpio_data)
		dev_err(&pdev->dev, "%s: cannot find cdc gpio info\n",
			__func__);

	return gpio_data;
}

/*
 * wcd_gpio_ctrl_select_sleep_state: select pinctrl sleep state
 * @np: pointer to struct device_node
 *
 * Returns error code for failure
 */
int wcd_gpio_ctrl_select_sleep_state(struct device_node *np)
{
	struct wcd_gpio_pinctrl_info *gpio_data;

	gpio_data = wcd_gpio_get_gpiodata(np);
	if (!gpio_data)
		return -EINVAL;

	if (!gpio_data->pinctrl_sleep) {
		pr_err("%s: pinctrl sleep state is null\n", __func__);
		return -EINVAL;
	}

	return pinctrl_select_state(gpio_data->pinctrl,
				    gpio_data->pinctrl_sleep);
}
EXPORT_SYMBOL(wcd_gpio_ctrl_select_sleep_state);

/*
 * wcd_gpio_ctrl_select_active_state: select pinctrl active state
 * @np: pointer to struct device_node
 *
 * Returns error code for failure
 */
int wcd_gpio_ctrl_select_active_state(struct device_node *np)
{
	struct wcd_gpio_pinctrl_info *gpio_data;

	gpio_data = wcd_gpio_get_gpiodata(np);
	if (!gpio_data)
		return -EINVAL;

	if (!gpio_data->pinctrl_active) {
		pr_err("%s: pinctrl active state is null\n", __func__);
		return -EINVAL;
	}

	return pinctrl_select_state(gpio_data->pinctrl,
				    gpio_data->pinctrl_active);
}
EXPORT_SYMBOL(wcd_gpio_ctrl_select_active_state);

static int wcd_gpio_ctrl_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct wcd_gpio_pinctrl_info *gpio_data;

	gpio_data = devm_kzalloc(&pdev->dev,
				 sizeof(struct wcd_gpio_pinctrl_info),
				 GFP_KERNEL);
	if (!gpio_data)
		return -ENOMEM;

	gpio_data->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(gpio_data->pinctrl)) {
		dev_err(&pdev->dev, "%s: Cannot get cdc gpio pinctrl:%ld\n",
			__func__, PTR_ERR(gpio_data->pinctrl));
		ret = PTR_ERR(gpio_data->pinctrl);
		goto err_pctrl_get;
	}

	gpio_data->pinctrl_active = pinctrl_lookup_state(
					gpio_data->pinctrl, "aud_active");
	if (IS_ERR_OR_NULL(gpio_data->pinctrl_active)) {
		dev_err(&pdev->dev, "%s: Cannot get aud_active pinctrl state:%ld\n",
			__func__, PTR_ERR(gpio_data->pinctrl_active));
		ret = PTR_ERR(gpio_data->pinctrl_active);
		goto err_lookup_state;
	}

	gpio_data->pinctrl_sleep = pinctrl_lookup_state(
					gpio_data->pinctrl, "aud_sleep");
	if (IS_ERR_OR_NULL(gpio_data->pinctrl_sleep)) {
		dev_err(&pdev->dev, "%s: Cannot get aud_sleep pinctrl state:%ld\n",
			__func__, PTR_ERR(gpio_data->pinctrl_sleep));
		ret = PTR_ERR(gpio_data->pinctrl_sleep);
		goto err_lookup_state;
	}

	/* Set pinctrl state to aud_sleep by default */
	ret = pinctrl_select_state(gpio_data->pinctrl,
				   gpio_data->pinctrl_sleep);
	if (ret)
		dev_err(&pdev->dev, "%s: set cdc gpio sleep state fail: %d\n",
			__func__, ret);

	dev_set_drvdata(&pdev->dev, gpio_data);
	return 0;

err_lookup_state:
	devm_pinctrl_put(gpio_data->pinctrl);
err_pctrl_get:
	devm_kfree(&pdev->dev, gpio_data);
	return ret;
}

static int wcd_gpio_ctrl_remove(struct platform_device *pdev)
{
	struct wcd_gpio_pinctrl_info *gpio_data;

	gpio_data = dev_get_drvdata(&pdev->dev);

	if (gpio_data && gpio_data->pinctrl)
		devm_pinctrl_put(gpio_data->pinctrl);

	devm_kfree(&pdev->dev, gpio_data);

	return 0;
}

static const struct of_device_id wcd_gpio_ctrl_match[] = {
	{.compatible = "qcom,wcd-gpio-ctrl"},
	{}
};

static struct platform_driver wcd_gpio_ctrl_driver = {
	.driver = {
		.name = "wcd-gpio-ctrl",
		.owner = THIS_MODULE,
		.of_match_table = wcd_gpio_ctrl_match,
	},
	.probe = wcd_gpio_ctrl_probe,
	.remove = wcd_gpio_ctrl_remove,
};
module_platform_driver(wcd_gpio_ctrl_driver);

MODULE_DESCRIPTION("WCD GPIO Control module platform driver");
MODULE_LICENSE("GPL v2");

