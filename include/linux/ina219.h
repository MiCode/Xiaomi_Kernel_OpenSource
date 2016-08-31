/*
 * linux/include/linux/ina219.h
 *
 * Copyright (c) 2011-2012, NVIDIA Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef _INA219_H
#define _INA219_H

#include <linux/types.h>

#define INA219_RAIL_NAME_SIZE	32

struct ina219_platform_data {
	u16 divisor; /*divisor needed to get current value */
	u32 calibration_data;
	u32 power_lsb;
	u32 precision_multiplier;
	u16 trig_conf;
	u16 cont_conf;
	u32 shunt_resistor; /*mOhms*/
	char rail_name[INA219_RAIL_NAME_SIZE];
};

#endif /* _LINUX_INA219_H */

