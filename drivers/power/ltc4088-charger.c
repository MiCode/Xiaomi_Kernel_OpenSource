/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/power/ltc4088-charger.h>

#define MAX_CURRENT_UA(n)	(n)
#define MAX_CURRENT_MA(n)	(n * MAX_CURRENT_UA(1000))

/**
 * ltc4088_max_current - A typical current values supported by the charger
 * @LTC4088_MAX_CURRENT_100mA:  100mA current
 * @LTC4088_MAX_CURRENT_500mA:  500mA current
 * @LTC4088_MAX_CURRENT_1A:     1A current
 */
enum ltc4088_max_current {
	LTC4088_MAX_CURRENT_100mA = 100,
	LTC4088_MAX_CURRENT_500mA = 500,
	LTC4088_MAX_CURRENT_1A = 1000,
};

/**
 * struct ltc4088_chg_chip - Device information
 * @dev:			Device pointer to access the parent
 * @lock:			Enable mutual exclusion
 * @usb_psy:			USB device information
 * @gpio_mode_select_d0:	GPIO #pin for D0 charger line
 * @gpio_mode_select_d1:	GPIO #pin for D1 charger line
 * @gpio_mode_select_d2:	GPIO #pin for D2 charger line
 * @max_current:		Maximum current that is supplied at this time
 */
struct ltc4088_chg_chip {
	struct device		*dev;
	struct mutex		lock;
	struct power_supply	usb_psy;
	unsigned int		gpio_mode_select_d0;
	unsigned int		gpio_mode_select_d1;
	unsigned int		gpio_mode_select_d2;
	unsigned int		max_current;
};

static enum power_supply_property pm_power_props[] = {
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_ONLINE,
};

static char *pm_power_supplied_to[] = {
	"battery",
};

static int ltc4088_set_charging(struct ltc4088_chg_chip *chip, bool enable)
{
	mutex_lock(&chip->lock);

	if (enable) {
		gpio_set_value_cansleep(chip->gpio_mode_select_d2, 0);
	} else {
		/* When disabling charger, set the max current to 0 also */
		chip->max_current = 0;
		gpio_set_value_cansleep(chip->gpio_mode_select_d0, 1);
		gpio_set_value_cansleep(chip->gpio_mode_select_d1, 1);
		gpio_set_value_cansleep(chip->gpio_mode_select_d2, 1);
	}

	mutex_unlock(&chip->lock);

	return 0;
}

static void ltc4088_set_max_current(struct ltc4088_chg_chip *chip, int value)
{
	mutex_lock(&chip->lock);

	/* If current is less than 100mA, we can not support that granularity */
	if (value <  MAX_CURRENT_MA(LTC4088_MAX_CURRENT_100mA)) {
		chip->max_current = 0;
		gpio_set_value_cansleep(chip->gpio_mode_select_d0, 1);
		gpio_set_value_cansleep(chip->gpio_mode_select_d1, 1);
	} else if (value <  MAX_CURRENT_MA(LTC4088_MAX_CURRENT_500mA)) {
		chip->max_current = MAX_CURRENT_MA(LTC4088_MAX_CURRENT_100mA);
		gpio_set_value_cansleep(chip->gpio_mode_select_d0, 0);
		gpio_set_value_cansleep(chip->gpio_mode_select_d1, 0);
	} else if (value <  MAX_CURRENT_MA(LTC4088_MAX_CURRENT_1A)) {
		chip->max_current = MAX_CURRENT_MA(LTC4088_MAX_CURRENT_500mA);
		gpio_set_value_cansleep(chip->gpio_mode_select_d0, 0);
		gpio_set_value_cansleep(chip->gpio_mode_select_d1, 1);
	} else {
		chip->max_current = MAX_CURRENT_MA(LTC4088_MAX_CURRENT_1A);
		gpio_set_value_cansleep(chip->gpio_mode_select_d0, 1);
		gpio_set_value_cansleep(chip->gpio_mode_select_d1, 0);
	}

	mutex_unlock(&chip->lock);
}

static void ltc4088_set_charging_off(struct ltc4088_chg_chip *chip)
{
	gpio_set_value_cansleep(chip->gpio_mode_select_d0, 1);
	gpio_set_value_cansleep(chip->gpio_mode_select_d1, 1);
}

static int ltc4088_set_initial_state(struct ltc4088_chg_chip *chip)
{
	int rc;

	rc = gpio_request(chip->gpio_mode_select_d0, "ltc4088_D0");
	if (rc) {
		pr_err("gpio request failed for GPIO %d\n",
				chip->gpio_mode_select_d0);
		return rc;
	}

	rc = gpio_request(chip->gpio_mode_select_d1, "ltc4088_D1");
	if (rc) {
		pr_err("gpio request failed for GPIO %d\n",
				chip->gpio_mode_select_d1);
		goto gpio_err_d0;
	}

	rc = gpio_request(chip->gpio_mode_select_d2, "ltc4088_D2");
	if (rc) {
		pr_err("gpio request failed for GPIO %d\n",
				chip->gpio_mode_select_d2);
		goto gpio_err_d1;
	}

	rc = gpio_direction_output(chip->gpio_mode_select_d0, 0);
	if (rc) {
		pr_err("failed to set direction for GPIO %d\n",
				chip->gpio_mode_select_d0);
		goto gpio_err_d2;
	}

	rc = gpio_direction_output(chip->gpio_mode_select_d1, 0);
	if (rc) {
		pr_err("failed to set direction for GPIO %d\n",
				chip->gpio_mode_select_d1);
		goto gpio_err_d2;
	}

	rc = gpio_direction_output(chip->gpio_mode_select_d2, 1);
	if (rc) {
		pr_err("failed to set direction for GPIO %d\n",
				chip->gpio_mode_select_d2);
		goto gpio_err_d2;
	}

	return 0;

gpio_err_d2:
	gpio_free(chip->gpio_mode_select_d2);
gpio_err_d1:
	gpio_free(chip->gpio_mode_select_d1);
gpio_err_d0:
	gpio_free(chip->gpio_mode_select_d0);
	return rc;
}

static int pm_power_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	struct ltc4088_chg_chip *chip;

	if (psy->type == POWER_SUPPLY_TYPE_USB) {
		chip = container_of(psy, struct ltc4088_chg_chip,
						usb_psy);
		switch (psp) {
		case POWER_SUPPLY_PROP_ONLINE:
			if (chip->max_current)
				val->intval = 1;
			else
				val->intval = 0;
			break;
		case POWER_SUPPLY_PROP_CURRENT_MAX:
			val->intval = chip->max_current;
			break;
		default:
			return -EINVAL;
		}
	}
	return 0;
}

static int pm_power_set_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	struct ltc4088_chg_chip *chip;

	if (psy->type == POWER_SUPPLY_TYPE_USB) {
		chip = container_of(psy, struct ltc4088_chg_chip,
						usb_psy);
		switch (psp) {
		case POWER_SUPPLY_PROP_ONLINE:
			ltc4088_set_charging(chip, val->intval);
			break;
		case POWER_SUPPLY_PROP_CURRENT_MAX:
			ltc4088_set_max_current(chip, val->intval);
			break;
		default:
			return -EINVAL;
		}
	}
	return 0;
}

static int __devinit ltc4088_charger_probe(struct platform_device *pdev)
{
	int rc;
	struct ltc4088_chg_chip *chip;
	const struct ltc4088_charger_platform_data *pdata
			= pdev->dev.platform_data;

	if (!pdata) {
		pr_err("missing platform data\n");
		return -EINVAL;
	}

	chip = kzalloc(sizeof(struct ltc4088_chg_chip),
					GFP_KERNEL);
	if (!chip) {
		pr_err("Cannot allocate pm_chg_chip\n");
		return -ENOMEM;
	}

	chip->dev = &pdev->dev;

	if (pdata->gpio_mode_select_d0 < 0 ||
		 pdata->gpio_mode_select_d1 < 0 ||
		 pdata->gpio_mode_select_d2 < 0) {
		pr_err("Invalid platform data supplied\n");
		rc = -EINVAL;
		goto free_chip;
	}

	mutex_init(&chip->lock);

	chip->gpio_mode_select_d0 = pdata->gpio_mode_select_d0;
	chip->gpio_mode_select_d1 = pdata->gpio_mode_select_d1;
	chip->gpio_mode_select_d2 = pdata->gpio_mode_select_d2;

	chip->usb_psy.name = "usb",
	chip->usb_psy.type = POWER_SUPPLY_TYPE_USB,
	chip->usb_psy.supplied_to = pm_power_supplied_to,
	chip->usb_psy.num_supplicants = ARRAY_SIZE(pm_power_supplied_to),
	chip->usb_psy.properties = pm_power_props,
	chip->usb_psy.num_properties = ARRAY_SIZE(pm_power_props),
	chip->usb_psy.get_property = pm_power_get_property,
	chip->usb_psy.set_property = pm_power_set_property,

	rc = power_supply_register(chip->dev, &chip->usb_psy);
	if (rc < 0) {
		pr_err("power_supply_register usb failed rc = %d\n", rc);
		goto free_chip;
	}

	platform_set_drvdata(pdev, chip);

	rc = ltc4088_set_initial_state(chip);
	if (rc < 0) {
		pr_err("setting initial state failed rc = %d\n", rc);
		goto unregister_usb;
	}

	return 0;

unregister_usb:
	platform_set_drvdata(pdev, NULL);
	power_supply_unregister(&chip->usb_psy);
free_chip:
	kfree(chip);

	return rc;
}

static int __devexit ltc4088_charger_remove(struct platform_device *pdev)
{
	struct ltc4088_chg_chip *chip = platform_get_drvdata(pdev);

	ltc4088_set_charging_off(chip);

	gpio_free(chip->gpio_mode_select_d2);
	gpio_free(chip->gpio_mode_select_d1);
	gpio_free(chip->gpio_mode_select_d0);

	power_supply_unregister(&chip->usb_psy);

	platform_set_drvdata(pdev, NULL);
	mutex_destroy(&chip->lock);
	kfree(chip);

	return 0;
}

static struct platform_driver ltc4088_charger_driver = {
	.probe	= ltc4088_charger_probe,
	.remove	= __devexit_p(ltc4088_charger_remove),
	.driver	= {
		.name	= LTC4088_CHARGER_DEV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init ltc4088_charger_init(void)
{
	return platform_driver_register(&ltc4088_charger_driver);
}

static void __exit ltc4088_charger_exit(void)
{
	platform_driver_unregister(&ltc4088_charger_driver);
}

subsys_initcall(ltc4088_charger_init);
module_exit(ltc4088_charger_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("LTC4088 charger/battery driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" LTC4088_CHARGER_DEV_NAME);
