/*
 * Qualcomm PMIC8XXX GPIO driver based on RPC
 *
 * Copyright (c) 2011, The Linux Foundation. All rights reserved.
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

#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/gpio-pm8xxx-rpc.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <mach/pmic.h>

struct pm8xxx_gpio_rpc_chip {
	struct list_head	link;
	struct gpio_chip	gpio_chip;
};

static LIST_HEAD(pm8xxx_gpio_rpc_chips);
static DEFINE_MUTEX(pm8xxx_gpio_chips_lock);

static int pm8xxx_gpio_rpc_get(struct pm8xxx_gpio_rpc_chip *pm8xxx_gpio_chip,
								unsigned gpio)
{
	int rc;

	if (gpio >= pm8xxx_gpio_chip->gpio_chip.ngpio
					|| pm8xxx_gpio_chip == NULL)
		return -EINVAL;

	rc =  pmic_gpio_get_value(gpio);

	return rc;
}

static int pm8xxx_gpio_rpc_set(struct pm8xxx_gpio_rpc_chip *pm8xxx_gpio_chip,
						 unsigned gpio, int value)
{
	int rc;

	if (gpio >= pm8xxx_gpio_chip->gpio_chip.ngpio ||
					pm8xxx_gpio_chip == NULL)
		return -EINVAL;

	rc = pmic_gpio_set_value(gpio, value);

	return rc;
}

static int pm8xxx_gpio_rpc_set_direction(struct pm8xxx_gpio_rpc_chip
			*pm8xxx_gpio_chip, unsigned gpio, int direction)
{
	int rc = 0;

	if (!direction || pm8xxx_gpio_chip == NULL)
		return -EINVAL;

	if (direction ==  PM_GPIO_DIR_IN)
		rc = pmic_gpio_direction_input(gpio);
	else if (direction == PM_GPIO_DIR_OUT)
		rc = pmic_gpio_direction_output(gpio);

	return rc;
}

static int pm8xxx_gpio_rpc_read(struct gpio_chip *gpio_chip, unsigned offset)
{
	struct pm8xxx_gpio_rpc_chip *pm8xxx_gpio_chip =
					dev_get_drvdata(gpio_chip->dev);

	return pm8xxx_gpio_rpc_get(pm8xxx_gpio_chip, offset);
}

static void pm8xxx_gpio_rpc_write(struct gpio_chip *gpio_chip,
						unsigned offset, int val)
{
	struct pm8xxx_gpio_rpc_chip *pm8xxx_gpio_chip =
					dev_get_drvdata(gpio_chip->dev);

	pm8xxx_gpio_rpc_set(pm8xxx_gpio_chip, offset, !!val);
}

static int pm8xxx_gpio_rpc_direction_input(struct gpio_chip *gpio_chip,
							unsigned offset)
{
	struct pm8xxx_gpio_rpc_chip *pm8xxx_gpio_chip =
					dev_get_drvdata(gpio_chip->dev);

	return pm8xxx_gpio_rpc_set_direction(pm8xxx_gpio_chip, offset,
							PM_GPIO_DIR_IN);
}

static int pm8xxx_gpio_rpc_direction_output(struct gpio_chip *gpio_chip,
						unsigned offset, int val)
{
	int ret = 0;

	struct pm8xxx_gpio_rpc_chip *pm8xxx_gpio_chip =
					dev_get_drvdata(gpio_chip->dev);

	ret = pm8xxx_gpio_rpc_set_direction(pm8xxx_gpio_chip, offset,
							PM_GPIO_DIR_OUT);
	if (!ret)
		ret = pm8xxx_gpio_rpc_set(pm8xxx_gpio_chip, offset, !!val);

	return ret;
}

static void pm8xxx_gpio_rpc_dbg_show(struct seq_file *s, struct gpio_chip
								*gpio_chip)
{
	struct pm8xxx_gpio_rpc_chip *pmxx_gpio_chip =
					dev_get_drvdata(gpio_chip->dev);
	u8 state, mode;
	const char *label;
	int i;

	for (i = 0; i < gpio_chip->ngpio; i++) {
		label = gpiochip_is_requested(gpio_chip, i);
		state = pm8xxx_gpio_rpc_get(pmxx_gpio_chip, i);
		mode =  pmic_gpio_get_direction(i);
		seq_printf(s, "gpio-%-3d (%-12.12s) %s %s",
				gpio_chip->base + i,
				label ? label : " ", mode ? "out" : "in",
				state ? "hi" : "lo");
		seq_printf(s, "\n");
	}
}

static int __devinit pm8xxx_gpio_rpc_probe(struct platform_device *pdev)
{
	int ret;
	struct pm8xxx_gpio_rpc_chip *pm8xxx_gpio_chip;
	const struct pm8xxx_gpio_rpc_platform_data *pdata =
					pdev->dev.platform_data;

	if (!pdata) {
		pr_err("missing platform data\n");
		return -EINVAL;
	}

	pm8xxx_gpio_chip = kzalloc(sizeof(struct pm8xxx_gpio_rpc_chip),
								GFP_KERNEL);
	if (!pm8xxx_gpio_chip) {
		pr_err("Cannot allocate pm8xxx_gpio_chip\n");
		return -ENOMEM;
	}

	pm8xxx_gpio_chip->gpio_chip.label = "pm8xxx-gpio-rpc";
	pm8xxx_gpio_chip->gpio_chip.direction_input	=
					pm8xxx_gpio_rpc_direction_input;
	pm8xxx_gpio_chip->gpio_chip.direction_output	=
					pm8xxx_gpio_rpc_direction_output;
	pm8xxx_gpio_chip->gpio_chip.get		= pm8xxx_gpio_rpc_read;
	pm8xxx_gpio_chip->gpio_chip.set		= pm8xxx_gpio_rpc_write;
	pm8xxx_gpio_chip->gpio_chip.dbg_show	= pm8xxx_gpio_rpc_dbg_show;
	pm8xxx_gpio_chip->gpio_chip.ngpio	= pdata->ngpios;
	pm8xxx_gpio_chip->gpio_chip.can_sleep	= 1;
	pm8xxx_gpio_chip->gpio_chip.dev		= &pdev->dev;
	pm8xxx_gpio_chip->gpio_chip.base	= pdata->gpio_base;

	mutex_lock(&pm8xxx_gpio_chips_lock);
	list_add(&pm8xxx_gpio_chip->link, &pm8xxx_gpio_rpc_chips);
	mutex_unlock(&pm8xxx_gpio_chips_lock);
	platform_set_drvdata(pdev, pm8xxx_gpio_chip);

	ret = gpiochip_add(&pm8xxx_gpio_chip->gpio_chip);
	if (ret) {
		pr_err("gpiochip_add failed ret = %d\n", ret);
		goto reset_drvdata;
	}

	pr_info("OK: base=%d, ngpio=%d\n", pm8xxx_gpio_chip->gpio_chip.base,
		pm8xxx_gpio_chip->gpio_chip.ngpio);

	return 0;

reset_drvdata:
	mutex_lock(&pm8xxx_gpio_chips_lock);
	list_del(&pm8xxx_gpio_chip->link);
	mutex_unlock(&pm8xxx_gpio_chips_lock);
	platform_set_drvdata(pdev, NULL);
	kfree(pm8xxx_gpio_chip);
	mutex_destroy(&pm8xxx_gpio_chips_lock);
	return ret;
}

static int __devexit pm8xxx_gpio_rpc_remove(struct platform_device *pdev)
{
	struct pm8xxx_gpio_rpc_chip *pm8xxx_gpio_chip =
						platform_get_drvdata(pdev);

	mutex_lock(&pm8xxx_gpio_chips_lock);
	list_del(&pm8xxx_gpio_chip->link);
	mutex_unlock(&pm8xxx_gpio_chips_lock);
	platform_set_drvdata(pdev, NULL);
	if (gpiochip_remove(&pm8xxx_gpio_chip->gpio_chip))
		pr_err("failed to remove gpio chip\n");
	kfree(pm8xxx_gpio_chip);
	mutex_destroy(&pm8xxx_gpio_chips_lock);
	return 0;
}

static struct platform_driver pm8xxx_gpio_rpc_driver = {
	.probe		= pm8xxx_gpio_rpc_probe,
	.remove		= __devexit_p(pm8xxx_gpio_rpc_remove),
	.driver		= {
		.name	= PM8XXX_GPIO_DEV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init pm8xxx_gpio_rpc_init(void)
{
	return platform_driver_register(&pm8xxx_gpio_rpc_driver);
}
postcore_initcall(pm8xxx_gpio_rpc_init);

static void __exit pm8xxx_gpio_rpc_exit(void)
{
	platform_driver_unregister(&pm8xxx_gpio_rpc_driver);
}
module_exit(pm8xxx_gpio_rpc_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC GPIO driver based on RPC");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" PM8XXX_GPIO_DEV_NAME);
