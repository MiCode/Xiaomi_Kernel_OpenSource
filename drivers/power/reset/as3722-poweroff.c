/*
 * Power off driver for ams AS3722 device.
 *
 * Copyright (c) 2013, NVIDIA Corporation.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */

#include <linux/mfd/as3722.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
 #include <linux/power/reset/system-pmic.h>
#include <linux/slab.h>

struct as3722_poweroff {
	struct device *dev;
	struct as3722 *as3722;
	struct system_pmic_dev *system_pmic_dev;
	bool use_power_off;
	bool use_power_reset;
};

static void as3722_pm_power_off(void *drv_data)
{
	struct as3722_poweroff *as3722_poweroff = drv_data;
	int ret;

	dev_info(as3722_poweroff->dev, " Powering off system\n");
	ret = as3722_update_bits(as3722_poweroff->as3722,
		AS3722_RESET_CONTROL_REG, AS3722_POWER_OFF, AS3722_POWER_OFF);
	if (ret < 0)
		dev_err(as3722_poweroff->dev,
			"RESET_CONTROL_REG update failed, %d\n", ret);
}

static void as3722_pm_power_reset(void *drv_data)
{
	struct as3722_poweroff *as3722_poweroff = drv_data;
	int ret;

	dev_info(as3722_poweroff->dev, " Power resetting of system\n");
	ret = as3722_update_bits(as3722_poweroff->as3722,
		AS3722_RESET_CONTROL_REG, AS3722_FORCE_RESET,
		AS3722_FORCE_RESET);
	if (ret < 0)
		dev_err(as3722_poweroff->dev,
			"RESET_CONTROL_REG update failed, %d\n", ret);
}

static struct system_pmic_ops as3722_pm_ops = {
	.power_off = as3722_pm_power_off,
	.power_reset = as3722_pm_power_reset,
};

static int as3722_poweroff_probe(struct platform_device *pdev)
{
	struct as3722_poweroff *as3722_poweroff;
	struct device_node *np = pdev->dev.parent->of_node;
	struct as3722 *as3722 = dev_get_drvdata(pdev->dev.parent);
	struct as3722_platform_data *pdata = as3722->dev->platform_data;
	struct system_pmic_config config;
	bool use_power_off = false;
	bool use_power_reset = false;

	if (pdata) {
		use_power_off = pdata->use_power_off;
		use_power_reset = pdata->use_power_reset;
	} else {
		if (np) {
			use_power_off = of_property_read_bool(np,
					"ams,system-power-controller");
			if (!use_power_off)
				use_power_off = of_property_read_bool(np,
					"system-pmic-power-off");
			use_power_reset = of_property_read_bool(np,
					"ams,system-power-controller");
			if (!use_power_reset)
				use_power_reset = of_property_read_bool(np,
					"system-pmic-power-reset");
		}
	}

	if (!use_power_off && !use_power_reset) {
		dev_warn(&pdev->dev,
			"power off and reset functionality not selected\n");
		return 0;
	}

	as3722_poweroff = devm_kzalloc(&pdev->dev, sizeof(*as3722_poweroff),
				GFP_KERNEL);
	if (!as3722_poweroff)
		return -ENOMEM;

	as3722_poweroff->as3722 = as3722;
	as3722_poweroff->dev = &pdev->dev;
	as3722_poweroff->use_power_off = use_power_off;
	as3722_poweroff->use_power_reset = use_power_reset;

	config.allow_power_off = use_power_off;
	config.allow_power_reset = use_power_reset;

	as3722_poweroff->system_pmic_dev = system_pmic_register(&pdev->dev,
				&as3722_pm_ops, &config, as3722_poweroff);
	if (IS_ERR(as3722_poweroff->system_pmic_dev)) {
		int ret = PTR_ERR(as3722_poweroff->system_pmic_dev);

		dev_err(&pdev->dev, "System PMIC registartion failed: %d\n",
			ret);
		return ret;
	}
	return 0;
}

static int as3722_poweroff_remove(struct platform_device *pdev)
{
	struct as3722_poweroff *as3722_poweroff = platform_get_drvdata(pdev);

	system_pmic_unregister(as3722_poweroff->system_pmic_dev);
	return 0;
}

static struct platform_driver as3722_poweroff_driver = {
	.driver = {
		.name = "as3722-power-off",
		.owner = THIS_MODULE,
	},
	.probe = as3722_poweroff_probe,
	.remove = as3722_poweroff_remove,
};

module_platform_driver(as3722_poweroff_driver);

MODULE_DESCRIPTION("Power off driver for ams AS3722 PMIC Device");
MODULE_ALIAS("platform:as3722-power-off");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_LICENSE("GPL v2");
