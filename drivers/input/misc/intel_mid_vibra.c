/*
 *  intel_mid_vibra.c - Intel Vibrator for Intel CherryTrail platform
 *
 *  Copyright (C) 2014 Intel Corp
 *  Author: B, Jayachandran <jayachandran.b@intel.com>
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */



#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/acpi.h>
#include <linux/pm_runtime.h>
#include <linux/input/intel_mid_vibra.h>
#include "mid_vibra.h"


static void vibra_disable(struct vibra_info *info)
{
	pr_err("%s: Disable", __func__);
	mutex_lock(&info->lock);
	vibra_gpio_set_value_cansleep(info, 0)
	info->enabled = false;
	info->pwm_configure(info, false);
	pm_runtime_put(info->dev);
	mutex_unlock(&info->lock);
	mutex_lock(&info->lock);
	vibra_gpio_set_value_cansleep(info, 0)
	info->enabled = false;
	info->pwm_configure(info, false);
	pm_runtime_put(info->dev);
	mutex_unlock(&info->lock);
}

static void vibra_drv_enable(struct vibra_info *info)
{
	pr_debug("%s: Enable", __func__);
	mutex_lock(&info->lock);
	pm_runtime_get_sync(info->dev);
	info->pwm_configure(info, true);
	vibra_gpio_set_value_cansleep(info, 1)
	info->enabled = true;
	mutex_unlock(&info->lock);
}

/*******************************************************************************
 * SYSFS                                                                       *
 ******************************************************************************/

static ssize_t vibra_show_vibrator(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct vibra_info *info = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", info->enabled);

}

static ssize_t vibra_set_vibrator(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t len)
{
	long vibrator_enable;
	struct vibra_info *info = dev_get_drvdata(dev);

	if (kstrtol(buf, 0, &vibrator_enable))
		return -EINVAL;
	if (vibrator_enable == info->enabled)
		return len;
	else if (vibrator_enable == 0)
		info->disable(info);
	else if (vibrator_enable == 1)
		info->enable(info);
	else
		return -EINVAL;
	return len;
}
unsigned long mid_vibra_base_unit;
unsigned long mid_vibra_duty_cycle;

static DEVICE_ATTR(vibrator, S_IRUGO | S_IWUSR,
		   vibra_show_vibrator, vibra_set_vibrator);
static DEVICE_ULONG_ATTR(pwm_baseunit, S_IRUGO | S_IWUSR,
				 mid_vibra_base_unit);
static DEVICE_ULONG_ATTR(pwm_ontime_div, S_IRUGO | S_IWUSR,
				 mid_vibra_duty_cycle);

static struct attribute *vibra_attrs[] = {
	&dev_attr_vibrator.attr,
	&dev_attr_pwm_baseunit.attr.attr,
	&dev_attr_pwm_ontime_div.attr.attr,
	0,
};

static const struct attribute_group vibra_attr_group = {
	.attrs = vibra_attrs,
};


/*** Module ***/
#if CONFIG_PM
static int intel_vibra_runtime_suspend(struct device *dev)
{
	struct vibra_info *info = dev_get_drvdata(dev);

	pr_debug("In %s\n", __func__);
	info->pwm_configure(info, false);
	return 0;
}

static int intel_vibra_runtime_resume(struct device *dev)
{
	pr_debug("In %s\n", __func__);
	return 0;
}

static void intel_vibra_complete(struct device *dev)
{
	pr_debug("In %s\n", __func__);
	intel_vibra_runtime_resume(dev);
}

static const struct dev_pm_ops intel_mid_vibra_pm_ops = {
	.prepare = intel_vibra_runtime_suspend,
	.complete = intel_vibra_complete,
	.runtime_suspend = intel_vibra_runtime_suspend,
	.runtime_resume = intel_vibra_runtime_resume,
};
#endif
struct vibra_info *mid_vibra_setup(struct device *dev,
				 struct mid_vibra_pdata *data)
{
	struct vibra_info *info;
	pr_debug("probe data div %x, base %x, alt_fn %d ext_drv %d, name:%s",
							data->time_divisor,
							data->base_unit,
							data->alt_fn,
							data->ext_drv,
							data->name);

	info =  devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		pr_err("%s: no memory for driver context", __func__);
		return NULL;
	}

	info->alt_fn = data->alt_fn;
	info->ext_drv = data->ext_drv;
	info->gpio_en = data->gpio_en;
	info->gpio_pwm = data->gpio_pwm;
	info->name = data->name;
	info->use_gpio_en = data->use_gpio_en;

	info->dev = dev;
	mutex_init(&info->lock);
	info->vibra_attr_group = &vibra_attr_group;
	mid_vibra_base_unit = data->base_unit;
	mid_vibra_duty_cycle = data->time_divisor;
	info->base_unit = &mid_vibra_base_unit;
	info->duty_cycle = &mid_vibra_duty_cycle;

	if (!strncmp(info->name, "VIBR22A8", 8)) {
		info->enable = vibra_drv_enable;
	} else {
		pr_err("%s: unsupported vibrator device", __func__);
		return NULL;
	}
	info->disable = vibra_disable;

	return info;
}

static const struct acpi_device_id vibra_acpi_ids[];

void *mid_vibra_acpi_get_drvdata(const char *hid)
{
	const struct acpi_device_id *id;

	for (id = vibra_acpi_ids; id->id[0]; id++)
		if (!strncmp(id->id, hid, 16))
			return (void *)id->driver_data;
	return 0;
}

static const struct acpi_device_id vibra_acpi_ids[] = {
	{ "VIBR22A8", (kernel_ulong_t) &vibra_pdata },
	{},
};
MODULE_DEVICE_TABLE(acpi, vibra_acpi_ids);

static struct platform_driver plat_vibra_driver = {
	.driver = {
		.name = "intel_mid_pmic_vibra",
		.owner = THIS_MODULE,
		.acpi_match_table = ACPI_PTR(vibra_acpi_ids),
#ifdef CONFIG_PM
		.pm = &intel_mid_vibra_pm_ops,
#endif
	},
	.probe = intel_mid_plat_vibra_probe,
	.remove = intel_mid_plat_vibra_remove,
};

/**
* intel_mid_vibra_init - Module init function
*
* Registers platform
* Init all data strutures
*/
static int __init intel_mid_vibra_init(void)
{
	int ret = 0;
	ret = platform_driver_register(&plat_vibra_driver);
	if (ret)
		pr_err("Platform register failed\n");

	return ret;
}

/**
* intel_mid_vibra_exit - Module exit function
*
* Unregisters platform
* Frees all data strutures
*/
static void __exit intel_mid_vibra_exit(void)
{
	platform_driver_unregister(&plat_vibra_driver);
	pr_debug("intel_mid_vibra driver exited\n");
	return;
}

late_initcall(intel_mid_vibra_init);
module_exit(intel_mid_vibra_exit);

MODULE_ALIAS("acpi:intel_mid_vibra");
MODULE_DESCRIPTION("Intel(R) MID Vibra driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("KP Jeeja <jeeja.kp@intel.com>");
