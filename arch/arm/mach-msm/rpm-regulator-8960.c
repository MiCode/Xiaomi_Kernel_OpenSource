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

static struct rpm_vreg_parts ncp_parts = {
	.request_len	= 1,
	.uV		= REQUEST_MEMBER(0, 0x007FFFFF,  0),
	.enable_state	= REQUEST_MEMBER(0, 0x00800000, 23),
	.comp_mode	= REQUEST_MEMBER(0, 0x01000000, 24),
	.freq		= REQUEST_MEMBER(0, 0x3E000000, 25),
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

static struct vreg_range ncp_ranges[] = {
	VOLTAGE_RANGE(1500000, 3050000, 50000),
};

static struct vreg_set_points pldo_set_points = SET_POINTS(pldo_ranges);
static struct vreg_set_points nldo_set_points = SET_POINTS(nldo_ranges);
static struct vreg_set_points nldo1200_set_points = SET_POINTS(nldo1200_ranges);
static struct vreg_set_points ln_ldo_set_points = SET_POINTS(ln_ldo_ranges);
static struct vreg_set_points smps_set_points = SET_POINTS(smps_ranges);
static struct vreg_set_points ftsmps_set_points = SET_POINTS(ftsmps_ranges);
static struct vreg_set_points ncp_set_points = SET_POINTS(ncp_ranges);

static struct vreg_set_points *all_set_points[] = {
	&pldo_set_points,
	&nldo_set_points,
	&nldo1200_set_points,
	&ln_ldo_set_points,
	&smps_set_points,
	&ftsmps_set_points,
	&ncp_set_points,
};

#define LDO(_id, _name, _name_pc, _ranges, _hpm_min_load, _requires_cxo) \
	[RPM_VREG_ID_PM8921_##_id] = { \
		.req = { \
			[0] = { .id = MSM_RPM_ID_PM8921_##_id##_0, }, \
			[1] = { .id = MSM_RPM_ID_PM8921_##_id##_1, }, \
		}, \
		.hpm_min_load  = RPM_VREG_8960_##_hpm_min_load##_HPM_MIN_LOAD, \
		.type		 = RPM_REGULATOR_TYPE_LDO, \
		.set_points	 = &_ranges##_set_points, \
		.part		 = &ldo_parts, \
		.id		 = RPM_VREG_ID_PM8921_##_id, \
		.rdesc.name	 = _name, \
		.rdesc_pc.name	 = _name_pc, \
		.requires_cxo	 = _requires_cxo, \
	}

#define SMPS(_id, _name, _name_pc, _ranges, _hpm_min_load) \
	[RPM_VREG_ID_PM8921_##_id] = { \
		.req = { \
			[0] = { .id = MSM_RPM_ID_PM8921_##_id##_0, }, \
			[1] = { .id = MSM_RPM_ID_PM8921_##_id##_1, }, \
		}, \
		.hpm_min_load  = RPM_VREG_8960_##_hpm_min_load##_HPM_MIN_LOAD, \
		.type		 = RPM_REGULATOR_TYPE_SMPS, \
		.set_points	 = &_ranges##_set_points, \
		.part		 = &smps_parts, \
		.id		 = RPM_VREG_ID_PM8921_##_id, \
		.rdesc.name	 = _name, \
		.rdesc_pc.name	 = _name_pc, \
	}

#define LVS(_id, _name, _name_pc) \
	[RPM_VREG_ID_PM8921_##_id] = { \
		.req = { \
			[0] = { .id = MSM_RPM_ID_PM8921_##_id, }, \
			[1] = { .id = -1, }, \
		}, \
		.type		 = RPM_REGULATOR_TYPE_VS, \
		.part		 = &switch_parts, \
		.id		 = RPM_VREG_ID_PM8921_##_id, \
		.rdesc.name	 = _name, \
		.rdesc_pc.name	 = _name_pc, \
	}

#define MVS(_vreg_id, _name, _name_pc, _rpm_id) \
	[RPM_VREG_ID_PM8921_##_vreg_id] = { \
		.req = { \
			[0] = { .id = MSM_RPM_ID_##_rpm_id, }, \
			[1] = { .id = -1, }, \
		}, \
		.type		 = RPM_REGULATOR_TYPE_VS, \
		.part		 = &switch_parts, \
		.id		 = RPM_VREG_ID_PM8921_##_vreg_id, \
		.rdesc.name	 = _name, \
		.rdesc_pc.name	 = _name_pc, \
	}

#define NCP(_id, _name, _name_pc) \
	[RPM_VREG_ID_PM8921_##_id] = { \
		.req = { \
			[0] = { .id = MSM_RPM_ID_##_id##_0, }, \
			[1] = { .id = MSM_RPM_ID_##_id##_1, }, \
		}, \
		.type		 = RPM_REGULATOR_TYPE_NCP, \
		.set_points	 = &ncp_set_points, \
		.part		 = &ncp_parts, \
		.id		 = RPM_VREG_ID_PM8921_##_id, \
		.rdesc.name	 = _name, \
		.rdesc_pc.name	 = _name_pc, \
	}

static struct vreg vregs[] = {
	LDO(L1,   "8921_l1",   "8921_l1_pc",  nldo,     LDO_150,  1),
	LDO(L2,   "8921_l2",   "8921_l2_pc",  nldo,     LDO_150,  1),
	LDO(L3,   "8921_l3",   "8921_l3_pc",  pldo,     LDO_150,  0),
	LDO(L4,   "8921_l4",   "8921_l4_pc",  pldo,     LDO_50,   0),
	LDO(L5,   "8921_l5",   "8921_l5_pc",  pldo,     LDO_300,  0),
	LDO(L6,   "8921_l6",   "8921_l6_pc",  pldo,     LDO_600,  0),
	LDO(L7,   "8921_l7",   "8921_l7_pc",  pldo,     LDO_150,  0),
	LDO(L8,   "8921_l8",   "8921_l8_pc",  pldo,     LDO_300,  0),
	LDO(L9,   "8921_l9",   "8921_l9_pc",  pldo,     LDO_300,  0),
	LDO(L10,  "8921_l10",  "8921_l10_pc", pldo,     LDO_600,  0),
	LDO(L11,  "8921_l11",  "8921_l11_pc", pldo,     LDO_150,  0),
	LDO(L12,  "8921_l12",  "8921_l12_pc", nldo,     LDO_150,  1),
	LDO(L13,  "8921_l13",  NULL,          ln_ldo,   LDO_5,    0),
	LDO(L14,  "8921_l14",  "8921_l14_pc", pldo,     LDO_50,   0),
	LDO(L15,  "8921_l15",  "8921_l15_pc", pldo,     LDO_150,  0),
	LDO(L16,  "8921_l16",  "8921_l16_pc", pldo,     LDO_300,  0),
	LDO(L17,  "8921_l17",  "8921_l17_pc", pldo,     LDO_150,  0),
	LDO(L18,  "8921_l18",  "8921_l18_pc", nldo,     LDO_150,  1),
	LDO(L21,  "8921_l21",  "8921_l21_pc", pldo,     LDO_150,  0),
	LDO(L22,  "8921_l22",  "8921_l22_pc", pldo,     LDO_150,  0),
	LDO(L23,  "8921_l23",  "8921_l23_pc", pldo,     LDO_150,  0),
	LDO(L24,  "8921_l24",  NULL,          nldo1200, LDO_1200, 0),
	LDO(L25,  "8921_l25",  NULL,          nldo1200, LDO_1200, 0),
	LDO(L26,  "8921_l26",  NULL,          nldo1200, LDO_1200, 0),
	LDO(L27,  "8921_l27",  NULL,          nldo1200, LDO_1200, 0),
	LDO(L28,  "8921_l28",  NULL,          nldo1200, LDO_1200, 0),
	LDO(L29,  "8921_l29",  "8921_l29_pc", pldo,     LDO_150,  0),

	SMPS(S1,  "8921_s1",   "8921_s1_pc",  smps,     SMPS_1500),
	SMPS(S2,  "8921_s2",   "8921_s2_pc",  smps,     SMPS_1500),
	SMPS(S3,  "8921_s3",   "8921_s3_pc",  smps,     SMPS_1500),
	SMPS(S4,  "8921_s4",   "8921_s4_pc",  smps,     SMPS_1500),
	SMPS(S5,  "8921_s5",   NULL,          ftsmps,   SMPS_2000),
	SMPS(S6,  "8921_s6",   NULL,          ftsmps,   SMPS_2000),
	SMPS(S7,  "8921_s7",   "8921_s7_pc",  smps,     SMPS_1500),
	SMPS(S8,  "8921_s8",   "8921_s8_pc",  smps,     SMPS_1500),

	LVS(LVS1, "8921_lvs1", "8921_lvs1_pc"),
	LVS(LVS2, "8921_lvs2", NULL),
	LVS(LVS3, "8921_lvs3", "8921_lvs3_pc"),
	LVS(LVS4, "8921_lvs4", "8921_lvs4_pc"),
	LVS(LVS5, "8921_lvs5", "8921_lvs5_pc"),
	LVS(LVS6, "8921_lvs6", "8921_lvs6_pc"),
	LVS(LVS7, "8921_lvs7", "8921_lvs7_pc"),
	MVS(USB_OTG,  "8921_usb_otg",  NULL, USB_OTG_SWITCH),
	MVS(HDMI_MVS, "8921_hdmi_mvs", NULL, HDMI_SWITCH),

	NCP(NCP,  "8921_ncp",  NULL),
};

static const char *pin_func_label[] = {
	[RPM_VREG_PIN_FN_8960_DONT_CARE]	= "don't care",
	[RPM_VREG_PIN_FN_8960_ENABLE]		= "on/off",
	[RPM_VREG_PIN_FN_8960_MODE]		= "HPM/LPM",
	[RPM_VREG_PIN_FN_8960_SLEEP_B]		= "sleep_b",
	[RPM_VREG_PIN_FN_8960_NONE]		= "none",
};

static const char *force_mode_label[] = {
	[RPM_VREG_FORCE_MODE_8960_NONE]		= "none",
	[RPM_VREG_FORCE_MODE_8960_LPM]		= "LPM",
	[RPM_VREG_FORCE_MODE_8960_AUTO]		= "auto",
	[RPM_VREG_FORCE_MODE_8960_HPM]		= "HPM",
	[RPM_VREG_FORCE_MODE_8960_BYPASS]	= "BYP",
};

static const char *power_mode_label[] = {
	[RPM_VREG_POWER_MODE_8960_HYSTERETIC]	= "HYS",
	[RPM_VREG_POWER_MODE_8960_PWM]		= "PWM",
};

static const char *pin_control_label[] = {
	" D1",
	" A0",
	" A1",
	" A2",
};

static int is_real_id(int id)
{
	return (id >= 0) && (id <= RPM_VREG_ID_PM8921_MAX_REAL);
}

static int pc_id_to_real_id(int id)
{
	int real_id;

	if (id >= RPM_VREG_ID_PM8921_L1_PC && id <= RPM_VREG_ID_PM8921_L12_PC)
		real_id = id - RPM_VREG_ID_PM8921_L1_PC;
	else if (id >= RPM_VREG_ID_PM8921_L14_PC
			&& id <= RPM_VREG_ID_PM8921_L23_PC)
		real_id = id - RPM_VREG_ID_PM8921_L14_PC
				+ RPM_VREG_ID_PM8921_L14;
	else if (id >= RPM_VREG_ID_PM8921_L29_PC
			&& id <= RPM_VREG_ID_PM8921_S4_PC)
		real_id = id - RPM_VREG_ID_PM8921_L29_PC
				+ RPM_VREG_ID_PM8921_L29;
	else if (id >= RPM_VREG_ID_PM8921_S7_PC
			&& id <= RPM_VREG_ID_PM8921_LVS1_PC)
		real_id = id - RPM_VREG_ID_PM8921_S7_PC + RPM_VREG_ID_PM8921_S7;
	else
		real_id = id - RPM_VREG_ID_PM8921_LVS3_PC
				+ RPM_VREG_ID_PM8921_LVS3;

	return real_id;
}

static struct vreg_config config = {
	.vregs			= vregs,
	.vregs_len		= ARRAY_SIZE(vregs),

	.vreg_id_min		= RPM_VREG_ID_PM8921_L1,
	.vreg_id_max		= RPM_VREG_ID_PM8921_MAX,

	.pin_func_none		= RPM_VREG_PIN_FN_8960_NONE,
	.pin_func_sleep_b	= RPM_VREG_PIN_FN_8960_SLEEP_B,

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

	.is_real_id		= is_real_id,
	.pc_id_to_real_id	= pc_id_to_real_id,
};

struct vreg_config *get_config_8960(void)
{
	return &config;
}
