/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __QTI_THERMAL_ZONE_INTERNAL_H
#define __QTI_THERMAL_ZONE_INTERNAL_H

#include <linux/thermal.h>
#include "../thermal_core.h"

/* Generic helpers for thermal zone -> change_mode ops */
static inline __maybe_unused int qti_tz_change_mode(struct thermal_zone_device *tz,
		enum thermal_device_mode mode)
{
	struct thermal_instance *instance;

	if (!tz)
		return 0;

	tz->passive = 0;
	tz->temperature = THERMAL_TEMP_INVALID;
	tz->prev_low_trip = -INT_MAX;
	tz->prev_high_trip = INT_MAX;
	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		instance->initialized = false;
		if (mode == THERMAL_DEVICE_DISABLED) {
			instance->target = THERMAL_NO_TARGET;
			instance->cdev->updated = false;
			thermal_cdev_update(instance->cdev);
		}
	}

	return 0;
}

static inline __maybe_unused int qti_update_tz_ops(struct thermal_zone_device *tz, bool enable)
{
	if (!tz || !tz->ops)
		return -EINVAL;

	mutex_lock(&tz->lock);
	if (enable) {
		if (!tz->ops->change_mode)
			tz->ops->change_mode = qti_tz_change_mode;
	} else {
		tz->ops->change_mode = NULL;
	}
	mutex_unlock(&tz->lock);
	return 0;
}

/* Generic helpers for thermal zone -> get_trend ops */
static __maybe_unused inline int qti_tz_get_trend(
				struct thermal_zone_device *tz,
				const struct thermal_trip *trip,
				enum thermal_trend *trend)
{
	int trip_temp = 0, trip_hyst = 0, temp;
	struct thermal_instance *instance;
	bool monitor_trip_only = false;

	if (!tz || !trip)
		return -EINVAL;

	trip_temp = READ_ONCE(trip->temperature);
	trip_hyst = READ_ONCE(trip->hysteresis);
	temp = READ_ONCE(tz->temperature);

	/*
	 * Handle only monitor trip clear condition, fallback to default
	 * trend estimation for all other cases.
	 * If all the instances of a given trip are monitor type(upper == lower),
	 * then only treat this trip as monitor trip and consider hysterisis for
	 * clear condition
	 */
	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		if (trip != instance->trip)
			continue;

		if (instance->lower != instance->upper)
			return -EINVAL;

		monitor_trip_only = true;
	}

	if (monitor_trip_only && temp < trip_temp &&
			(temp > (trip_temp - trip_hyst))) {
		*trend = THERMAL_TREND_STABLE;
		return 0;
	}

	return -EINVAL;
}
#endif  // __QTI_THERMAL_ZONE_INTERNAL_H
