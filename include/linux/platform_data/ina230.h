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

#define MEASURE_BUS_VOLT 1

/*
Config register for ina230 (/ ina226):
Some of these values may be needed to calculate platform_data values
D15|D14 D13 D12|D11 D10 D09|D08 D07 D06|D05 D04 D03|D02 D01 D00
rst|-   -   -  |AVG        |Vbus_CT    |Vsh_CT     |MODE
*/
#define INA230_RESET		(1 << 15)
#define INA230_VBUS_CT		(0 << 6) /* Vbus 140us conversion time */
#define INA230_VSH_CT		(0 << 3) /* Vshunt 140us conversion time */

#if MEASURE_BUS_VOLT
#define INA230_CONT_MODE	7	/* Continuous Bus and shunt measure */
#define INA230_TRIG_MODE	3	/* Triggered Bus and shunt measure */
#else
#define INA230_CONT_MODE	5	/* Continuous Shunt measurement */
#define INA230_TRIG_MODE	1	/* Triggered Shunt measurement */
#endif

#define INA230_POWER_DOWN	0

#define INA3221_RAIL_NAME_SIZE	32

struct ina230_platform_data {
	const char rail_name[INA3221_RAIL_NAME_SIZE];
	s32 current_threshold;
	s32 resistor;
	s32 min_cores_online;
	u32 calibration_data;
	u32 power_lsb;
	u16 trig_conf;
	u16 cont_conf;
	u16 divisor;
	u32 shunt_resistor;
	u32 precision_multiplier;
	int shunt_polarity_inverted; /* 0: not invert, 1: inverted */
	int alert_latch_enable;
};

#endif /* _INA230_H */
