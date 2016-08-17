/*
 * board-pmu-defines.h: Most of macro definition used in board-xxx-power files.
 *
 * Copyright (c) 2013, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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

#ifndef _MACH_TEGRA_BOARD_PMU_DEFINES_H
#define _MACH_TEGRA_BOARD_PMU_DEFINES_H

#define REGULATOR_MODE_0	0
#define PALMAS_PDATA_INIT(_name, _minmv, _maxmv, _supply_reg,		\
	_always_on, _boot_on, _apply_uv, _init_mode)			\
	static struct regulator_init_data reg_idata_##_name = {		\
		.constraints = {					\
			.name = palmas_rails(_name),			\
			.min_uV = (_minmv)*1000,			\
			.max_uV = (_maxmv)*1000,			\
			.valid_modes_mask = (REGULATOR_MODE_NORMAL |	\
					REGULATOR_MODE_STANDBY),	\
			.valid_ops_mask = (REGULATOR_CHANGE_MODE |	\
					REGULATOR_CHANGE_STATUS |	\
					REGULATOR_CHANGE_VOLTAGE),	\
			.always_on = _always_on,			\
			.boot_on = _boot_on,				\
			.apply_uV = _apply_uv,				\
			.initial_mode = REGULATOR_MODE_##_init_mode,	\
		},							\
		.num_consumer_supplies =				\
			ARRAY_SIZE(palmas_##_name##_supply),		\
		.consumer_supplies = palmas_##_name##_supply,		\
		.supply_regulator = _supply_reg,			\
	}


#define PALMAS_PINMUX(_pin, _mux, _pud, _od)				\
{									\
	.pin_name = PALMAS_PIN_NAME_##_pin,				\
	.pin_mux_option = PALMAS_PINMUX_##_mux,			\
	.pin_pull_up_dn = PALMAS_PIN_CONFIG_##_pud,			\
	.open_drain_state = PALMAS_PIN_CONFIG_OD_##_od,			\
}

#define PALMAS_REGS_PDATA(_name, _minmv, _maxmv, _supply_reg,		\
	_always_on, _boot_on, _apply_uv, _init_mode,			\
	_warm_reset, _roof_floor, _mode_sleep, _tstep, _vsel)		\
	static struct regulator_init_data reg_idata_##_name = {		\
		.constraints = {					\
			.name = palmas_rails(_name),			\
			.min_uV = (_minmv)*1000,			\
			.max_uV = (_maxmv)*1000,			\
			.valid_modes_mask = (REGULATOR_MODE_NORMAL |	\
					REGULATOR_MODE_STANDBY),	\
			.valid_ops_mask = (REGULATOR_CHANGE_MODE |	\
					REGULATOR_CHANGE_STATUS |	\
					REGULATOR_CHANGE_VOLTAGE),	\
			.always_on = _always_on | (_roof_floor != 0),	\
			.boot_on = _boot_on | (_roof_floor != 0),	\
			.apply_uV = _apply_uv,				\
			.initial_mode = REGULATOR_MODE_##_init_mode,	\
		},							\
		.num_consumer_supplies =				\
			ARRAY_SIZE(palmas_##_name##_supply),		\
		.consumer_supplies = palmas_##_name##_supply,		\
		.supply_regulator = _supply_reg,			\
	};								\
	static struct palmas_reg_init reg_init_data_##_name = {		\
		.warm_reset = _warm_reset,				\
		.roof_floor = _roof_floor,				\
		.mode_sleep = _mode_sleep,				\
		.tstep = _tstep,					\
		.vsel = _vsel,						\
	}

#endif
