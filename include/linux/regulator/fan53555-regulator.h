/*
 * include/linux/regulator/fan53555.h
 *
 * Interface for regulator driver for FairChild fan53555 Processor power supply
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

#ifndef __REGULATOR_FAN53555_H
#define __REGULATOR_FAN53555_H

#include <linux/regulator/machine.h>

/*
 * struct fan53555_regulator_platform_data - fan53555 regulator platform data.
 * When VSEL pin is low, VOUT is set by the VSEL0 register.
 * When VSEL pin is high, VOUT is set by the VSEL1 register
 *
 * @reg_init_data: The regulator init data.
 * @vsel_id: Select the voltage id register.
 * @vsel0_buck_en: Software Buck enable when EN=high & VSEL=low
 * @vsel0_mode: Force PWM mode(1) or Allow auto-PFM mode(0) during light load
 * @init_vsel0_min_uV: initial micro volts when EN=high & VSEL=low
 * @init_vsel0_max_uV: initial micro volts when EN=high & VSEL=low
 * @vsel1_buck_en: Software Buck enable when EN=high & VSEL=high
 * @vsel1_mode: Force PWM mode(1) or Allow auto-PFM mode(0) during light load
 * @init_vsel1_min_uV: initial micro volts when EN=high & VSEL=high
 * @init_vsel1_max_uV: initial micro volts when EN=high & VSEL=high
 * @output_discharge: when the regulator is disabled, VOUT discharges
 * @slew_rate: slew rate of trasitioning from a low to high voltage
 */

struct fan53555_regulator_platform_data {
	struct regulator_init_data reg_init_data;
	int vsel_id;
	unsigned vsel0_buck_en:1;
	unsigned vsel0_mode:1;
	int init_vsel0_min_uV;
	int init_vsel0_max_uV;
	unsigned vsel1_buck_en:1;
	unsigned vsel1_mode:1;
	int init_vsel1_min_uV;
	int init_vsel1_max_uV;
	unsigned output_discharge:1;
	unsigned slew_rate:3;
};

#endif	/* __REGULATOR_FAN53555_H */
