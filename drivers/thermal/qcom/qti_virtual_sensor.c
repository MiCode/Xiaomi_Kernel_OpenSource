/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
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


#include <linux/thermal.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/platform_device.h>

#include "qti_virtual_sensor.h"

static const struct virtual_sensor_data qti_virtual_sensors[] = {
	{
		.virt_zone_name = "gpu-virt-max-step",
		.num_sensors = 2,
		.sensor_names = {"gpu0-usr",
				"gpu1-usr"},
		.logic = VIRT_MAXIMUM,
	},
	{
		.virt_zone_name = "silv-virt-max-step",
		.num_sensors = 4,
		.sensor_names = {"cpu0-silver-usr",
				"cpu1-silver-usr",
				"cpu2-silver-usr",
				"cpu3-silver-usr"},
		.logic = VIRT_MAXIMUM,
	},
	{
		.virt_zone_name = "gold-virt-max-step",
		.num_sensors = 4,
		.sensor_names = {"cpu0-gold-usr",
				"cpu1-gold-usr",
				"cpu2-gold-usr",
				"cpu3-gold-usr"},
		.logic = VIRT_MAXIMUM,
	},
	{
		.virt_zone_name = "hexa-silv-max-step",
		.num_sensors = 6,
		.sensor_names = {"cpu0-silver-usr",
				"cpu1-silver-usr",
				"cpu2-silver-usr",
				"cpu3-silver-usr",
				"cpu4-silver-usr",
				"cpu5-silver-usr"},
		.logic = VIRT_MAXIMUM,
	},
	{
		.virt_zone_name = "dual-gold-max-step",
		.num_sensors = 2,
		.sensor_names = {"cpu0-gold-usr",
				"cpu1-gold-usr"},
		.logic = VIRT_MAXIMUM,
	},
	{
		.virt_zone_name = "deca-cpu-max-step",
		.num_sensors = 10,
		.sensor_names = {"apc0-cpu0-usr",
				"apc0-cpu1-usr",
				"apc0-cpu2-usr",
				"apc0-cpu3-usr",
				"apc0-l2-usr",
				"apc1-cpu0-usr",
				"apc1-cpu1-usr",
				"apc1-cpu2-usr",
				"apc1-cpu3-usr",
				"apc1-l2-usr"},
		.logic = VIRT_MAXIMUM,
	},
	{
		.virt_zone_name = "apc-0-max-step",
		.num_sensors = 5,
		.sensor_names = {"cpu-0-0-usr",
				"cpu-0-1-usr",
				"cpu-0-2-usr",
				"cpu-0-3-usr",
				"cpuss-0-usr"},
		.logic = VIRT_MAXIMUM,
	},
	{
		.virt_zone_name = "apc-1-max-step",
		.num_sensors = 9,
		.sensor_names = {"cpu-1-0-usr",
				"cpu-1-1-usr",
				"cpu-1-2-usr",
				"cpu-1-3-usr",
				"cpu-1-4-usr",
				"cpu-1-5-usr",
				"cpu-1-6-usr",
				"cpu-1-7-usr",
				"cpuss-1-usr"},
		.logic = VIRT_MAXIMUM,
	},
	{
		.virt_zone_name = "gpuss-max-step",
		.num_sensors = 2,
		.sensor_names = {"gpuss-0-usr",
				"gpuss-1-usr"},
		.logic = VIRT_MAXIMUM,
	},
	{
		.virt_zone_name = "cpuss-max-step",
		.num_sensors = 5,
		.sensor_names = {"cpuss-0-usr",
				"cpuss-1-usr",
				"cpuss-2-usr",
				"cpuss-3-usr",
				"cpuss-usr"},
		.logic = VIRT_MAXIMUM,
	},
	{
		.virt_zone_name = "cpuss0-max-step",
		.num_sensors = 4,
		.sensor_names = {"cpuss-0-usr",
				"cpuss-1-usr",
				"cpuss-2-usr",
				"cpuss-3-usr"},
		.logic = VIRT_MAXIMUM,
	},
	{
		.virt_zone_name = "apc1-max-step",
		.num_sensors = 4,
		.sensor_names = {"cpu-1-0-usr",
				"cpu-1-1-usr",
				"cpu-1-2-usr",
				"cpu-1-3-usr"},
		.logic = VIRT_MAXIMUM,
	},
	{
		.virt_zone_name = "cpu-0-max-step",
		.num_sensors = 7,
		.sensor_names = {"cpu-0-0-usr",
				"cpu-0-1-usr",
				"cpu-0-2-usr",
				"cpu-0-3-usr",
				"cpu-0-4-usr",
				"cpu-0-5-usr",
				"cpuss-0-usr"},
		.logic = VIRT_MAXIMUM,
	},
	{
		.virt_zone_name = "cpu-1-max-step",
		.num_sensors = 5,
		.sensor_names = {"cpu-1-0-usr",
				"cpu-1-1-usr",
				"cpu-1-2-usr",
				"cpu-1-3-usr",
				"cpuss-1-usr"},
		.logic = VIRT_MAXIMUM,
	},
	{
		.virt_zone_name = "hepta-cpu-max-step",
		.num_sensors = 7,
		.sensor_names = {"cpu-1-0-usr",
				"cpu-1-1-usr",
				"cpu-1-2-usr",
				"cpu-1-3-usr",
				"cpuss-0-usr",
				"cpuss-1-usr",
				"cpuss-2-usr"},
		.logic = VIRT_MAXIMUM,
	},
	{
		.virt_zone_name = "quad-gpuss-max-step",
		.num_sensors = 4,
		.sensor_names = {"gpuss-0-usr",
				"gpuss-1-usr",
				"gpuss-2-usr",
				"gpuss-3-usr"},
		.logic = VIRT_MAXIMUM,
	},
	{
		.virt_zone_name = "hexa-cpu-max-step",
		.num_sensors = 6,
		.sensor_names = {"apc1-cpu0-usr",
				"apc1-cpu1-usr",
				"apc1-cpu2-usr",
				"apc1-cpu3-usr",
				"cpuss0-usr",
				"cpuss1-usr"},
		.logic = VIRT_MAXIMUM,
	},
};

int qti_virtual_sensor_register(struct device *dev)
{
	int sens_ct = 0;
	static int idx;
	struct thermal_zone_device *tz;

	sens_ct = ARRAY_SIZE(qti_virtual_sensors);
	for (; idx < sens_ct; idx++) {
		tz = devm_thermal_of_virtual_sensor_register(dev,
				&qti_virtual_sensors[idx]);
		if (IS_ERR(tz))
			dev_dbg(dev, "sensor:%d register error:%ld\n",
					idx, PTR_ERR(tz));
		else
			dev_dbg(dev, "sensor:%d registered\n", idx);
	}

	return 0;
}
EXPORT_SYMBOL(qti_virtual_sensor_register);
