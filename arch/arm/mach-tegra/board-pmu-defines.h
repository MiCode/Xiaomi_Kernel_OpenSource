/*
 * board-pmu-defines.h: Most of macro definition used in board-xxx-power files.
 *
 * Copyright (c) 2013, NVIDIA Corporation. All rights reserved.
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
	.pin = _pin,							\
	.function = _mux,						\
	.prop_bias_pull = _pud,						\
	.prop_open_drain = _od,						\
}

#define PALMAS_REGS_PDATA(_name, _minmv, _maxmv, _supply_reg,		\
	_always_on, _boot_on, _apply_uv, _init_mode,			\
	_warm_reset, _roof_floor, _mode_sleep, _ramp_us, _vsel)		\
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
			.ramp_delay = _ramp_us,				\
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
		.enable_gpio = -EINVAL,					\
		.vsel = _vsel,						\
	}

#define AS3722_SUPPLY(_name) "as3722_"#_name
#define AMS_PDATA_INIT(_name, _supply_reg, _min_uV, _max_uV, _always_on,\
			 _boot_on, _apply_uV, _ext_cntrl)		\
static struct regulator_init_data as3722_##_name##_reg_idata =		\
{									\
	.supply_regulator = _supply_reg,				\
	.constraints = {						\
		.name = AS3722_SUPPLY(_name),				\
		.min_uV = _min_uV,					\
		.max_uV = _max_uV,					\
		.valid_modes_mask = (REGULATOR_MODE_NORMAL |		\
				     REGULATOR_MODE_STANDBY),		\
		.valid_ops_mask = (REGULATOR_CHANGE_MODE |		\
				   REGULATOR_CHANGE_STATUS |		\
				   REGULATOR_CHANGE_VOLTAGE),		\
		.always_on = _always_on,				\
		.boot_on = _boot_on,					\
		.apply_uV = _apply_uV,					\
	},								\
	.num_consumer_supplies =					\
		ARRAY_SIZE(as3722_##_name##_supply),			\
	.consumer_supplies = as3722_##_name##_supply,			\
};									\
static struct as3722_regulator_platform_data as3722_##_name##_reg_pdata =	\
{									\
	.reg_init = &as3722_##_name##_reg_idata,			\
	.ext_control = _ext_cntrl					\
}									\

#define AS3722_PIN_CONTROL(_pin, _func, _bias_pull, _od, _hi_imp, _gpio_mode)  \
	{							\
		.pin = _pin,					\
		.function = _func,				\
		.prop_bias_pull = _bias_pull,			\
		.prop_open_drain = _od,				\
		.prop_high_impedance = _hi_imp,			\
		.prop_gpio_mode = _gpio_mode,			\
	}

/* Macro for defining fixed regulator sub device data */
#define fixed_sync_supply(_name) "fixed_reg_en_"#_name

#define FIXED_REG_DRV(_id, _drv, _var, _name, _in_supply,		\
	_always_on, _boot_on, _gpio_nr, _open_drain,			\
	_active_high, _boot_state, _millivolts, _sdelay)		\
static struct regulator_init_data ri_data_##_var =			\
{									\
	.supply_regulator = _in_supply,					\
	.num_consumer_supplies =					\
			ARRAY_SIZE(fixed_reg_en_##_name##_supply),	\
	.consumer_supplies = fixed_reg_en_##_name##_supply,		\
	.constraints = {						\
		.valid_modes_mask = (REGULATOR_MODE_NORMAL |		\
					REGULATOR_MODE_STANDBY),	\
		.valid_ops_mask = (REGULATOR_CHANGE_MODE |		\
					REGULATOR_CHANGE_STATUS |	\
					REGULATOR_CHANGE_VOLTAGE),	\
		.always_on = _always_on,				\
		.boot_on = _boot_on,					\
	},								\
};									\
static struct fixed_voltage_config fixed_reg_en_##_var##_pdata =	\
{									\
	.supply_name = fixed_sync_supply(_name),			\
	.microvolts = _millivolts * 1000,				\
	.gpio = _gpio_nr,						\
	.gpio_is_open_drain = _open_drain,				\
	.enable_high = _active_high,					\
	.enabled_at_boot = _boot_state,					\
	.init_data = &ri_data_##_var,					\
	.startup_delay = _sdelay					\
};									\
static struct platform_device fixed_reg_en_##_var##_dev = {		\
	.name = #_drv,							\
	.id = _id,							\
	.dev = {							\
		.platform_data = &fixed_reg_en_##_var##_pdata,		\
	},								\
}

#define FIXED_SYNC_REG(_id, _var, _name, _in_supply,			\
	_always_on, _boot_on, _gpio_nr, _open_drain,			\
	_active_high, _boot_state, _millivolts, _sdelay)		\
	FIXED_REG_DRV(_id, reg-fixed-sync-voltage, _var, _name, 	\
	_in_supply, _always_on, _boot_on, _gpio_nr, _open_drain,	\
	_active_high, _boot_state, _millivolts, _sdelay)

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
/* Macro definition of dfll bypass device */
#define DFLL_BYPASS(_board, _min, _step, _size, _us_sel, _msel_gpio)	       \
static struct regulator_init_data _board##_dfll_bypass_init_data = {	       \
	.num_consumer_supplies = ARRAY_SIZE(_board##_dfll_bypass_consumers),   \
	.consumer_supplies = _board##_dfll_bypass_consumers,		       \
	.constraints = {						       \
		.valid_modes_mask = (REGULATOR_MODE_IDLE |		       \
				REGULATOR_MODE_NORMAL),			       \
		.valid_ops_mask = (REGULATOR_CHANGE_MODE |		       \
				REGULATOR_CHANGE_STATUS |		       \
				REGULATOR_CHANGE_VOLTAGE),		       \
		.min_uV = (_min),					       \
		.max_uV = ((_size) - 1) * (_step) + (_min),		       \
		.always_on = 1,						       \
		.boot_on = 1,						       \
	},								       \
};									       \
static struct tegra_dfll_bypass_platform_data _board##_dfll_bypass_pdata = {   \
	.reg_init_data = &_board##_dfll_bypass_init_data,		       \
	.uV_step = (_step),						       \
	.linear_min_sel = 0,						       \
	.n_voltages = (_size),						       \
	.voltage_time_sel = _us_sel,					       \
	.msel_gpio = _msel_gpio,					       \
};									       \
static struct platform_device _board##_dfll_bypass_dev = {		       \
	.name = "tegra_dfll_bypass",					       \
	.id = -1,							       \
	.dev = {							       \
		.platform_data = &_board##_dfll_bypass_pdata,		       \
	},								       \
}
#endif

#endif
