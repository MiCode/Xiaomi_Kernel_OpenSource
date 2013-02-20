/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
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
#ifndef __KP_FLIP_SWITCH_H_
#define __KP_FLIP_SWITCH_H_
/* flip switch driver platform data */
struct flip_switch_pdata {
	int flip_gpio;
	int left_key;
	int right_key;
	int wakeup;
	int active_low;
	int (*flip_mpp_config) (void);
	char name[25];
};
#endif
