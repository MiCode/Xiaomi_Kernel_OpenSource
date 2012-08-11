/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_INCLUDE_MACH_RPM_REGULATOR_8930_H
#define __ARCH_ARM_MACH_MSM_INCLUDE_MACH_RPM_REGULATOR_8930_H

/* Pin control input signals. */
#define RPM_VREG_PIN_CTRL_PM8038_D1	0x01
#define RPM_VREG_PIN_CTRL_PM8038_A0	0x02
#define RPM_VREG_PIN_CTRL_PM8038_A1	0x04
#define RPM_VREG_PIN_CTRL_PM8038_A2	0x08

/**
 * enum rpm_vreg_pin_fn_8930 - RPM regulator pin function choices
 * %RPM_VREG_PIN_FN_8930_DONT_CARE:	do not care about pin control state of
 *					the regulator; allow another master
 *					processor to specify pin control
 * %RPM_VREG_PIN_FN_8930_ENABLE:	pin control switches between disable and
 *					enable
 * %RPM_VREG_PIN_FN_8930_MODE:		pin control switches between LPM and HPM
 * %RPM_VREG_PIN_FN_8930_SLEEP_B:	regulator is forced into LPM when
 *					sleep_b signal is asserted
 * %RPM_VREG_PIN_FN_8930_NONE:		do not use pin control for the regulator
 *					and do not allow another master to
 *					request pin control
 *
 * The pin function specified in platform data corresponds to the active state
 * pin function value.  Pin function will be NONE until a consumer requests
 * pin control to be enabled.
 */
enum rpm_vreg_pin_fn_8930 {
	RPM_VREG_PIN_FN_8930_DONT_CARE,
	RPM_VREG_PIN_FN_8930_ENABLE,
	RPM_VREG_PIN_FN_8930_MODE,
	RPM_VREG_PIN_FN_8930_SLEEP_B,
	RPM_VREG_PIN_FN_8930_NONE,
};

/**
 * enum rpm_vreg_force_mode_8930 - RPM regulator force mode choices
 * %RPM_VREG_FORCE_MODE_8930_PIN_CTRL:	allow pin control usage
 * %RPM_VREG_FORCE_MODE_8930_NONE:	do not force any mode
 * %RPM_VREG_FORCE_MODE_8930_LPM:	force into low power mode
 * %RPM_VREG_FORCE_MODE_8930_AUTO:	allow regulator to automatically select
 *					its own mode based on realtime current
 *					draw (only available for SMPS
 *					regulators)
 * %RPM_VREG_FORCE_MODE_8930_HPM:	force into high power mode
 * %RPM_VREG_FORCE_MODE_8930_BYPASS:	set regulator to use bypass mode, i.e.
 *					to act as a switch and not regulate
 *					(only available for LDO regulators)
 *
 * Force mode is used to override aggregation with other masters and to set
 * special operating modes.
 */
enum rpm_vreg_force_mode_8930 {
	RPM_VREG_FORCE_MODE_8930_PIN_CTRL = 0,
	RPM_VREG_FORCE_MODE_8930_NONE = 0,
	RPM_VREG_FORCE_MODE_8930_LPM,
	RPM_VREG_FORCE_MODE_8930_AUTO,		/* SMPS only */
	RPM_VREG_FORCE_MODE_8930_HPM,
	RPM_VREG_FORCE_MODE_8930_BYPASS,	/* LDO only */
};

/**
 * enum rpm_vreg_power_mode_8930 - power mode for SMPS regulators
 * %RPM_VREG_POWER_MODE_8930_HYSTERETIC: Use hysteretic mode for HPM and when
 *					 usage goes high in AUTO
 * %RPM_VREG_POWER_MODE_8930_PWM:	 Use PWM mode for HPM and when usage
 *					 goes high in AUTO
 */
enum rpm_vreg_power_mode_8930 {
	RPM_VREG_POWER_MODE_8930_HYSTERETIC,
	RPM_VREG_POWER_MODE_8930_PWM,
};

/**
 * enum rpm_vreg_id - RPM regulator ID numbers (both real and pin control)
 */
enum rpm_vreg_id_8930 {
	RPM_VREG_ID_PM8038_L1,
	RPM_VREG_ID_PM8038_L2,
	RPM_VREG_ID_PM8038_L3,
	RPM_VREG_ID_PM8038_L4,
	RPM_VREG_ID_PM8038_L5,
	RPM_VREG_ID_PM8038_L6,
	RPM_VREG_ID_PM8038_L7,
	RPM_VREG_ID_PM8038_L8,
	RPM_VREG_ID_PM8038_L9,
	RPM_VREG_ID_PM8038_L10,
	RPM_VREG_ID_PM8038_L11,
	RPM_VREG_ID_PM8038_L12,
	RPM_VREG_ID_PM8038_L13,
	RPM_VREG_ID_PM8038_L14,
	RPM_VREG_ID_PM8038_L15,
	RPM_VREG_ID_PM8038_L16,
	RPM_VREG_ID_PM8038_L17,
	RPM_VREG_ID_PM8038_L18,
	RPM_VREG_ID_PM8038_L19,
	RPM_VREG_ID_PM8038_L20,
	RPM_VREG_ID_PM8038_L21,
	RPM_VREG_ID_PM8038_L22,
	RPM_VREG_ID_PM8038_L23,
	RPM_VREG_ID_PM8038_L24,
	RPM_VREG_ID_PM8038_L25,
	RPM_VREG_ID_PM8038_L26,
	RPM_VREG_ID_PM8038_L27,
	RPM_VREG_ID_PM8038_S1,
	RPM_VREG_ID_PM8038_S2,
	RPM_VREG_ID_PM8038_S3,
	RPM_VREG_ID_PM8038_S4,
	RPM_VREG_ID_PM8038_S5,
	RPM_VREG_ID_PM8038_S6,
	RPM_VREG_ID_PM8038_LVS1,
	RPM_VREG_ID_PM8038_LVS2,
	RPM_VREG_ID_PM8038_VDD_DIG_CORNER,
	RPM_VREG_ID_PM8038_MAX_REAL = RPM_VREG_ID_PM8038_VDD_DIG_CORNER,

	/* The following are IDs for regulator devices to enable pin control. */
	RPM_VREG_ID_PM8038_L2_PC,
	RPM_VREG_ID_PM8038_L3_PC,
	RPM_VREG_ID_PM8038_L4_PC,
	RPM_VREG_ID_PM8038_L5_PC,
	RPM_VREG_ID_PM8038_L6_PC,
	RPM_VREG_ID_PM8038_L7_PC,
	RPM_VREG_ID_PM8038_L8_PC,
	RPM_VREG_ID_PM8038_L9_PC,
	RPM_VREG_ID_PM8038_L10_PC,
	RPM_VREG_ID_PM8038_L11_PC,
	RPM_VREG_ID_PM8038_L12_PC,
	RPM_VREG_ID_PM8038_L14_PC,
	RPM_VREG_ID_PM8038_L15_PC,
	RPM_VREG_ID_PM8038_L17_PC,
	RPM_VREG_ID_PM8038_L18_PC,
	RPM_VREG_ID_PM8038_L21_PC,
	RPM_VREG_ID_PM8038_L22_PC,
	RPM_VREG_ID_PM8038_L23_PC,
	RPM_VREG_ID_PM8038_L26_PC,
	RPM_VREG_ID_PM8038_S1_PC,
	RPM_VREG_ID_PM8038_S2_PC,
	RPM_VREG_ID_PM8038_S3_PC,
	RPM_VREG_ID_PM8038_S4_PC,
	RPM_VREG_ID_PM8038_LVS1_PC,
	RPM_VREG_ID_PM8038_LVS2_PC,
	RPM_VREG_ID_PM8038_MAX = RPM_VREG_ID_PM8038_LVS2_PC,
};

/* Minimum high power mode loads in uA. */
#define RPM_VREG_8930_LDO_5_HPM_MIN_LOAD		0
#define RPM_VREG_8930_LDO_50_HPM_MIN_LOAD		5000
#define RPM_VREG_8930_LDO_150_HPM_MIN_LOAD		10000
#define RPM_VREG_8930_LDO_300_HPM_MIN_LOAD		10000
#define RPM_VREG_8930_LDO_600_HPM_MIN_LOAD		10000
#define RPM_VREG_8930_LDO_1200_HPM_MIN_LOAD		10000
#define RPM_VREG_8930_SMPS_1500_HPM_MIN_LOAD		100000
#define RPM_VREG_8930_SMPS_2000_HPM_MIN_LOAD		100000

#endif
