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

#ifndef __ARCH_ARM_MACH_MSM_RPM_REGULATOR_8960_H
#define __ARCH_ARM_MACH_MSM_RPM_REGULATOR_8960_H

/* Pin control input signals. */
#define RPM_VREG_PIN_CTRL_NONE		0x00
#define RPM_VREG_PIN_CTRL_EN0		0x01
#define RPM_VREG_PIN_CTRL_EN1		0x02
#define RPM_VREG_PIN_CTRL_EN2		0x04
#define RPM_VREG_PIN_CTRL_EN3		0x08
#define RPM_VREG_PIN_CTRL_ALL		0x0F

#define RPM_VREG_PIN_CTRL_PM8921_D1	RPM_VREG_PIN_CTRL_EN0
#define RPM_VREG_PIN_CTRL_PM8921_A0	RPM_VREG_PIN_CTRL_EN1
#define RPM_VREG_PIN_CTRL_PM8921_A1	RPM_VREG_PIN_CTRL_EN2
#define RPM_VREG_PIN_CTRL_PM8921_A2	RPM_VREG_PIN_CTRL_EN3

/**
 * enum rpm_vreg_pin_fn - RPM regulator pin function choices
 * %RPM_VREG_PIN_FN_DONT_CARE:	do not care about pin control state of the
 *				regulator; allow another master processor to
 *				specify pin control
 * %RPM_VREG_PIN_FN_ENABLE:	pin control switches between disable and enable
 * %RPM_VREG_PIN_FN_MODE:	pin control switches between LPM and HPM
 * %RPM_VREG_PIN_FN_SLEEP_B:	regulator is forced into LPM when sleep_b signal
 *				is asserted
 * %RPM_VREG_PIN_FN_NONE:	do not use pin control for the regulator and do
 *				not allow another master to request pin control
 *
 * The pin function specified in platform data corresponds to the active state
 * pin function value.  Pin function will be NONE until a consumer requests
 * pin control to be enabled.
 */
enum rpm_vreg_pin_fn {
	RPM_VREG_PIN_FN_DONT_CARE,
	RPM_VREG_PIN_FN_ENABLE,
	RPM_VREG_PIN_FN_MODE,
	RPM_VREG_PIN_FN_SLEEP_B,
	RPM_VREG_PIN_FN_NONE,
};

/**
 * enum rpm_vreg_force_mode - RPM regulator force mode choices
 * %RPM_VREG_FORCE_MODE_PIN_CTRL: allow pin control usage
 * %RPM_VREG_FORCE_MODE_NONE:	  do not force any mode
 * %RPM_VREG_FORCE_MODE_LPM:	  force into low power mode
 * %RPM_VREG_FORCE_MODE_AUTO:	  allow regulator to automatically select its
 *				  own mode based on realtime current draw
 *				  (only available for SMPS regulators)
 * %RPM_VREG_FORCE_MODE_HPM:	  force into high power mode
 * %RPM_VREG_FORCE_MODE_BYPASS:   set regulator to use bypass mode, i.e. to act
 *				  as a switch and not regulate (only available
 *				  for LDO regulators)
 *
 * Force mode is used to override aggregation with other masters and to set
 * special operating modes.
 */
enum rpm_vreg_force_mode {
	RPM_VREG_FORCE_MODE_PIN_CTRL = 0,
	RPM_VREG_FORCE_MODE_NONE = 0,
	RPM_VREG_FORCE_MODE_LPM,
	RPM_VREG_FORCE_MODE_AUTO,		/* SMPS only */
	RPM_VREG_FORCE_MODE_HPM,
	RPM_VREG_FORCE_MODE_BYPASS,		/* LDO only */
};

/**
 * enum rpm_vreg_power_mode - power mode for SMPS regulators
 * %RPM_VREG_POWER_MODE_HYSTERETIC:	Use hysteretic mode for HPM and when
 *					usage goes high in AUTO
 * %RPM_VREG_POWER_MODE_PWM:		Use PWM mode for HPM and when usage goes
 *					high in AUTO
 */
enum rpm_vreg_power_mode {
	RPM_VREG_POWER_MODE_HYSTERETIC,
	RPM_VREG_POWER_MODE_PWM,
};

/**
 * enum rpm_vreg_state - enable state for switch or NCP
 */
enum rpm_vreg_state {
	RPM_VREG_STATE_OFF,
	RPM_VREG_STATE_ON,
};

/**
 * enum rpm_vreg_freq - switching frequency for SMPS or NCP
 */
enum rpm_vreg_freq {
	RPM_VREG_FREQ_NONE,
	RPM_VREG_FREQ_19p20,
	RPM_VREG_FREQ_9p60,
	RPM_VREG_FREQ_6p40,
	RPM_VREG_FREQ_4p80,
	RPM_VREG_FREQ_3p84,
	RPM_VREG_FREQ_3p20,
	RPM_VREG_FREQ_2p74,
	RPM_VREG_FREQ_2p40,
	RPM_VREG_FREQ_2p13,
	RPM_VREG_FREQ_1p92,
	RPM_VREG_FREQ_1p75,
	RPM_VREG_FREQ_1p60,
	RPM_VREG_FREQ_1p48,
	RPM_VREG_FREQ_1p37,
	RPM_VREG_FREQ_1p28,
	RPM_VREG_FREQ_1p20,
};

/**
 * enum rpm_vreg_id - RPM regulator ID numbers (both real and pin control)
 */
enum rpm_vreg_id {
	RPM_VREG_ID_PM8921_L1,
	RPM_VREG_ID_PM8921_L2,
	RPM_VREG_ID_PM8921_L3,
	RPM_VREG_ID_PM8921_L4,
	RPM_VREG_ID_PM8921_L5,
	RPM_VREG_ID_PM8921_L6,
	RPM_VREG_ID_PM8921_L7,
	RPM_VREG_ID_PM8921_L8,
	RPM_VREG_ID_PM8921_L9,
	RPM_VREG_ID_PM8921_L10,
	RPM_VREG_ID_PM8921_L11,
	RPM_VREG_ID_PM8921_L12,
	RPM_VREG_ID_PM8921_L14,
	RPM_VREG_ID_PM8921_L15,
	RPM_VREG_ID_PM8921_L16,
	RPM_VREG_ID_PM8921_L17,
	RPM_VREG_ID_PM8921_L18,
	RPM_VREG_ID_PM8921_L21,
	RPM_VREG_ID_PM8921_L22,
	RPM_VREG_ID_PM8921_L23,
	RPM_VREG_ID_PM8921_L24,
	RPM_VREG_ID_PM8921_L25,
	RPM_VREG_ID_PM8921_L26,
	RPM_VREG_ID_PM8921_L27,
	RPM_VREG_ID_PM8921_L28,
	RPM_VREG_ID_PM8921_L29,
	RPM_VREG_ID_PM8921_S1,
	RPM_VREG_ID_PM8921_S2,
	RPM_VREG_ID_PM8921_S3,
	RPM_VREG_ID_PM8921_S4,
	RPM_VREG_ID_PM8921_S5,
	RPM_VREG_ID_PM8921_S6,
	RPM_VREG_ID_PM8921_S7,
	RPM_VREG_ID_PM8921_S8,
	RPM_VREG_ID_PM8921_LVS1,
	RPM_VREG_ID_PM8921_LVS2,
	RPM_VREG_ID_PM8921_LVS3,
	RPM_VREG_ID_PM8921_LVS4,
	RPM_VREG_ID_PM8921_LVS5,
	RPM_VREG_ID_PM8921_LVS6,
	RPM_VREG_ID_PM8921_LVS7,
	RPM_VREG_ID_PM8921_USB_OTG,
	RPM_VREG_ID_PM8921_HDMI_MVS,
	RPM_VREG_ID_PM8921_NCP,
	RPM_VREG_ID_PM8921_MAX_REAL = RPM_VREG_ID_PM8921_NCP,

	/* The following are IDs for regulator devices to enable pin control. */
	RPM_VREG_ID_PM8921_L1_PC,
	RPM_VREG_ID_PM8921_L2_PC,
	RPM_VREG_ID_PM8921_L3_PC,
	RPM_VREG_ID_PM8921_L4_PC,
	RPM_VREG_ID_PM8921_L5_PC,
	RPM_VREG_ID_PM8921_L6_PC,
	RPM_VREG_ID_PM8921_L7_PC,
	RPM_VREG_ID_PM8921_L8_PC,
	RPM_VREG_ID_PM8921_L9_PC,
	RPM_VREG_ID_PM8921_L10_PC,
	RPM_VREG_ID_PM8921_L11_PC,
	RPM_VREG_ID_PM8921_L12_PC,
	RPM_VREG_ID_PM8921_L14_PC,
	RPM_VREG_ID_PM8921_L15_PC,
	RPM_VREG_ID_PM8921_L16_PC,
	RPM_VREG_ID_PM8921_L17_PC,
	RPM_VREG_ID_PM8921_L18_PC,
	RPM_VREG_ID_PM8921_L21_PC,
	RPM_VREG_ID_PM8921_L22_PC,
	RPM_VREG_ID_PM8921_L23_PC,

	RPM_VREG_ID_PM8921_L29_PC,
	RPM_VREG_ID_PM8921_S1_PC,
	RPM_VREG_ID_PM8921_S2_PC,
	RPM_VREG_ID_PM8921_S3_PC,
	RPM_VREG_ID_PM8921_S4_PC,

	RPM_VREG_ID_PM8921_S7_PC,
	RPM_VREG_ID_PM8921_S8_PC,
	RPM_VREG_ID_PM8921_LVS1_PC,

	RPM_VREG_ID_PM8921_LVS3_PC,
	RPM_VREG_ID_PM8921_LVS4_PC,
	RPM_VREG_ID_PM8921_LVS5_PC,
	RPM_VREG_ID_PM8921_LVS6_PC,
	RPM_VREG_ID_PM8921_LVS7_PC,

	RPM_VREG_ID_PM8921_MAX = RPM_VREG_ID_PM8921_LVS7_PC,
};

/* Minimum high power mode loads in uA. */
#define RPM_VREG_LDO_50_HPM_MIN_LOAD		5000
#define RPM_VREG_LDO_150_HPM_MIN_LOAD		10000
#define RPM_VREG_LDO_300_HPM_MIN_LOAD		10000
#define RPM_VREG_LDO_600_HPM_MIN_LOAD		10000
#define RPM_VREG_LDO_1200_HPM_MIN_LOAD		10000
#define RPM_VREG_SMPS_1500_HPM_MIN_LOAD		100000
#define RPM_VREG_SMPS_2000_HPM_MIN_LOAD		100000

/**
 * struct rpm_regulator_init_data - RPM regulator initialization data
 * @init_data:		regulator constraints
 * @id:			regulator id; from enum rpm_vreg_id
 * @sleep_selectable:	flag which indicates that regulator should be accessable
 *			by external private API and that spinlocks should be
 *			used instead of mutex locks
 * @system_uA:		current drawn from regulator not accounted for by any
 *			regulator framework consumer
 * @pull_down_enable:	0 = no pulldown, 1 = pulldown when regulator disabled
 * @freq:		enum value representing the switching frequency of an
 *			SMPS or NCP
 * @pin_ctrl:		pin control inputs to use for the regulator; should be
 *			a combination of RPM_VREG_PIN_CTRL_* values
 * @pin_fn:		action to perform when pin control pin(s) is/are active
 * @force_mode:		used to specify a force mode which overrides the votes
 *			of other RPM masters.
 * @default_uV:		initial voltage to set the regulator to if enable is
 *			called before set_voltage (e.g. when boot_on or
 *			always_on is set).
 * @peak_uA:		initial peak load requirement sent in RPM request; used
 *			to determine initial mode.
 * @avg_uA:		average load requirement sent in RPM request
 * @state:		initial enable state sent in RPM request for switch or
 *			NCP
 */
struct rpm_regulator_init_data {
	struct regulator_init_data	init_data;
	enum rpm_vreg_id		id;
	int				sleep_selectable;
	int				system_uA;
	unsigned			pull_down_enable;
	enum rpm_vreg_freq		freq;
	unsigned			pin_ctrl;
	enum rpm_vreg_pin_fn		pin_fn;
	enum rpm_vreg_force_mode	force_mode;
	enum rpm_vreg_power_mode	power_mode;
	int				default_uV;
	unsigned			peak_uA;
	unsigned			avg_uA;
	enum rpm_vreg_state		state;
};

/**
 * struct rpm_regulator_platform_data - RPM regulator platform data
 */
struct rpm_regulator_platform_data {
	struct rpm_regulator_init_data	*init_data;
	int				num_regulators;
};

/**
 * enum rpm_vreg_voter - RPM regulator voter IDs for private APIs
 */
enum rpm_vreg_voter {
	RPM_VREG_VOTER_REG_FRAMEWORK,	/* for internal use only */
	RPM_VREG_VOTER1,		/* for use by the acpu-clock driver */
	RPM_VREG_VOTER2,		/* for use by the acpu-clock driver */
	RPM_VREG_VOTER3,		/* for use by other drivers */
	RPM_VREG_VOTER_COUNT,
};

#endif
