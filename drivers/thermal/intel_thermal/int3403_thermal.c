/*
 * ACPI INT3403 thermal driver
 * Copyright (c) 2013, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>

#define INT3403_TYPE_SENSOR		0x03
#define INT3403_PERF_CHANGED_EVENT	0x80
#define INT3403_THERMAL_EVENT		0x90

#define DECI_KELVIN_TO_MILLI_CELSIUS(t, off) (((t) - (off)) * 100)
#define KELVIN_OFFSET	2732
#define MILLI_CELSIUS_TO_DECI_KELVIN(t, off) (((t) / 100) + (off))

#define ACPI_INT3403_CLASS		"int3403"
#define ACPI_INT3403_FILE_STATE		"state"

struct int3403_sensor {
	struct thermal_zone_device *tzone;
	unsigned long *thresholds;
};

static int sys_get_curr_temp(struct thermal_zone_device *tzone,
				unsigned long *temp)
{
	struct acpi_device *device = tzone->devdata;
	unsigned long long tmp;
	acpi_status status;

	status = acpi_evaluate_integer(device->handle, "_TMP", NULL, &tmp);
	if (ACPI_FAILURE(status))
		return -EIO;

	*temp = DECI_KELVIN_TO_MILLI_CELSIUS(tmp, KELVIN_OFFSET);

	return 0;
}

static int sys_get_trip_hyst(struct thermal_zone_device *tzone,
		int trip, unsigned long *temp)
{
	struct acpi_device *device = tzone->devdata;
	unsigned long long hyst;
	acpi_status status;

	status = acpi_evaluate_integer(device->handle, "GTSH", NULL, &hyst);
	if (ACPI_FAILURE(status))
		return -EIO;

	*temp = DECI_KELVIN_TO_MILLI_CELSIUS(hyst, KELVIN_OFFSET);

	return 0;
}

static int sys_get_trip_temp(struct thermal_zone_device *tzone,
		int trip, unsigned long *temp)
{
	struct acpi_device *device = tzone->devdata;
	struct int3403_sensor *obj = acpi_driver_data(device);

	/*
	 * get_trip_temp is a mandatory callback but
	 * PATx method doesn't return any value, so return
	 * cached value, which was last set from user space.
	 */
	*temp = obj->thresholds[trip];

	return 0;
}

static int sys_get_trip_type(struct thermal_zone_device *thermal,
		int trip, enum thermal_trip_type *type)
{
	/* Mandatory callback, may not mean much here */
	*type = THERMAL_TRIP_PASSIVE;

	return 0;
}

int sys_set_trip_temp(struct thermal_zone_device *tzone, int trip,
							unsigned long temp)
{
	struct acpi_device *device = tzone->devdata;
	acpi_status status;
	char name[10];
	int ret = 0;
	struct int3403_sensor *obj = acpi_driver_data(device);

	snprintf(name, sizeof(name), "PAT%d", trip);
	if (acpi_has_method(device->handle, name)) {
		status = acpi_execute_simple_method(device->handle, name,
				MILLI_CELSIUS_TO_DECI_KELVIN(temp,
							KELVIN_OFFSET));
		if (ACPI_FAILURE(status))
			ret = -EIO;
		else
			obj->thresholds[trip] = temp;
	} else {
		ret = -EIO;
		dev_err(&device->dev, "sys_set_trip_temp: method not found\n");
	}

	return ret;
}

static struct thermal_zone_device_ops tzone_ops = {
	.get_temp = sys_get_curr_temp,
	.get_trip_temp = sys_get_trip_temp,
	.get_trip_type = sys_get_trip_type,
	.set_trip_temp = sys_set_trip_temp,
	.get_trip_hyst =  sys_get_trip_hyst,
};

static void acpi_thermal_notify(acpi_handle handle,
		u32 event, void *data)
{
	struct acpi_device *device = data;
	struct int3403_sensor *obj;

	if (!device)
		return;

	obj = acpi_driver_data(device);
	if (!obj)
		return;

	switch (event) {
	case INT3403_PERF_CHANGED_EVENT:
		break;
	case INT3403_THERMAL_EVENT:
		thermal_zone_device_update(obj->tzone);
		break;
	default:
		dev_err(&device->dev, "Unsupported event [0x%x]\n", event);
		break;
	}
}

static int acpi_int3403_add(struct platform_device *pdev)
{
	struct acpi_device *device = ACPI_COMPANION(&(pdev->dev));
	int result = 0;
	unsigned long long ptyp;
	acpi_status status;
	struct int3403_sensor *obj;
	unsigned long long trip_cnt;
	int trip_mask = 0;

	if (!device)
		return -EINVAL;

	status = acpi_evaluate_integer(device->handle, "PTYP", NULL, &ptyp);
	if (ACPI_FAILURE(status))
		return -EINVAL;

	if (ptyp != INT3403_TYPE_SENSOR)
		return -EINVAL;

	obj = devm_kzalloc(&device->dev, sizeof(*obj), GFP_KERNEL);
	if (!obj)
		return -ENOMEM;

	device->driver_data = obj;

	status = acpi_evaluate_integer(device->handle, "PATC", NULL,
						&trip_cnt);
	if (ACPI_FAILURE(status))
		trip_cnt = 0;

	if (trip_cnt) {
		/* We have to cache, thresholds can't be readback */
		obj->thresholds = devm_kzalloc(&device->dev,
					sizeof(*obj->thresholds) * trip_cnt,
					GFP_KERNEL);
		if (!obj->thresholds) {
			result = -ENOMEM;
			goto err_free_obj;
		}
		trip_mask = BIT(trip_cnt) - 1;
	}

	obj->tzone = thermal_zone_device_register(acpi_device_bid(device),
				trip_cnt, trip_mask, device, &tzone_ops,
				NULL, 0, 0);
	if (IS_ERR(obj->tzone)) {
		result = PTR_ERR(obj->tzone);
		obj->tzone = NULL;
		goto err_free_obj;
	}

	result = acpi_install_notify_handler(device->handle,
			ACPI_DEVICE_NOTIFY, acpi_thermal_notify,
			(void *)device);
	if (result)
		goto err_free_obj;

	return 0;

 err_free_obj:
	if (obj->tzone)
		thermal_zone_device_unregister(obj->tzone);
	kfree(obj->thresholds);
	kfree(obj);
	return result;
}

static int acpi_int3403_remove(struct platform_device *pdev)
{
	struct acpi_device *device = ACPI_COMPANION(&pdev->dev);
	struct int3403_sensor *obj;

	obj = acpi_driver_data(device);
	thermal_zone_device_unregister(obj->tzone);

	return 0;
}

ACPI_MODULE_NAME("int3403");
static const struct acpi_device_id int3403_device_ids[] = {
	{"INT3403", 0},
	{"", 0},
};
MODULE_DEVICE_TABLE(acpi, int3403_device_ids);

static struct platform_driver acpi_int3403_driver = {
	.probe = acpi_int3403_add,
	.remove = acpi_int3403_remove,
	.driver = {
		.name = "INT3403",
		.owner  = THIS_MODULE,
		.acpi_match_table = int3403_device_ids,
	},
};

module_platform_driver(acpi_int3403_driver);

MODULE_AUTHOR("Srinivas Pandruvada <srinivas.pandruvada@linux.intel.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ACPI INT3403 thermal driver");
