/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __QTI_THERMAL_ZONE_INTERNAL_H
#define __QTI_THERMAL_ZONE_INTERNAL_H

#include <linux/thermal.h>
#include <trace/hooks/thermal.h>
#include "../thermal_core.h"

static void disable_cdev_stats(void *unused,
		struct thermal_cooling_device *cdev, bool *disable)
{
	*disable = true;
}

/* Generic thermal vendor hooks initialization API */
static inline __maybe_unused void thermal_vendor_hooks_init(void)
{
	int ret;

	ret = register_trace_android_vh_disable_thermal_cooling_stats(
			disable_cdev_stats, NULL);
	if (ret) {
		pr_err("Failed to register disable thermal cdev stats hooks\n");
		return;
	}
}

static inline __maybe_unused void thermal_vendor_hooks_exit(void)
{
	unregister_trace_android_vh_disable_thermal_cooling_stats(
			disable_cdev_stats, NULL);
}

/* Generic helpers for thermal zone -> get_trend ops */
static __maybe_unused inline int qti_tz_get_trend(
				struct thermal_zone_device *tz, int trip,
				enum thermal_trend *trend)
{
	int trip_temp = 0, trip_hyst = 0, temp, ret;
	enum thermal_trip_type type = -1;

	if (!tz)
		return -EINVAL;

	ret = tz->ops->get_trip_temp(tz, trip, &trip_temp);
	if (ret)
		return ret;

	ret = tz->ops->get_trip_type(tz, trip, &type);
	if (ret)
		return ret;

	if (tz->ops->get_trip_hyst) {
		ret = tz->ops->get_trip_hyst(tz, trip, &trip_hyst);
		if (ret)
			return ret;
	}
	temp = READ_ONCE(tz->temperature);

	/*
	 * Handle only monitor trip clear condition, fallback to default
	 * trend estimation for all other cases.
	 */
	if ((type == THERMAL_TRIP_ACTIVE) && trip_hyst && (temp < trip_temp)) {
		if (temp > (trip_temp - trip_hyst)) {
			*trend = THERMAL_TREND_STABLE;
			return 0;
		}
	}

	return -EINVAL;
}

#endif  // __QTI_THERMAL_ZONE_INTERNAL_H
