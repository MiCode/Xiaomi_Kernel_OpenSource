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

#include <linux/regulator/pm8xxx-regulator.h>

#include "board-8930.h"

#define VREG_CONSUMERS(_id) \
	static struct regulator_consumer_supply vreg_consumers_##_id[]

/*
 * Consumer specific regulator names:
 *			 regulator name		consumer dev_name
 */
VREG_CONSUMERS(L1) = {
	REGULATOR_SUPPLY("8038_l1",		NULL),
};
VREG_CONSUMERS(L2) = {
	REGULATOR_SUPPLY("8038_l2",		NULL),
};
VREG_CONSUMERS(L3) = {
	REGULATOR_SUPPLY("8038_l3",		NULL),
};
VREG_CONSUMERS(L4) = {
	REGULATOR_SUPPLY("8038_l4",		NULL),
};
VREG_CONSUMERS(L5) = {
	REGULATOR_SUPPLY("8038_l5",		NULL),
};
VREG_CONSUMERS(L6) = {
	REGULATOR_SUPPLY("8038_l6",		NULL),
};
VREG_CONSUMERS(L7) = {
	REGULATOR_SUPPLY("8038_l7",		NULL),
};
VREG_CONSUMERS(L8) = {
	REGULATOR_SUPPLY("8038_l8",		NULL),
};
VREG_CONSUMERS(L9) = {
	REGULATOR_SUPPLY("8038_l9",		NULL),
};
VREG_CONSUMERS(L10) = {
	REGULATOR_SUPPLY("8038_l10",		NULL),
};
VREG_CONSUMERS(L11) = {
	REGULATOR_SUPPLY("8038_l11",		NULL),
};
VREG_CONSUMERS(L12) = {
	REGULATOR_SUPPLY("8038_l12",		NULL),
};
VREG_CONSUMERS(L14) = {
	REGULATOR_SUPPLY("8038_l14",		NULL),
};
VREG_CONSUMERS(L15) = {
	REGULATOR_SUPPLY("8038_l15",		NULL),
};
VREG_CONSUMERS(L16) = {
	REGULATOR_SUPPLY("8038_l16",		NULL),
};
VREG_CONSUMERS(L17) = {
	REGULATOR_SUPPLY("8038_l17",		NULL),
};
VREG_CONSUMERS(L18) = {
	REGULATOR_SUPPLY("8038_l18",		NULL),
};
VREG_CONSUMERS(L19) = {
	REGULATOR_SUPPLY("8038_l19",		NULL),
};
VREG_CONSUMERS(L20) = {
	REGULATOR_SUPPLY("8038_l20",		NULL),
};
VREG_CONSUMERS(L21) = {
	REGULATOR_SUPPLY("8038_l21",		NULL),
};
VREG_CONSUMERS(L22) = {
	REGULATOR_SUPPLY("8038_l22",		NULL),
};
VREG_CONSUMERS(L23) = {
	REGULATOR_SUPPLY("8038_l23",		NULL),
};
VREG_CONSUMERS(L24) = {
	REGULATOR_SUPPLY("8038_l24",		NULL),
};
VREG_CONSUMERS(L26) = {
	REGULATOR_SUPPLY("8038_l26",		NULL),
};
VREG_CONSUMERS(L27) = {
	REGULATOR_SUPPLY("8038_l27",		NULL),
};
VREG_CONSUMERS(S1) = {
	REGULATOR_SUPPLY("8038_s1",		NULL),
};
VREG_CONSUMERS(S2) = {
	REGULATOR_SUPPLY("8038_s2",		NULL),
};
VREG_CONSUMERS(S3) = {
	REGULATOR_SUPPLY("8038_s3",		NULL),
};
VREG_CONSUMERS(S4) = {
	REGULATOR_SUPPLY("8038_s4",		NULL),
};
VREG_CONSUMERS(S5) = {
	REGULATOR_SUPPLY("8038_s5",		NULL),
};
VREG_CONSUMERS(S6) = {
	REGULATOR_SUPPLY("8038_s6",		NULL),
};
VREG_CONSUMERS(LVS1) = {
	REGULATOR_SUPPLY("8038_lvs1",		NULL),
};
VREG_CONSUMERS(LVS2) = {
	REGULATOR_SUPPLY("8038_lvs2",		NULL),
};
VREG_CONSUMERS(EXT_5V) = {
	REGULATOR_SUPPLY("ext_5v",		NULL),
};
VREG_CONSUMERS(EXT_OTG_SW) = {
	REGULATOR_SUPPLY("ext_otg_sw",		NULL),
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

#define PM8XXX_FTSMPS(_id, _name, _always_on, _pull_down, _min_uV, _max_uV, \
		_enable_time, _supply_regulator, _system_uA, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, REGULATOR_MODE_NORMAL, \
		REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS \
		| REGULATOR_CHANGE_MODE, 0, _pull_down, _always_on, \
		_supply_regulator, _system_uA, _enable_time, _reg_id)

#define PM8XXX_VS(_id, _name, _always_on, _pull_down, _enable_time, \
		_supply_regulator, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, 0, 0, 0, REGULATOR_CHANGE_STATUS, 0, \
		_pull_down, _always_on, _supply_regulator, 0, _enable_time, \
		_reg_id)

#define PM8XXX_VS300(_id, _name, _always_on, _pull_down, _enable_time, \
		_supply_regulator, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, 0, 0, 0, REGULATOR_CHANGE_STATUS, 0, \
		_pull_down, _always_on, _supply_regulator, 0, _enable_time, \
		_reg_id)

#define PM8XXX_NCP(_id, _name, _always_on, _min_uV, _max_uV, _enable_time, \
		_supply_regulator, _reg_id) \
	PM8XXX_VREG_INIT(_id, _name, _min_uV, _max_uV, 0, \
		REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS, 0, 0, \
		_always_on, _supply_regulator, 0, _enable_time, _reg_id)

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

#define GPIO_VREG(_id, _reg_name, _gpio_label, _gpio, _supply_regulator) \
	[MSM8930_GPIO_VREG_ID_##_id] = { \
		.init_data = { \
			.constraints = { \
				.valid_ops_mask	= REGULATOR_CHANGE_STATUS, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id), \
			.consumer_supplies	= vreg_consumers_##_id, \
			.supply_regulator	= _supply_regulator, \
		}, \
		.regulator_name = _reg_name, \
		.gpio_label	= _gpio_label, \
		.gpio		= _gpio, \
	}

/* GPIO regulator constraints */
struct gpio_regulator_platform_data
msm8930_gpio_regulator_pdata[] __devinitdata = {
	/*        ID          vreg_name     gpio_label     gpio  supply */
	GPIO_VREG(EXT_5V,     "ext_5v",     "ext_5v_en",     63, NULL),
	GPIO_VREG(EXT_OTG_SW, "ext_otg_sw", "ext_otg_sw_en", 97, "ext_5v"),
};

/* PM8038 regulator constraints */
struct pm8xxx_regulator_platform_data
msm8930_pm8038_regulator_pdata[] __devinitdata = {
	/*
	 *	    ID  name always_on pd min_uV   max_uV   en_t supply
	 *	system_uA reg_ID
	 */
	PM8XXX_SMPS(S1, "8038_s1", 1, 1,  1150000, 1150000, 500, NULL, 100000,
		26),
	PM8XXX_SMPS(S2, "8038_s2", 1, 1,  1400000, 1400000, 500, NULL, 100000,
		27),
	PM8XXX_SMPS(S3, "8038_s3", 0, 1,  1150000, 1150000, 500, NULL, 0, 28),
	PM8XXX_SMPS(S4, "8038_s4", 1, 1,  2200000, 2200000, 500, NULL, 100000,
		29),

	PM8XXX_FTSMPS(S5, "8038_s5", 0, 1, 950000, 1150000, 500, NULL, 0, 30),
	PM8XXX_FTSMPS(S6, "8038_s6", 0, 1, 950000, 1150000, 500, NULL, 0, 31),

	PM8XXX_NLDO1200(L1, "8038_l1",  0, 1, 1300000, 1300000, 200, "8038_s2",
		0, 1),
	PM8XXX_LDO(L2,  "8038_l2",  0, 1, 1200000, 1200000, 200, "8038_s2", 0,
		2),
	PM8XXX_LDO(L3,  "8038_l3",  0, 1, 3075000, 3075000, 200, NULL, 0, 3),
	PM8XXX_LDO(L4,  "8038_l4",  1, 1, 1800000, 1800000, 200, NULL, 10000,
		4),
	PM8XXX_LDO(L5,  "8038_l5",  0, 1, 2950000, 2950000, 200, NULL, 0, 5),
	PM8XXX_LDO(L6,  "8038_l6",  0, 1, 2950000, 2950000, 200, NULL, 0, 6),
	PM8XXX_LDO(L7,  "8038_l7",  0, 1, 2050000, 2050000, 200, "8038_s4", 0,
		7),
	PM8XXX_LDO(L8,  "8038_l8",  0, 1, 2800000, 2800000, 200, NULL, 0, 8),
	PM8XXX_LDO(L9,  "8038_l9",  0, 1, 2850000, 2850000, 200, NULL, 0, 9),
	PM8XXX_LDO(L10, "8038_l10", 0, 1, 2900000, 2900000, 200, NULL, 0, 10),
	PM8XXX_LDO(L11, "8038_l11", 1, 1, 1800000, 1800000, 200, "8038_s4",
		10000, 11),
	PM8XXX_LDO(L12, "8038_l12", 0, 1, 1200000, 1200000, 200, "8038_s2", 0,
		12),
	PM8XXX_LDO(L14, "8038_l14", 0, 1, 1800000, 1800000, 200, NULL, 0, 13),
	PM8XXX_LDO(L15, "8038_l15", 0, 1, 1800000, 2950000, 200, NULL, 0, 14),
	PM8XXX_NLDO1200(L16, "8038_l16", 0, 1, 1050000, 1050000, 200, "8038_s3",
		0, 15),
	PM8XXX_LDO(L17, "8038_l17", 0, 1, 1800000, 2950000, 200, NULL, 0, 16),
	PM8XXX_LDO(L18, "8038_l18", 0, 1, 1800000, 1800000, 200, NULL, 0, 17),
	PM8XXX_NLDO1200(L19, "8038_l19", 0, 1, 1050000, 1050000, 200, "8038_s3",
		0, 18),
	PM8XXX_NLDO1200(L20, "8038_l20", 1, 1, 1200000, 1200000, 200, "8038_s2",
		10000, 19),
	PM8XXX_LDO(L21, "8038_l21", 0, 1, 1900000, 1900000, 200, "8038_s4", 0,
		20),
	PM8XXX_LDO(L22, "8038_l22", 1, 1, 2950000, 2950000, 200, NULL, 10000,
		21),
	PM8XXX_LDO(L23, "8038_l23", 0, 1, 1800000, 1800000, 200, "8038_s4", 0,
		22),
	PM8XXX_NLDO1200(L24, "8038_l24", 1, 1, 1150000, 1150000, 200, "8038_s2",
		10000, 23),
	PM8XXX_LDO(L26, "8038_l26", 1, 1, 1050000, 1050000, 200, "8038_s2",
		10000, 24),
	PM8XXX_NLDO1200(L27, "8038_l27", 0, 1, 1050000, 1050000, 200, "8038_s3",
		0, 25),

	/*	  ID	name  always_on pd		   en_t supply reg_ID */
	PM8XXX_VS(LVS1, "8038_lvs1", 0, 1,		     0, "8038_l11", 32),
	PM8XXX_VS(LVS2, "8038_lvs2", 0, 1,		     0, "8038_l11", 33),

};

int msm8930_pm8038_regulator_pdata_len __devinitdata =
	ARRAY_SIZE(msm8930_pm8038_regulator_pdata);
