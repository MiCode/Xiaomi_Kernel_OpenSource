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
#include <linux/edp.h>

struct max8831_backlight_data {
	struct device		*max8831_dev;
	int			id;
	int			current_brightness;
	struct regulator	*regulator;

	int (*notify)(struct device *dev, int brightness);
	bool (*is_powered)(void);
	struct edp_client *max8831_edp_client;
	int *edp_brightness_states;
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

static int max8831_backlight_set_with_edp(struct backlight_device *bl,
	int brightness)
{
	struct max8831_backlight_data *data = bl_get_data(bl);
	struct device *dev = data->max8831_dev;
	unsigned int approved_state;
	int unsigned approved_brightness;
	int ret;
	unsigned int edp_state;
	unsigned int edp_brightness;
	unsigned int i;

	if (data->max8831_edp_client) {
		for (i = 0; i < MAX8831_EDP_NUM_STATES; i++) {
			edp_brightness = data->edp_brightness_states[i];
			if (brightness > edp_brightness) {
				/* Choose the next higher EDP state */
				if (i)
					i--;
				break;
			} else if (brightness == edp_brightness)
				break;
		}
		edp_state = i;
		ret = edp_update_client_request(data->max8831_edp_client,
			edp_state, &approved_state);
		if (ret) {
			dev_err(dev, "E state transition failed\n");
			return ret;
		}

		approved_brightness =
			data->edp_brightness_states[approved_state];
		if (brightness > approved_brightness)
			brightness = approved_brightness;
	}

	max8831_backlight_set(bl, brightness);
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

	return max8831_backlight_set_with_edp(bl, brightness);
}

static void max8831_backlight_edpcb(unsigned int new_state, void *priv_data)
{
	struct backlight_device *bl_device = (struct backlight_device *) priv_data;
	struct max8831_backlight_data *data = bl_get_data(bl_device);
	max8831_backlight_set(bl_device,
			data->edp_brightness_states[new_state]);
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

static int __devinit max8831_bl_probe(struct platform_device *pdev)
{
	struct max8831_backlight_data *data;
	struct backlight_device *bl;
	struct backlight_properties props;
	struct platform_max8831_backlight_data *pData = pdev->dev.platform_data;
	struct edp_manager *battery_manager = NULL;
	int ret;


	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	data->max8831_dev = pdev->dev.parent;
	data->current_brightness = 0;
	data->id = pdev->id;
	data->notify = pData->notify;
	data->is_powered = pData->is_powered;
	data->edp_brightness_states = pData->edp_brightness;
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

	data->max8831_edp_client = devm_kzalloc(&pdev->dev,
					sizeof(struct edp_client), GFP_KERNEL);
	if (IS_ERR_OR_NULL(data->max8831_edp_client)) {
		dev_err(&pdev->dev, "could not allocate edp client\n");
		return PTR_ERR(data->max8831_edp_client);
	}

	strncpy(data->max8831_edp_client->name, "backlight", EDP_NAME_LEN - 1);
	data->max8831_edp_client->name[EDP_NAME_LEN - 1] = '\0';
	data->max8831_edp_client->states = pData->edp_states;
	data->max8831_edp_client->num_states = MAX8831_EDP_NUM_STATES;
	data->max8831_edp_client->e0_index = MAX8831_EDP_ZERO;
	data->max8831_edp_client->private_data = bl;
	data->max8831_edp_client->priority = EDP_MAX_PRIO + 2;
	data->max8831_edp_client->throttle = max8831_backlight_edpcb;
	data->max8831_edp_client->notify_promotion = max8831_backlight_edpcb;

	battery_manager = edp_get_manager("battery");
	if (!battery_manager) {
		dev_err(&pdev->dev, "unable to get edp manager\n");
	} else {
		ret = edp_register_client(battery_manager,
					data->max8831_edp_client);
		if (ret) {
			dev_err(&pdev->dev, "unable to register edp client\n");
		} else {
			ret = edp_update_client_request(
					data->max8831_edp_client,
						MAX8831_EDP_ZERO, NULL);
			if (ret) {
				dev_err(&pdev->dev,
					"unable to set E0 EDP state\n");
				edp_unregister_client(data->max8831_edp_client);
			} else {
				goto edp_success;
			}
		}
	}

	devm_kfree(&pdev->dev, data->max8831_edp_client);
	data->max8831_edp_client = NULL;

edp_success:

	bl->props.brightness = pData->dft_brightness;

	platform_set_drvdata(pdev, bl);
	backlight_update_status(bl);
	return 0;
}

static int __devexit max8831_bl_remove(struct platform_device *pdev)
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

	ret = max8831_backlight_set_with_edp(bl, 0);
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
	.remove	= __devexit_p(max8831_bl_remove),
};
module_platform_driver(max8831_bl_driver);

MODULE_AUTHOR("Chaitanya Bandi <bandik@nvidia.com>");
MODULE_DESCRIPTION("MAX8831 Backlight display driver");
MODULE_LICENSE("GPL v2");
