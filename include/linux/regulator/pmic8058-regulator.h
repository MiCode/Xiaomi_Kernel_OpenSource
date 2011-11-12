/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

#ifndef __PMIC8058_REGULATOR_H__
#define __PMIC8058_REGULATOR_H__

#include <linux/regulator/machine.h>

/* Low dropout regulator ids */
#define PM8058_VREG_ID_L0	0
#define PM8058_VREG_ID_L1	1
#define PM8058_VREG_ID_L2	2
#define PM8058_VREG_ID_L3	3
#define PM8058_VREG_ID_L4	4
#define PM8058_VREG_ID_L5	5
#define PM8058_VREG_ID_L6	6
#define PM8058_VREG_ID_L7	7
#define PM8058_VREG_ID_L8	8
#define PM8058_VREG_ID_L9	9
#define PM8058_VREG_ID_L10	10
#define PM8058_VREG_ID_L11	11
#define PM8058_VREG_ID_L12	12
#define PM8058_VREG_ID_L13	13
#define PM8058_VREG_ID_L14	14
#define PM8058_VREG_ID_L15	15
#define PM8058_VREG_ID_L16	16
#define PM8058_VREG_ID_L17	17
#define PM8058_VREG_ID_L18	18
#define PM8058_VREG_ID_L19	19
#define PM8058_VREG_ID_L20	20
#define PM8058_VREG_ID_L21	21
#define PM8058_VREG_ID_L22	22
#define PM8058_VREG_ID_L23	23
#define PM8058_VREG_ID_L24	24
#define PM8058_VREG_ID_L25	25

/* Switched-mode power supply regulator ids */
#define PM8058_VREG_ID_S0	26
#define PM8058_VREG_ID_S1	27
#define PM8058_VREG_ID_S2	28
#define PM8058_VREG_ID_S3	29
#define PM8058_VREG_ID_S4	30

/* Low voltage switch regulator ids */
#define PM8058_VREG_ID_LVS0	31
#define PM8058_VREG_ID_LVS1	32

/* Negative charge pump regulator id */
#define PM8058_VREG_ID_NCP	33

#define PM8058_VREG_MAX		(PM8058_VREG_ID_NCP + 1)

#define PM8058_VREG_PIN_CTRL_NONE	0x00
#define PM8058_VREG_PIN_CTRL_A0		0x01
#define PM8058_VREG_PIN_CTRL_A1		0x02
#define PM8058_VREG_PIN_CTRL_D0		0x04
#define PM8058_VREG_PIN_CTRL_D1		0x08

/* Minimum high power mode loads in uA. */
#define PM8058_VREG_LDO_50_HPM_MIN_LOAD		5000
#define PM8058_VREG_LDO_150_HPM_MIN_LOAD	10000
#define PM8058_VREG_LDO_300_HPM_MIN_LOAD	10000
#define PM8058_VREG_SMPS_HPM_MIN_LOAD		50000

/* Pin ctrl enables/disables or toggles high/low power modes */
enum pm8058_vreg_pin_fn {
	PM8058_VREG_PIN_FN_ENABLE = 0,
	PM8058_VREG_PIN_FN_MODE,
};

struct pm8058_vreg_pdata {
	struct regulator_init_data	init_data;
	int				id;
	unsigned			pull_down_enable;
	unsigned			pin_ctrl;
	enum pm8058_vreg_pin_fn		pin_fn;
};

#endif
