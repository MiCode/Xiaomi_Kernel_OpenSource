/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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
#define DEBUG
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/mfd/spk-id.h>


struct spk_id_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *pull_down;
	struct pinctrl_state *pull_up;
	struct pinctrl_state *no_pull;
	int gpio;
	int state;
};


static struct spk_id_info *spk_id_get_info(struct device_node *np)
{
	struct platform_device *pdev;
	struct spk_id_info *info;

	if (!np) {
		pr_err("%s: device node is null\n", __func__);
		return NULL;
	}

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		pr_err("%s: platform device not found!\n", __func__);
		return NULL;
	}

	info = dev_get_drvdata(&pdev->dev);
	if (!info)
		dev_err(&pdev->dev, "%s: cannot find spk id info\n", __func__);

	return info;
}

int spk_id_get_pin_3state(struct device_node *np)
{
	struct spk_id_info *info;
	int pu = 0;
	int pd = 0;

	info = spk_id_get_info(np);
	if (!info)
		return -EINVAL;

	if (IS_ERR_OR_NULL(info->pinctrl)) {
		pr_err("%s: pin ctrl is invalid:%ld\n", __func__, PTR_ERR(info->pinctrl));
		return -EINVAL;
	}

	if (!gpio_is_valid(info->gpio)) {
		pr_err("%s: gpio is invalid:%d\n", __func__, info->gpio);
		return -EINVAL;
	}

	pinctrl_select_state(info->pinctrl, info->pull_down);
	msleep(3);
	pd = gpio_get_value(info->gpio);

	pinctrl_select_state(info->pinctrl, info->pull_up);
	msleep(3);
	pu = gpio_get_value(info->gpio);


	if ((pd == pu) && (pd == 0)) {
		pr_info("%s: id pin%d = %d\n", __func__, info->gpio, pd);
		pinctrl_select_state(info->pinctrl, info->pull_down);
		info->state = PIN_PULL_DOWN;
	} else if ((pd == pu) && (pd == 1)) {
		pr_info("%s: id pin%d = %d\n", __func__, info->gpio, pd);
		pinctrl_select_state(info->pinctrl, info->pull_up);
		info->state = PIN_PULL_UP;
	} else {
		pr_info("%s: id pin%d = 2\n", __func__, info->gpio);
		pinctrl_select_state(info->pinctrl, info->no_pull);
		info->state = PIN_FLOAT;
	}

	return info->state;
}
EXPORT_SYMBOL(spk_id_get_pin_3state);

static int spk_id_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct spk_id_info *info;

	info = devm_kzalloc(&pdev->dev,
				 sizeof(struct spk_id_info),
				 GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	dev_dbg(&pdev->dev, "%s: device %s\n", __func__, pdev->name);
	info->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(info->pinctrl)) {
		dev_err(&pdev->dev, "%s: Cannot get speaker id gpio pinctrl:%ld\n",
			__func__, PTR_ERR(info->pinctrl));
		ret = PTR_ERR(info->pinctrl);
		goto err_pctrl_get;
	}

	info->pull_down = pinctrl_lookup_state(
					info->pinctrl, "pull_down");
	if (IS_ERR_OR_NULL(info->pull_down)) {
		dev_err(&pdev->dev, "%s: Cannot get pull_down pinctrl state:%ld\n",
			__func__, PTR_ERR(info->pull_down));
		ret = PTR_ERR(info->pull_down);
		goto err_lookup_state;
	}

	info->pull_up = pinctrl_lookup_state(
					info->pinctrl, "pull_up");
	if (IS_ERR_OR_NULL(info->pull_up)) {
		dev_err(&pdev->dev, "%s: Cannot get pull_up pinctrl state:%ld\n",
			__func__, PTR_ERR(info->pull_up));
		ret = PTR_ERR(info->pull_up);
		goto err_lookup_state;
	}

	info->no_pull = pinctrl_lookup_state(
					info->pinctrl, "no_pull");
	if (IS_ERR_OR_NULL(info->no_pull)) {
		dev_err(&pdev->dev, "%s: Cannot get no_pull pinctrl state:%ld\n",
			__func__, PTR_ERR(info->no_pull));
		ret = PTR_ERR(info->no_pull);
		goto err_lookup_state;
	}

	info->gpio = of_get_named_gpio(pdev->dev.of_node,
					    "audio,speaker-id-gpio", 0);
	if (gpio_is_valid(info->gpio)) {
		ret = gpio_request(info->gpio, "speaker-id");
		if (ret) {
			dev_err(&pdev->dev, "%s: Failed to request gpio %d\n",
				__func__, info->gpio);
			goto err_lookup_state;
		}
	}

	dev_set_drvdata(&pdev->dev, info);
	return 0;

err_lookup_state:
	devm_pinctrl_put(info->pinctrl);
err_pctrl_get:
	devm_kfree(&pdev->dev, info);
	return ret;
}

static int spk_id_remove(struct platform_device *pdev)
{
	struct spk_id_info *info;

	info = dev_get_drvdata(&pdev->dev);

	if (info) {
		if (info->pinctrl)
			devm_pinctrl_put(info->pinctrl);
		if (gpio_is_valid(info->gpio))
			gpio_free(info->gpio);
	}

	devm_kfree(&pdev->dev, info);
	return 0;
}

static const struct of_device_id spk_id_match[] = {
	{.compatible = "audio,speaker-id"},
	{}
};

static struct platform_driver spk_id_driver = {
	.driver = {
		.name = "spk-id",
		.owner = THIS_MODULE,
		.of_match_table = spk_id_match,
	},
	.probe = spk_id_probe,
	.remove = spk_id_remove,
};
module_platform_driver(spk_id_driver);

MODULE_DESCRIPTION("Speaker ID platform driver");
MODULE_LICENSE("GPL v2");
