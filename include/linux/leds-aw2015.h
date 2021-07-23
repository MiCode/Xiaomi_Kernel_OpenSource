/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINUX_AW2015_LED_H__
#define __LINUX_AW2015_LED_H__

/* The definition of each time described as shown in figure.
 *        /-----------\
 *       /      |      \
 *      /|      |      |\
 *     / |      |      | \-----------
 *       |hold_time_ms |      |
 *       |             |      |
 * rise_time_ms  fall_time_ms |
 *                       off_time_ms
 */

struct aw2015_platform_data {
	int imax;
	int currents[4]; 	/* ILEDx_y (x=1~3, y=1~4), 4 pre-defined currents */
	int delay_time_ms;
	int rise_time_ms;
	int hold_time_ms;
	int fall_time_ms;
	int slot_time_ms;
	int off_time_ms;
	struct aw2015_led *led;
	u8 cex;
	u8 mpulse;
};

#endif
