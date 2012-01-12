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

#include "board-msm7x27a-regulator.h"

#define VOLTAGE_RANGE(min_uV, max_uV, step_uV)	((max_uV - min_uV) / step_uV)

/* Physically available PMIC regulator voltage setpoint ranges */
#define p_ranges VOLTAGE_RANGE(1500000, 3300000, 25000)

#define n_ranges VOLTAGE_RANGE(750000, 1525000, 12500)

#define s_ranges (VOLTAGE_RANGE(700000, 1500000, 12500) + \
			VOLTAGE_RANGE(1500000, 3050000, 25000))

#define PCOM_VREG_CONSUMERS(name) \
	static struct regulator_consumer_supply __pcom_vreg_supply_##name[]

#define PCOM_VREG_INIT_DATA(_name, _supply, _min_uV, _max_uV, _always_on, \
		_boot_on, _apply_uV, _supply_uV)\
{ \
	.supply_regulator = _supply, \
	.consumer_supplies = __pcom_vreg_supply_##_name, \
	.num_consumer_supplies = ARRAY_SIZE(__pcom_vreg_supply_##_name), \
	.constraints = { \
		.name = #_name, \
		.min_uV = _min_uV, \
		.max_uV = _max_uV, \
		.valid_modes_mask = REGULATOR_MODE_NORMAL, \
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | \
				  REGULATOR_CHANGE_STATUS, \
		.input_uV = _supply_uV, \
		.apply_uV = _apply_uV, \
		.boot_on = _boot_on, \
		.always_on = _always_on \
	} \
}

#define PCOM_VREG_SMP(_name, _id, _supply, _min_uV, _max_uV, _rise_time, \
	_pulldown, _always_on, _boot_on, _apply_uV, _supply_uV, _range) \
{ \
	.init_data = PCOM_VREG_INIT_DATA(_name, _supply, _min_uV, _max_uV, \
			_always_on, _boot_on, _apply_uV, _supply_uV), \
	.id = _id, \
	.rise_time = _rise_time, \
	.pulldown = _pulldown, \
	.negative = 0, \
	.n_voltages = _range##_ranges, \
}

#define PCOM_VREG_LDO PCOM_VREG_SMP

#define PCOM_VREG_NCP(_name, _id, _supply, _min_uV, _max_uV, _rise_time, \
		_always_on, _boot_on, _apply_uV, _supply_uV) \
{ \
	.init_data = PCOM_VREG_INIT_DATA(_name, _supply, -(_min_uV), \
		-(_max_uV), _always_on, _boot_on, _apply_uV, _supply_uV), \
	.id = _id, \
	.rise_time = _rise_time, \
	.pulldown = -1, \
	.negative = 1, \
}

PCOM_VREG_CONSUMERS(smps1) = {
	REGULATOR_SUPPLY("smps1",	NULL),
	REGULATOR_SUPPLY("msmc1",	NULL),
};

PCOM_VREG_CONSUMERS(smps2) = {
	REGULATOR_SUPPLY("smps2",	NULL),
	REGULATOR_SUPPLY("msmc2",	NULL),
};

PCOM_VREG_CONSUMERS(smps3) = {
	REGULATOR_SUPPLY("smps3",	NULL),
	REGULATOR_SUPPLY("msme1",	NULL),
	REGULATOR_SUPPLY("vcc_i2c",	"1-004a"),
	REGULATOR_SUPPLY("vcc_i2c",	"1-0038"),
};

PCOM_VREG_CONSUMERS(smps4) = {
	REGULATOR_SUPPLY("smps4",	NULL),
	REGULATOR_SUPPLY("rf",		NULL),
};

PCOM_VREG_CONSUMERS(ldo01) = {
	REGULATOR_SUPPLY("ldo01",	NULL),
	REGULATOR_SUPPLY("ldo1",	NULL),
	REGULATOR_SUPPLY("rfrx1",	NULL),
};

PCOM_VREG_CONSUMERS(ldo02) = {
	REGULATOR_SUPPLY("ldo02",	NULL),
	REGULATOR_SUPPLY("ldo2",	NULL),
	REGULATOR_SUPPLY("rfrx2",	NULL),
};

PCOM_VREG_CONSUMERS(ldo03) = {
	REGULATOR_SUPPLY("ldo03",	NULL),
	REGULATOR_SUPPLY("ldo3",	NULL),
	REGULATOR_SUPPLY("mddi",	NULL),
};

PCOM_VREG_CONSUMERS(ldo04) = {
	REGULATOR_SUPPLY("ldo04",	NULL),
	REGULATOR_SUPPLY("ldo4",	NULL),
	REGULATOR_SUPPLY("pllx",	NULL),
};

PCOM_VREG_CONSUMERS(ldo05) = {
	REGULATOR_SUPPLY("ldo05",	NULL),
	REGULATOR_SUPPLY("ldo5",	NULL),
	REGULATOR_SUPPLY("wlan2",	NULL),
};

PCOM_VREG_CONSUMERS(ldo06) = {
	REGULATOR_SUPPLY("ldo06",	NULL),
	REGULATOR_SUPPLY("ldo6",	NULL),
	REGULATOR_SUPPLY("wlan3",	NULL),
};

PCOM_VREG_CONSUMERS(ldo07) = {
	REGULATOR_SUPPLY("ldo07",	NULL),
	REGULATOR_SUPPLY("ldo7",	NULL),
	REGULATOR_SUPPLY("msma",	NULL),
};

PCOM_VREG_CONSUMERS(ldo08) = {
	REGULATOR_SUPPLY("ldo08",	NULL),
	REGULATOR_SUPPLY("ldo8",	NULL),
	REGULATOR_SUPPLY("tcxo",	NULL),
};

PCOM_VREG_CONSUMERS(ldo09) = {
	REGULATOR_SUPPLY("ldo09",	NULL),
	REGULATOR_SUPPLY("ldo9",	NULL),
	REGULATOR_SUPPLY("usb2",	NULL),
};

PCOM_VREG_CONSUMERS(ldo10) = {
	REGULATOR_SUPPLY("ldo10",	NULL),
	REGULATOR_SUPPLY("emmc",	NULL),
};

PCOM_VREG_CONSUMERS(ldo11) = {
	REGULATOR_SUPPLY("ldo11",	NULL),
	REGULATOR_SUPPLY("wlan_tcx0",	NULL),
};

PCOM_VREG_CONSUMERS(ldo12) = {
	REGULATOR_SUPPLY("ldo12",	NULL),
	REGULATOR_SUPPLY("gp2",		NULL),
	REGULATOR_SUPPLY("vdd_ana",	"1-004a"),
	REGULATOR_SUPPLY("vdd",		"1-0038"),
};

PCOM_VREG_CONSUMERS(ldo13) = {
	REGULATOR_SUPPLY("ldo13",	NULL),
	REGULATOR_SUPPLY("mmc",		NULL),
};

PCOM_VREG_CONSUMERS(ldo14) = {
	REGULATOR_SUPPLY("ldo14",	NULL),
	REGULATOR_SUPPLY("usb",		NULL),
};

PCOM_VREG_CONSUMERS(ldo15) = {
	REGULATOR_SUPPLY("ldo15",	NULL),
	REGULATOR_SUPPLY("usim2",	NULL),
};

PCOM_VREG_CONSUMERS(ldo16) = {
	REGULATOR_SUPPLY("ldo16",	NULL),
	REGULATOR_SUPPLY("ruim",	NULL),
};

PCOM_VREG_CONSUMERS(ldo17) = {
	REGULATOR_SUPPLY("ldo17",	NULL),
	REGULATOR_SUPPLY("bt",		NULL),
};

PCOM_VREG_CONSUMERS(ldo18) = {
	REGULATOR_SUPPLY("ldo18",	NULL),
	REGULATOR_SUPPLY("rftx",	NULL),
};

PCOM_VREG_CONSUMERS(ldo19) = {
	REGULATOR_SUPPLY("ldo19",	NULL),
	REGULATOR_SUPPLY("wlan4",	NULL),
};

PCOM_VREG_CONSUMERS(ncp)   = {
	REGULATOR_SUPPLY("ncp",		NULL),
};

static struct proccomm_regulator_info msm7x27a_pcom_vreg_info[] = {
	/* Standard regulators (SMPS and LDO)
	 * R = rise time (us)
	 * P = pulldown (1 = pull down, 0 = float, -1 = don't care)
	 * A = always on
	 * B = boot on
	 * V = automatic voltage set (meaningful for single-voltage regs only)
	 * S = supply voltage (uV)
	 * T = type of regulator (smps, pldo, nldo)
	 *            name   id  supp  min uV    max uV  R   P  A  B  V  S  T*/
	PCOM_VREG_SMP(smps1,  3, NULL, 1100000, 1100000, 0, -1, 0, 0, 0, 0, s),
	PCOM_VREG_SMP(smps2,  4, NULL, 1100000, 1100000, 0, -1, 0, 0, 0, 0, s),
	PCOM_VREG_SMP(smps3,  2, NULL, 1800000, 1800000, 0, -1, 0, 0, 0, 0, s),
	PCOM_VREG_SMP(smps4, 24, NULL, 2100000, 2100000, 0, -1, 0, 0, 0, 0, s),
	PCOM_VREG_LDO(ldo01, 12, NULL, 1800000, 2100000, 0, -1, 0, 0, 0, 0, p),
	PCOM_VREG_LDO(ldo02, 13, NULL, 2850000, 2850000, 0, -1, 0, 0, 0, 0, p),
	PCOM_VREG_LDO(ldo03, 49, NULL, 1200000, 1200000, 0, -1, 0, 0, 0, 0, n),
	PCOM_VREG_LDO(ldo04, 50, NULL, 1100000, 1100000, 0, -1, 0, 0, 0, 0, n),
	PCOM_VREG_LDO(ldo05, 45, NULL, 1300000, 1350000, 0, -1, 0, 0, 0, 0, n),
	PCOM_VREG_LDO(ldo06, 51, NULL, 1200000, 1200000, 0, -1, 0, 0, 0, 0, n),
	PCOM_VREG_LDO(ldo07,  0, NULL, 2600000, 2600000, 0, -1, 0, 0, 0, 0, p),
	PCOM_VREG_LDO(ldo08,  9, NULL, 2850000, 2850000, 0, -1, 0, 0, 0, 0, p),
	PCOM_VREG_LDO(ldo09, 44, NULL, 1800000, 1800000, 0, -1, 0, 0, 0, 0, p),
	PCOM_VREG_LDO(ldo10, 52, NULL, 1800000, 3000000, 0, -1, 0, 0, 0, 0, p),
	PCOM_VREG_LDO(ldo11, 53, NULL, 1800000, 1800000, 0, -1, 0, 0, 0, 0, p),
	PCOM_VREG_LDO(ldo12, 21, NULL, 2850000, 2850000, 0, -1, 0, 0, 0, 0, p),
	PCOM_VREG_LDO(ldo13, 18, NULL, 2850000, 2850000, 0, -1, 0, 0, 0, 0, p),
	PCOM_VREG_LDO(ldo14, 16, NULL, 3300000, 3300000, 0, -1, 0, 0, 0, 0, p),
	PCOM_VREG_LDO(ldo15, 54, NULL, 1800000, 2850000, 0, -1, 0, 0, 0, 0, p),
	PCOM_VREG_LDO(ldo16, 19, NULL, 1800000, 2850000, 0, -1, 0, 0, 0, 0, p),
	PCOM_VREG_LDO(ldo17, 56, NULL, 2900000, 3300000, 0, -1, 0, 0, 0, 0, p),
	PCOM_VREG_LDO(ldo18, 11, NULL, 2700000, 2700000, 0, -1, 0, 0, 0, 0, p),
	PCOM_VREG_LDO(ldo19, 57, NULL, 1200000, 1800000, 0, -1, 0, 0, 0, 0, p),

	PCOM_VREG_NCP(ncp,   31, NULL, -1800000, -1800000, 0,     0, 0, 0, 0),
};

struct proccomm_regulator_platform_data msm7x27a_proccomm_regulator_data = {
	.regs = msm7x27a_pcom_vreg_info,
	.nregs = ARRAY_SIZE(msm7x27a_pcom_vreg_info)
};
