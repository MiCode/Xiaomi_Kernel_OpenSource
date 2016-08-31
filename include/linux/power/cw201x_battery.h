/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __POWER_CW201x_BATTERY_H_
#define __POWER_CW201x_BATTERY_H_

#define CW201x_PROFILE_SIZE 64
#define CW201x_MAX_REGS 0x4F

struct cw201x_platform_data {
	const char *tz_name;
	uint8_t alert_threshold;
	uint8_t battery_profile_tbl[CW201x_PROFILE_SIZE];
};
#endif
