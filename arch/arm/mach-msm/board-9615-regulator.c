/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/regulator/pm8018-regulator.h>

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
};
VREG_CONSUMERS(L14) = {
	REGULATOR_SUPPLY("8018_l14",		NULL),
};
VREG_CONSUMERS(S1) = {
	REGULATOR_SUPPLY("8018_s1",		NULL),
	REGULATOR_SUPPLY("HSUSB_VDDCX",		"msm_otg"),
};
VREG_CONSUMERS(S2) = {
	REGULATOR_SUPPLY("8018_s2",		NULL),
};
VREG_CONSUMERS(S3) = {
	REGULATOR_SUPPLY("8018_s3",		NULL),
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

#define PM8018_VREG_INIT(_id, _min_uV, _max_uV, _modes, _ops, _apply_uV, \
			 _pull_down, _always_on, _supply_regulator, \
			 _system_uA, _enable_time) \
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
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id), \
			.consumer_supplies	= vreg_consumers_##_id, \
			.supply_regulator	= _supply_regulator, \
		}, \
		.id			= PM8018_VREG_ID_##_id, \
		.pull_down_enable	= _pull_down, \
		.system_uA		= _system_uA, \
		.enable_time		= _enable_time, \
	}

#define PM8018_VREG_INIT_LDO(_id, _always_on, _pull_down, _min_uV, _max_uV, \
		_enable_time, _supply_regulator, _system_uA) \
	PM8018_VREG_INIT(_id, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		| REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE | \
		REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE | \
		REGULATOR_CHANGE_DRMS, 0, _pull_down, _always_on, \
		_supply_regulator, _system_uA, _enable_time)

#define PM8018_VREG_INIT_NLDO1200(_id, _always_on, _pull_down, _min_uV, \
		_max_uV, _enable_time, _supply_regulator, _system_uA) \
	PM8018_VREG_INIT(_id, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		| REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE | \
		REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE | \
		REGULATOR_CHANGE_DRMS, 0, _pull_down, _always_on, \
		_supply_regulator, _system_uA, _enable_time)

#define PM8018_VREG_INIT_SMPS(_id, _always_on, _pull_down, _min_uV, _max_uV, \
		_enable_time, _supply_regulator, _system_uA) \
	PM8018_VREG_INIT(_id, _min_uV, _max_uV, REGULATOR_MODE_NORMAL \
		| REGULATOR_MODE_IDLE, REGULATOR_CHANGE_VOLTAGE | \
		REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE | \
		REGULATOR_CHANGE_DRMS, 0, _pull_down, _always_on, \
		_supply_regulator, _system_uA, _enable_time)

#define PM8018_VREG_INIT_VS(_id, _always_on, _pull_down, _enable_time, \
		_supply_regulator) \
	PM8018_VREG_INIT(_id, 0, 0, 0, REGULATOR_CHANGE_STATUS, 0, _pull_down, \
		_always_on, _supply_regulator, 0, _enable_time)

/* Pin control initialization */
#define PM8018_PC_INIT(_id, _always_on, _pin_fn, _pin_ctrl, _supply_regulator) \
	{ \
		.init_data = { \
			.constraints = { \
				.valid_ops_mask	= REGULATOR_CHANGE_STATUS, \
				.always_on	= _always_on, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id##_PC), \
			.consumer_supplies	= vreg_consumers_##_id##_PC, \
			.supply_regulator  = _supply_regulator, \
		}, \
		.id	  = PM8018_VREG_ID_##_id##_PC, \
		.pin_fn	  = PM8018_VREG_PIN_FN_##_pin_fn, \
		.pin_ctrl = _pin_ctrl, \
	}

/* PM8018 regulator constraints */
struct pm8018_regulator_platform_data
msm_pm8018_regulator_pdata[] __devinitdata = {
	/*		      ID  a_on  pd min_uV   max_uV en_t supply sys_uA */
	PM8018_VREG_INIT_SMPS(S1,    1, 1, 1150000, 1150000, 500, NULL, 100000),
	PM8018_VREG_INIT_SMPS(S2,    0, 1, 1225000, 1300000, 500, NULL, 0),
	PM8018_VREG_INIT_SMPS(S3,    1, 1, 1800000, 1800000, 500, NULL, 100000),
	PM8018_VREG_INIT_SMPS(S4,    0, 1, 2100000, 2200000, 500, NULL, 0),
	PM8018_VREG_INIT_SMPS(S5,    1, 1, 1350000, 1350000, 500, NULL, 100000),

	PM8018_VREG_INIT_LDO(L2,     1, 1, 1800000, 1800000, 200, NULL, 10000),
	PM8018_VREG_INIT_LDO(L3,     0, 1, 1800000, 1800000, 200, NULL, 0),
	PM8018_VREG_INIT_LDO(L4,     0, 1, 3075000, 3075000, 200, NULL, 0),
	PM8018_VREG_INIT_LDO(L5,     0, 1, 2850000, 2850000, 200, NULL, 0),
	PM8018_VREG_INIT_LDO(L6,     0, 1, 1800000, 2850000, 200, NULL, 0),
	PM8018_VREG_INIT_LDO(L7,     0, 1, 1850000, 1900000, 200, "8018_s4", 0),
	PM8018_VREG_INIT_LDO(L8,     0, 1, 1200000, 1200000, 200, "8018_s3", 0),
	PM8018_VREG_INIT_LDO(L9,     1, 1, 1150000, 1150000, 200, "8018_s5",
			     10000),
	PM8018_VREG_INIT_LDO(L10,    0, 1, 1050000, 1050000, 200, "8018_s5", 0),
	PM8018_VREG_INIT_LDO(L11,    0, 1, 1050000, 1050000, 200, "8018_s5", 0),
	PM8018_VREG_INIT_LDO(L12,    0, 1, 1050000, 1050000, 200, "8018_s5", 0),
	PM8018_VREG_INIT_LDO(L13,    0, 1, 2950000, 2950000, 200, NULL, 0),
	PM8018_VREG_INIT_LDO(L14,    0, 1, 2850000, 2850000, 200, NULL, 0),

	/*                  ID    a_on  pd                  en_t  supply */
	PM8018_VREG_INIT_VS(LVS1,    0, 1,                     0, "8018_s3"),
};

int msm_pm8018_regulator_pdata_len __devinitdata =
	ARRAY_SIZE(msm_pm8018_regulator_pdata);
