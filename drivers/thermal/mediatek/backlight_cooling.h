/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _BACKLIGHT_COOLING_H
#define _BACKLIGHT_COOLING_H

#include <linux/thermal.h>

/*===========================================================
 *  Macro Definitions
 *===========================================================
 */
#define BACKLIGHT_COOLING_UNLIMITED_STATE	(0)
#define BACKLIGHT_COOLING_MAX_STATE			(100)
#define MAX_BACKLIGHT_COOLER_NAME_LEN		(20)

/*==================================================
 * Type Definitions
 *==================================================
 */
/**
 * struct backlight_cooling_device - data for backlight cooling device
 * @name: naming string for this cooling device
 * @target_state: target cooling state which is set in set_cur_state()
 *	callback.
 * @max_state: maximum state supported for this cooling device
 * @cdev: thermal_cooling_device pointer to keep track of the
 *	registered cooling device.
 * @throttle: callback function to handle throttle request
 * @dev: device node pointer
 */
struct backlight_cooling_device {
	char name[MAX_BACKLIGHT_COOLER_NAME_LEN];
	unsigned long target_state;
	unsigned long max_state;
	struct thermal_cooling_device *cdev;
	int (*throttle)(struct backlight_cooling_device *bl_cdev, unsigned long state);
	struct device *dev;
};

#endif
