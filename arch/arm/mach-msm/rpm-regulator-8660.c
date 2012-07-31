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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include "rpm-regulator-private.h"

/* RPM regulator request formats */
static struct rpm_vreg_parts ldo_parts = {
	.request_len	= 2,
	.mV		= REQUEST_MEMBER(0, 0x00000FFF,  0),
	.ip		= REQUEST_MEMBER(0, 0x00FFF000, 12),
	.fm		= REQUEST_MEMBER(0, 0x03000000, 24),
	.pc		= REQUEST_MEMBER(0, 0x3C000000, 26),
	.pf		= REQUEST_MEMBER(0, 0xC0000000, 30),
	.pd		= REQUEST_MEMBER(1, 0x00000001,  0),
	.ia		= REQUEST_MEMBER(1, 0x00001FFE,  1),
};

static struct rpm_vreg_parts smps_parts = {
	.request_len	= 2,
	.mV		= REQUEST_MEMBER(0, 0x00000FFF,  0),
	.ip		= REQUEST_MEMBER(0, 0x00FFF000, 12),
	.fm		= REQUEST_MEMBER(0, 0x03000000, 24),
	.pc		= REQUEST_MEMBER(0, 0x3C000000, 26),
	.pf		= REQUEST_MEMBER(0, 0xC0000000, 30),
	.pd		= REQUEST_MEMBER(1, 0x00000001,  0),
	.ia		= REQUEST_MEMBER(1, 0x00001FFE,  1),
	.freq		= REQUEST_MEMBER(1, 0x001FE000, 13),
	.freq_clk_src	= REQUEST_MEMBER(1, 0x00600000, 21),
};

static struct rpm_vreg_parts switch_parts = {
	.request_len	= 1,
	.enable_state	= REQUEST_MEMBER(0, 0x00000001,  0),
	.pd		= REQUEST_MEMBER(0, 0x00000002,  1),
	.pc		= REQUEST_MEMBER(0, 0x0000003C,  2),
	.pf		= REQUEST_MEMBER(0, 0x000000C0,  6),
	.hpm		= REQUEST_MEMBER(0, 0x00000300,  8),
};

static struct rpm_vreg_parts ncp_parts = {
	.request_len	= 1,
	.mV		= REQUEST_MEMBER(0, 0x00000FFF,  0),
	.enable_state	= REQUEST_MEMBER(0, 0x00001000, 12),
	.comp_mode	= REQUEST_MEMBER(0, 0x00002000, 13),
	.freq		= REQUEST_MEMBER(0, 0x003FC000, 14),
};

/* Physically available PMIC regulator voltage setpoint ranges */
static struct vreg_range pldo_ranges[] = {
	VOLTAGE_RANGE( 750000, 1487500, 12500),
	VOLTAGE_RANGE(1500000, 3075000, 25000),
	VOLTAGE_RANGE(3100000, 4900000, 50000),
};

static struct vreg_range nldo_ranges[] = {
	VOLTAGE_RANGE( 750000, 1537500, 12500),
};

static struct vreg_range smps_ranges[] = {
	VOLTAGE_RANGE( 375000,  737500, 12500),
	VOLTAGE_RANGE( 750000, 1487500, 12500),
	VOLTAGE_RANGE(1500000, 3075000, 25000),
};

static struct vreg_range ftsmps_ranges[] = {
	VOLTAGE_RANGE( 350000,  650000, 50000),
	VOLTAGE_RANGE( 700000, 1400000, 12500),
	VOLTAGE_RANGE(1500000, 3300000, 50000),
};

static struct vreg_range ncp_ranges[] = {
	VOLTAGE_RANGE(1500000, 3050000, 50000),
};

static struct vreg_set_points pldo_set_points = SET_POINTS(pldo_ranges);
static struct vreg_set_points nldo_set_points = SET_POINTS(nldo_ranges);
static struct vreg_set_points smps_set_points = SET_POINTS(smps_ranges);
static struct vreg_set_points ftsmps_set_points = SET_POINTS(ftsmps_ranges);
static struct vreg_set_points ncp_set_points = SET_POINTS(ncp_ranges);

static struct vreg_set_points *all_set_points[] = {
	&pldo_set_points,
	&nldo_set_points,
	&smps_set_points,
	&ftsmps_set_points,
	&ncp_set_points,
};

#define LDO(_vreg_id, _rpm_id, _name, _name_pc, _ranges, _hpm_min_load, \
		_requires_cxo) \
	[RPM_VREG_ID_##_vreg_id] = { \
		.req = { \
			[0] = { .id = MSM_RPM_ID_##_rpm_id##_0, }, \
			[1] = { .id = MSM_RPM_ID_##_rpm_id##_1, }, \
		}, \
		.hpm_min_load  = RPM_VREG_8660_##_hpm_min_load##_HPM_MIN_LOAD, \
		.type		 = RPM_REGULATOR_TYPE_LDO, \
		.set_points	 = &_ranges##_set_points, \
		.part		 = &ldo_parts, \
		.id		 = RPM_VREG_ID_##_vreg_id, \
		.rdesc.name	 = _name, \
		.rdesc_pc.name	 = _name_pc, \
		.requires_cxo	 = _requires_cxo, \
	}

#define SMPS(_vreg_id, _rpm_id, _name, _name_pc, _ranges, _hpm_min_load) \
	[RPM_VREG_ID_##_vreg_id] = { \
		.req = { \
			[0] = { .id = MSM_RPM_ID_##_rpm_id##_0, }, \
			[1] = { .id = MSM_RPM_ID_##_rpm_id##_1, }, \
		}, \
		.hpm_min_load  = RPM_VREG_8660_##_hpm_min_load##_HPM_MIN_LOAD, \
		.type		 = RPM_REGULATOR_TYPE_SMPS, \
		.set_points	 = &_ranges##_set_points, \
		.part		 = &smps_parts, \
		.id		 = RPM_VREG_ID_##_vreg_id, \
		.rdesc.name	 = _name, \
		.rdesc_pc.name	 = _name_pc, \
	}

#define LVS(_vreg_id, _rpm_id, _name, _name_pc) \
	[RPM_VREG_ID_##_vreg_id] = { \
		.req = { \
			[0] = { .id = MSM_RPM_ID_##_rpm_id, }, \
			[1] = { .id = -1, }, \
		}, \
		.type		 = RPM_REGULATOR_TYPE_VS, \
		.part		 = &switch_parts, \
		.id		 = RPM_VREG_ID_##_vreg_id, \
		.rdesc.name	 = _name, \
		.rdesc_pc.name	 = _name_pc, \
	}

#define MVS(_vreg_id, _rpm_id, _name, _name_pc) \
		LVS(_vreg_id, _rpm_id, _name, _name_pc)

#define NCP(_vreg_id, _rpm_id, _name, _name_pc) \
	[RPM_VREG_ID_##_vreg_id] = { \
		.req = { \
			[0] = { .id = MSM_RPM_ID_##_rpm_id##_0, }, \
			[1] = { .id = MSM_RPM_ID_##_rpm_id##_1, }, \
		}, \
		.type		 = RPM_REGULATOR_TYPE_NCP, \
		.set_points	 = &ncp_set_points, \
		.part		 = &ncp_parts, \
		.id		 = RPM_VREG_ID_##_vreg_id, \
		.rdesc.name	 = _name, \
		.rdesc_pc.name	 = _name_pc, \
	}

static struct vreg vregs[] = {
	LDO(PM8058_L0,   LDO0,   "8058_l0",   "8058_l0_pc",  nldo, LDO_150, 1),
	LDO(PM8058_L1,   LDO1,   "8058_l1",   "8058_l1_pc",  nldo, LDO_300, 1),
	LDO(PM8058_L2,   LDO2,   "8058_l2",   "8058_l2_pc",  pldo, LDO_300, 0),
	LDO(PM8058_L3,   LDO3,   "8058_l3",   "8058_l3_pc",  pldo, LDO_150, 0),
	LDO(PM8058_L4,   LDO4,   "8058_l4",   "8058_l4_pc",  pldo, LDO_50,  0),
	LDO(PM8058_L5,   LDO5,   "8058_l5",   "8058_l5_pc",  pldo, LDO_300, 0),
	LDO(PM8058_L6,   LDO6,   "8058_l6",   "8058_l6_pc",  pldo, LDO_50,  0),
	LDO(PM8058_L7,   LDO7,   "8058_l7",   "8058_l7_pc",  pldo, LDO_50,  0),
	LDO(PM8058_L8,   LDO8,   "8058_l8",   "8058_l8_pc",  pldo, LDO_300, 0),
	LDO(PM8058_L9,   LDO9,   "8058_l9",   "8058_l9_pc",  pldo, LDO_300, 0),
	LDO(PM8058_L10,  LDO10,  "8058_l10",  "8058_l10_pc", pldo, LDO_300, 0),
	LDO(PM8058_L11,  LDO11,  "8058_l11",  "8058_l11_pc", pldo, LDO_150, 0),
	LDO(PM8058_L12,  LDO12,  "8058_l12",  "8058_l12_pc", pldo, LDO_150, 0),
	LDO(PM8058_L13,  LDO13,  "8058_l13",  "8058_l13_pc", pldo, LDO_300, 0),
	LDO(PM8058_L14,  LDO14,  "8058_l14",  "8058_l14_pc", pldo, LDO_300, 0),
	LDO(PM8058_L15,  LDO15,  "8058_l15",  "8058_l15_pc", pldo, LDO_300, 0),
	LDO(PM8058_L16,  LDO16,  "8058_l16",  "8058_l16_pc", pldo, LDO_300, 0),
	LDO(PM8058_L17,  LDO17,  "8058_l17",  "8058_l17_pc", pldo, LDO_150, 0),
	LDO(PM8058_L18,  LDO18,  "8058_l18",  "8058_l18_pc", pldo, LDO_150, 0),
	LDO(PM8058_L19,  LDO19,  "8058_l19",  "8058_l19_pc", pldo, LDO_150, 0),
	LDO(PM8058_L20,  LDO20,  "8058_l20",  "8058_l20_pc", pldo, LDO_150, 0),
	LDO(PM8058_L21,  LDO21,  "8058_l21",  "8058_l21_pc", nldo, LDO_150, 1),
	LDO(PM8058_L22,  LDO22,  "8058_l22",  "8058_l22_pc", nldo, LDO_300, 1),
	LDO(PM8058_L23,  LDO23,  "8058_l23",  "8058_l23_pc", nldo, LDO_300, 1),
	LDO(PM8058_L24,  LDO24,  "8058_l24",  "8058_l24_pc", nldo, LDO_150, 1),
	LDO(PM8058_L25,  LDO25,  "8058_l25",  "8058_l25_pc", nldo, LDO_150, 1),

	SMPS(PM8058_S0,  SMPS0,  "8058_s0",   "8058_s0_pc",  smps, SMPS),
	SMPS(PM8058_S1,  SMPS1,  "8058_s1",   "8058_s1_pc",  smps, SMPS),
	SMPS(PM8058_S2,  SMPS2,  "8058_s2",   "8058_s2_pc",  smps, SMPS),
	SMPS(PM8058_S3,  SMPS3,  "8058_s3",   "8058_s3_pc",  smps, SMPS),
	SMPS(PM8058_S4,  SMPS4,  "8058_s4",   "8058_s4_pc",  smps, SMPS),

	LVS(PM8058_LVS0, LVS0,   "8058_lvs0", "8058_lvs0_pc"),
	LVS(PM8058_LVS1, LVS1,   "8058_lvs1", "8058_lvs1_pc"),

	NCP(PM8058_NCP,  NCP,    "8058_ncp",  NULL),

	LDO(PM8901_L0,   LDO0B,  "8901_l0",   "8901_l0_pc",  nldo, LDO_300, 1),
	LDO(PM8901_L1,   LDO1B,  "8901_l1",   "8901_l1_pc",  pldo, LDO_300, 0),
	LDO(PM8901_L2,   LDO2B,  "8901_l2",   "8901_l2_pc",  pldo, LDO_300, 0),
	LDO(PM8901_L3,   LDO3B,  "8901_l3",   "8901_l3_pc",  pldo, LDO_300, 0),
	LDO(PM8901_L4,   LDO4B,  "8901_l4",   "8901_l4_pc",  pldo, LDO_300, 0),
	LDO(PM8901_L5,   LDO5B,  "8901_l5",   "8901_l5_pc",  pldo, LDO_300, 0),
	LDO(PM8901_L6,   LDO6B,  "8901_l6",   "8901_l6_pc",  pldo, LDO_300, 0),

	SMPS(PM8901_S0,  SMPS0B, "8901_s0",   "8901_s0_pc", ftsmps, FTSMPS),
	SMPS(PM8901_S1,  SMPS1B, "8901_s1",   "8901_s1_pc", ftsmps, FTSMPS),
	SMPS(PM8901_S2,  SMPS2B, "8901_s2",   "8901_s2_pc", ftsmps, FTSMPS),
	SMPS(PM8901_S3,  SMPS3B, "8901_s3",   "8901_s3_pc", ftsmps, FTSMPS),
	SMPS(PM8901_S4,  SMPS4B, "8901_s4",   "8901_s4_pc", ftsmps, FTSMPS),

	LVS(PM8901_LVS0, LVS0B,  "8901_lvs0", "8901_lvs0_pc"),
	LVS(PM8901_LVS1, LVS1B,  "8901_lvs1", "8901_lvs1_pc"),
	LVS(PM8901_LVS2, LVS2B,  "8901_lvs2", "8901_lvs2_pc"),
	LVS(PM8901_LVS3, LVS3B,  "8901_lvs3", "8901_lvs3_pc"),

	MVS(PM8901_MVS0, MVS,    "8901_mvs0", "8901_mvs0_pc"),
};

static const char *pin_func_label[] = {
	[RPM_VREG_PIN_FN_8660_ENABLE]		= "on/off",
	[RPM_VREG_PIN_FN_8660_MODE]		= "HPM/LPM",
	[RPM_VREG_PIN_FN_8660_SLEEP_B]		= "sleep_b",
	[RPM_VREG_PIN_FN_8660_NONE]		= "none",
};

static const char *force_mode_label[] = {
	[RPM_VREG_FORCE_MODE_8660_NONE]		= "none",
	[RPM_VREG_FORCE_MODE_8660_LPM]		= "LPM",
	[RPM_VREG_FORCE_MODE_8660_HPM]		= "HPM",
};

static const char *pin_control_label[] = {
	" A0",
	" A1",
	" D0",
	" D1",
};

static int is_real_id(int id)
{
	return (id >= 0) && (id <= RPM_VREG_ID_8660_MAX_REAL);
}

static int pc_id_to_real_id(int id)
{
	int real_id;

	if (id >= RPM_VREG_ID_PM8058_L0_PC && id <= RPM_VREG_ID_PM8058_LVS1_PC)
		real_id = id - RPM_VREG_ID_PM8058_L0_PC + RPM_VREG_ID_PM8058_L0;
	else
		real_id = id - RPM_VREG_ID_PM8901_L0_PC + RPM_VREG_ID_PM8901_L0;

	return real_id;
}

static struct vreg_config config = {
	.vregs				= vregs,
	.vregs_len			= ARRAY_SIZE(vregs),

	.vreg_id_min			= RPM_VREG_ID_PM8058_L0,
	.vreg_id_max			= RPM_VREG_ID_8660_MAX,

	.pin_func_none			= RPM_VREG_PIN_FN_8660_NONE,
	.pin_func_sleep_b		= RPM_VREG_PIN_FN_8660_SLEEP_B,

	.mode_lpm			= REGULATOR_MODE_IDLE,
	.mode_hpm			= REGULATOR_MODE_NORMAL,

	.set_points			= all_set_points,
	.set_points_len			= ARRAY_SIZE(all_set_points),

	.label_pin_ctrl			= pin_control_label,
	.label_pin_ctrl_len		= ARRAY_SIZE(pin_control_label),
	.label_pin_func			= pin_func_label,
	.label_pin_func_len		= ARRAY_SIZE(pin_func_label),
	.label_force_mode		= force_mode_label,
	.label_force_mode_len		= ARRAY_SIZE(force_mode_label),

	.is_real_id			= is_real_id,
	.pc_id_to_real_id		= pc_id_to_real_id,

	.use_legacy_optimum_mode	= 1,
	.ia_follows_ip			= 1,
};

struct vreg_config *get_config_8660(void)
{
	return &config;
}
