/*
 * dollar_cove_battery.h: platform data struct for dollar cove battery
 *
 * Copyright (C) 2013 Intel Corporation
 * Ramakrishna Pallala <ramakrishna.pallala@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __DOLLAR_COVE_BATTERY_H__
#define __DOLLAR_COVE_BATTERY_H__

#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/power_supply.h>
#include <linux/power/battery_id.h>

#define BAT_CURVE_SIZE		32
#define BATTID_LEN		8

struct dollarcove_fg_pdata {
	char battid[BATTID_LEN + 1];

	int design_cap;
	int design_min_volt;
	int design_max_volt;
	int max_temp;
	int min_temp;

	int cap1;
	int cap0;
	int rdc1;
	int rdc0;
	int bat_curve[BAT_CURVE_SIZE];
};

#endif	/* __DOLLAR_COVE_BATTERY_H__ */
