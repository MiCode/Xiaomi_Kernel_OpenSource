/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

#ifndef __REGULATOR_PM8XXX_REGULATOR_H__
#define __REGULATOR_PM8XXX_REGULATOR_H__

#include <linux/kernel.h>
#include <linux/regulator/machine.h>

#define PM8XXX_REGULATOR_DEV_NAME	"pm8xxx-regulator"

/* Pin control input pins. */
#define PM8XXX_VREG_PIN_CTRL_NONE	0x00
#define PM8XXX_VREG_PIN_CTRL_EN0	0x01
#define PM8XXX_VREG_PIN_CTRL_EN1	0x02
#define PM8XXX_VREG_PIN_CTRL_EN2	0x04
#define PM8XXX_VREG_PIN_CTRL_EN3	0x08
#define PM8XXX_VREG_PIN_CTRL_ALL	0x0F

#define PM8921_VREG_PIN_CTRL_NONE	PM8XXX_VREG_PIN_CTRL_NONE
#define PM8921_VREG_PIN_CTRL_D1		PM8XXX_VREG_PIN_CTRL_EN0
#define PM8921_VREG_PIN_CTRL_A0		PM8XXX_VREG_PIN_CTRL_EN1
#define PM8921_VREG_PIN_CTRL_A1		PM8XXX_VREG_PIN_CTRL_EN2
#define PM8921_VREG_PIN_CTRL_A2		PM8XXX_VREG_PIN_CTRL_EN3

/* Minimum high power mode loads in uA. */
#define PM8XXX_VREG_LDO_50_HPM_MIN_LOAD		5000
#define PM8XXX_VREG_LDO_150_HPM_MIN_LOAD	10000
#define PM8XXX_VREG_LDO_300_HPM_MIN_LOAD	10000
#define PM8XXX_VREG_LDO_600_HPM_MIN_LOAD	10000
#define PM8XXX_VREG_LDO_1200_HPM_MIN_LOAD	10000
#define PM8XXX_VREG_SMPS_1500_HPM_MIN_LOAD	100000
#define PM8XXX_VREG_SMPS_2000_HPM_MIN_LOAD	100000

#define REGULATOR_TEST_BANKS_MAX		8

/**
 * enum pm8xxx_vreg_pin_function - action to perform when pin control is active
 * %PM8XXX_VREG_PIN_FN_ENABLE:	pin control enables the regulator
 * %PM8XXX_VREG_PIN_FN_MODE:	pin control changes mode from LPM to HPM
 */
enum pm8xxx_vreg_pin_function {
	PM8XXX_VREG_PIN_FN_ENABLE = 0,
	PM8XXX_VREG_PIN_FN_MODE,
};

/**
 * struct pm8xxx_regulator_platform_data - PMIC 8921 regulator platform data
 * @init_data:		regulator constraints
 * @id:			regulator id. Any value unique among pm8xxx_regulator
 *			devices is acceptable.
 * @pull_down_enable:	0 = no pulldown, 1 = pulldown when regulator disabled
 * @pin_ctrl:		pin control inputs to use for the regulator; should be
 *			a combination of PM8XXX_VREG_PIN_CTRL_* values
 * @pin_fn:		action to perform when pin control pin is active
 * @system_uA:		current drawn from regulator not accounted for by any
 *			regulator framework consumer
 * @enable_time:	time in us taken to enable a regulator to the maximum
 *			allowed voltage for the system.  This is dependent upon
 *			the load and capacitance for a regulator on the board.
 * @slew_rate:		worst case rate of change of regulator output voltage
 *			in units of uV/us (V/s).  This is dependent upon the
 *			load and capacitance for a regulator on the board.
 * @ocp_enable:		enable over current protection logic (available for
 *			LVS and MVS type switches)
 * @ocp_enable_time:	time in us to delay between enabling the switch and then
 *			enabling OCP for it.  This delay is needed to avoid
 *			false triggering due to inrush current.
 */
struct pm8xxx_regulator_platform_data {
	struct regulator_init_data	init_data;
	int				id;
	unsigned			pull_down_enable;
	unsigned			pin_ctrl;
	enum pm8xxx_vreg_pin_function	pin_fn;
	int				system_uA;
	int				enable_time;
	int				slew_rate;
	unsigned			ocp_enable;
	int				ocp_enable_time;
};

#endif
