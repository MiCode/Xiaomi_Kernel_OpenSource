/*
 * max15569-regulator.c -- max15569 regulator driver
 *
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

#ifndef __LINUX_REGULATOR_MAX15569_H
#define __LINUX_REGULATOR_MAX15569_H

/*
 * struct max15569_regulator_platform_data - max15569 regulator platform data.
 *
 * @reg_init_data: The regulator init data.
 * @max_voltage_uV: Maximum possible voltage for alarm.
 * @base_voltage_uV: Base voltage
 * slew_rate_uv_per_us: Slew rate uV/us. The values can be
 *		4 to 44 mV/us.
 */
struct max15569_regulator_platform_data {
	struct regulator_init_data *reg_init_data;
	bool enable_overcurrent_alarm;
	int max_voltage_uV;
	int base_voltage_uV;
	unsigned int slew_rate_mv_per_us;
};

#endif /* __LINUX_REGULATOR_MAX15569_H */
