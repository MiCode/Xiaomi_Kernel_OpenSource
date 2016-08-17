/*
 * include/linux/platform_data/ina230.h
 *
 * Copyright (c) 2009-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _INA230_H
#define _INA230_H

#include <linux/types.h>

#define INA3221_RAIL_NAME_SIZE	32

#define POWER_LSB_TO_CURRENT_LSB_RATIO 25
struct ina230_platform_data {
	const char rail_name[INA3221_RAIL_NAME_SIZE];
	s32 current_threshold;
	s32 resistor;
	s32 min_cores_online;
	u32 calibration_data;
	u32 power_lsb;
	u16 divisor;
	u32 shunt_resistor;
	u32 precision_multiplier;
	int shunt_polarity_inverted; /* 0: not invert, 1: inverted */
};

#endif /* _INA230_H */
