/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef ISL97900_LED_H
#define ISL97900_LED_H

#include <linux/of.h>

enum isl_function {
	ISL_LED_BRIGHTNESS_RGB_LEVEL,
	ISL_LED_BRIGHTNESS_RED_LEVEL,
	ISL_LED_BRIGHTNESS_GREEN_LEVEL,
	ISL_LED_BRIGHTNESS_BLUE_LEVEL,
	ISL_LED_BRIGHTNESS_EVENT_MAX,
};

#if IS_ENABLED(CONFIG_ISL97900_LED)
int isl97900_led_event(struct device_node *node,
			enum isl_function event,
			u32 level);

int isl97900_led_cali_data_update(struct device_node *node,
			u32 red_level,
			u32 green_level,
			u32 blue_level);
#else
int isl97900_led_event(struct device_node *node,
			enum isl_function event,
			u32 level)
{
	return 0;
}

int isl97900_led_cali_data_update(struct device_node *node,
			u32 red_level,
			u32 green_level,
			u32 blue_level)
{
	return 0;
}

#endif /* CONFIG_ISL97900_LED */

#endif
