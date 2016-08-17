/*
 * include/linux/regulator/tps6236x.h
 *
 * Interface for regulator driver for TI TPS6236x Processor core supply
 *
 * Copyright (C) 2011 NVIDIA Corporation
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
 * 51 Franklin Street, Fifth Floor, Boston, MA	02110-1301, USA.
 *
 */

#ifndef __REGULATOR_TPS6236X_H
#define __REGULATOR_TPS6236X_H

#include <linux/regulator/machine.h>

/*
 * struct tps6236x_regulator_platform_data - tps6236x regulator platform data.
 *
 * @reg_init_data: The regulator init data.
 * @internal_pd_enable: internal pull down enable or not.
 * @vsel: Select the voltage id register.
 * @init_uV: initial micro volts which need to be set.
 * @init_enable: Enable or do not enable the rails during initialization.
 * @init_apply: Init parameter applied or not.
 * @is_force_pwm: Enable force pwm or not. If not then PFM mode configuration
 *                will be used.
 * @enable_discharge: Enable discharge the output capacitor via a typ. 300Ohm
 *		      path
 */

struct tps6236x_regulator_platform_data {
	struct regulator_init_data reg_init_data;
	int internal_pd_enable;
	int vsel;
	int init_uV;
	unsigned init_apply:1;
	bool is_force_pwm;
	bool enable_discharge;
};

#endif /* __REGULATOR_TPS6236X_H */
