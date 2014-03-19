/*
 * INT3402 thermal driver for memory temperature reporting
 *
 * Copyright (C) 2014, Intel Corporation
 * Authors: Aaron Lu <aaron.lu@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/acpi.h>
#include <linux/thermal.h>

static int int3402_thermal_get_temp(struct thermal_zone_device *zone,
				    unsigned long *temp)
{
	struct acpi_device *adev = zone->devdata;
	unsigned long long tmp;
	acpi_status status;

	status = acpi_evaluate_integer(adev->handle, "_TMP", NULL, &tmp);
	if (ACPI_FAILURE(status))
		return -ENODEV;

	/* _TMP returns the temperature in tenths of degrees Kelvin */
	*temp = DECI_KELVIN_TO_MILLICELSIUS(tmp);

	return 0;
}

static struct thermal_zone_device_ops int3402_thermal_zone_ops = {
	.get_temp       = int3402_thermal_get_temp,
};


static int int3402_thermal_probe(struct platform_device *pdev)
{
	struct acpi_device *adev = ACPI_COMPANION(&pdev->dev);
	struct thermal_zone_device *zone;

	if (!acpi_has_method(adev, "_TMP"))
		return -ENODEV;

	zone = thermal_zone_device_register("int3402_thermal", 0, 0, adev,
					&int3402_thermal_zone_ops, NULL, 0, 0);
	if (IS_ERR(zone))
		return PTR_ERR(zone);
	platform_set_drvdata(pdev, zone);

	return 0;
}

static int int3402_thermal_remove(struct platform_device *pdev)
{
	struct thermal_zone_device *zone = platform_get_drvdata(pdev);
	thermal_zone_device_unregister(zone);
	return 0;
}

static const struct acpi_device_id int3402_thermal_match[] = {
	{"INT3402", 0},
	{}
};

MODULE_DEVICE_TABLE(acpi, int3402_thermal_match);

static struct platform_driver int3402_thermal_driver = {
	.probe = int3402_thermal_probe,
	.remove = int3402_thermal_remove,
	.driver = {
		   .name = "int3402 thermal",
		   .owner = THIS_MODULE,
		   .acpi_match_table = int3402_thermal_match,
		   },
};

module_platform_driver(int3402_thermal_driver);

MODULE_DESCRIPTION("INT3402 Thermal driver");
MODULE_LICENSE("GPL");
