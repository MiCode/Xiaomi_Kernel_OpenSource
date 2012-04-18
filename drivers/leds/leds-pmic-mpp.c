/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/slab.h>

#include <mach/pmic.h>

#define LED_MPP(x)		((x) & 0xFF)
#define LED_CURR(x)		((x) >> 16)

struct pmic_mpp_led_data {
	struct led_classdev cdev;
	int curr;
	int mpp;
};

static void pm_mpp_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	struct pmic_mpp_led_data *led;
	int ret;

	led = container_of(led_cdev, struct pmic_mpp_led_data, cdev);

	if (value < LED_OFF || value > led->cdev.max_brightness) {
		dev_err(led->cdev.dev, "Invalid brightness value");
		return;
	}

	ret = pmic_secure_mpp_config_i_sink(led->mpp, led->curr,
			value ? PM_MPP__I_SINK__SWITCH_ENA :
				PM_MPP__I_SINK__SWITCH_DIS);
	if (ret)
		dev_err(led_cdev->dev, "can't set mpp led\n");
}

static int pmic_mpp_led_probe(struct platform_device *pdev)
{
	const struct led_platform_data *pdata = pdev->dev.platform_data;
	struct pmic_mpp_led_data *led, *tmp_led;
	int i, rc;

	if (!pdata) {
		dev_err(&pdev->dev, "platform data not supplied\n");
		return -EINVAL;
	}

	led = kcalloc(pdata->num_leds, sizeof(*led), GFP_KERNEL);
	if (!led) {
		dev_err(&pdev->dev, "failed to alloc memory\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, led);

	for (i = 0; i < pdata->num_leds; i++) {
		tmp_led	= &led[i];
		tmp_led->cdev.name = pdata->leds[i].name;
		tmp_led->cdev.brightness_set = pm_mpp_led_set;
		tmp_led->cdev.brightness = LED_OFF;
		tmp_led->cdev.max_brightness = LED_FULL;
		tmp_led->mpp = LED_MPP(pdata->leds[i].flags);
		tmp_led->curr = LED_CURR(pdata->leds[i].flags);

		if (tmp_led->curr < PM_MPP__I_SINK__LEVEL_5mA ||
			tmp_led->curr > PM_MPP__I_SINK__LEVEL_40mA) {
			dev_err(&pdev->dev, "invalid current\n");
			goto unreg_led_cdev;
		}

		rc = led_classdev_register(&pdev->dev, &tmp_led->cdev);
		if (rc) {
			dev_err(&pdev->dev, "failed to register led\n");
			goto unreg_led_cdev;
		}
	}

	return 0;

unreg_led_cdev:
	while (i)
		led_classdev_unregister(&led[--i].cdev);

	kfree(led);
	return rc;

}

static int __devexit pmic_mpp_led_remove(struct platform_device *pdev)
{
	const struct led_platform_data *pdata = pdev->dev.platform_data;
	struct pmic_mpp_led_data *led = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < pdata->num_leds; i++)
		led_classdev_unregister(&led[i].cdev);

	kfree(led);

	return 0;
}

static struct platform_driver pmic_mpp_led_driver = {
	.probe		= pmic_mpp_led_probe,
	.remove		= __devexit_p(pmic_mpp_led_remove),
	.driver		= {
		.name	= "pmic-mpp-leds",
		.owner	= THIS_MODULE,
	},
};

static int __init pmic_mpp_led_init(void)
{
	return platform_driver_register(&pmic_mpp_led_driver);
}
module_init(pmic_mpp_led_init);

static void __exit pmic_mpp_led_exit(void)
{
	platform_driver_unregister(&pmic_mpp_led_driver);
}
module_exit(pmic_mpp_led_exit);

MODULE_DESCRIPTION("PMIC MPP LEDs driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:pmic-mpp-leds");
