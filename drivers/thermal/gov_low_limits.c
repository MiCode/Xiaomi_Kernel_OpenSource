/*
 *  Copyright (C) 2012 Intel Corp
 *  Copyright (C) 2012 Durgadoss R <durgadoss.r@intel.com>
 *  Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/thermal.h>
#include <trace/events/thermal.h>

#include "thermal_core.h"

static void thermal_zone_trip_update(struct thermal_zone_device *tz, int trip)
{
	int trip_temp, trip_hyst;
	enum thermal_trip_type trip_type;
	struct thermal_instance *instance;
	bool throttle;
	int old_target;

	tz->ops->get_trip_temp(tz, trip, &trip_temp);
	tz->ops->get_trip_type(tz, trip, &trip_type);
	if (tz->ops->get_trip_hyst) {
		tz->ops->get_trip_hyst(tz, trip, &trip_hyst);
		trip_hyst = trip_temp + trip_hyst;
	} else {
		trip_hyst = trip_temp;
	}

	mutex_lock(&tz->lock);

	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		if (instance->trip != trip)
			continue;

		if ((tz->temperature <= trip_temp) ||
			(instance->target != THERMAL_NO_TARGET
				&& tz->temperature < trip_hyst))
			throttle = true;
		else
			throttle = false;

		dev_dbg(&tz->device,
			"Trip%d[type=%d,temp=%d,hyst=%d],throttle=%d\n",
			trip, trip_type, trip_temp, trip_hyst, throttle);

		old_target = instance->target;
		instance->target = (throttle) ? instance->upper
					: THERMAL_NO_TARGET;
		dev_dbg(&instance->cdev->device, "old_target=%d, target=%d\n",
					old_target, (int)instance->target);

		if (old_target == instance->target)
			continue;

		if (old_target == THERMAL_NO_TARGET &&
				instance->target != THERMAL_NO_TARGET) {
			trace_thermal_zone_trip(tz, trip, trip_type, true);
			tz->passive += 1;
		} else if (old_target != THERMAL_NO_TARGET &&
				instance->target == THERMAL_NO_TARGET) {
			trace_thermal_zone_trip(tz, trip, trip_type, false);
			tz->passive -= 1;
		}

		instance->cdev->updated = false; /* cdev needs update */
	}

	mutex_unlock(&tz->lock);
}

/**
 * low_limits_throttle - throttles devices associated with the given zone
 * @tz - thermal_zone_device
 * @trip - the trip point
 *
 * Throttling Logic: If the sensor reading goes below a trip point, the
 * pre-defined mitigation will be applied for the cooling device.
 * If the sensor reading goes above the trip hysteresis, the
 * mitigation will be removed.
 */
static int low_limits_throttle(struct thermal_zone_device *tz, int trip)
{
	struct thermal_instance *instance;

	thermal_zone_trip_update(tz, trip);

	mutex_lock(&tz->lock);

	list_for_each_entry(instance, &tz->thermal_instances, tz_node)
		thermal_cdev_update(instance->cdev);

	mutex_unlock(&tz->lock);

	return 0;
}

static struct thermal_governor thermal_gov_low_limits_floor = {
	.name		= "low_limits_floor",
	.throttle	= low_limits_throttle,
	.min_state_throttle = 1,
};

static struct thermal_governor thermal_gov_low_limits_cap = {
	.name		= "low_limits_cap",
	.throttle	= low_limits_throttle,
};

int thermal_gov_low_limits_register(void)
{
	thermal_register_governor(&thermal_gov_low_limits_cap);
	return thermal_register_governor(&thermal_gov_low_limits_floor);
}

void thermal_gov_low_limits_unregister(void)
{
	thermal_unregister_governor(&thermal_gov_low_limits_cap);
	thermal_unregister_governor(&thermal_gov_low_limits_floor);
}
