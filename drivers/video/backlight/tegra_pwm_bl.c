/*
 * linux/drivers/video/backlight/tegra_pwm_bl.c
 *
 * Tegra pwm backlight driver
 *
 * Copyright (C) 2011 NVIDIA Corporation
 * Author: Renuka Apte <rapte@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/tegra_pwm_bl.h>
#include <linux/gpio.h>
#include <mach/dc.h>

struct tegra_pwm_bl_data {
	struct device *dev;
	int which_dc;
	int (*notify)(struct device *, int brightness);
	struct tegra_dc_pwm_params params;
	int (*check_fb)(struct device *dev, struct fb_info *info);
};

static int tegra_pwm_backlight_update_status(struct backlight_device *bl)
{
	struct tegra_pwm_bl_data *tbl = dev_get_drvdata(&bl->dev);
	int brightness = bl->props.brightness;
	int max = bl->props.max_brightness;
	struct tegra_dc *dc;

	if (bl->props.power != FB_BLANK_UNBLANK)
		brightness = 0;

	if (bl->props.fb_blank != FB_BLANK_UNBLANK)
		brightness = 0;

	if (tbl->notify)
		brightness = tbl->notify(tbl->dev, brightness);

	if (brightness > max)
		dev_err(&bl->dev, "Invalid brightness value: %d max: %d\n",
		brightness, max);

#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
	/* map API brightness range from (0~255) to hw range (0~128) */
	tbl->params.duty_cycle = (brightness * 128) / 255;
#else
	tbl->params.duty_cycle = brightness & 0xFF;
#endif

	/* Call tegra display controller function to update backlight */
	dc = tegra_dc_get_dc(tbl->which_dc);
	if (dc)
		tegra_dc_config_pwm(dc, &tbl->params);
	else
		dev_err(&bl->dev, "tegra display controller not available\n");

	return 0;
}

static int tegra_pwm_backlight_get_brightness(struct backlight_device *bl)
{
	return bl->props.brightness;
}

static int tegra_pwm_backlight_check_fb(struct backlight_device *bl,
					struct fb_info *info)
{
	struct tegra_pwm_bl_data *tbl = dev_get_drvdata(&bl->dev);
	return !tbl->check_fb || tbl->check_fb(tbl->dev, info);
}

static const struct backlight_ops tegra_pwm_backlight_ops = {
	.update_status	= tegra_pwm_backlight_update_status,
	.get_brightness	= tegra_pwm_backlight_get_brightness,
	.check_fb	= tegra_pwm_backlight_check_fb,
};

static int tegra_pwm_backlight_probe(struct platform_device *pdev)
{
	struct backlight_properties props;
	struct platform_tegra_pwm_backlight_data *data;
	struct backlight_device *bl;
	struct tegra_pwm_bl_data *tbl;
	int ret;

	data = pdev->dev.platform_data;
	if (!data) {
		dev_err(&pdev->dev, "failed to find platform data\n");
		return -EINVAL;
	}

	tbl = kzalloc(sizeof(*tbl), GFP_KERNEL);
	if (!tbl) {
		dev_err(&pdev->dev, "no memory for state\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	tbl->dev = &pdev->dev;
	tbl->which_dc = data->which_dc;
	tbl->notify = data->notify;
	tbl->check_fb = data->check_fb;
	tbl->params.which_pwm = data->which_pwm;
	tbl->params.gpio_conf_to_sfio = data->gpio_conf_to_sfio;
	tbl->params.period = data->period;
	tbl->params.clk_div = data->clk_div;
	tbl->params.clk_select = data->clk_select;

	/* If backlight pin is sfio, request for it */
	if (gpio_is_valid(tbl->params.gpio_conf_to_sfio)) {
		ret = gpio_request(tbl->params.gpio_conf_to_sfio, "disp_bl");
		if (ret)
			dev_err(&pdev->dev, "backlight gpio request failed\n");
	}

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = data->max_brightness;
	bl = backlight_device_register(dev_name(&pdev->dev), &pdev->dev, tbl,
			&tegra_pwm_backlight_ops, &props);
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bl);
		goto err_bl;
	}

	bl->props.brightness = data->dft_brightness;

	if (gpio_is_valid(tbl->params.gpio_conf_to_sfio))
		gpio_free(tbl->params.gpio_conf_to_sfio);
	backlight_update_status(bl);

	platform_set_drvdata(pdev, bl);
	return 0;

err_bl:
	kfree(tbl);
err_alloc:
	return ret;
}

static int tegra_pwm_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct tegra_pwm_bl_data *tbl = dev_get_drvdata(&bl->dev);

	backlight_device_unregister(bl);
	kfree(tbl);
	return 0;
}

static struct platform_driver tegra_pwm_backlight_driver = {
	.driver		= {
		.name	= "tegra-pwm-bl",
		.owner	= THIS_MODULE,
	},
	.probe		= tegra_pwm_backlight_probe,
	.remove		= tegra_pwm_backlight_remove,
};

static int __init tegra_pwm_backlight_init(void)
{
	return platform_driver_register(&tegra_pwm_backlight_driver);
}
late_initcall(tegra_pwm_backlight_init);

static void __exit tegra_pwm_backlight_exit(void)
{
	platform_driver_unregister(&tegra_pwm_backlight_driver);
}
module_exit(tegra_pwm_backlight_exit);

MODULE_DESCRIPTION("Tegra PWM Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:tegra-pwm-backlight");

