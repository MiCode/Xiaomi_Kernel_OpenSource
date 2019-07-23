// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/thermal.h>

#include "thermal_core.h"

/**
 * backward_compatible_throttle
 * @tz - thermal_zone_device
 *
 * This function update the cooler state by monitoring the current
 * temperature and trip points
 */
static int backward_compatible_throttle(struct thermal_zone_device *tz,
	 int trip)
{
	int trip_temp;
	struct thermal_instance *instance;

	if (trip == THERMAL_TRIPS_NONE)
		trip_temp = tz->forced_passive;
	else
		tz->ops->get_trip_temp(tz, trip, &trip_temp);

	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		if (instance->trip != trip)
			continue;

		if (tz->temperature >= trip_temp)
			instance->target = 1;
		else
			instance->target = 0;
		instance->cdev->updated = false;
		thermal_cdev_update(instance->cdev);
	}

	return 0;
}

static struct thermal_governor thermal_gov_backward_compatible = {
	.name = "backward_compatible",
	.throttle = backward_compatible_throttle,
};

int thermal_gov_backward_compatible_register(void)
{
	return thermal_register_governor(&thermal_gov_backward_compatible);
}

void thermal_gov_backward_compatible_unregister(void)
{
	thermal_unregister_governor(&thermal_gov_backward_compatible);
}
