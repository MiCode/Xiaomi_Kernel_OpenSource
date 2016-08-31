/*
 * Backlight LEDs driver for MAX8831
 *
 * Copyright (c) 2008-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/mfd/max8831.h>
#include <linux/max8831_backlight.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>

struct max8831_backlight_data {
	struct device		*max8831_dev;
	int			id;
	int			current_brightness;
	struct regulator	*regulator;

	int (*notify)(struct device *dev, int brightness);
	bool (*is_powered)(void);
};

static int max8831_backlight_set(struct backlight_device *bl, int brightness)
{
	struct max8831_backlight_data *data = bl_get_data(bl);
	struct device *dev = data->max8831_dev;

	/* ranges from 0-255 */
	data->current_brightness = brightness;

	if (data->is_powered)
		if (!data->is_powered())
			return 0;

	if (data->id == MAX8831_BL_LEDS) {
		/* map 0-255 brightness to max8831 range */
		brightness = brightness * MAX8831_BL_LEDS_MAX_CURR / 255;

		if (brightness == 0) {
			max8831_update_bits(dev, MAX8831_CTRL,
			(MAX8831_CTRL_LED1_ENB | MAX8831_CTRL_LED2_ENB), 0);
		} else {
			max8831_update_bits(dev, MAX8831_CTRL,
			(MAX8831_CTRL_LED1_ENB | MAX8831_CTRL_LED2_ENB),
			(MAX8831_CTRL_LED1_ENB | MAX8831_CTRL_LED2_ENB));

			max8831_write(dev, MAX8831_CURRENT_CTRL_LED1,
								brightness);
			max8831_write(dev, MAX8831_CURRENT_CTRL_LED2,
								brightness);
		}
	}
	return 0;
}

static int max8831_backlight_update_status(struct backlight_device *bl)
{
	struct max8831_backlight_data *data = bl_get_data(bl);
	int brightness = bl->props.brightness;

/*
	if (bl->props.power != FB_BLANK_UNBLANK)
		brightness = 0;

	if (bl->props.fb_blank != FB_BLANK_UNBLANK)
		brightness = 0;
*/

	if (data->notify)
		brightness = data->notify(data->max8831_dev, brightness);

	return max8831_backlight_set(bl, brightness);
}

static int max8831_backlight_get_brightness(struct backlight_device *bl)
{
	struct max8831_backlight_data *data = bl_get_data(bl);
	return data->current_brightness;
}

static const struct backlight_ops max8831_backlight_ops = {
	.update_status	= max8831_backlight_update_status,
	.get_brightness	= max8831_backlight_get_brightness,
};

static int max8831_bl_probe(struct platform_device *pdev)
{
	struct max8831_backlight_data *data;
	struct backlight_device *bl;
	struct backlight_properties props;
	struct platform_max8831_backlight_data *pData = pdev->dev.platform_data;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	data->max8831_dev = pdev->dev.parent;
	data->current_brightness = 0;
	data->id = pdev->id;
	data->notify = pData->notify;
	data->is_powered = pData->is_powered;
	data->regulator = regulator_get(data->max8831_dev,
			"vin");
	if (IS_ERR(data->regulator)) {
		dev_err(&pdev->dev, "%s: Unable to get the backlight regulator\n",
		       __func__);
		data->regulator = NULL;
	} else {
		regulator_enable(data->regulator);
	}

	props.type = BACKLIGHT_RAW;
	props.max_brightness = 255;
	bl = backlight_device_register(pdev->name, data->max8831_dev, data,
				       &max8831_backlight_ops, &props);
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		return PTR_ERR(bl);
	}

	bl->props.brightness = pData->dft_brightness;

	platform_set_drvdata(pdev, bl);
	backlight_update_status(bl);
	return 0;
}

static int max8831_bl_remove(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct max8831_backlight_data *data = bl_get_data(bl);

	if (data->regulator != NULL)
		regulator_put(data->regulator);

	backlight_device_unregister(bl);

	kfree(data);
	return 0;
}
#ifdef CONFIG_PM
static int max8831_bl_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct max8831_backlight_data *data = bl_get_data(bl);
	int ret;

	ret = max8831_backlight_set(bl, 0);
	if (data->regulator)
		regulator_disable(data->regulator);
	return ret;
}

static int max8831_bl_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct max8831_backlight_data *data = bl_get_data(bl);

	if (data->regulator)
		regulator_enable(data->regulator);
	backlight_update_status(bl);
	return 0;
}

static const struct dev_pm_ops max8831_bl_pm_ops = {
	.suspend	= max8831_bl_suspend,
	.resume		= max8831_bl_resume,
};
#endif

static struct platform_driver max8831_bl_driver = {
	.driver = {
		.name	= "max8831_display_bl",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &max8831_bl_pm_ops,
#endif
	},
	.probe	= max8831_bl_probe,
	.remove	= max8831_bl_remove,
};
module_platform_driver(max8831_bl_driver);

MODULE_AUTHOR("Chaitanya Bandi <bandik@nvidia.com>");
MODULE_DESCRIPTION("MAX8831 Backlight display driver");
MODULE_LICENSE("GPL v2");
