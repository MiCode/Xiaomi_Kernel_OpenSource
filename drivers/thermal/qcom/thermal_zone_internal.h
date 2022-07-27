/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __QTI_THERMAL_ZONE_INTERNAL_H
#define __QTI_THERMAL_ZONE_INTERNAL_H

#include <linux/thermal.h>
#include <trace/hooks/thermal.h>
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

static void disable_cdev_stats(void *unused,
		struct thermal_cooling_device *cdev, int *disable)
{
	*disable = 1;
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

#endif  // __QTI_THERMAL_ZONE_INTERNAL_H
