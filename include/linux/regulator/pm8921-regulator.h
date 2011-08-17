/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#ifndef __PM8921_REGULATOR_H__
#define __PM8921_REGULATOR_H__

#include <linux/regulator/machine.h>

#define PM8921_REGULATOR_DEV_NAME	"pm8921-regulator"

/**
 * enum pm8921_vreg_id - PMIC 8921 regulator ID numbers
 */
enum pm8921_vreg_id {
	PM8921_VREG_ID_L1 = 0,
	PM8921_VREG_ID_L2,
	PM8921_VREG_ID_L3,
	PM8921_VREG_ID_L4,
	PM8921_VREG_ID_L5,
	PM8921_VREG_ID_L6,
	PM8921_VREG_ID_L7,
	PM8921_VREG_ID_L8,
	PM8921_VREG_ID_L9,
	PM8921_VREG_ID_L10,
	PM8921_VREG_ID_L11,
	PM8921_VREG_ID_L12,
	PM8921_VREG_ID_L14,
	PM8921_VREG_ID_L15,
	PM8921_VREG_ID_L16,
	PM8921_VREG_ID_L17,
	PM8921_VREG_ID_L18,
	PM8921_VREG_ID_L21,
	PM8921_VREG_ID_L22,
	PM8921_VREG_ID_L23,
	PM8921_VREG_ID_L24,
	PM8921_VREG_ID_L25,
	PM8921_VREG_ID_L26,
	PM8921_VREG_ID_L27,
	PM8921_VREG_ID_L28,
	PM8921_VREG_ID_L29,
	PM8921_VREG_ID_S1,
	PM8921_VREG_ID_S2,
	PM8921_VREG_ID_S3,
	PM8921_VREG_ID_S4,
	PM8921_VREG_ID_S5,
	PM8921_VREG_ID_S6,
	PM8921_VREG_ID_S7,
	PM8921_VREG_ID_S8,
	PM8921_VREG_ID_LVS1,
	PM8921_VREG_ID_LVS2,
	PM8921_VREG_ID_LVS3,
	PM8921_VREG_ID_LVS4,
	PM8921_VREG_ID_LVS5,
	PM8921_VREG_ID_LVS6,
	PM8921_VREG_ID_LVS7,
	PM8921_VREG_ID_USB_OTG,
	PM8921_VREG_ID_HDMI_MVS,
	PM8921_VREG_ID_NCP,
	/* The following are IDs for regulator devices to enable pin control. */
	PM8921_VREG_ID_L1_PC,
	PM8921_VREG_ID_L2_PC,
	PM8921_VREG_ID_L3_PC,
	PM8921_VREG_ID_L4_PC,
	PM8921_VREG_ID_L5_PC,
	PM8921_VREG_ID_L6_PC,
	PM8921_VREG_ID_L7_PC,
	PM8921_VREG_ID_L8_PC,
	PM8921_VREG_ID_L9_PC,
	PM8921_VREG_ID_L10_PC,
	PM8921_VREG_ID_L11_PC,
	PM8921_VREG_ID_L12_PC,
	PM8921_VREG_ID_L14_PC,
	PM8921_VREG_ID_L15_PC,
	PM8921_VREG_ID_L16_PC,
	PM8921_VREG_ID_L17_PC,
	PM8921_VREG_ID_L18_PC,
	PM8921_VREG_ID_L21_PC,
	PM8921_VREG_ID_L22_PC,
	PM8921_VREG_ID_L23_PC,

	PM8921_VREG_ID_L29_PC,
	PM8921_VREG_ID_S1_PC,
	PM8921_VREG_ID_S2_PC,
	PM8921_VREG_ID_S3_PC,
	PM8921_VREG_ID_S4_PC,

	PM8921_VREG_ID_S7_PC,
	PM8921_VREG_ID_S8_PC,
	PM8921_VREG_ID_LVS1_PC,

	PM8921_VREG_ID_LVS3_PC,
	PM8921_VREG_ID_LVS4_PC,
	PM8921_VREG_ID_LVS5_PC,
	PM8921_VREG_ID_LVS6_PC,
	PM8921_VREG_ID_LVS7_PC,

	PM8921_VREG_ID_MAX,
};

/* Pin control input pins. */
#define PM8921_VREG_PIN_CTRL_NONE	0x00
#define PM8921_VREG_PIN_CTRL_D1		0x01
#define PM8921_VREG_PIN_CTRL_A0		0x02
#define PM8921_VREG_PIN_CTRL_A1		0x04
#define PM8921_VREG_PIN_CTRL_A2		0x08

/* Minimum high power mode loads in uA. */
#define PM8921_VREG_LDO_50_HPM_MIN_LOAD		5000
#define PM8921_VREG_LDO_150_HPM_MIN_LOAD	10000
#define PM8921_VREG_LDO_300_HPM_MIN_LOAD	10000
#define PM8921_VREG_LDO_600_HPM_MIN_LOAD	10000
#define PM8921_VREG_LDO_1200_HPM_MIN_LOAD	10000
#define PM8921_VREG_SMPS_1500_HPM_MIN_LOAD	100000
#define PM8921_VREG_SMPS_2000_HPM_MIN_LOAD	100000

/**
 * enum pm8921_vreg_pin_function - action to perform when pin control is active
 * %PM8921_VREG_PIN_FN_ENABLE:	pin control enables the regulator
 * %PM8921_VREG_PIN_FN_MODE:	pin control changes mode from LPM to HPM
 */
enum pm8921_vreg_pin_function {
	PM8921_VREG_PIN_FN_ENABLE = 0,
	PM8921_VREG_PIN_FN_MODE,
};

/**
 * struct pm8921_regulator_platform_data - PMIC 8921 regulator platform data
 * @init_data:		regulator constraints
 * @id:			regulator id; from enum pm8921_vreg_id
 * @pull_down_enable:	0 = no pulldown, 1 = pulldown when regulator disabled
 * @pin_ctrl:		pin control inputs to use for the regulator; should be
 *			a combination of PM8921_VREG_PIN_CTRL_* values
 * @pin_fn:		action to perform when pin control pin is active
 * @system_uA:		current drawn from regulator not accounted for by any
 *			regulator framework consumer
 */
struct pm8921_regulator_platform_data {
	struct regulator_init_data	init_data;
	enum pm8921_vreg_id		id;
	unsigned			pull_down_enable;
	unsigned			pin_ctrl;
	enum pm8921_vreg_pin_function	pin_fn;
	int				system_uA;
};

#endif
