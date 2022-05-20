/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _WIFI_COOLING_H
#define _WIFI_COOLING_H

#include <linux/thermal.h>

/*===========================================================
 *  Macro Definitions
 *===========================================================
 */
#define WIFI_COOLING_UNLIMITED_STATE	(0)
#define MAX_WIFI_COOLER_NAME_LEN		(20)
/*connsys has 5 thermal throttle levels */
#define WIFI_COOLING_MAX_STATE			(5)
/*==================================================
 * Type Definitions
 *==================================================
 */

/**
 * struct wifi_cooling_device - data for wifi cooling device
 * @name: naming string for this cooling device
 * @target_state: target cooling state which is set in set_cur_state()
 *	callback.
 * @max_state: maximum state supported for this cooling device
 * @cdev: thermal_cooling_device pointer to keep track of the
 *	registered cooling device.
 * @throttle: callback function to handle throttle request
 * @dev: device node pointer
 */
struct wifi_cooling_device {
	char name[MAX_WIFI_COOLER_NAME_LEN];
	unsigned long target_state;
	unsigned long max_state;
	struct thermal_cooling_device *cdev;
	struct device *dev;
	const struct wifi_cooling_platform_data *throttle;
};


/**
 * struct wifi_cooling_platform_data - wifi cooler platform dependent data
 * @state_to_wifi_limit: callback function set cooling state Level
 */
struct wifi_cooling_platform_data {
	int (*state_to_wifi_limit)(struct wifi_cooling_device *wifi_cdev);
};
#endif
