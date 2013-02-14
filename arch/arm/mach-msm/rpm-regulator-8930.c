/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
	.uV		= REQUEST_MEMBER(0, 0x007FFFFF,  0),
	.pd		= REQUEST_MEMBER(0, 0x00800000, 23),
	.pc		= REQUEST_MEMBER(0, 0x0F000000, 24),
	.pf		= REQUEST_MEMBER(0, 0xF0000000, 28),
	.ip		= REQUEST_MEMBER(1, 0x000003FF,  0),
	.ia		= REQUEST_MEMBER(1, 0x000FFC00, 10),
	.fm		= REQUEST_MEMBER(1, 0x00700000, 20),
};

static struct rpm_vreg_parts smps_parts = {
	.request_len	= 2,
	.uV		= REQUEST_MEMBER(0, 0x007FFFFF,  0),
	.pd		= REQUEST_MEMBER(0, 0x00800000, 23),
	.pc		= REQUEST_MEMBER(0, 0x0F000000, 24),
	.pf		= REQUEST_MEMBER(0, 0xF0000000, 28),
	.ip		= REQUEST_MEMBER(1, 0x000003FF,  0),
	.ia		= REQUEST_MEMBER(1, 0x000FFC00, 10),
	.fm		= REQUEST_MEMBER(1, 0x00700000, 20),
	.pm		= REQUEST_MEMBER(1, 0x00800000, 23),
	.freq		= REQUEST_MEMBER(1, 0x1F000000, 24),
	.freq_clk_src	= REQUEST_MEMBER(1, 0x60000000, 29),
};

static struct rpm_vreg_parts switch_parts = {
	.request_len	= 1,
	.enable_state	= REQUEST_MEMBER(0, 0x00000001,  0),
	.pd		= REQUEST_MEMBER(0, 0x00000002,  1),
	.pc		= REQUEST_MEMBER(0, 0x0000003C,  2),
	.pf		= REQUEST_MEMBER(0, 0x000003C0,  6),
	.hpm		= REQUEST_MEMBER(0, 0x00000C00, 10),
};

static struct rpm_vreg_parts corner_parts = {
	.request_len	= 1,
	.uV		= REQUEST_MEMBER(0, 0x00000003,  0),
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

static struct vreg_range nldo1200_ranges[] = {
	VOLTAGE_RANGE( 375000,  743750,  6250),
	VOLTAGE_RANGE( 750000, 1537500, 12500),
};

static struct vreg_range ln_ldo_ranges[] = {
	VOLTAGE_RANGE( 690000, 1110000,  60000),
	VOLTAGE_RANGE(1380000, 2220000, 120000),
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

static struct vreg_range corner_ranges[] = {
	VOLTAGE_RANGE(RPM_VREG_CORNER_NONE, RPM_VREG_CORNER_HIGH, 1),
};

static struct vreg_set_points pldo_set_points = SET_POINTS(pldo_ranges);
static struct vreg_set_points nldo_set_points = SET_POINTS(nldo_ranges);
static struct vreg_set_points nldo1200_set_points = SET_POINTS(nldo1200_ranges);
static struct vreg_set_points ln_ldo_set_points = SET_POINTS(ln_ldo_ranges);
static struct vreg_set_points smps_set_points = SET_POINTS(smps_ranges);
static struct vreg_set_points ftsmps_set_points = SET_POINTS(ftsmps_ranges);
static struct vreg_set_points corner_set_points = SET_POINTS(corner_ranges);

static struct vreg_set_points *all_set_points[] = {
	&pldo_set_points,
	&nldo_set_points,
	&nldo1200_set_points,
	&ln_ldo_set_points,
	&smps_set_points,
	&ftsmps_set_points,
	&corner_set_points,
};

#define LDO(_id, _name, _name_pc, _ranges, _hpm_min_load, _requires_cxo) \
	[RPM_VREG_ID_PM##_id] = { \
		.req = { \
			[0] = { .id = MSM_RPM_ID_PM##_id##_0, }, \
			[1] = { .id = MSM_RPM_ID_PM##_id##_1, }, \
		}, \
		.hpm_min_load  = RPM_VREG_8930_##_hpm_min_load##_HPM_MIN_LOAD, \
		.type		 = RPM_REGULATOR_TYPE_LDO, \
		.set_points	 = &_ranges##_set_points, \
		.part		 = &ldo_parts, \
		.id		 = RPM_VREG_ID_PM##_id, \
		.rdesc.name	 = _name, \
		.rdesc_pc.name	 = _name_pc, \
		.requires_cxo	 = _requires_cxo, \
	}

#define SMPS(_id, _name, _name_pc, _ranges, _hpm_min_load) \
	[RPM_VREG_ID_PM##_id] = { \
		.req = { \
			[0] = { .id = MSM_RPM_ID_PM##_id##_0, }, \
			[1] = { .id = MSM_RPM_ID_PM##_id##_1, }, \
		}, \
		.hpm_min_load  = RPM_VREG_8930_##_hpm_min_load##_HPM_MIN_LOAD, \
		.type		 = RPM_REGULATOR_TYPE_SMPS, \
		.set_points	 = &_ranges##_set_points, \
		.part		 = &smps_parts, \
		.id		 = RPM_VREG_ID_PM##_id, \
		.rdesc.name	 = _name, \
		.rdesc_pc.name	 = _name_pc, \
	}

#define LVS(_id, _name, _name_pc) \
	[RPM_VREG_ID_PM##_id] = { \
		.req = { \
			[0] = { .id = MSM_RPM_ID_PM##_id, }, \
			[1] = { .id = -1, }, \
		}, \
		.type		 = RPM_REGULATOR_TYPE_VS, \
		.part		 = &switch_parts, \
		.id		 = RPM_VREG_ID_PM##_id, \
		.rdesc.name	 = _name, \
		.rdesc_pc.name	 = _name_pc, \
	}

#define MVS(_vreg_id, _name, _name_pc, _rpm_id) \
	[RPM_VREG_ID_PM##_vreg_id] = { \
		.req = { \
			[0] = { .id = MSM_RPM_ID_##_rpm_id, }, \
			[1] = { .id = -1, }, \
		}, \
		.type		 = RPM_REGULATOR_TYPE_VS, \
		.part		 = &switch_parts, \
		.id		 = RPM_VREG_ID_PM##_vreg_id, \
		.rdesc.name	 = _name, \
		.rdesc_pc.name	 = _name_pc, \
	}

#define CORNER(_id, _rpm_id, _name, _ranges) \
	[RPM_VREG_ID_PM##_id] = { \
		.req = { \
			[0] = { .id = MSM_RPM_ID_##_rpm_id, }, \
			[1] = { .id = -1, }, \
		}, \
		.type		 = RPM_REGULATOR_TYPE_CORNER, \
		.set_points	 = &_ranges##_set_points, \
		.part		 = &corner_parts, \
		.id		 = RPM_VREG_ID_PM##_id, \
		.rdesc.name	 = _name, \
	}

static struct vreg vregs_msm8930_pm8038[] = {
	LDO(8038_L1,   "8038_l1",   NULL,          nldo1200, LDO_1200, 1),
	LDO(8038_L2,   "8038_l2",   "8038_l2_pc",  nldo,     LDO_150,  1),
	LDO(8038_L3,   "8038_l3",   "8038_l3_pc",  pldo,     LDO_50,   0),
	LDO(8038_L4,   "8038_l4",   "8038_l4_pc",  pldo,     LDO_50,   0),
	LDO(8038_L5,   "8038_l5",   "8038_l5_pc",  pldo,     LDO_600,  0),
	LDO(8038_L6,   "8038_l6",   "8038_l6_pc",  pldo,     LDO_600,  0),
	LDO(8038_L7,   "8038_l7",   "8038_l7_pc",  pldo,     LDO_600,  0),
	LDO(8038_L8,   "8038_l8",   "8038_l8_pc",  pldo,     LDO_300,  0),
	LDO(8038_L9,   "8038_l9",   "8038_l9_pc",  pldo,     LDO_300,  0),
	LDO(8038_L10,  "8038_l10",  "8038_l10_pc", pldo,     LDO_600,  0),
	LDO(8038_L11,  "8038_l11",  "8038_l11_pc", pldo,     LDO_600,  0),
	LDO(8038_L12,  "8038_l12",  "8038_l12_pc", nldo,     LDO_300,  1),
	LDO(8038_L13,  "8038_l13",  NULL,          ln_ldo,   LDO_5,    0),
	LDO(8038_L14,  "8038_l14",  "8038_l14_pc", pldo,     LDO_50,   0),
	LDO(8038_L15,  "8038_l15",  "8038_l15_pc", pldo,     LDO_150,  0),
	LDO(8038_L16,  "8038_l16",  NULL,          nldo1200, LDO_1200, 1),
	LDO(8038_L17,  "8038_l17",  "8038_l17_pc", pldo,     LDO_150,  0),
	LDO(8038_L18,  "8038_l18",  "8038_l18_pc", pldo,     LDO_50,   0),
	LDO(8038_L19,  "8038_l19",  NULL,          nldo1200, LDO_1200, 1),
	LDO(8038_L20,  "8038_l20",  NULL,          nldo1200, LDO_1200, 1),
	LDO(8038_L21,  "8038_l21",  "8038_l21_pc", pldo,     LDO_150,  0),
	LDO(8038_L22,  "8038_l22",  "8038_l22_pc", pldo,     LDO_50,   0),
	LDO(8038_L23,  "8038_l23",  "8038_l23_pc", pldo,     LDO_50,   0),
	LDO(8038_L24,  "8038_l24",  NULL,          nldo1200, LDO_1200, 1),
	LDO(8038_L25,  "8038_l25",  NULL,          ln_ldo,   LDO_5,    0),
	LDO(8038_L26,  "8038_l26",  "8038_l26_pc", nldo,     LDO_150,  1),
	LDO(8038_L27,  "8038_l27",  NULL,          nldo1200, LDO_1200, 1),

	SMPS(8038_S1,  "8038_s1",   "8038_s1_pc",  smps,     SMPS_1500),
	SMPS(8038_S2,  "8038_s2",   "8038_s2_pc",  smps,     SMPS_1500),
	SMPS(8038_S3,  "8038_s3",   "8038_s3_pc",  smps,     SMPS_1500),
	SMPS(8038_S4,  "8038_s4",   "8038_s4_pc",  smps,     SMPS_1500),
	SMPS(8038_S5,  "8038_s5",   NULL,          ftsmps,   SMPS_2000),
	SMPS(8038_S6,  "8038_s6",   NULL,          ftsmps,   SMPS_2000),

	LVS(8038_LVS1, "8038_lvs1", "8038_lvs1_pc"),
	LVS(8038_LVS2, "8038_lvs2", "8038_lvs2_pc"),

	CORNER(8038_VDD_DIG_CORNER, VOLTAGE_CORNER, "vdd_dig_corner", corner),
};

static struct vreg vregs_msm8930_pm8917[] = {
	LDO(8917_L1,   "8917_l1",   "8917_l1_pc",  nldo,     LDO_150,  1),
	LDO(8917_L2,   "8917_l2",   "8917_l2_pc",  nldo,     LDO_150,  1),
	LDO(8917_L3,   "8917_l3",   "8917_l3_pc",  pldo,     LDO_150,  0),
	LDO(8917_L4,   "8917_l4",   "8917_l4_pc",  pldo,     LDO_50,   0),
	LDO(8917_L5,   "8917_l5",   "8917_l5_pc",  pldo,     LDO_300,  0),
	LDO(8917_L6,   "8917_l6",   "8917_l6_pc",  pldo,     LDO_600,  0),
	LDO(8917_L7,   "8917_l7",   "8917_l7_pc",  pldo,     LDO_150,  0),
	LDO(8917_L8,   "8917_l8",   "8917_l8_pc",  pldo,     LDO_300,  0),
	LDO(8917_L9,   "8917_l9",   "8917_l9_pc",  pldo,     LDO_300,  0),
	LDO(8917_L10,  "8917_l10",  "8917_l10_pc", pldo,     LDO_600,  0),
	LDO(8917_L11,  "8917_l11",  "8917_l11_pc", pldo,     LDO_150,  0),
	LDO(8917_L12,  "8917_l12",  "8917_l12_pc", nldo,     LDO_150,  1),
	LDO(8917_L14,  "8917_l14",  "8917_l14_pc", pldo,     LDO_50,   0),
	LDO(8917_L15,  "8917_l15",  "8917_l15_pc", pldo,     LDO_150,  0),
	LDO(8917_L16,  "8917_l16",  "8917_l16_pc", pldo,     LDO_300,  0),
	LDO(8917_L17,  "8917_l17",  "8917_l17_pc", pldo,     LDO_150,  0),
	LDO(8917_L18,  "8917_l18",  "8917_l18_pc", nldo,     LDO_150,  1),
	LDO(8917_L21,  "8917_l21",  "8917_l21_pc", pldo,     LDO_150,  0),
	LDO(8917_L22,  "8917_l22",  "8917_l22_pc", pldo,     LDO_150,  0),
	LDO(8917_L23,  "8917_l23",  "8917_l23_pc", pldo,     LDO_150,  0),
	LDO(8917_L24,  "8917_l24",  NULL,          nldo1200, LDO_1200, 0),
	LDO(8917_L25,  "8917_l25",  NULL,          nldo1200, LDO_1200, 0),
	LDO(8917_L26,  "8917_l26",  NULL,          nldo1200, LDO_1200, 0),
	LDO(8917_L27,  "8917_l27",  NULL,          nldo1200, LDO_1200, 0),
	LDO(8917_L28,  "8917_l28",  NULL,          nldo1200, LDO_1200, 0),
	LDO(8917_L29,  "8917_l29",  "8917_l29_pc", pldo,     LDO_150,  0),
	LDO(8917_L30,  "8917_l30",  "8917_l30_pc", pldo,     LDO_150,  0),
	LDO(8917_L31,  "8917_l31",  "8917_l31_pc", pldo,     LDO_150,  0),
	LDO(8917_L32,  "8917_l32",  "8917_l32_pc", pldo,     LDO_150,  0),
	LDO(8917_L33,  "8917_l33",  "8917_l33_pc", pldo,     LDO_150,  0),
	LDO(8917_L34,  "8917_l34",  "8917_l34_pc", pldo,     LDO_150,  0),
	LDO(8917_L35,  "8917_l35",  "8917_l35_pc", pldo,     LDO_300,  0),
	LDO(8917_L36,  "8917_l36",  "8917_l36_pc", pldo,     LDO_50,   0),

	SMPS(8917_S1,  "8917_s1",   "8917_s1_pc",  smps,     SMPS_1500),
	SMPS(8917_S2,  "8917_s2",   "8917_s2_pc",  smps,     SMPS_1500),
	SMPS(8917_S3,  "8917_s3",   "8917_s3_pc",  smps,     SMPS_1500),
	SMPS(8917_S4,  "8917_s4",   "8917_s4_pc",  smps,     SMPS_1500),
	SMPS(8917_S5,  "8917_s5",   NULL,          ftsmps,   SMPS_2000),
	SMPS(8917_S6,  "8917_s6",   NULL,          ftsmps,   SMPS_2000),
	SMPS(8917_S7,  "8917_s7",   "8917_s7_pc",  smps,     SMPS_1500),
	SMPS(8917_S8,  "8917_s8",   "8917_s8_pc",  smps,     SMPS_1500),

	LVS(8917_LVS1, "8917_lvs1", "8917_lvs1_pc"),
	LVS(8917_LVS3, "8917_lvs3", "8917_lvs3_pc"),
	LVS(8917_LVS4, "8917_lvs4", "8917_lvs4_pc"),
	LVS(8917_LVS5, "8917_lvs5", "8917_lvs5_pc"),
	LVS(8917_LVS6, "8917_lvs6", "8917_lvs6_pc"),
	LVS(8917_LVS7, "8917_lvs7", "8917_lvs7_pc"),
	MVS(8917_USB_OTG,  "8917_usb_otg",  NULL, USB_OTG_SWITCH),

	CORNER(8917_VDD_DIG_CORNER, VOLTAGE_CORNER, "vdd_dig_corner", corner),
};

static const char *pin_func_label[] = {
	[RPM_VREG_PIN_FN_8930_DONT_CARE]	= "don't care",
	[RPM_VREG_PIN_FN_8930_ENABLE]		= "on/off",
	[RPM_VREG_PIN_FN_8930_MODE]		= "HPM/LPM",
	[RPM_VREG_PIN_FN_8930_SLEEP_B]		= "sleep_b",
	[RPM_VREG_PIN_FN_8930_NONE]		= "none",
};

static const char *force_mode_label[] = {
	[RPM_VREG_FORCE_MODE_8930_NONE]		= "none",
	[RPM_VREG_FORCE_MODE_8930_LPM]		= "LPM",
	[RPM_VREG_FORCE_MODE_8930_AUTO]		= "auto",
	[RPM_VREG_FORCE_MODE_8930_HPM]		= "HPM",
	[RPM_VREG_FORCE_MODE_8930_BYPASS]	= "BYP",
};

static const char *power_mode_label[] = {
	[RPM_VREG_POWER_MODE_8930_HYSTERETIC]	= "HYS",
	[RPM_VREG_POWER_MODE_8930_PWM]		= "PWM",
};

static const char *pin_control_label[] = {
	" D1",
	" A0",
	" A1",
	" A2",
};

static int is_real_id_msm8930_pm8038(int id)
{
	return (id >= 0) && (id <= RPM_VREG_ID_PM8038_MAX_REAL);
}

static int pc_id_to_real_id_msm8930_pm8038(int id)
{
	int real_id = 0;

	if (id >= RPM_VREG_ID_PM8038_L2_PC && id <= RPM_VREG_ID_PM8038_L12_PC)
		real_id = id - RPM_VREG_ID_PM8038_L2_PC
				+ RPM_VREG_ID_PM8038_L2;
	else if (id >= RPM_VREG_ID_PM8038_L14_PC
			&& id <= RPM_VREG_ID_PM8038_L15_PC)
		real_id = id - RPM_VREG_ID_PM8038_L14_PC
				+ RPM_VREG_ID_PM8038_L14;
	else if (id >= RPM_VREG_ID_PM8038_L17_PC
			&& id <= RPM_VREG_ID_PM8038_L18_PC)
		real_id = id - RPM_VREG_ID_PM8038_L17_PC
				+ RPM_VREG_ID_PM8038_L17;
	else if (id >= RPM_VREG_ID_PM8038_L21_PC
			&& id <= RPM_VREG_ID_PM8038_L23_PC)
		real_id = id - RPM_VREG_ID_PM8038_L21_PC
				+ RPM_VREG_ID_PM8038_L21;
	else if (id == RPM_VREG_ID_PM8038_L26_PC)
		real_id = RPM_VREG_ID_PM8038_L26;
	else if (id >= RPM_VREG_ID_PM8038_S1_PC
			&& id <= RPM_VREG_ID_PM8038_S4_PC)
		real_id = id - RPM_VREG_ID_PM8038_S1_PC
				+ RPM_VREG_ID_PM8038_S1;
	else if (id >= RPM_VREG_ID_PM8038_LVS1_PC
			&& id <= RPM_VREG_ID_PM8038_LVS2_PC)
		real_id = id - RPM_VREG_ID_PM8038_LVS1_PC
				+ RPM_VREG_ID_PM8038_LVS1;

	return real_id;
}

static int is_real_id_msm8930_pm8917(int id)
{
	return (id >= 0) && (id <= RPM_VREG_ID_PM8917_MAX_REAL);
}

static int pc_id_to_real_id_msm8930_pm8917(int id)
{
	int real_id = 0;

	if (id >= RPM_VREG_ID_PM8917_L1_PC && id <= RPM_VREG_ID_PM8917_L23_PC)
		real_id = id - RPM_VREG_ID_PM8917_L1_PC
				+ RPM_VREG_ID_PM8917_L1;
	else if (id >= RPM_VREG_ID_PM8917_L29_PC
			&& id <= RPM_VREG_ID_PM8917_S4_PC)
		real_id = id - RPM_VREG_ID_PM8917_L29_PC
				+ RPM_VREG_ID_PM8917_L29;
	else if (id >= RPM_VREG_ID_PM8917_S7_PC
			&& id <= RPM_VREG_ID_PM8917_LVS7_PC)
		real_id = id - RPM_VREG_ID_PM8917_S7_PC
				+ RPM_VREG_ID_PM8917_S7;

	return real_id;
}

static struct vreg_config config_msm8930_pm8038 = {
	.vregs			= vregs_msm8930_pm8038,
	.vregs_len		= ARRAY_SIZE(vregs_msm8930_pm8038),

	.vreg_id_min		= RPM_VREG_ID_PM8038_L1,
	.vreg_id_max		= RPM_VREG_ID_PM8038_MAX,

	.pin_func_none		= RPM_VREG_PIN_FN_8930_NONE,
	.pin_func_sleep_b	= RPM_VREG_PIN_FN_8930_SLEEP_B,

	.mode_lpm		= REGULATOR_MODE_IDLE,
	.mode_hpm		= REGULATOR_MODE_NORMAL,

	.set_points		= all_set_points,
	.set_points_len		= ARRAY_SIZE(all_set_points),

	.label_pin_ctrl		= pin_control_label,
	.label_pin_ctrl_len	= ARRAY_SIZE(pin_control_label),
	.label_pin_func		= pin_func_label,
	.label_pin_func_len	= ARRAY_SIZE(pin_func_label),
	.label_force_mode	= force_mode_label,
	.label_force_mode_len	= ARRAY_SIZE(force_mode_label),
	.label_power_mode	= power_mode_label,
	.label_power_mode_len	= ARRAY_SIZE(power_mode_label),

	.is_real_id		= is_real_id_msm8930_pm8038,
	.pc_id_to_real_id	= pc_id_to_real_id_msm8930_pm8038,
};

static struct vreg_config config_msm8930_pm8917 = {
	.vregs			= vregs_msm8930_pm8917,
	.vregs_len		= ARRAY_SIZE(vregs_msm8930_pm8917),

	.vreg_id_min		= RPM_VREG_ID_PM8917_L1,
	.vreg_id_max		= RPM_VREG_ID_PM8917_MAX,

	.pin_func_none		= RPM_VREG_PIN_FN_8930_NONE,
	.pin_func_sleep_b	= RPM_VREG_PIN_FN_8930_SLEEP_B,

	.mode_lpm		= REGULATOR_MODE_IDLE,
	.mode_hpm		= REGULATOR_MODE_NORMAL,

	.set_points		= all_set_points,
	.set_points_len		= ARRAY_SIZE(all_set_points),

	.label_pin_ctrl		= pin_control_label,
	.label_pin_ctrl_len	= ARRAY_SIZE(pin_control_label),
	.label_pin_func		= pin_func_label,
	.label_pin_func_len	= ARRAY_SIZE(pin_func_label),
	.label_force_mode	= force_mode_label,
	.label_force_mode_len	= ARRAY_SIZE(force_mode_label),
	.label_power_mode	= power_mode_label,
	.label_power_mode_len	= ARRAY_SIZE(power_mode_label),

	.is_real_id		= is_real_id_msm8930_pm8917,
	.pc_id_to_real_id	= pc_id_to_real_id_msm8930_pm8917,
};

struct vreg_config *get_config_8930(void)
{
	return &config_msm8930_pm8038;
}

struct vreg_config *get_config_8930_pm8917(void)
{
	return &config_msm8930_pm8917;
}
