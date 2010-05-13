/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __PMIC8901_REGULATOR_H__
#define __PMIC8901_REGULATOR_H__

#include <linux/regulator/machine.h>

/* Low dropout regulator ids */
#define PM8901_VREG_ID_L0	0
#define PM8901_VREG_ID_L1	1
#define PM8901_VREG_ID_L2	2
#define PM8901_VREG_ID_L3	3
#define PM8901_VREG_ID_L4	4
#define PM8901_VREG_ID_L5	5
#define PM8901_VREG_ID_L6	6

/* Switched-mode power supply regulator ids */
#define PM8901_VREG_ID_S0	7
#define PM8901_VREG_ID_S1	8
#define PM8901_VREG_ID_S2	9
#define PM8901_VREG_ID_S3	10
#define PM8901_VREG_ID_S4	11

/* Low voltage switch regulator ids */
#define PM8901_VREG_ID_LVS0	12
#define PM8901_VREG_ID_LVS1	13
#define PM8901_VREG_ID_LVS2	14
#define PM8901_VREG_ID_LVS3	15

/* Medium voltage switch regulator ids */
#define PM8901_VREG_ID_MVS0	16

/* USB OTG voltage switch regulator ids */
#define PM8901_VREG_ID_USB_OTG	17

/* HDMI medium voltage switch regulator ids */
#define PM8901_VREG_ID_HDMI_MVS	18

#define PM8901_VREG_MAX		(PM8901_VREG_ID_HDMI_MVS + 1)

#define PM8901_VREG_PIN_CTRL_NONE	0x00
#define PM8901_VREG_PIN_CTRL_A0		0x01
#define PM8901_VREG_PIN_CTRL_A1		0x02
#define PM8901_VREG_PIN_CTRL_D0		0x04
#define PM8901_VREG_PIN_CTRL_D1		0x08

/* Minimum high power mode loads in uA. */
#define PM8901_VREG_LDO_300_HPM_MIN_LOAD	10000
#define PM8901_VREG_FTSMPS_HPM_MIN_LOAD		100000

/* Pin ctrl enables/disables or toggles high/low power modes */
enum pm8901_vreg_pin_fn {
	PM8901_VREG_PIN_FN_ENABLE = 0,
	PM8901_VREG_PIN_FN_MODE,
};

struct pm8901_vreg_pdata {
	struct regulator_init_data	init_data;
	int				id;
	unsigned			pull_down_enable;
	unsigned			pin_ctrl;
	enum pm8901_vreg_pin_fn		pin_fn;
};

#endif
