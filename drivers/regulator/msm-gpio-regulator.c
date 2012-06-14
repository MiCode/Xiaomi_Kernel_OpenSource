/*
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/msm-gpio-regulator.h>

struct gpio_vreg {
	struct regulator_desc	desc;
	struct regulator_dev	*rdev;
	char			*gpio_label;
	char			*name;
	unsigned		gpio;
	int			active_low;
	bool			gpio_requested;
};

static int gpio_vreg_request_gpio(struct gpio_vreg *vreg)
{
	int rc = 0;

	/* Request GPIO now if it hasn't been requested before. */
	if (!vreg->gpio_requested) {
		rc = gpio_request(vreg->gpio, vreg->gpio_label);
		if (rc < 0) {
			pr_err("failed to request gpio %u (%s), rc=%d\n",
				vreg->gpio, vreg->gpio_label, rc);
			return rc;
		} else {
			vreg->gpio_requested = true;
		}

		rc = gpio_sysfs_set_active_low(vreg->gpio, vreg->active_low);
		if (rc < 0)
			pr_err("active_low=%d failed for gpio %u, rc=%d\n",
				vreg->active_low, vreg->gpio, rc);
	}

	return rc;
}

static int gpio_vreg_is_enabled(struct regulator_dev *rdev)
{
	struct gpio_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = gpio_vreg_request_gpio(vreg);
	if (rc < 0)
		return rc;

	return (gpio_get_value_cansleep(vreg->gpio) ? 1 : 0) ^ vreg->active_low;
}

static int gpio_vreg_enable(struct regulator_dev *rdev)
{
	struct gpio_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = gpio_vreg_request_gpio(vreg);
	if (rc < 0)
		return rc;

	return gpio_direction_output(vreg->gpio, !vreg->active_low);
}

static int gpio_vreg_disable(struct regulator_dev *rdev)
{
	struct gpio_vreg *vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = gpio_vreg_request_gpio(vreg);
	if (rc < 0)
		return rc;

	return gpio_direction_output(vreg->gpio, vreg->active_low);
}

static struct regulator_ops gpio_vreg_ops = {
	.enable		= gpio_vreg_enable,
	.disable	= gpio_vreg_disable,
	.is_enabled	= gpio_vreg_is_enabled,
};

static int __devinit gpio_vreg_probe(struct platform_device *pdev)
{
	const struct gpio_regulator_platform_data *pdata;
	struct gpio_vreg *vreg;
	int rc = 0;

	pdata = pdev->dev.platform_data;

	if (!pdata) {
		pr_err("platform data required.\n");
		return -EINVAL;
	}

	if (!pdata->gpio_label) {
		pr_err("gpio_label required.\n");
		return -EINVAL;
	}

	if (!pdata->regulator_name) {
		pr_err("regulator_name required.\n");
		return -EINVAL;
	}

	vreg = kzalloc(sizeof(struct gpio_vreg), GFP_KERNEL);
	if (!vreg) {
		pr_err("kzalloc failed.\n");
		return -ENOMEM;
	}

	vreg->name = kstrdup(pdata->regulator_name, GFP_KERNEL);
	if (!vreg->name) {
		pr_err("kzalloc failed.\n");
		rc = -ENOMEM;
		goto free_vreg;
	}

	vreg->gpio_label = kstrdup(pdata->gpio_label, GFP_KERNEL);
	if (!vreg->gpio_label) {
		pr_err("kzalloc failed.\n");
		rc = -ENOMEM;
		goto free_name;
	}

	vreg->gpio		= pdata->gpio;
	vreg->active_low	= (pdata->active_low ? 1 : 0);
	vreg->gpio_requested	= false;

	vreg->desc.name		= vreg->name;
	vreg->desc.id		= pdev->id;
	vreg->desc.ops		= &gpio_vreg_ops;
	vreg->desc.type		= REGULATOR_VOLTAGE;
	vreg->desc.owner	= THIS_MODULE;

	vreg->rdev = regulator_register(&vreg->desc, &pdev->dev,
					&pdata->init_data, vreg, NULL);
	if (IS_ERR(vreg->rdev)) {
		rc = PTR_ERR(vreg->rdev);
		pr_err("%s: regulator_register failed, rc=%d.\n", vreg->name,
			rc);
		goto free_gpio_label;
	}

	platform_set_drvdata(pdev, vreg);

	pr_info("id=%d, name=%s, gpio=%u, gpio_label=%s\n", pdev->id,
		vreg->name, vreg->gpio, vreg->gpio_label);

	return rc;

free_gpio_label:
	kfree(vreg->gpio_label);
free_name:
	kfree(vreg->name);
free_vreg:
	kfree(vreg);

	return rc;
}

static int __devexit gpio_vreg_remove(struct platform_device *pdev)
{
	struct gpio_vreg *vreg = platform_get_drvdata(pdev);

	if (vreg->gpio_requested)
		gpio_free(vreg->gpio);

	regulator_unregister(vreg->rdev);
	kfree(vreg->name);
	kfree(vreg->gpio_label);
	kfree(vreg);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver gpio_vreg_driver = {
	.probe = gpio_vreg_probe,
	.remove = __devexit_p(gpio_vreg_remove),
	.driver = {
		.name = GPIO_REGULATOR_DEV_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init gpio_vreg_init(void)
{
	return platform_driver_register(&gpio_vreg_driver);
}

static void __exit gpio_vreg_exit(void)
{
	platform_driver_unregister(&gpio_vreg_driver);
}

postcore_initcall(gpio_vreg_init);
module_exit(gpio_vreg_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("GPIO regulator driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" GPIO_REGULATOR_DEV_NAME);
