/*
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/regulator/pm8xxx-regulator.h>
#include <linux/regulator/msm-gpio-regulator.h>
#include <mach/rpm-regulator.h>

#include "board-9615.h"

#define VREG_CONSUMERS(_id) \
	static struct regulator_consumer_supply vreg_consumers_##_id[]

/*
 * Consumer specific regulator names:
 *			 regulator name		consumer dev_name
 */
VREG_CONSUMERS(L2) = {
	REGULATOR_SUPPLY("8018_l2",		NULL),
	REGULATOR_SUPPLY("HSUSB_1p8",		"msm_otg"),
};
VREG_CONSUMERS(L3) = {
	REGULATOR_SUPPLY("8018_l3",		NULL),
};
VREG_CONSUMERS(L4) = {
	REGULATOR_SUPPLY("8018_l4",		NULL),
	REGULATOR_SUPPLY("HSUSB_3p3",		"msm_otg"),
};
VREG_CONSUMERS(L5) = {
	REGULATOR_SUPPLY("8018_l5",		NULL),
};
VREG_CONSUMERS(L6) = {
	REGULATOR_SUPPLY("8018_l6",		NULL),
};
VREG_CONSUMERS(L7) = {
	REGULATOR_SUPPLY("8018_l7",		NULL),
};
VREG_CONSUMERS(L8) = {
	REGULATOR_SUPPLY("8018_l8",		NULL),
};
VREG_CONSUMERS(L9) = {
	REGULATOR_SUPPLY("8018_l9",		NULL),
};
VREG_CONSUMERS(L10) = {
	REGULATOR_SUPPLY("8018_l10",		NULL),
};
VREG_CONSUMERS(L11) = {
	REGULATOR_SUPPLY("8018_l11",		NULL),
};
VREG_CONSUMERS(L12) = {
	REGULATOR_SUPPLY("8018_l12",		NULL),
};
VREG_CONSUMERS(L13) = {
	REGULATOR_SUPPLY("8018_l13",		NULL),
	REGULATOR_SUPPLY("sdc_vdd_io",		"msm_sdcc.1"),
};
VREG_CONSUMERS(L14) = {
	REGULATOR_SUPPLY("8018_l14",		NULL),
	REGULATOR_SUPPLY("VDDI2",		"ebi2_lcd.0"),
};
VREG_CONSUMERS(S1) = {
	REGULATOR_SUPPLY("8018_s1",		NULL),
};
VREG_CONSUMERS(S2) = {
	REGULATOR_SUPPLY("8018_s2",		NULL),
	REGULATOR_SUPPLY("CDC_VDDA_A_1P2V",	"tabla-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_A_1P2V",	"tabla2x-slim"),
	REGULATOR_SUPPLY("VDDD_CDC_D",		"tabla-slim"),
	REGULATOR_SUPPLY("VDDD_CDC_D",		"tabla2x-slim"),
	REGULATOR_SUPPLY("VDDD_CDC_D",          "0-000d"),
	REGULATOR_SUPPLY("CDC_VDDA_A_1P2V",     "0-000d"),
	REGULATOR_SUPPLY("VDDD_CDC_D",		"tabla top level"),
	REGULATOR_SUPPLY("CDC_VDDA_A_1P2V",	"tabla top level"),
};
VREG_CONSUMERS(S3) = {
	REGULATOR_SUPPLY("8018_s3",		NULL),
	REGULATOR_SUPPLY("wlan_vreg",		"wlan_ar6000_pm_dev"),
	REGULATOR_SUPPLY("CDC_VDD_CP",		"tabla-slim"),
	REGULATOR_SUPPLY("CDC_VDD_CP",		"tabla2x-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_RX",		"tabla-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_RX",		"tabla2x-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_TX",		"tabla-slim"),
	REGULATOR_SUPPLY("CDC_VDDA_TX",		"tabla2x-slim"),
	REGULATOR_SUPPLY("VDDIO_CDC",		"tabla-slim"),
	REGULATOR_SUPPLY("VDDIO_CDC",		"tabla2x-slim"),
	REGULATOR_SUPPLY("VDDIO_CDC",		"tabla top level"),
	REGULATOR_SUPPLY("CDC_VDD_CP",		"tabla top level"),
	REGULATOR_SUPPLY("CDC_VDDA_TX",		"tabla top level"),
	REGULATOR_SUPPLY("CDC_VDDA_RX",		"tabla top level"),
	REGULATOR_SUPPLY("VDDIO_CDC",		"0-000d"),
	REGULATOR_SUPPLY("CDC_VDD_CP",		"0-000d"),
	REGULATOR_SUPPLY("CDC_VDDA_TX",		"0-000d"),
	REGULATOR_SUPPLY("CDC_VDDA_RX",		"0-000d"),
};
VREG_CONSUMERS(S4) = {
	REGULATOR_SUPPLY("8018_s4",		NULL),
};
VREG_CONSUMERS(S5) = {
	REGULATOR_SUPPLY("8018_s5",		NULL),
};
VREG_CONSUMERS(LVS1) = {
	REGULATOR_SUPPLY("8018_lvs1",		NULL),
};
VREG_CONSUMERS(EXT_2P95V) = {
	REGULATOR_SUPPLY("ext_2p95v",		NULL),
	REGULATOR_SUPPLY("sdc_vdd",		"msm_sdcc.1"),
};
VREG_CONSUMERS(VDD_DIG_CORNER) = {
	REGULATOR_SUPPLY("hsusb_vdd_dig",	"msm_otg"),
	REGULATOR_SUPPLY("hsic_vdd_dig",	"msm_hsic_peripheral"),
	REGULATOR_SUPPLY("hsic_vdd_dig",	"msm_hsic_host"),
};

#define PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, _modes, _ops, \
			 _apply_uV, _pull_down, _always_on, _supply_regulator, \
			 _system_uA, _enable_time, _reg_id) \
	{ \
		.init_data = { \
			.constraints = { \
				.valid_modes_mask	= _modes, \
				.valid_ops_mask		= _ops, \
				.min_uV			= _min_uV, \
				.max_uV			= _max_uV, \
				.input_uV		= _max_uV, \
				.apply_uV		= _apply_uV, \
				.always_on		= _always_on, \
				.name			= _name, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id), \
			.consumer_supplies	= vreg_consumers_##_id, \
			.supply_regulator	= _supply_regulator, \
		}, \
		.id			= _reg_id, \
		.pull_down_enable	= _pull_down, \
		.system_uA		= _system_uA, \
		.enable_time		= _enable_time, \
	}

#define PM8XXX_LDO(_id, _name, _always_on, _pull_down, _min_uV, _max_uV, \
		_enable_time, _supply_regulator, _system_uA, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		| REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE | \
		REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE | \
		REGULATOR_CHANGE_DRMS, 0, _pull_down, _always_on, \
		_supply_regulator, _system_uA, _enable_time, _reg_id)

#define PM8XXX_NLDO1200(_id, _name, _always_on, _pull_down, _min_uV, \
		_max_uV, _enable_time, _supply_regulator, _system_uA, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		| REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE | \
		REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE | \
		REGULATOR_CHANGE_DRMS, 0, _pull_down, _always_on, \
		_supply_regulator, _system_uA, _enable_time, _reg_id)

#define PM8XXX_SMPS(_id, _name, _always_on, _pull_down, _min_uV, _max_uV, \
		_enable_time, _supply_regulator, _system_uA, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		| REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE | \
		REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE | \
		REGULATOR_CHANGE_DRMS, 0, _pull_down, _always_on, \
		_supply_regulator, _system_uA, _enable_time, _reg_id)

#define PM8XXX_VS(_id, _name, _always_on, _pull_down, _enable_time, \
		_supply_regulator, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, 0, 0, 0, REGULATOR_CHANGE_STATUS, 0, \
		_pull_down, _always_on, _supply_regulator, 0, _enable_time, \
		_reg_id)

/* Pin control initialization */
#define PM8XXX_PC(_id, _name, _always_on, _pin_fn, _pin_ctrl, \
		  _supply_regulator, _reg_id) \
	{ \
		.init_data = { \
			.constraints = { \
				.valid_ops_mask	= REGULATOR_CHANGE_STATUS, \
				.always_on	= _always_on, \
				.name		= _name, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id##_PC), \
			.consumer_supplies	= vreg_consumers_##_id##_PC, \
			.supply_regulator  = _supply_regulator, \
		}, \
		.id		= _reg_id, \
		.pin_fn		= PM8XXX_VREG_PIN_FN_##_pin_fn, \
		.pin_ctrl	= _pin_ctrl, \
	}

#define RPM_INIT(_id, _min_uV, _max_uV, _modes, _ops, _apply_uV, _default_uV, \
		 _peak_uA, _avg_uA, _pull_down, _pin_ctrl, _freq, _pin_fn, \
		 _force_mode, _sleep_set_force_mode, _power_mode, _state, \
		 _sleep_selectable, _always_on, _supply_regulator, _system_uA) \
	{ \
		.init_data = { \
			.constraints = { \
				.valid_modes_mask	= _modes, \
				.valid_ops_mask		= _ops, \
				.min_uV			= _min_uV, \
				.max_uV			= _max_uV, \
				.input_uV		= _min_uV, \
				.apply_uV		= _apply_uV, \
				.always_on		= _always_on, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id), \
			.consumer_supplies	= vreg_consumers_##_id, \
			.supply_regulator	= _supply_regulator, \
		}, \
		.id			= RPM_VREG_ID_PM8018_##_id, \
		.default_uV		= _default_uV, \
		.peak_uA		= _peak_uA, \
		.avg_uA			= _avg_uA, \
		.pull_down_enable	= _pull_down, \
		.pin_ctrl		= _pin_ctrl, \
		.freq			= RPM_VREG_FREQ_##_freq, \
		.pin_fn			= _pin_fn, \
		.force_mode		= _force_mode, \
		.sleep_set_force_mode	= _sleep_set_force_mode, \
		.power_mode		= _power_mode, \
		.state			= _state, \
		.sleep_selectable	= _sleep_selectable, \
		.system_uA		= _system_uA, \
	}

#define RPM_LDO(_id, _always_on, _pd, _sleep_selectable, _min_uV, _max_uV, \
		_supply_regulator, _system_uA, _init_peak_uA) \
	RPM_INIT(_id, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		 | REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE \
		 | REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE \
		 | REGULATOR_CHANGE_DRMS, 0, _max_uV, _init_peak_uA, 0, _pd, \
		 RPM_VREG_PIN_CTRL_NONE, NONE, RPM_VREG_PIN_FN_9615_NONE, \
		 RPM_VREG_FORCE_MODE_9615_NONE, \
		 RPM_VREG_FORCE_MODE_9615_NONE, RPM_VREG_POWER_MODE_9615_PWM, \
		 RPM_VREG_STATE_OFF, _sleep_selectable, _always_on, \
		 _supply_regulator, _system_uA)

#define RPM_SMPS(_id, _always_on, _pd, _sleep_selectable, _min_uV, _max_uV, \
		 _supply_regulator, _system_uA, _freq) \
	RPM_INIT(_id, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		 | REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE \
		 | REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE \
		 | REGULATOR_CHANGE_DRMS, 0, _max_uV, _system_uA, 0, _pd, \
		 RPM_VREG_PIN_CTRL_NONE, _freq, RPM_VREG_PIN_FN_9615_NONE, \
		 RPM_VREG_FORCE_MODE_9615_NONE, \
		 RPM_VREG_FORCE_MODE_9615_NONE, RPM_VREG_POWER_MODE_9615_PWM, \
		 RPM_VREG_STATE_OFF, _sleep_selectable, _always_on, \
		 _supply_regulator, _system_uA)

#define RPM_VS(_id, _always_on, _pd, _sleep_selectable, _supply_regulator) \
	RPM_INIT(_id, 0, 0, 0, REGULATOR_CHANGE_STATUS, 0, 0, 1000, 1000, _pd, \
		 RPM_VREG_PIN_CTRL_NONE, NONE, RPM_VREG_PIN_FN_9615_NONE, \
		 RPM_VREG_FORCE_MODE_9615_NONE, \
		 RPM_VREG_FORCE_MODE_9615_NONE, RPM_VREG_POWER_MODE_9615_PWM, \
		 RPM_VREG_STATE_OFF, _sleep_selectable, _always_on, \
		 _supply_regulator, 0)

#define RPM_CORNER(_id, _always_on, _sleep_selectable, _min_uV, _max_uV, \
		_supply_regulator) \
	RPM_INIT(_id, _min_uV, _max_uV, 0, REGULATOR_CHANGE_VOLTAGE \
		 | REGULATOR_CHANGE_STATUS, 0, _max_uV, 0, 0, 0, \
		 RPM_VREG_PIN_CTRL_NONE, NONE, RPM_VREG_PIN_FN_9615_NONE, \
		 RPM_VREG_FORCE_MODE_9615_NONE, \
		 RPM_VREG_FORCE_MODE_9615_NONE, RPM_VREG_POWER_MODE_9615_PWM, \
		 RPM_VREG_STATE_OFF, _sleep_selectable, _always_on, \
		 _supply_regulator, 0)

/* Pin control initialization */
#define RPM_PC_INIT(_id, _always_on, _pin_fn, _pin_ctrl, _supply_regulator) \
	{ \
		.init_data = { \
			.constraints = { \
				.valid_ops_mask	= REGULATOR_CHANGE_STATUS, \
				.always_on	= _always_on, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id##_PC), \
			.consumer_supplies	= vreg_consumers_##_id##_PC, \
			.supply_regulator	= _supply_regulator, \
		}, \
		.id	  = RPM_VREG_ID_PM8018_##_id##_PC, \
		.pin_fn	  = RPM_VREG_PIN_FN_9615_##_pin_fn, \
		.pin_ctrl = _pin_ctrl, \
	}

#define GPIO_VREG_INIT(_id, _reg_name, _gpio_label, _gpio) \
	[GPIO_VREG_ID_##_id] = { \
		.init_data = { \
			.constraints = { \
				.valid_ops_mask	= REGULATOR_CHANGE_STATUS, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id), \
			.consumer_supplies	= vreg_consumers_##_id, \
		}, \
		.regulator_name = _reg_name, \
		.gpio_label	= _gpio_label, \
		.gpio		= _gpio, \
	}

/* GPIO regulator constraints */
struct gpio_regulator_platform_data msm_gpio_regulator_pdata[] = {
	GPIO_VREG_INIT(EXT_2P95V, "ext_2p95v", "ext_2p95_en", 18),
};

/* PM8018 regulator constraints */
struct pm8xxx_regulator_platform_data
msm_pm8018_regulator_pdata[] __devinitdata = {
};

static struct rpm_regulator_init_data
msm_rpm_regulator_init_data[] __devinitdata = {
	/*	 ID    a_on pd ss min_uV   max_uV  supply sys_uA  freq */
	RPM_SMPS(S1,     0, 1, 1,  500000, 1150000, NULL, 100000, 1p60),
	RPM_SMPS(S2,     0, 1, 0, 1225000, 1300000, NULL, 0,	  1p60),
	RPM_SMPS(S3,     1, 1, 0, 1800000, 1800000, NULL, 100000, 1p60),
	RPM_SMPS(S4,     0, 1, 0, 2100000, 2200000, NULL, 0,	  1p60),
	RPM_SMPS(S5,     1, 1, 0, 1350000, 1350000, NULL, 100000, 1p60),

	/*	 ID    a_on pd ss min_uV   max_uV  supply  sys_uA init_ip */
	RPM_LDO(L2,      1, 1, 0, 1800000, 1800000, NULL,      0, 10000),
	RPM_LDO(L3,      1, 1, 0, 1800000, 1800000, NULL,      0, 0),
	RPM_LDO(L4,      0, 1, 0, 3075000, 3075000, NULL,      0, 0),
	RPM_LDO(L5,      0, 1, 0, 2850000, 2850000, NULL,      0, 0),
	RPM_LDO(L6,      0, 1, 0, 1800000, 2850000, NULL,      0, 0),
	RPM_LDO(L7,      0, 1, 0, 1850000, 1900000, "8018_s4", 0, 0),
	RPM_LDO(L8,      0, 1, 0, 1200000, 1200000, "8018_s3", 0, 0),
	RPM_LDO(L9,      0, 1, 1,  750000, 1150000, "8018_s5", 10000, 10000),
	RPM_LDO(L10,     0, 1, 0, 1050000, 1050000, "8018_s5", 0, 0),
	RPM_LDO(L11,     0, 1, 0, 1050000, 1050000, "8018_s5", 0, 0),
	RPM_LDO(L12,     0, 1, 0, 1050000, 1050000, "8018_s5", 0, 0),
	RPM_LDO(L13,     0, 1, 0, 1850000, 2950000, NULL,      0, 0),
	RPM_LDO(L14,     0, 1, 0, 2850000, 2850000, NULL,      0, 0),

	/*	ID    a_on pd ss		    supply */
	RPM_VS(LVS1,    0, 1, 0,		    "8018_s3"),

	/*	   ID            a_on ss min_corner  max_corner  supply */
	RPM_CORNER(VDD_DIG_CORNER, 0, 1, RPM_VREG_CORNER_NONE,
		RPM_VREG_CORNER_HIGH, NULL),
};

int msm_pm8018_regulator_pdata_len __devinitdata =
	ARRAY_SIZE(msm_pm8018_regulator_pdata);

struct rpm_regulator_platform_data
msm_rpm_regulator_9615_pdata __devinitdata = {
	.init_data		= msm_rpm_regulator_init_data,
	.num_regulators		= ARRAY_SIZE(msm_rpm_regulator_init_data),
	.version		= RPM_VREG_VERSION_9615,
	.vreg_id_vdd_mem	= RPM_VREG_ID_PM8018_L9,
	.vreg_id_vdd_dig	= RPM_VREG_ID_PM8018_VDD_DIG_CORNER,
};
